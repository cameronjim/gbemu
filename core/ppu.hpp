#pragma once

#include "interrupts.hpp"
#include "state.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace gb {

inline constexpr uint32_t kLcdWidth = 160;
inline constexpr uint32_t kLcdHeight = 144;

inline constexpr uint16_t kRegLcdc = 0xFF40;
inline constexpr uint16_t kRegStat = 0xFF41;
inline constexpr uint16_t kRegScy = 0xFF42;
inline constexpr uint16_t kRegScx = 0xFF43;
inline constexpr uint16_t kRegLy = 0xFF44;
inline constexpr uint16_t kRegLyc = 0xFF45;
inline constexpr uint16_t kRegBgp = 0xFF47;
inline constexpr uint16_t kRegObp0 = 0xFF48;
inline constexpr uint16_t kRegObp1 = 0xFF49;
inline constexpr uint16_t kRegWy = 0xFF4A;
inline constexpr uint16_t kRegWx = 0xFF4B;
inline constexpr uint16_t kRegVbk = 0xFF4F;
inline constexpr uint16_t kRegBcps = 0xFF68;
inline constexpr uint16_t kRegBcpd = 0xFF69;
inline constexpr uint16_t kRegOcps = 0xFF6A;
inline constexpr uint16_t kRegOcpd = 0xFF6B;
inline constexpr uint16_t kRegOpri = 0xFF6C;

inline constexpr size_t kVramBankSize = 0x2000;
inline constexpr size_t kVramBanks = 2;
// 8 palettes of 4 colors, two bytes each
inline constexpr size_t kPaletteRamSize = 64;

enum class PpuMode : uint8_t { HBlank = 0, VBlank = 1, OamScan = 2, Drawing = 3 };

class Ppu {
public:
    explicit Ppu(InterruptLine& irq) : irq_(irq) {}

    // called per instruction with elapsed t-cycles
    void tick(uint32_t tcycles);

    // cgb only; dmg keeps bank 0 wired and the vbk register dead
    void set_cgb_mode(bool cgb) {
        cgb_ = cgb;
        vram_bank_ = 0;
        opri_ = 0;
    }

    uint8_t read_vram(uint16_t offset) const {
        return vram_[vram_bank_][offset];
    }
    void write_vram(uint16_t offset, uint8_t value) {
        vram_[vram_bank_][offset] = value;
    }
    uint8_t read_oam(uint16_t offset) const {
        return oam_[offset];
    }
    void write_oam(uint16_t offset, uint8_t value) {
        oam_[offset] = value;
    }

    uint8_t read_register(uint16_t addr) const;
    void write_register(uint16_t addr, uint8_t value);

    std::span<const uint8_t> framebuffer() const {
        return framebuffer_;
    }
    // rgb555 bg/window output; filled in cgb mode only, contents undefined in dmg mode
    std::span<const uint16_t> framebuffer_color() const {
        return framebuffer_color_;
    }
    // per-pixel source: low byte tile index, bit 8 set for sprite pixels
    std::span<const uint16_t> tile_ids() const {
        return tile_ids_;
    }
    // debug accessor for the tile viewer
    std::span<const uint8_t> vram() const {
        return vram_[0];
    }
    uint8_t vram_bank() const {
        return vram_bank_;
    }
    PpuMode mode() const {
        return mode_;
    }
    bool lcd_enabled() const {
        return (lcdc_ & 0x80) != 0;
    }
    // completed scanouts, advances at each vblank entry
    uint64_t frame_count() const {
        return frames_;
    }

    static constexpr size_t kStateSize = kVramBanks * kVramBankSize + 0xA0 + 4 + 14 + 2 * kPaletteRamSize + 3;
    void save_state(StateWriter& w) const;
    void load_state(StateReader& r);

private:
    // per-scanline render targets, shared by the bg and window fetch
    struct ScanlineOut {
        // raw 2-bit color index, kept for sprite priority decisions
        std::span<uint8_t> colors;
        std::span<uint16_t> ids;
        // cgb bg attribute bit 7 per pixel, consumed by sprite priority
        std::span<uint8_t> priority;
        std::span<uint16_t> rgb;
    };

    // the bg map and its tile indices always come from bank 0
    uint8_t vram0(uint32_t offset) const {
        return vram_[0][offset];
    }
    // pandocs "lcd color palettes (cgb)": 8 bytes per palette, low byte first, r 0-4, g 5-9, b 10-14
    static uint16_t palette_rgb(std::span<const uint8_t> ram, uint8_t palette, uint8_t index) {
        const size_t off = (static_cast<size_t>(palette) * 4 + index) * 2;
        return static_cast<uint16_t>((ram[off] | (ram[off + 1] << 8)) & 0x7FFF);
    }
    uint16_t bg_rgb(uint8_t palette, uint8_t index) const {
        return palette_rgb(bg_palette_, palette, index);
    }
    uint16_t obj_rgb(uint8_t palette, uint8_t index) const {
        return palette_rgb(obj_palette_, palette, index);
    }

    PpuMode compute_mode() const;
    void enter_mode(PpuMode mode);
    void compare_lyc();
    void render_scanline();
    void fetch_map_pixel(uint16_t map_base, uint8_t map_x, uint8_t map_y, uint32_t out_x,
                         const ScanlineOut& out) const;
    void render_bg(const ScanlineOut& out) const;
    bool render_window(const ScanlineOut& out) const;
    void render_sprites(const ScanlineOut& out, std::span<uint8_t> row) const;
    bool sprite_wins(uint8_t bg_color, uint8_t bg_priority, uint8_t attr) const;

    InterruptLine& irq_;
    std::array<std::array<uint8_t, kVramBankSize>, kVramBanks> vram_{};
    std::array<uint8_t, 0xA0> oam_{};
    std::array<uint8_t, kLcdWidth * kLcdHeight> framebuffer_{};
    std::array<uint16_t, kLcdWidth * kLcdHeight> framebuffer_color_{};
    std::array<uint16_t, kLcdWidth * kLcdHeight> tile_ids_{};
    std::array<uint8_t, kPaletteRamSize> bg_palette_{};
    std::array<uint8_t, kPaletteRamSize> obj_palette_{};
    uint32_t dot_ = 0;
    // presentation metadata only, deliberately not serialized
    uint64_t frames_ = 0;
    uint8_t ly_ = 0;
    uint8_t lyc_ = 0;
    // pandocs power-up values
    uint8_t lcdc_ = 0x91;
    uint8_t bgp_ = 0xFC;
    uint8_t scy_ = 0;
    uint8_t scx_ = 0;
    // hardware leaves obp undefined at power-up
    uint8_t obp0_ = 0x00;
    uint8_t obp1_ = 0x00;
    uint8_t wy_ = 0;
    uint8_t wx_ = 0;
    // internal window line, advances only on lines the window rendered
    uint8_t window_line_ = 0;
    // writable stat bits 3-6 only
    uint8_t stat_enables_ = 0;
    // cpu-visible vram bank; the renderer picks its bank from the bg and oam attributes
    uint8_t vram_bank_ = 0;
    // raw spec bytes: index in bits 0-5, auto-increment in bit 7; bit 6 is unused
    uint8_t bcps_ = 0;
    uint8_t ocps_ = 0;
    // pandocs "obj priority mode": cgb boots to oam-index priority, bit 0 selects the dmg x order
    uint8_t opri_ = 0;
    bool cgb_ = false;
    PpuMode mode_ = PpuMode::OamScan;
};

} // namespace gb
