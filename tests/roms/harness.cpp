#include "gameboy.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <span>
#include <string>
#include <vector>

// headless test rom runner; drives the gameboy facade exactly like a frontend
namespace {

std::vector<uint8_t> read_file(const char* path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        return {};
    }
    const std::streamoff size = file.tellg();
    if (size <= 0 || size > 8 * 1024 * 1024) {
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

uint64_t fnv1a(std::span<const uint8_t> bytes) {
    uint64_t hash = 1469598103934665603ull;
    for (uint8_t b : bytes) {
        hash ^= b;
        hash *= 1099511628211ull;
    }
    return hash;
}

// rgb555 channel to 8 bits, replicating the high bits into the low ones
char expand5(uint16_t channel) {
    return static_cast<char>((channel << 3) | (channel >> 2));
}

// the hashed image: rgb555 little-endian pairs for a cgb cart, shade indices for a dmg one
std::vector<uint8_t> scanout_bytes(const gb::Gameboy& gameboy) {
    if (!gameboy.cgb_mode()) {
        const std::span<const uint8_t> fb = gameboy.framebuffer();
        return std::vector<uint8_t>(fb.begin(), fb.end());
    }
    std::vector<uint8_t> bytes;
    bytes.reserve(gameboy.framebuffer_color().size() * 2);
    for (uint16_t color : gameboy.framebuffer_color()) {
        bytes.push_back(static_cast<uint8_t>(color & 0xFF));
        bytes.push_back(static_cast<uint8_t>(color >> 8));
    }
    return bytes;
}

void write_ppm(const gb::Gameboy& gameboy, const char* path) {
    std::ofstream ppm(path, std::ios::binary);
    ppm << "P6\n160 144\n255\n";
    if (gameboy.cgb_mode()) {
        for (uint16_t color : gameboy.framebuffer_color()) {
            const char px[3] = {expand5(static_cast<uint16_t>(color & 0x1F)),
                                expand5(static_cast<uint16_t>((color >> 5) & 0x1F)),
                                expand5(static_cast<uint16_t>((color >> 10) & 0x1F))};
            ppm.write(px, 3);
        }
        return;
    }
    for (uint8_t index : gameboy.framebuffer()) {
        const char v = static_cast<char>(255 - index * 85);
        const char px[3] = {v, v, v};
        ppm.write(px, 3);
    }
}

// framebuffer channel: run a fixed frame count, hash the scanout, dump a ppm on mismatch
int run_framebuffer(gb::Gameboy& gameboy, const std::string& expect, uint64_t frames, const char* rom_path) {
    for (uint64_t i = 0; i < frames; ++i) {
        gameboy.run_frame();
    }
    const std::vector<uint8_t> image = scanout_bytes(gameboy);
    char got[17];
    std::snprintf(got, sizeof(got), "%016llx", static_cast<unsigned long long>(fnv1a(image)));
    if (expect == got) {
        std::printf("PASS %s\n", rom_path);
        return 0;
    }
    write_ppm(gameboy, "framebuffer-mismatch.ppm");
    std::fprintf(stderr, "FAIL %s\nexpected %s got %s, wrote framebuffer-mismatch.ppm\n", rom_path,
                 expect.c_str(), got);
    return 1;
}

// mooneye protocol: on pass registers hold the fibonacci sequence, on fail 0x42 everywhere
int run_fibonacci(gb::Gameboy& gameboy, uint64_t budget, const char* rom_path) {
    uint64_t last_cycles = ~0ull;
    while (gameboy.cycles() < budget && gameboy.cycles() != last_cycles) {
        last_cycles = gameboy.cycles();
        gameboy.run_frame();
        const gb::CpuRegs& r = gameboy.debug_regs();
        if (r.b == 3 && r.c == 5 && r.d == 8 && r.e == 13 && r.h == 21 && r.l == 34) {
            std::printf("PASS %s\n", rom_path);
            return 0;
        }
        if (r.b == 0x42 && r.c == 0x42 && r.d == 0x42 && r.e == 0x42) {
            break;
        }
    }
    std::fprintf(stderr, "FAIL %s\n", rom_path);
    return 1;
}

int run_serial(gb::Gameboy& gameboy, const std::string& expect, uint64_t budget, const char* rom_path) {
    std::string output;
    gameboy.set_serial_sink([&output](uint8_t b) { output.push_back(static_cast<char>(b)); });
    uint64_t last_cycles = ~0ull;
    while (gameboy.cycles() < budget && gameboy.cycles() != last_cycles) {
        last_cycles = gameboy.cycles();
        gameboy.run_frame();
        if (output.find(expect) != std::string::npos) {
            std::printf("PASS %s\n", rom_path);
            return 0;
        }
        if (output.find("Failed") != std::string::npos) {
            break;
        }
    }
    std::fprintf(stderr, "FAIL %s\nserial output:\n%s\n", rom_path, output.c_str());
    return 1;
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::fprintf(stderr, "usage: gbrom_harness <rom> <channel> <expect> [budget_tcycles]\n");
        return 2;
    }
    const char* rom_path = argv[1];
    const std::string channel = argv[2];
    const std::string expect = argv[3];
    const uint64_t budget = argc > 4 ? std::strtoull(argv[4], nullptr, 10) : 2000000000ull;

    const std::vector<uint8_t> rom = read_file(rom_path);
    if (rom.empty()) {
        std::fprintf(stderr, "cannot read %s\n", rom_path);
        return 2;
    }
    gb::Gameboy gameboy;
    if (!gameboy.load_rom(rom)) {
        std::fprintf(stderr, "load_rom rejected %s\n", rom_path);
        return 2;
    }
    if (channel == "serial") {
        return run_serial(gameboy, expect, budget, rom_path);
    }
    if (channel == "framebuffer") {
        // the budget column carries the frame count for this channel
        return run_framebuffer(gameboy, expect, budget, rom_path);
    }
    if (channel == "fibonacci") {
        return run_fibonacci(gameboy, budget, rom_path);
    }
    std::fprintf(stderr, "channel %s not implemented\n", channel.c_str());
    return 2;
}
