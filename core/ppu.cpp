#include "ppu.hpp"

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
}

void Ppu::render_scanline() {
    uint8_t* row = &framebuffer_[static_cast<uint32_t>(ly_) * kLcdWidth];
    std::span<uint16_t> ids(&tile_ids_[static_cast<uint32_t>(ly_) * kLcdWidth], kLcdWidth);
    for (uint16_t& id : ids) {
        id = 0;
    }
    // raw 2-bit bg/window colors kept for sprite priority decisions
    std::array<uint8_t, kLcdWidth> colors{};
    if ((lcdc_ & 0x01) != 0) {
        render_bg(colors, ids);
        if (render_window(colors, ids)) {
            ++window_line_;
        }
    }
    for (uint32_t x = 0; x < kLcdWidth; ++x) {
        row[x] = static_cast<uint8_t>((bgp_ >> (colors[x] * 2)) & 0x03);
    }
    if ((lcdc_ & 0x02) != 0) {
        render_sprites(colors, std::span<uint8_t>(row, kLcdWidth), ids);
    }
}

void Ppu::render_bg(std::span<uint8_t> colors, std::span<uint16_t> ids) const {
    const uint16_t map_base = (lcdc_ & 0x08) != 0 ? 0x1C00 : 0x1800;
    const bool unsigned_mode = (lcdc_ & 0x10) != 0;
    const uint8_t bgy = static_cast<uint8_t>(scy_ + ly_);
    const uint8_t row_in_tile = bgy & 7;
    for (uint32_t x = 0; x < kLcdWidth; ++x) {
        const uint8_t bgx = static_cast<uint8_t>(scx_ + x);
        const uint8_t tile = vram0(map_base + (bgy / 8) * 32 + (bgx / 8));
        uint16_t tile_addr;
        if (unsigned_mode) {
            tile_addr = static_cast<uint16_t>(tile * 16);
        } else {
            // lcdc bit 4 clear: signed indexing from 0x9000
            tile_addr = static_cast<uint16_t>(0x1000 + static_cast<int8_t>(tile) * 16);
        }
        const uint8_t lo = vram0(tile_addr + row_in_tile * 2);
        const uint8_t hi = vram0(tile_addr + row_in_tile * 2 + 1);
        // bit 7 is the leftmost pixel
        const uint8_t px = bgx & 7;
        colors[x] = static_cast<uint8_t>((((hi >> (7 - px)) & 1) << 1) | ((lo >> (7 - px)) & 1));
        ids[x] = tile;
    }
}

bool Ppu::render_window(std::span<uint8_t> colors, std::span<uint16_t> ids) const {
    if ((lcdc_ & 0x20) == 0 || ly_ < wy_ || wx_ > 166) {
        return false;
    }
    const uint16_t map_base = (lcdc_ & 0x40) != 0 ? 0x1C00 : 0x1800;
    const bool unsigned_mode = (lcdc_ & 0x10) != 0;
    const uint8_t row_in_tile = window_line_ & 7;
    // wx has a fixed -7 offset
    const int start_x = static_cast<int>(wx_) - 7;
    for (int x = std::max(0, start_x); x < static_cast<int>(kLcdWidth); ++x) {
        const uint32_t wx_pixel = static_cast<uint32_t>(x - start_x);
        const uint8_t tile = vram0(map_base + (window_line_ / 8) * 32 + (wx_pixel / 8));
        uint16_t tile_addr;
        if (unsigned_mode) {
            tile_addr = static_cast<uint16_t>(tile * 16);
        } else {
            tile_addr = static_cast<uint16_t>(0x1000 + static_cast<int8_t>(tile) * 16);
        }
        const uint8_t lo = vram0(tile_addr + row_in_tile * 2);
        const uint8_t hi = vram0(tile_addr + row_in_tile * 2 + 1);
        const uint8_t px = wx_pixel & 7;
        colors[static_cast<uint32_t>(x)] =
            static_cast<uint8_t>((((hi >> (7 - px)) & 1) << 1) | ((lo >> (7 - px)) & 1));
        ids[static_cast<uint32_t>(x)] = tile;
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
