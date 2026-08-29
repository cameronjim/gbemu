#include "ppu.hpp"

#include <algorithm>

namespace gb {

namespace {
constexpr uint32_t kDotsPerLine = 456;
constexpr uint8_t kLinesPerFrame = 154;
constexpr uint8_t kFirstVBlankLine = 144;
// v1: fixed mode 3 length
constexpr uint32_t kOamDots = 80;
constexpr uint32_t kDrawEndDot = 80 + 172;
constexpr uint8_t kStatHBlankEnable = 0x08;
constexpr uint8_t kStatVBlankEnable = 0x10;
constexpr uint8_t kStatOamEnable = 0x20;
constexpr uint8_t kStatLycEnable = 0x40;

// pandocs: only data writes auto-increment, and the index wraps inside the 64 bytes
uint8_t advance_palette_index(uint8_t spec) {
    if ((spec & 0x80) == 0) {
        return spec;
    }
    return static_cast<uint8_t>(0x80 | ((spec + 1) & 0x3F));
}
} // namespace

PpuMode Ppu::compute_mode() const {
    if (ly_ >= kFirstVBlankLine) {
        return PpuMode::VBlank;
    }
    if (dot_ < kOamDots) {
        return PpuMode::OamScan;
    }
    if (dot_ < kDrawEndDot) {
        return PpuMode::Drawing;
    }
    return PpuMode::HBlank;
}

void Ppu::tick(uint32_t tcycles) {
    if (!lcd_enabled()) {
        return;
    }
    dot_ += tcycles;
    while (dot_ >= kDotsPerLine) {
        dot_ -= kDotsPerLine;
        ly_ = static_cast<uint8_t>((ly_ + 1) % kLinesPerFrame);
        if (ly_ == 0) {
            window_line_ = 0;
        }
        if (ly_ == kFirstVBlankLine) {
            ++frames_;
            irq_.request(kIntVBlank);
            if ((stat_enables_ & kStatVBlankEnable) != 0) {
                irq_.request(kIntStat);
            }
        }
        compare_lyc();
    }
    const PpuMode mode = compute_mode();
    if (mode != mode_) {
        enter_mode(mode);
    }
}

void Ppu::enter_mode(PpuMode mode) {
    mode_ = mode;
    switch (mode) {
    case PpuMode::Drawing:
        render_scanline();
        break;
    case PpuMode::HBlank:
        if ((stat_enables_ & kStatHBlankEnable) != 0) {
            irq_.request(kIntStat);
        }
        break;
    case PpuMode::OamScan:
        if ((stat_enables_ & kStatOamEnable) != 0) {
            irq_.request(kIntStat);
        }
        break;
    case PpuMode::VBlank:
        break;
    }
}

void Ppu::compare_lyc() {
    if (ly_ == lyc_ && (stat_enables_ & kStatLycEnable) != 0) {
        irq_.request(kIntStat);
    }
}

uint8_t Ppu::read_register(uint16_t addr) const {
    switch (addr) {
    case kRegLcdc:
        return lcdc_;
    case kRegStat: {
        const uint8_t lyc_flag = ly_ == lyc_ ? 0x04 : 0x00;
        const uint8_t mode_bits = lcd_enabled() ? static_cast<uint8_t>(mode_) : 0;
        // bit 7 unused, reads 1
        return static_cast<uint8_t>(0x80 | stat_enables_ | lyc_flag | mode_bits);
    }
    case kRegScy:
        return scy_;
    case kRegScx:
        return scx_;
    case kRegLy:
        return ly_;
    case kRegLyc:
        return lyc_;
    case kRegBgp:
        return bgp_;
    case kRegObp0:
        return obp0_;
    case kRegObp1:
        return obp1_;
    case kRegWy:
        return wy_;
    case kRegWx:
        return wx_;
    case kRegVbk:
        // pandocs: vbk reads back with the unused bits set
        return cgb_ ? static_cast<uint8_t>(0xFE | vram_bank_) : 0xFF;
    case kRegBcps:
        // bit 6 is unused and reads 1
        return cgb_ ? static_cast<uint8_t>(0x40 | bcps_) : 0xFF;
    case kRegOcps:
        return cgb_ ? static_cast<uint8_t>(0x40 | ocps_) : 0xFF;
    case kRegBcpd:
        // v1: no mode 3 access gating, palette reads always see the addressed byte
        return cgb_ ? bg_palette_[bcps_ & 0x3F] : 0xFF;
    case kRegOcpd:
        return cgb_ ? obj_palette_[ocps_ & 0x3F] : 0xFF;
    default:
        return 0xFF;
    }
}

void Ppu::write_register(uint16_t addr, uint8_t value) {
    switch (addr) {
    case kRegLcdc: {
        const bool was_enabled = lcd_enabled();
        lcdc_ = value;
        if (was_enabled && !lcd_enabled()) {
            // lcd off: ly and mode reset, no ppu interrupts
            ly_ = 0;
            dot_ = 0;
            mode_ = PpuMode::HBlank;
        }
        break;
    }
    case kRegStat:
        stat_enables_ = static_cast<uint8_t>(value & 0x78);
        break;
    case kRegScy:
        scy_ = value;
        break;
    case kRegScx:
        scx_ = value;
        break;
    case kRegLy:
        // read-only
        break;
    case kRegLyc:
        lyc_ = value;
        compare_lyc();
        break;
    case kRegBgp:
        bgp_ = value;
        break;
    case kRegObp0:
        obp0_ = value;
        break;
    case kRegObp1:
        obp1_ = value;
        break;
    case kRegWy:
        wy_ = value;
        break;
    case kRegWx:
        wx_ = value;
        break;
    case kRegVbk:
        if (cgb_) {
            vram_bank_ = static_cast<uint8_t>(value & 0x01);
        }
        break;
    case kRegBcps:
        if (cgb_) {
            bcps_ = static_cast<uint8_t>(value & 0xBF);
        }
        break;
    case kRegOcps:
        if (cgb_) {
            ocps_ = static_cast<uint8_t>(value & 0xBF);
        }
        break;
    case kRegBcpd:
        // v1: no mode 3 access gating, palette writes always land
        if (cgb_) {
            bg_palette_[bcps_ & 0x3F] = value;
            bcps_ = advance_palette_index(bcps_);
        }
        break;
    case kRegOcpd:
        if (cgb_) {
            obj_palette_[ocps_ & 0x3F] = value;
            ocps_ = advance_palette_index(ocps_);
        }
        break;
    default:
        break;
    }
}

void Ppu::save_state(StateWriter& w) const {
    for (const auto& bank : vram_) {
        w.bytes(bank);
    }
    w.bytes(oam_);
    w.u32(dot_);
    w.u8(ly_);
    w.u8(lyc_);
    w.u8(lcdc_);
    w.u8(bgp_);
    w.u8(scy_);
    w.u8(scx_);
    w.u8(obp0_);
    w.u8(obp1_);
    w.u8(wy_);
    w.u8(wx_);
    w.u8(window_line_);
    w.u8(stat_enables_);
    w.u8(vram_bank_);
    w.u8(static_cast<uint8_t>(mode_));
    w.bytes(bg_palette_);
    w.bytes(obj_palette_);
    w.u8(bcps_);
    w.u8(ocps_);
}

void Ppu::load_state(StateReader& r) {
    for (auto& bank : vram_) {
        r.bytes(bank);
    }
    r.bytes(oam_);
    dot_ = r.u32() % 456;
    ly_ = static_cast<uint8_t>(r.u8() % 154);
    lyc_ = r.u8();
    lcdc_ = r.u8();
    bgp_ = r.u8();
    scy_ = r.u8();
    scx_ = r.u8();
    obp0_ = r.u8();
    obp1_ = r.u8();
    wy_ = r.u8();
    wx_ = r.u8();
    window_line_ = static_cast<uint8_t>(r.u8() % 154);
    stat_enables_ = static_cast<uint8_t>(r.u8() & 0x78);
    const uint8_t vram_bank = static_cast<uint8_t>(r.u8() & 0x01);
    // dmg has no vbk; bank 0 is its only legal value
    vram_bank_ = cgb_ ? vram_bank : 0;
    mode_ = static_cast<PpuMode>(r.u8() & 0x03);
    r.bytes(bg_palette_);
    r.bytes(obj_palette_);
    // bit 6 is unused; a tampered index is also masked at every use site
    bcps_ = static_cast<uint8_t>(r.u8() & 0xBF);
    ocps_ = static_cast<uint8_t>(r.u8() & 0xBF);
}

void Ppu::render_scanline() {
    const uint32_t line = static_cast<uint32_t>(ly_) * kLcdWidth;
    uint8_t* row = &framebuffer_[line];
    std::span<uint16_t> ids(&tile_ids_[line], kLcdWidth);
    std::span<uint16_t> rgb(&framebuffer_color_[line], kLcdWidth);
    for (uint32_t x = 0; x < kLcdWidth; ++x) {
        ids[x] = 0;
        rgb[x] = 0;
    }
    // raw 2-bit bg/window colors kept for sprite priority decisions
    std::array<uint8_t, kLcdWidth> colors{};
    std::array<uint8_t, kLcdWidth> priority{};
    const ScanlineOut out{colors, ids, priority, rgb};
    if ((lcdc_ & 0x01) != 0) {
        render_bg(out);
        if (render_window(out)) {
            ++window_line_;
        }
    }
    for (uint32_t x = 0; x < kLcdWidth; ++x) {
        // bgp is inert on cgb, so the shade buffer keeps the raw index there
        row[x] = cgb_ ? colors[x] : static_cast<uint8_t>((bgp_ >> (colors[x] * 2)) & 0x03);
    }
    if ((lcdc_ & 0x02) != 0) {
        render_sprites(colors, std::span<uint8_t>(row, kLcdWidth), ids);
    }
}

void Ppu::fetch_map_pixel(uint16_t map_base, uint8_t map_x, uint8_t map_y, uint32_t out_x,
                          const ScanlineOut& out) const {
    const uint16_t map_addr = static_cast<uint16_t>(map_base + (map_y / 8) * 32 + (map_x / 8));
    const uint8_t tile = vram0(map_addr);
    // pandocs "bg map attributes (cgb)": vram bank 1 mirrors the map with attribute bytes
    const uint8_t attr = cgb_ ? vram_[1][map_addr] : 0;
    uint8_t row_in_tile = map_y & 7;
    if ((attr & 0x40) != 0) {
        row_in_tile = static_cast<uint8_t>(7 - row_in_tile);
    }
    uint16_t tile_addr;
    if ((lcdc_ & 0x10) != 0) {
        tile_addr = static_cast<uint16_t>(tile * 16);
    } else {
        // lcdc bit 4 clear: signed indexing from 0x9000
        tile_addr = static_cast<uint16_t>(0x1000 + static_cast<int8_t>(tile) * 16);
    }
    const auto& bank = vram_[(attr & 0x08) != 0 ? 1 : 0];
    const uint8_t lo = bank[tile_addr + row_in_tile * 2];
    const uint8_t hi = bank[tile_addr + row_in_tile * 2 + 1];
    uint8_t px = map_x & 7;
    if ((attr & 0x20) != 0) {
        px = static_cast<uint8_t>(7 - px);
    }
    // bit 7 is the leftmost pixel
    const uint8_t shift = static_cast<uint8_t>(7 - px);
    out.colors[out_x] = static_cast<uint8_t>((((hi >> shift) & 1) << 1) | ((lo >> shift) & 1));
    out.ids[out_x] = tile;
    out.priority[out_x] = static_cast<uint8_t>((attr >> 7) & 1);
    out.rgb[out_x] = bg_rgb(static_cast<uint8_t>(attr & 0x07), out.colors[out_x]);
}

void Ppu::render_bg(const ScanlineOut& out) const {
    const uint16_t map_base = (lcdc_ & 0x08) != 0 ? 0x1C00 : 0x1800;
    const uint8_t bgy = static_cast<uint8_t>(scy_ + ly_);
    for (uint32_t x = 0; x < kLcdWidth; ++x) {
        fetch_map_pixel(map_base, static_cast<uint8_t>(scx_ + x), bgy, x, out);
    }
}

bool Ppu::render_window(const ScanlineOut& out) const {
    if ((lcdc_ & 0x20) == 0 || ly_ < wy_ || wx_ > 166) {
        return false;
    }
    const uint16_t map_base = (lcdc_ & 0x40) != 0 ? 0x1C00 : 0x1800;
    // wx has a fixed -7 offset
    const int start_x = static_cast<int>(wx_) - 7;
    for (int x = std::max(0, start_x); x < static_cast<int>(kLcdWidth); ++x) {
        const uint8_t wx_pixel = static_cast<uint8_t>(x - start_x);
        fetch_map_pixel(map_base, wx_pixel, window_line_, static_cast<uint32_t>(x), out);
    }
    return true;
}

void Ppu::render_sprites(std::span<const uint8_t> colors, std::span<uint8_t> row,
                         std::span<uint16_t> ids) const {
    const uint8_t height = (lcdc_ & 0x04) != 0 ? 16 : 8;
    const int line = static_cast<int>(ly_) + 16;
    // mode 2 selection: first 10 by oam order whose y-range covers the line
    std::array<uint8_t, 10> selected{};
    uint32_t count = 0;
    for (uint8_t i = 0; i < 40 && count < 10; ++i) {
        const int oam_y = oam_[i * 4];
        if (line >= oam_y && line < oam_y + height) {
            selected[count++] = i;
        }
    }
    // dmg priority: lower x wins, then earlier oam; draw lowest priority first
    const auto draws_before = [this](uint8_t a, uint8_t b) {
        if (oam_[a * 4 + 1] != oam_[b * 4 + 1]) {
            return oam_[a * 4 + 1] > oam_[b * 4 + 1];
        }
        return a > b;
    };
    for (uint32_t i = 1; i < count; ++i) {
        const uint8_t key = selected[i];
        uint32_t j = i;
        while (j > 0 && draws_before(key, selected[j - 1])) {
            selected[j] = selected[j - 1];
            --j;
        }
        selected[j] = key;
    }
    for (uint32_t s = 0; s < count; ++s) {
        const uint8_t* entry = &oam_[selected[s] * 4];
        const int sprite_x = static_cast<int>(entry[1]) - 8;
        const uint8_t attr = entry[3];
        uint8_t sprite_row = static_cast<uint8_t>(line - entry[0]);
        if ((attr & 0x40) != 0) {
            sprite_row = static_cast<uint8_t>(height - 1 - sprite_row);
        }
        uint8_t tile = entry[2];
        if (height == 16) {
            // 8x16: index low bit ignored
            tile = static_cast<uint8_t>(tile & 0xFE);
            if (sprite_row >= 8) {
                tile = static_cast<uint8_t>(tile + 1);
                sprite_row = static_cast<uint8_t>(sprite_row - 8);
            }
        }
        const uint8_t lo = vram0(tile * 16 + sprite_row * 2);
        const uint8_t hi = vram0(tile * 16 + sprite_row * 2 + 1);
        const uint8_t obp = (attr & 0x10) != 0 ? obp1_ : obp0_;
        for (uint8_t px = 0; px < 8; ++px) {
            const int x = sprite_x + px;
            if (x < 0 || x >= static_cast<int>(kLcdWidth)) {
                continue;
            }
            const uint8_t bit = (attr & 0x20) != 0 ? px : static_cast<uint8_t>(7 - px);
            const uint8_t color = static_cast<uint8_t>((((hi >> bit) & 1) << 1) | ((lo >> bit) & 1));
            if (color == 0) {
                // sprite color 0 is always transparent
                continue;
            }
            if ((attr & 0x80) != 0 && colors[static_cast<uint32_t>(x)] != 0) {
                // bg-over-obj: nonzero bg colors stay on top
                continue;
            }
            row[static_cast<uint32_t>(x)] = static_cast<uint8_t>((obp >> (color * 2)) & 0x03);
            ids[static_cast<uint32_t>(x)] = static_cast<uint16_t>(0x100 | tile);
        }
    }
}

} // namespace gb
