#include "cartridge.hpp"
#include "cpu.hpp"
#include "gameboy.hpp"
#include "palette.hpp"
#include "trace.hpp"

#include <SDL.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr int kScale = 5;
constexpr int kWidth = static_cast<int>(gb::kLcdWidth);
constexpr int kHeight = static_cast<int>(gb::kLcdHeight);
// largest licensed gb rom is 8mb
constexpr std::streamoff kMaxRomFileSize = 8 * 1024 * 1024;

std::vector<uint8_t> read_file(const char* path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        return {};
    }
    const std::streamoff size = file.tellg();
    if (size <= 0 || size > kMaxRomFileSize) {
        return {};
    }
    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(bytes.data()), size);
    if (!file) {
        return {};
    }
    return bytes;
}

const char* cart_type_name(gb::CartType type) {
    switch (type) {
    case gb::CartType::RomOnly:
        return "rom_only";
    case gb::CartType::Mbc1:
        return "mbc1";
    case gb::CartType::Mbc3:
        return "mbc3";
    }
    return "unknown";
}

// which colorizer dresses the frame; unknown games stay plain gray
enum class Look : uint8_t { Plain, Tetris, Crossy, Flappy };

Look detect_look(std::span<const uint8_t> bytes) {
    const std::optional<gb::Cartridge> cart = gb::Cartridge::parse(bytes);
    if (!cart) {
        return Look::Plain;
    }
    if (cart->title() == "CROSSY") {
        return Look::Crossy;
    }
    if (cart->title() == "FLAPPY") {
        return Look::Flappy;
    }
    if (cart->title().rfind("TETRIS", 0) == 0) {
        return Look::Tetris;
    }
    return Look::Plain;
}

uint32_t shade_pixel(Look look, uint16_t id, uint8_t shade, uint32_t x, uint32_t y, uint16_t block_mask,
                     uint16_t sprite_mask) {
    if (look == Look::Crossy) {
        return colorize_crossy(id, shade);
    }
    if (look == Look::Flappy) {
        return colorize_flappy(id, shade);
    }
    return colorize(id, shade, x, y, block_mask, sprite_mask);
}

std::string save_path(const char* rom_path) {
    return std::string(rom_path) + ".sav";
}

void load_battery_ram(gb::Gameboy& gameboy, const char* rom_path) {
    const std::span<uint8_t> ram = gameboy.external_ram();
    if (!gameboy.has_battery() || ram.empty()) {
        return;
    }
    std::ifstream in(save_path(rom_path), std::ios::binary);
    if (!in) {
        return;
    }
    in.read(reinterpret_cast<char*>(ram.data()), static_cast<std::streamsize>(ram.size()));
    std::printf("loaded %s\n", save_path(rom_path).c_str());
}

void save_battery_ram(gb::Gameboy& gameboy, const char* rom_path) {
    const std::span<const uint8_t> ram = gameboy.external_ram();
    if (!gameboy.has_battery() || ram.empty()) {
        return;
    }
    std::ofstream out(save_path(rom_path), std::ios::binary);
    if (!out) {
        return;
    }
    out.write(reinterpret_cast<const char*>(ram.data()), static_cast<std::streamsize>(ram.size()));
}

bool print_cartridge(std::span<const uint8_t> bytes) {
    std::string reason;
    const std::optional<gb::Cartridge> cart = gb::Cartridge::parse(bytes, &reason);
    if (!cart) {
        std::fprintf(stderr, "cartridge rejected: %s\n", reason.c_str());
        return false;
    }
    std::string ram = "none";
    if (cart->ram_size() > 0) {
        ram = std::to_string(cart->ram_size() / 1024) + "KB";
    }
    const unsigned rom_kb = static_cast<unsigned>(cart->rom_size() / 1024);
    std::printf("title=%s type=%s rom=%uKB ram=%s checksum=ok\n", cart->title().c_str(),
                cart_type_name(cart->type()), rom_kb, ram.c_str());
    return true;
}

struct Options {
    const char* rom_path = nullptr;
    const char* doctor_path = nullptr;
    const char* dump_ppm_path = nullptr;
    uint64_t trace_from = 0;
    uint64_t frames = 600;
    int volume = 25;
    bool ok = true;
};

Options parse_args(int argc, char* argv[]) {
    Options opt;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg == "--doctor" && i + 1 < argc) {
            opt.doctor_path = argv[++i];
        } else if (arg == "--trace-from" && i + 1 < argc) {
            opt.trace_from = std::strtoull(argv[++i], nullptr, 10);
        } else if (arg == "--dump-ppm" && i + 1 < argc) {
            opt.dump_ppm_path = argv[++i];
        } else if (arg == "--frames" && i + 1 < argc) {
            opt.frames = std::strtoull(argv[++i], nullptr, 10);
        } else if (arg == "--volume" && i + 1 < argc) {
            opt.volume = std::atoi(argv[++i]);
            opt.volume = opt.volume < 0 ? 0 : (opt.volume > 100 ? 100 : opt.volume);
        } else if (!arg.empty() && arg[0] == '-') {
            opt.ok = false;
        } else {
            opt.rom_path = argv[i];
        }
    }
    return opt;
}

// flat scaffolding memory for doctor runs until the bus lands in milestone 04
struct FlatMemory final : gb::Memory {
    uint8_t read8(uint16_t addr) override {
        return mem[addr];
    }
    void write8(uint16_t addr, uint8_t value) override {
        mem[addr] = value;
    }

    std::array<uint8_t, 0x10000> mem{};
};

bool write_ppm(const gb::Gameboy& gameboy, Look look, uint16_t block_mask, uint16_t sprite_mask,
               const char* path) {
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        std::fprintf(stderr, "cannot open %s\n", path);
        return false;
    }
    out << "P6\n" << gb::kLcdWidth << " " << gb::kLcdHeight << "\n255\n";
    const std::span<const uint8_t> fb = gameboy.framebuffer();
    const std::span<const uint16_t> ids = gameboy.framebuffer_tiles();
    for (uint32_t y = 0; y < gb::kLcdHeight; ++y) {
        for (uint32_t x = 0; x < gb::kLcdWidth; ++x) {
            const size_t i = y * gb::kLcdWidth + x;
            const uint32_t rgb = shade_pixel(look, ids[i], fb[i], x, y, block_mask, sprite_mask);
            const char px[3] = {static_cast<char>(rgb >> 16), static_cast<char>(rgb >> 8),
                                static_cast<char>(rgb)};
            out.write(px, 3);
        }
    }
    std::printf("wrote %s\n", path);
    return true;
}

// window icon: per-game <rom>.icon.bmp wins, else gbemu.bmp beside the executable
void set_window_icon(SDL_Window* window, const char* rom_path) {
    SDL_Surface* icon = nullptr;
    if (rom_path != nullptr) {
        icon = SDL_LoadBMP((std::string(rom_path) + ".icon.bmp").c_str());
    }
    if (icon == nullptr) {
        char* base = SDL_GetBasePath();
        if (base != nullptr) {
            icon = SDL_LoadBMP((std::string(base) + "gbemu.bmp").c_str());
            SDL_free(base);
        }
    }
    if (icon == nullptr) {
        icon = SDL_LoadBMP("gbemu.bmp");
    }
    if (icon == nullptr) {
        return;
    }
    SDL_SetWindowIcon(window, icon);
    SDL_FreeSurface(icon);
}

// second window: all 384 tiles as a 16x24 grid, decoded straight from vram
class TileViewer {
public:
    bool visible() const {
        return window_ != nullptr;
    }
    uint32_t window_id() const {
        return window_ != nullptr ? SDL_GetWindowID(window_) : 0;
    }
    void toggle() {
        if (visible()) {
            close();
            return;
        }
        window_ = SDL_CreateWindow("tiles", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, kTilesW * 3,
                                   kTilesH * 3, 0);
        if (window_ == nullptr) {
            return;
        }
        renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED);
        const uint32_t format = SDL_PIXELFORMAT_ARGB8888;
        texture_ = SDL_CreateTexture(renderer_, format, SDL_TEXTUREACCESS_STREAMING, kTilesW, kTilesH);
    }
    void close() {
        if (texture_ != nullptr) {
            SDL_DestroyTexture(texture_);
            texture_ = nullptr;
        }
        if (renderer_ != nullptr) {
            SDL_DestroyRenderer(renderer_);
            renderer_ = nullptr;
        }
        if (window_ != nullptr) {
            SDL_DestroyWindow(window_);
            window_ = nullptr;
        }
    }
    void render(std::span<const uint8_t> vram) {
        if (!visible() || renderer_ == nullptr || texture_ == nullptr) {
            return;
        }
        constexpr std::array<uint32_t, 4> kGrays = {0xFF000000u, 0xFF4C4C55u, 0xFF9C9CA8u, 0xFFFFFFFFu};
        for (uint32_t tile = 0; tile < 384; ++tile) {
            const uint32_t tx = (tile % 16) * 8;
            const uint32_t ty = (tile / 16) * 8;
            for (uint32_t row = 0; row < 8; ++row) {
                const uint8_t lo = vram[tile * 16 + row * 2];
                const uint8_t hi = vram[tile * 16 + row * 2 + 1];
                for (uint32_t px = 0; px < 8; ++px) {
                    const uint8_t color =
                        static_cast<uint8_t>((((hi >> (7 - px)) & 1) << 1) | ((lo >> (7 - px)) & 1));
                    pixels_[(ty + row) * kTilesW + tx + px] = kGrays[color];
                }
            }
        }
        SDL_UpdateTexture(texture_, nullptr, pixels_.data(), kTilesW * 4);
        SDL_RenderClear(renderer_);
        SDL_RenderCopy(renderer_, texture_, nullptr, nullptr);
        SDL_RenderPresent(renderer_);
    }

private:
    static constexpr int kTilesW = 128;
    static constexpr int kTilesH = 192;
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;
    std::array<uint32_t, kTilesW * kTilesH> pixels_{};
};

int run_doctor(std::span<const uint8_t> rom, const char* out_path, uint64_t trace_from) {
    // cap covers the longest cpu_instrs subtest; logs get huge, flush in blocks
    constexpr uint64_t kMaxInstructions = 8000000;
    constexpr size_t kFlushBytes = 1u << 20;

    FlatMemory flat;
    const size_t n = std::min(rom.size(), flat.mem.size());
    std::copy_n(rom.begin(), n, flat.mem.begin());
    gb::DoctorMemory mem(flat);
    gb::Cpu cpu(mem);
    gb::Trace trace(trace_from);

    std::ofstream out(out_path, std::ios::binary);
    if (!out) {
        std::fprintf(stderr, "cannot open %s\n", out_path);
        return 1;
    }
    std::string buffer;
    buffer.reserve(kFlushBytes + 128);
    for (uint64_t i = 0; i < kMaxInstructions && cpu.status() == gb::CpuStatus::Running; ++i) {
        trace.log(cpu.regs(), mem, buffer);
        cpu.step();
        if (buffer.size() >= kFlushBytes) {
            out.write(buffer.data(), static_cast<std::streamsize>(buffer.size()));
            buffer.clear();
        }
    }
    out.write(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    if (cpu.status() == gb::CpuStatus::Stopped) {
        std::fprintf(stderr, "cpu trapped at pc=%04X opcode=%02X\n", cpu.trap_pc(), cpu.trap_opcode());
    }
    std::printf("doctor log: %llu instructions\n", static_cast<unsigned long long>(trace.count()));
    return 0;
}

// everything the per-frame step touches; static so the emscripten loop outlives main
struct App {
    std::unique_ptr<gb::Gameboy> gameboy;
    Options opt;
    // the 16 block-style tile bitmaps harvested from the loaded rom
    std::array<std::array<uint8_t, 16>, 16> styles{};
    bool have_styles = false;
    Look look = Look::Plain;
    // which button the held esc key is standing in for
    gb::Button esc_button = gb::Button::B;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* texture = nullptr;
    SDL_Window* window = nullptr;
    SDL_AudioDeviceID audio_dev = 0;
    TileViewer tile_viewer;
    std::array<uint32_t, gb::kLcdWidth * gb::kLcdHeight> pixels{};
    std::array<int16_t, 8192> audio_buf{};
    uint64_t frame_count = 0;
    bool paused = false;
    bool running = true;
    // one active pad; dpad and stick are tracked apart so neither can mask a release from the other
    SDL_GameController* controller = nullptr;
    std::array<bool, 4> dpad_held{};
    std::array<bool, 4> stick_held{};
    bool controller_fast_forward = false;
};

App g_app;

void main_loop_step(void* arg);

// fingerprint the block-style bank so only real blocks get colorized
void harvest_styles(App& app, std::span<const uint8_t> rom) {
    // the solid bank of our patched tetris, or style 0 of the stock game;
    // absent in other games, which then render plain white-on-black
    std::array<uint8_t, 16> solid{};
    for (size_t i = 0; i < 16; i += 2) {
        solid[i] = 0x00;
        solid[i + 1] = 0xFF;
    }
    constexpr std::array<uint8_t, 16> kStyle0 = {0xFF, 0xFF, 0xFF, 0x81, 0xFF, 0x81, 0xE7, 0x99,
                                                 0xE7, 0x99, 0xFF, 0x81, 0xFF, 0x81, 0xFF, 0xFF};
    app.have_styles = false;
    for (size_t off = 0; off + 16 * 16 <= rom.size(); ++off) {
        const auto begin = rom.begin() + static_cast<ptrdiff_t>(off);
        const bool solid_bank =
            std::equal(solid.begin(), solid.end(), begin) && std::equal(begin, begin + 16 * 15, begin + 16);
        if (solid_bank || std::equal(kStyle0.begin(), kStyle0.end(), begin)) {
            for (size_t t = 0; t < 16; ++t) {
                std::copy_n(rom.begin() + static_cast<ptrdiff_t>(off + t * 16), 16, app.styles[t].begin());
            }
            app.have_styles = true;
            return;
        }
    }
}

// which tile slots at the given vram base currently hold a block style;
// 0x800 is the bg block bank, 0x000 the falling piece's sprite mirror
uint16_t style_mask(const App& app, size_t base) {
    if (!app.have_styles || app.gameboy == nullptr) {
        return 0;
    }
    const std::span<const uint8_t> vram = app.gameboy->debug_vram();
    uint16_t mask = 0;
    for (size_t slot = 0; slot < 16; ++slot) {
        const size_t at = base + slot * 16;
        for (const std::array<uint8_t, 16>& style : app.styles) {
            if (std::equal(style.begin(), style.end(), vram.begin() + static_cast<ptrdiff_t>(at))) {
                mask = static_cast<uint16_t>(mask | (1u << slot));
                break;
            }
        }
    }
    return mask;
}

// direction buttons are 0-3 in gb::Button, so they index straight into dpad_held/stick_held
size_t dir_index(gb::Button button) {
    return static_cast<size_t>(button);
}

// open the first connected controller, if any and none is open yet
void open_first_controller(App& app) {
    if (app.controller != nullptr) {
        return;
    }
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (SDL_IsGameController(i)) {
            app.controller = SDL_GameControllerOpen(i);
            if (app.controller != nullptr) {
                return;
            }
        }
    }
}

// release everything a controller could hold so a disconnect never leaves a button stuck
void release_controller_buttons(App& app, gb::Gameboy& gameboy) {
    static constexpr std::array<gb::Button, 4> kDirs = {gb::Button::Right, gb::Button::Left, gb::Button::Up,
                                                        gb::Button::Down};
    for (gb::Button dir : kDirs) {
        app.dpad_held[dir_index(dir)] = false;
        app.stick_held[dir_index(dir)] = false;
        gameboy.set_button(dir, false);
    }
    gameboy.set_button(gb::Button::A, false);
    gameboy.set_button(gb::Button::B, false);
    gameboy.set_button(gb::Button::Start, false);
    gameboy.set_button(gb::Button::Select, false);
    app.controller_fast_forward = false;
}

// a direction is held if either the dpad or the stick says so
void set_direction(App& app, gb::Gameboy& gameboy, gb::Button dir) {
    const size_t i = dir_index(dir);
    gameboy.set_button(dir, app.dpad_held[i] || app.stick_held[i]);
}

// left stick as a d-pad; separate press/release thresholds so it doesn't chatter at the edge
void update_stick_axis(App& app, gb::Gameboy& gameboy, uint8_t axis, int16_t value) {
    constexpr int16_t kPress = 12000;
    constexpr int16_t kRelease = 8000;
    gb::Button neg;
    gb::Button pos;
    if (axis == SDL_CONTROLLER_AXIS_LEFTX) {
        neg = gb::Button::Left;
        pos = gb::Button::Right;
    } else if (axis == SDL_CONTROLLER_AXIS_LEFTY) {
        neg = gb::Button::Up;
        pos = gb::Button::Down;
    } else {
        return;
    }
    if (value > kPress) {
        app.stick_held[dir_index(pos)] = true;
    } else if (value < kRelease) {
        app.stick_held[dir_index(pos)] = false;
    }
    if (value < -kPress) {
        app.stick_held[dir_index(neg)] = true;
    } else if (value > -kRelease) {
        app.stick_held[dir_index(neg)] = false;
    }
    set_direction(app, gameboy, pos);
    set_direction(app, gameboy, neg);
}

// right trigger stands in for holding tab: same fast-forward flag, same hysteresis idea
void update_trigger_axis(App& app, int16_t value) {
    constexpr int16_t kPress = 16000;
    constexpr int16_t kRelease = 12000;
    if (value > kPress) {
        app.controller_fast_forward = true;
    } else if (value < kRelease) {
        app.controller_fast_forward = false;
    }
}

std::unique_ptr<gb::Gameboy> make_gameboy(std::span<const uint8_t> bytes) {
    auto gameboy = std::make_unique<gb::Gameboy>();
    if (!gameboy->load_rom(bytes)) {
        return nullptr;
    }
    // test rom output channel
    gameboy->set_serial_sink([](uint8_t b) { std::fputc(b, stdout); });
    // host time injected once; the core stays clock-free
    gameboy->set_rtc_seconds(static_cast<uint64_t>(std::time(nullptr)));
    return gameboy;
}

// debug hook: run headless for n frames, then dump the framebuffer as a ppm
int dump_framebuffer_ppm(App& app, uint64_t frames, const char* path) {
    for (uint64_t i = 0; i < frames; ++i) {
        app.gameboy->run_frame();
    }
    return write_ppm(*app.gameboy, app.look, style_mask(app, 0x800), style_mask(app, 0x000), path) ? 0 : 1;
}

} // namespace

#ifdef __EMSCRIPTEN__
// browser file input lands here through the shell page
extern "C" EMSCRIPTEN_KEEPALIVE void wasm_load_rom(const uint8_t* data, int len) {
    if (data == nullptr || len <= 0) {
        return;
    }
    const std::span<const uint8_t> bytes(data, static_cast<size_t>(len));
    std::unique_ptr<gb::Gameboy> next = make_gameboy(bytes);
    if (next == nullptr) {
        std::printf("rom rejected\n");
        return;
    }
    g_app.gameboy = std::move(next);
    g_app.look = detect_look(bytes);
    harvest_styles(g_app, bytes);
    std::printf("rom loaded\n");
}
#endif

namespace {

int main_impl(int argc, char* argv[]) {
    App& app = g_app;
    app.opt = parse_args(argc, argv);
#ifdef __EMSCRIPTEN__
    // the embedded homebrew demo boots by default
    if (app.opt.rom_path == nullptr) {
        app.opt.rom_path = "demo.gb";
    }
#endif
    const Options& opt = app.opt;
    if (!opt.ok || (opt.doctor_path != nullptr && opt.rom_path == nullptr)) {
        std::fprintf(stderr, "usage: gbemu-sdl [--doctor <path>] [--trace-from <n>] [--dump-ppm <path>] "
                             "[--frames <n>] [--volume 0-100] [rom]\n");
        return 1;
    }

    app.gameboy = std::make_unique<gb::Gameboy>();
    if (opt.rom_path != nullptr) {
        const std::vector<uint8_t> bytes = read_file(opt.rom_path);
        if (bytes.empty()) {
            std::fprintf(stderr, "cannot read %s\n", opt.rom_path);
            return 1;
        }
        if (opt.doctor_path != nullptr) {
            // doctor scaffolding runs on flat memory, so mbc-typed test roms are fine
            return run_doctor(bytes, opt.doctor_path, opt.trace_from);
        }
        if (!print_cartridge(bytes)) {
            return 1;
        }
        app.gameboy = make_gameboy(bytes);
        if (app.gameboy == nullptr) {
            std::fprintf(stderr, "load_rom failed\n");
            return 1;
        }
        app.look = detect_look(bytes);
        harvest_styles(app, bytes);
        load_battery_ram(*app.gameboy, opt.rom_path);
        if (opt.dump_ppm_path != nullptr) {
            return dump_framebuffer_ppm(app, opt.frames, opt.dump_ppm_path);
        }
    }

    // crisp pixels: no dpi stretching, nearest-neighbour scaling
    SDL_SetHint("SDL_WINDOWS_DPI_AWARENESS", "permonitorv2");
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
        std::fprintf(stderr, "sdl init failed: %s\n", SDL_GetError());
        return 1;
    }
    // a pre-plugged pad may not fire a device-added event, so look for one now
    open_first_controller(app);

    SDL_AudioSpec want{};
    want.freq = 48000;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    SDL_AudioSpec have{};
    app.audio_dev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (app.audio_dev != 0) {
        SDL_PauseAudioDevice(app.audio_dev, 0);
    }

    const int pos = SDL_WINDOWPOS_CENTERED;
    app.window = SDL_CreateWindow("gbemu", pos, pos, kWidth * kScale, kHeight * kScale, SDL_WINDOW_RESIZABLE);
    if (app.window == nullptr) {
        std::fprintf(stderr, "sdl window failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    set_window_icon(app.window, opt.rom_path);

    const uint32_t renderer_flags = SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC;
    app.renderer = SDL_CreateRenderer(app.window, -1, renderer_flags);
    if (app.renderer == nullptr) {
        std::fprintf(stderr, "sdl renderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(app.window);
        SDL_Quit();
        return 1;
    }
    // letterboxed scaling at any window size; whole multiples only so pixels stay uniform
    SDL_RenderSetLogicalSize(app.renderer, kWidth, kHeight);
    SDL_RenderSetIntegerScale(app.renderer, SDL_TRUE);

    const uint32_t texture_format = SDL_PIXELFORMAT_ARGB8888;
    const int texture_access = SDL_TEXTUREACCESS_STREAMING;
    app.texture = SDL_CreateTexture(app.renderer, texture_format, texture_access, kWidth, kHeight);
    if (app.texture == nullptr) {
        std::fprintf(stderr, "sdl texture failed: %s\n", SDL_GetError());
        SDL_DestroyRenderer(app.renderer);
        SDL_DestroyWindow(app.window);
        SDL_Quit();
        return 1;
    }
#ifdef __EMSCRIPTEN__
    // browsers forbid blocking loops
    emscripten_set_main_loop_arg(main_loop_step, &app, 0, 1);
#else
    while (app.running) {
        main_loop_step(&app);
    }
#endif

    if (opt.rom_path != nullptr) {
        save_battery_ram(*app.gameboy, opt.rom_path);
    }
    if (app.audio_dev != 0) {
        SDL_CloseAudioDevice(app.audio_dev);
    }
    if (app.controller != nullptr) {
        SDL_GameControllerClose(app.controller);
    }
    app.tile_viewer.close();
    SDL_DestroyTexture(app.texture);
    SDL_DestroyRenderer(app.renderer);
    SDL_DestroyWindow(app.window);
    SDL_Quit();
    return 0;
}

void main_loop_step(void* arg) {
    App& app = *static_cast<App*>(arg);
    if (!app.running) {
        return;
    }
    gb::Gameboy& gameboy = *app.gameboy;
    const Options& opt = app.opt;
    TileViewer& tile_viewer = app.tile_viewer;
    bool& running = app.running;
    bool& paused = app.paused;
    SDL_Event event;
    {
        while (SDL_PollEvent(&event) != 0) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
            if ((event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) && event.key.repeat == 0) {
                const bool down = event.type == SDL_KEYDOWN;
                switch (event.key.keysym.sym) {
                case SDLK_RIGHT:
                case SDLK_d:
                    gameboy.set_button(gb::Button::Right, down);
                    break;
                case SDLK_LEFT:
                case SDLK_a:
                    gameboy.set_button(gb::Button::Left, down);
                    break;
                case SDLK_UP:
                case SDLK_w:
                    gameboy.set_button(gb::Button::Up, down);
                    break;
                case SDLK_DOWN:
                case SDLK_s:
                    gameboy.set_button(gb::Button::Down, down);
                    break;
                case SDLK_f:
                case SDLK_z:
                case SDLK_SPACE:
                    gameboy.set_button(gb::Button::A, down);
                    break;
                case SDLK_g:
                case SDLK_x:
                    gameboy.set_button(gb::Button::B, down);
                    break;
                case SDLK_RETURN:
                case SDLK_e:
                    gameboy.set_button(gb::Button::Start, down);
                    break;
                case SDLK_ESCAPE:
                    // esc backs out of menus with b, opens the pause menu in game
                    if (down) {
                        app.esc_button = style_mask(app, 0x800) == 0 ? gb::Button::B : gb::Button::Start;
                    }
                    gameboy.set_button(app.esc_button, down);
                    break;
                case SDLK_r:
                // windows fakes shift releases around arrow presses, so shift
                // cannot chord with the d-pad; r is the reliable select
                case SDLK_RSHIFT:
                case SDLK_BACKSPACE:
                    gameboy.set_button(gb::Button::Select, down);
                    break;
                default:
                    break;
                }
            }
            if (event.type == SDL_CONTROLLERBUTTONDOWN || event.type == SDL_CONTROLLERBUTTONUP) {
                const bool down = event.type == SDL_CONTROLLERBUTTONDOWN;
                switch (event.cbutton.button) {
                case SDL_CONTROLLER_BUTTON_A:
                    gameboy.set_button(gb::Button::A, down);
                    break;
                case SDL_CONTROLLER_BUTTON_B:
                case SDL_CONTROLLER_BUTTON_X:
                    gameboy.set_button(gb::Button::B, down);
                    break;
                case SDL_CONTROLLER_BUTTON_START:
                    gameboy.set_button(gb::Button::Start, down);
                    break;
                case SDL_CONTROLLER_BUTTON_BACK:
                    gameboy.set_button(gb::Button::Select, down);
                    break;
                case SDL_CONTROLLER_BUTTON_DPAD_UP:
                    app.dpad_held[dir_index(gb::Button::Up)] = down;
                    set_direction(app, gameboy, gb::Button::Up);
                    break;
                case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                    app.dpad_held[dir_index(gb::Button::Down)] = down;
                    set_direction(app, gameboy, gb::Button::Down);
                    break;
                case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
                    app.dpad_held[dir_index(gb::Button::Left)] = down;
                    set_direction(app, gameboy, gb::Button::Left);
                    break;
                case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                    app.dpad_held[dir_index(gb::Button::Right)] = down;
                    set_direction(app, gameboy, gb::Button::Right);
                    break;
                default:
                    break;
                }
            }
            if (event.type == SDL_CONTROLLERAXISMOTION) {
                if (event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX ||
                    event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) {
                    update_stick_axis(app, gameboy, event.caxis.axis, event.caxis.value);
                } else if (event.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT) {
                    update_trigger_axis(app, event.caxis.value);
                }
            }
            if (event.type == SDL_CONTROLLERDEVICEADDED) {
                if (app.controller == nullptr && SDL_IsGameController(event.cdevice.which)) {
                    app.controller = SDL_GameControllerOpen(event.cdevice.which);
                }
            }
            if (event.type == SDL_CONTROLLERDEVICEREMOVED) {
                if (app.controller != nullptr && SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(
                                                     app.controller)) == event.cdevice.which) {
                    SDL_GameControllerClose(app.controller);
                    app.controller = nullptr;
                    release_controller_buttons(app, gameboy);
                    open_first_controller(app);
                }
            }
            if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_F10) {
                    write_ppm(gameboy, app.look, style_mask(app, 0x800), style_mask(app, 0x000),
                              "framebuffer.ppm");
                }
                if (event.key.keysym.sym == SDLK_t) {
                    tile_viewer.toggle();
                }
                if (event.key.keysym.sym == SDLK_p) {
                    paused = !paused;
                }
                if (event.key.keysym.sym == SDLK_F5 && opt.rom_path != nullptr) {
                    std::vector<uint8_t> state;
                    gameboy.save_state(state);
                    std::ofstream out(std::string(opt.rom_path) + ".state", std::ios::binary);
                    out.write(reinterpret_cast<const char*>(state.data()),
                              static_cast<std::streamsize>(state.size()));
                    std::printf("state saved\n");
                }
                if (event.key.keysym.sym == SDLK_F8 && opt.rom_path != nullptr) {
                    const std::string path = std::string(opt.rom_path) + ".state";
                    const std::vector<uint8_t> state = read_file(path.c_str());
                    std::printf(gameboy.load_state(state) ? "state loaded\n" : "state load failed\n");
                }
            }
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE) {
                if (tile_viewer.visible() && event.window.windowID == tile_viewer.window_id()) {
                    tile_viewer.close();
                } else {
                    running = false;
                }
            }
        }

        const bool fast_forward =
            SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_TAB] != 0 || app.controller_fast_forward;
        // audio drives pacing: keep the queue in a 50-100ms band; vsync is presentation only
        const bool audio_paced = app.audio_dev != 0 && opt.rom_path != nullptr;
        if (paused) {
            SDL_Delay(10);
        } else if (fast_forward && opt.rom_path != nullptr) {
            // fast-forward: 4 frames per present, muted
            for (int i = 0; i < 4; ++i) {
                gameboy.run_frame();
            }
            while (gameboy.read_audio(app.audio_buf) > 0) {
            }
        } else if (audio_paced) {
            constexpr uint32_t kTargetBytes = 48000 * 2 * sizeof(int16_t) / 10;
            uint32_t frames_this_pass = 0;
            while (SDL_GetQueuedAudioSize(app.audio_dev) < kTargetBytes && frames_this_pass++ < 8) {
                gameboy.run_frame();
                size_t n;
                while ((n = gameboy.read_audio(app.audio_buf)) > 0) {
                    for (size_t i = 0; i < n; ++i) {
                        app.audio_buf[i] = static_cast<int16_t>(app.audio_buf[i] * opt.volume / 100);
                    }
                    SDL_QueueAudio(app.audio_dev, app.audio_buf.data(),
                                   static_cast<uint32_t>(n * sizeof(int16_t)));
                }
            }
        } else {
            gameboy.run_frame();
        }
        const std::span<const uint8_t> fb = gameboy.framebuffer();
        const std::span<const uint16_t> ids = gameboy.framebuffer_tiles();
        const uint16_t block_mask = style_mask(app, 0x800);
        const uint16_t sprite_mask = style_mask(app, 0x000);
        for (uint32_t y = 0; y < gb::kLcdHeight; ++y) {
            for (uint32_t x = 0; x < gb::kLcdWidth; ++x) {
                const size_t i = y * gb::kLcdWidth + x;
                app.pixels[i] = shade_pixel(app.look, ids[i], fb[i], x, y, block_mask, sprite_mask);
            }
        }

        SDL_UpdateTexture(app.texture, nullptr, app.pixels.data(), kWidth * 4);
        SDL_RenderClear(app.renderer);
        SDL_RenderCopy(app.renderer, app.texture, nullptr, nullptr);
        SDL_RenderPresent(app.renderer);
        tile_viewer.render(gameboy.debug_vram());

        // battery save every ~30s of frames
        ++app.frame_count;
        if (opt.rom_path != nullptr && app.frame_count % 1800 == 0) {
            save_battery_ram(gameboy, opt.rom_path);
        }
    }
}

} // namespace

int main(int argc, char* argv[]) {
    return main_impl(argc, argv);
}
