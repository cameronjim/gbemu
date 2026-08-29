#include "ppu.hpp"

#include "interrupts.hpp"
#include "state.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <vector>

namespace {

struct Rig {
    gb::InterruptLine irq;
    gb::Ppu ppu{irq};

    Rig() {
        irq.write(0);
    }
    uint8_t stat_mode() {
        return static_cast<uint8_t>(ppu.read_register(gb::kRegStat) & 0x03);
    }
    uint8_t ly() {
        return ppu.read_register(gb::kRegLy);
    }
    // write one bg tile row: tile index, row, bitplanes
    void set_tile_row(uint16_t tile_base, uint8_t row, uint8_t lo, uint8_t hi) {
        ppu.write_vram(static_cast<uint16_t>(tile_base + row * 2), lo);
        ppu.write_vram(static_cast<uint16_t>(tile_base + row * 2 + 1), hi);
    }
    // render line 0 by ticking into mode 3
    void render_line0() {
        ppu.tick(80 + 1);
    }
    // instruction-sized ticks so every mode transition renders
    void tick_lines(uint32_t lines) {
        for (uint32_t i = 0; i < lines * 456 / 4; ++i) {
            ppu.tick(4);
        }
    }
    void set_oam(uint8_t index, uint8_t y, uint8_t x, uint8_t tile, uint8_t attr) {
        ppu.write_oam(static_cast<uint16_t>(index * 4), y);
        ppu.write_oam(static_cast<uint16_t>(index * 4 + 1), x);
        ppu.write_oam(static_cast<uint16_t>(index * 4 + 2), tile);
        ppu.write_oam(static_cast<uint16_t>(index * 4 + 3), attr);
    }
    // same row written into vram bank 1
    void set_tile_row_bank1(uint16_t tile_base, uint8_t row, uint8_t lo, uint8_t hi) {
        ppu.write_register(gb::kRegVbk, 0x01);
        set_tile_row(tile_base, row, lo, hi);
        ppu.write_register(gb::kRegVbk, 0x00);
    }
    // bank 1 mirrors the tile map with attribute bytes
    void set_bg_attr(uint16_t map_offset, uint8_t attr) {
        ppu.write_register(gb::kRegVbk, 0x01);
        ppu.write_vram(map_offset, attr);
        ppu.write_register(gb::kRegVbk, 0x00);
    }
    // one bg palette entry through bcps/bcpd, low byte first
    void set_bg_color(uint8_t palette, uint8_t index, uint16_t rgb) {
        ppu.write_register(gb::kRegBcps, static_cast<uint8_t>(0x80 | (palette * 8 + index * 2)));
        ppu.write_register(gb::kRegBcpd, static_cast<uint8_t>(rgb & 0xFF));
        ppu.write_register(gb::kRegBcpd, static_cast<uint8_t>(rgb >> 8));
    }
    uint8_t read_bg_palette(uint8_t index) {
        ppu.write_register(gb::kRegBcps, index);
        return ppu.read_register(gb::kRegBcpd);
    }
    // cgb mode with bg on and unsigned tile addressing
    void cgb_bg_setup() {
        ppu.set_cgb_mode(true);
        ppu.write_register(gb::kRegLcdc, 0x91);
        ppu.write_register(gb::kRegBgp, 0xE4);
    }
    // sprites enabled, bg enabled, unsigned tiles
    void sprite_setup() {
        ppu.write_register(gb::kRegLcdc, 0x93);
        ppu.write_register(gb::kRegBgp, 0xE4);
        ppu.write_register(gb::kRegObp0, 0xE4);
        ppu.write_register(gb::kRegObp1, 0xE4);
    }
};

} // namespace

TEST_CASE("mode_sequence_and_dot_budgets_per_line") {
    Rig rig;
    REQUIRE(rig.stat_mode() == 2);
    rig.ppu.tick(80);
    REQUIRE(rig.stat_mode() == 3);
    rig.ppu.tick(172);
    REQUIRE(rig.stat_mode() == 0);
    rig.ppu.tick(204);
    REQUIRE(rig.ly() == 1);
    REQUIRE(rig.stat_mode() == 2);
}

TEST_CASE("ly_increments_and_wraps_at_154") {
    Rig rig;
    rig.ppu.tick(456 * 153);
    REQUIRE(rig.ly() == 153);
    rig.ppu.tick(456);
    REQUIRE(rig.ly() == 0);
}

TEST_CASE("vblank_interrupt_at_line_144") {
    Rig rig;
    rig.ppu.tick(456 * 144);
    REQUIRE(rig.ly() == 144);
    REQUIRE(rig.stat_mode() == 1);
    REQUIRE((rig.irq.read() & gb::kIntVBlank) != 0);
}

TEST_CASE("lyc_coincidence_sets_stat_bit_and_interrupts_when_enabled") {
    Rig rig;
    rig.ppu.write_register(gb::kRegLyc, 5);
    rig.ppu.write_register(gb::kRegStat, 0x40);
    rig.ppu.tick(456 * 4);
    REQUIRE((rig.ppu.read_register(gb::kRegStat) & 0x04) == 0);
    REQUIRE((rig.irq.read() & gb::kIntStat) == 0);
    rig.ppu.tick(456);
    REQUIRE((rig.ppu.read_register(gb::kRegStat) & 0x04) != 0);
    REQUIRE((rig.irq.read() & gb::kIntStat) != 0);
}

TEST_CASE("bg_scanline_unsigned_addressing") {
    Rig rig;
    // lcdc bit 4 set: tile 1 lives at 0x8010
    rig.ppu.write_register(gb::kRegLcdc, 0x91);
    rig.ppu.write_register(gb::kRegBgp, 0xE4);
    rig.ppu.write_vram(0x1800, 1);
    rig.set_tile_row(0x0010, 0, 0xFF, 0x00);
    rig.render_line0();
    REQUIRE(rig.ppu.framebuffer()[0] == 1);
    REQUIRE(rig.ppu.framebuffer()[7] == 1);
    REQUIRE(rig.ppu.framebuffer()[8] == 0);
}

TEST_CASE("bg_scanline_signed_addressing") {
    Rig rig;
    // lcdc bit 4 clear: index 0xff resolves to 0x9000 - 16 = 0x8ff0
    rig.ppu.write_register(gb::kRegLcdc, 0x81);
    rig.ppu.write_register(gb::kRegBgp, 0xE4);
    rig.ppu.write_vram(0x1800, 0xFF);
    rig.set_tile_row(0x0FF0, 0, 0x00, 0xFF);
    rig.render_line0();
    REQUIRE(rig.ppu.framebuffer()[0] == 2);
    REQUIRE(rig.ppu.framebuffer()[7] == 2);
    REQUIRE(rig.ppu.framebuffer()[8] == 0);
}

TEST_CASE("scx_scy_wraparound") {
    Rig rig;
    rig.ppu.write_register(gb::kRegLcdc, 0x91);
    rig.ppu.write_register(gb::kRegBgp, 0xE4);
    // tile at map cell (31, 31), bottom-right corner of the 256x256 bg
    rig.ppu.write_vram(0x1800 + 31 * 32 + 31, 1);
    rig.set_tile_row(0x0010, 7, 0xFF, 0x00);
    // scroll so bg (248..255, 255) lands at screen x 0..7 of line 0
    rig.ppu.write_register(gb::kRegScx, 248);
    rig.ppu.write_register(gb::kRegScy, 255);
    rig.render_line0();
    REQUIRE(rig.ppu.framebuffer()[0] == 1);
    REQUIRE(rig.ppu.framebuffer()[7] == 1);
    // x 8 wraps to bg x 0, an empty tile
    REQUIRE(rig.ppu.framebuffer()[8] == 0);
}

TEST_CASE("bitplane_merge_bit7_is_leftmost") {
    Rig rig;
    rig.ppu.write_register(gb::kRegLcdc, 0x91);
    rig.ppu.write_register(gb::kRegBgp, 0xE4);
    rig.ppu.write_vram(0x1800, 1);
    // lo bit7 and hi bit7 set: leftmost pixel color 3, rest 0
    rig.set_tile_row(0x0010, 0, 0x80, 0x80);
    rig.render_line0();
    REQUIRE(rig.ppu.framebuffer()[0] == 3);
    REQUIRE(rig.ppu.framebuffer()[1] == 0);
}

TEST_CASE("bgp_palette_applies") {
    Rig rig;
    rig.ppu.write_register(gb::kRegLcdc, 0x91);
    // map color 1 to shade 3
    rig.ppu.write_register(gb::kRegBgp, 0x0C);
    rig.ppu.write_vram(0x1800, 1);
    rig.set_tile_row(0x0010, 0, 0xFF, 0x00);
    rig.render_line0();
    REQUIRE(rig.ppu.framebuffer()[0] == 3);
}

TEST_CASE("oam_offsets_y16_x8") {
    Rig rig;
    rig.sprite_setup();
    rig.set_tile_row(0x0010, 0, 0xFF, 0x00);
    // oam y 16, x 8 puts the sprite at screen (0, 0)
    rig.set_oam(0, 16, 8, 1, 0x00);
    rig.render_line0();
    REQUIRE(rig.ppu.framebuffer()[0] == 1);
    REQUIRE(rig.ppu.framebuffer()[7] == 1);
    REQUIRE(rig.ppu.framebuffer()[8] == 0);
}

TEST_CASE("sprite_limit_first_ten_in_oam_order") {
    Rig rig;
    rig.sprite_setup();
    rig.set_tile_row(0x0010, 0, 0xFF, 0x00);
    for (uint8_t i = 0; i < 11; ++i) {
        rig.set_oam(i, 16, static_cast<uint8_t>(8 + i * 8), 1, 0x00);
    }
    rig.render_line0();
    // sprites 0..9 render, the 11th is dropped
    REQUIRE(rig.ppu.framebuffer()[9 * 8] == 1);
    REQUIRE(rig.ppu.framebuffer()[10 * 8] == 0);
}

TEST_CASE("sprite_color0_transparent") {
    Rig rig;
    rig.sprite_setup();
    // bg color 1 everywhere in tile 2
    rig.ppu.write_vram(0x1800, 2);
    rig.set_tile_row(0x0020, 0, 0xFF, 0x00);
    // sprite tile 1 row: left half color 0, right half color 3
    rig.set_tile_row(0x0010, 0, 0x0F, 0x0F);
    rig.set_oam(0, 16, 8, 1, 0x00);
    rig.render_line0();
    // transparent sprite pixels leave the bg visible
    REQUIRE(rig.ppu.framebuffer()[0] == 1);
    REQUIRE(rig.ppu.framebuffer()[4] == 3);
}

TEST_CASE("bg_over_obj_priority_bit") {
    Rig rig;
    rig.sprite_setup();
    // bg: color 2 in the left tile, color 0 in the next
    rig.ppu.write_vram(0x1800, 2);
    rig.set_tile_row(0x0020, 0, 0x00, 0xFF);
    rig.set_tile_row(0x0010, 0, 0xFF, 0x00);
    rig.set_oam(0, 16, 8, 1, 0x80);
    rig.set_oam(1, 16, 16, 1, 0x80);
    rig.render_line0();
    // nonzero bg wins under the priority bit, zero bg lets the sprite through
    REQUIRE(rig.ppu.framebuffer()[0] == 2);
    REQUIRE(rig.ppu.framebuffer()[8] == 1);
}

TEST_CASE("x_flip_and_y_flip") {
    Rig rig;
    rig.sprite_setup();
    // row 0: left nibble solid; row 7: color 2 row
    rig.set_tile_row(0x0010, 0, 0xF0, 0x00);
    rig.set_tile_row(0x0010, 7, 0x00, 0xFF);
    rig.set_oam(0, 16, 8, 1, 0x20);
    rig.render_line0();
    // x-flip moves the solid nibble right
    REQUIRE(rig.ppu.framebuffer()[0] == 0);
    REQUIRE(rig.ppu.framebuffer()[4] == 1);

    Rig rig2;
    rig2.sprite_setup();
    rig2.set_tile_row(0x0010, 0, 0xF0, 0x00);
    rig2.set_tile_row(0x0010, 7, 0x00, 0xFF);
    rig2.set_oam(0, 16, 8, 1, 0x40);
    rig2.render_line0();
    // y-flip shows tile row 7 on the sprite's first line
    REQUIRE(rig2.ppu.framebuffer()[0] == 2);
}

TEST_CASE("tile_index_low_bit_ignored_in_8x16") {
    Rig rig;
    // 8x16 sprites via lcdc bit 2
    rig.ppu.write_register(gb::kRegLcdc, 0x97);
    rig.ppu.write_register(gb::kRegObp0, 0xE4);
    rig.set_tile_row(0x0020, 0, 0xFF, 0x00);
    rig.set_tile_row(0x0030, 0, 0x00, 0xFF);
    // odd index 3 resolves to tile 2 for the top half
    rig.set_oam(0, 16, 8, 3, 0x00);
    rig.render_line0();
    REQUIRE(rig.ppu.framebuffer()[0] == 1);
}

TEST_CASE("obp0_obp1_palettes_apply") {
    Rig rig;
    rig.sprite_setup();
    // obp1 remaps color 1 to shade 3
    rig.ppu.write_register(gb::kRegObp1, 0x0C);
    rig.set_tile_row(0x0010, 0, 0xFF, 0x00);
    rig.set_oam(0, 16, 8, 1, 0x00);
    rig.set_oam(1, 16, 24, 1, 0x10);
    rig.render_line0();
    REQUIRE(rig.ppu.framebuffer()[0] == 1);
    REQUIRE(rig.ppu.framebuffer()[16] == 3);
}

TEST_CASE("window_wx_minus_7_offset") {
    Rig rig;
    // window on, window map 0x9c00
    rig.ppu.write_register(gb::kRegLcdc, 0xF1);
    rig.ppu.write_register(gb::kRegBgp, 0xE4);
    rig.ppu.write_register(gb::kRegWy, 0);
    rig.ppu.write_register(gb::kRegWx, 14);
    rig.ppu.write_vram(0x1C00, 1);
    rig.set_tile_row(0x0010, 0, 0xFF, 0x00);
    rig.render_line0();
    // wx 14 puts the window's first pixel at screen x 7
    REQUIRE(rig.ppu.framebuffer()[6] == 0);
    REQUIRE(rig.ppu.framebuffer()[7] == 1);
}

TEST_CASE("window_internal_line_counter_pauses") {
    Rig rig;
    rig.ppu.write_register(gb::kRegBgp, 0xE4);
    rig.ppu.write_register(gb::kRegWy, 0);
    rig.ppu.write_register(gb::kRegWx, 7);
    // window map row 0 solid, row 1 empty
    for (uint16_t i = 0; i < 32; ++i) {
        rig.ppu.write_vram(static_cast<uint16_t>(0x1C00 + i), 1);
    }
    for (uint8_t row = 0; row < 8; ++row) {
        rig.set_tile_row(0x0010, row, 0xFF, 0x00);
    }
    // window on for lines 0-3
    rig.ppu.write_register(gb::kRegLcdc, 0xF1);
    rig.tick_lines(4);
    // window off for lines 4-7
    rig.ppu.write_register(gb::kRegLcdc, 0xD1);
    rig.tick_lines(4);
    // window back on for line 8: internal counter resumed at 4, still tile row 0
    rig.ppu.write_register(gb::kRegLcdc, 0xF1);
    rig.tick_lines(1);
    REQUIRE(rig.ppu.framebuffer()[8 * 160 + 0] == 1);
    // a non-pausing counter would already be in the empty map row
    REQUIRE(rig.ppu.framebuffer()[3 * 160 + 0] == 1);
    REQUIRE(rig.ppu.framebuffer()[5 * 160 + 0] == 0);
}

TEST_CASE("tile_ids_record_source_tile_and_sprite_flag") {
    Rig rig;
    rig.sprite_setup();
    rig.ppu.write_vram(0x1800, 2);
    rig.set_tile_row(0x0020, 0, 0xFF, 0x00);
    rig.set_tile_row(0x0010, 0, 0xFF, 0x00);
    rig.set_oam(0, 16, 16, 1, 0x00);
    rig.render_line0();
    // bg pixel reports its map tile, sprite pixel reports tile plus the flag
    REQUIRE(rig.ppu.tile_ids()[0] == 0x0002);
    REQUIRE(rig.ppu.tile_ids()[8] == 0x0101);
}

TEST_CASE("lcd_off_resets_ly_and_mode") {
    Rig rig;
    rig.ppu.tick(456 * 10);
    REQUIRE(rig.ly() == 10);
    rig.ppu.write_register(gb::kRegLcdc, 0x11);
    REQUIRE(rig.ly() == 0);
    REQUIRE(rig.stat_mode() == 0);
    rig.ppu.tick(456 * 5);
    REQUIRE(rig.ly() == 0);
    // no ppu interrupts while off
    REQUIRE(rig.irq.read() == 0);
    rig.ppu.write_register(gb::kRegLcdc, 0x91);
    rig.ppu.tick(456);
    REQUIRE(rig.ly() == 1);
}

TEST_CASE("vbk_selects_the_cpu_visible_vram_bank") {
    Rig rig;
    rig.ppu.set_cgb_mode(true);
    rig.ppu.write_vram(0x0000, 0x11);
    rig.ppu.write_register(gb::kRegVbk, 0x01);
    rig.ppu.write_vram(0x0000, 0x22);
    REQUIRE(rig.ppu.read_vram(0x0000) == 0x22);
    rig.ppu.write_register(gb::kRegVbk, 0x00);
    REQUIRE(rig.ppu.read_vram(0x0000) == 0x11);
}

TEST_CASE("vbk_reads_back_with_unused_bits_set") {
    Rig rig;
    rig.ppu.set_cgb_mode(true);
    REQUIRE(rig.ppu.read_register(gb::kRegVbk) == 0xFE);
    // only bit 0 is decoded
    rig.ppu.write_register(gb::kRegVbk, 0xFE);
    REQUIRE(rig.ppu.read_register(gb::kRegVbk) == 0xFE);
    rig.ppu.write_register(gb::kRegVbk, 0xFF);
    REQUIRE(rig.ppu.read_register(gb::kRegVbk) == 0xFF);
    REQUIRE(rig.ppu.vram_bank() == 1);
}

TEST_CASE("vbk_is_dead_in_dmg_mode") {
    Rig rig;
    rig.ppu.write_vram(0x0000, 0x11);
    rig.ppu.write_register(gb::kRegVbk, 0x01);
    REQUIRE(rig.ppu.read_register(gb::kRegVbk) == 0xFF);
    REQUIRE(rig.ppu.vram_bank() == 0);
    REQUIRE(rig.ppu.read_vram(0x0000) == 0x11);
}

TEST_CASE("dmg_rendering_ignores_bank_1_entirely") {
    Rig rig;
    // dmg has no vbk, so stage bank 1 through cgb mode and run the scanline as dmg
    rig.ppu.set_cgb_mode(true);
    rig.set_bg_attr(0x1800, 0xEF);
    rig.set_tile_row_bank1(0x0010, 0, 0x00, 0x00);
    rig.ppu.set_cgb_mode(false);
    rig.ppu.write_register(gb::kRegLcdc, 0x91);
    rig.ppu.write_register(gb::kRegBgp, 0xE4);
    rig.ppu.write_vram(0x1800, 1);
    rig.set_tile_row(0x0010, 0, 0xFF, 0xFF);
    rig.render_line0();
    // no attribute lookup, no bank select, bgp still maps the shade
    REQUIRE(rig.ppu.framebuffer()[0] == 3);
    REQUIRE(rig.ppu.tile_ids()[0] == 0x0001);
}

TEST_CASE("palette_ram_auto_increments_on_writes_only") {
    Rig rig;
    rig.ppu.set_cgb_mode(true);
    rig.ppu.write_register(gb::kRegBcps, 0x80);
    REQUIRE(rig.ppu.read_register(gb::kRegBcps) == 0xC0);
    rig.ppu.write_register(gb::kRegBcpd, 0x11);
    rig.ppu.write_register(gb::kRegBcpd, 0x22);
    // index advanced twice, autoincrement bit and the unused bit 6 still read back
    REQUIRE(rig.ppu.read_register(gb::kRegBcps) == 0xC2);
    rig.ppu.write_register(gb::kRegBcps, 0x80);
    REQUIRE(rig.ppu.read_register(gb::kRegBcpd) == 0x11);
    REQUIRE(rig.ppu.read_register(gb::kRegBcpd) == 0x11);
    REQUIRE(rig.ppu.read_register(gb::kRegBcps) == 0xC0);
    // a clear autoincrement bit leaves the index alone across writes
    rig.ppu.write_register(gb::kRegBcps, 0x05);
    rig.ppu.write_register(gb::kRegBcpd, 0x33);
    rig.ppu.write_register(gb::kRegBcpd, 0x44);
    REQUIRE(rig.ppu.read_register(gb::kRegBcps) == 0x45);
    REQUIRE(rig.ppu.read_register(gb::kRegBcpd) == 0x44);
}

TEST_CASE("palette_index_wraps_inside_the_64_bytes") {
    Rig rig;
    rig.ppu.set_cgb_mode(true);
    rig.ppu.write_register(gb::kRegBcps, 0xBF);
    rig.ppu.write_register(gb::kRegBcpd, 0x77);
    REQUIRE(rig.ppu.read_register(gb::kRegBcps) == 0xC0);
    rig.ppu.write_register(gb::kRegBcpd, 0x88);
    REQUIRE(rig.read_bg_palette(0x3F) == 0x77);
    REQUIRE(rig.read_bg_palette(0x00) == 0x88);
}

TEST_CASE("obj_palette_ram_is_independent_of_bg") {
    Rig rig;
    rig.ppu.set_cgb_mode(true);
    rig.ppu.write_register(gb::kRegBcps, 0x80);
    rig.ppu.write_register(gb::kRegBcpd, 0xAA);
    rig.ppu.write_register(gb::kRegOcps, 0x80);
    rig.ppu.write_register(gb::kRegOcpd, 0x55);
    REQUIRE(rig.read_bg_palette(0) == 0xAA);
    rig.ppu.write_register(gb::kRegOcps, 0x00);
    REQUIRE(rig.ppu.read_register(gb::kRegOcpd) == 0x55);
}

TEST_CASE("palette_registers_are_dead_in_dmg_mode") {
    Rig rig;
    rig.ppu.write_register(gb::kRegBcps, 0x80);
    rig.ppu.write_register(gb::kRegBcpd, 0xAA);
    rig.ppu.write_register(gb::kRegOcps, 0x80);
    rig.ppu.write_register(gb::kRegOcpd, 0x55);
    REQUIRE(rig.ppu.read_register(gb::kRegBcps) == 0xFF);
    REQUIRE(rig.ppu.read_register(gb::kRegBcpd) == 0xFF);
    REQUIRE(rig.ppu.read_register(gb::kRegOcps) == 0xFF);
    REQUIRE(rig.ppu.read_register(gb::kRegOcpd) == 0xFF);
    // the writes were dropped, not buffered
    rig.ppu.set_cgb_mode(true);
    REQUIRE(rig.read_bg_palette(0) == 0x00);
}

TEST_CASE("cgb_bg_palette_selects_the_rgb555_color") {
    Rig rig;
    rig.cgb_bg_setup();
    rig.ppu.write_vram(0x1800, 1);
    rig.ppu.write_vram(0x1801, 1);
    // map cell 2 keeps the blank tile 0
    rig.set_tile_row(0x0010, 0, 0xFF, 0x00);
    // palette 3 color 1 = r5 g10 b20; palette 5 color 1 has bit 15 set and must lose it
    rig.set_bg_color(3, 1, 0x5145);
    rig.set_bg_color(5, 1, 0xFFFF);
    rig.set_bg_attr(0x1800, 0x03);
    rig.set_bg_attr(0x1801, 0x05);
    rig.set_bg_attr(0x1802, 0x03);
    rig.render_line0();
    REQUIRE(rig.ppu.framebuffer_color()[0] == 0x5145);
    REQUIRE(rig.ppu.framebuffer_color()[8] == 0x7FFF);
    // color 0 of palette 3 was never written, so it stays black
    REQUIRE(rig.ppu.framebuffer_color()[16] == 0x0000);
}

TEST_CASE("cgb_bg_attribute_x_flip_and_y_flip") {
    Rig rig;
    rig.cgb_bg_setup();
    rig.ppu.write_vram(0x1800, 1);
    // row 0: left nibble color 1; row 7: whole row color 2
    rig.set_tile_row(0x0010, 0, 0xF0, 0x00);
    rig.set_tile_row(0x0010, 7, 0x00, 0xFF);
    rig.set_bg_attr(0x1800, 0x20);
    rig.render_line0();
    // x-flip moves the solid nibble to the right half
    REQUIRE(rig.ppu.framebuffer()[0] == 0);
    REQUIRE(rig.ppu.framebuffer()[4] == 1);

    Rig rig2;
    rig2.cgb_bg_setup();
    rig2.ppu.write_vram(0x1800, 1);
    rig2.set_tile_row(0x0010, 0, 0xF0, 0x00);
    rig2.set_tile_row(0x0010, 7, 0x00, 0xFF);
    rig2.set_bg_attr(0x1800, 0x40);
    rig2.render_line0();
    // y-flip shows tile row 7 on the map row's first line
    REQUIRE(rig2.ppu.framebuffer()[0] == 2);
    REQUIRE(rig2.ppu.framebuffer()[7] == 2);
}

TEST_CASE("cgb_bg_attribute_bit_3_fetches_bank_1_tile_data") {
    Rig rig;
    rig.cgb_bg_setup();
    rig.ppu.write_vram(0x1800, 1);
    rig.ppu.write_vram(0x1801, 1);
    // same tile index, different data per bank
    rig.set_tile_row(0x0010, 0, 0x00, 0x00);
    rig.set_tile_row_bank1(0x0010, 0, 0xFF, 0xFF);
    rig.set_bg_attr(0x1801, 0x08);
    rig.render_line0();
    REQUIRE(rig.ppu.framebuffer()[0] == 0);
    REQUIRE(rig.ppu.framebuffer()[8] == 3);
    // the map still reports the bank 0 tile index
    REQUIRE(rig.ppu.tile_ids()[8] == 0x0001);
}

TEST_CASE("cgb_window_shares_the_bg_attribute_path") {
    Rig rig;
    rig.ppu.set_cgb_mode(true);
    // window on at x 0, window map 0x9c00
    rig.ppu.write_register(gb::kRegLcdc, 0xF1);
    rig.ppu.write_register(gb::kRegBgp, 0xE4);
    rig.ppu.write_register(gb::kRegWy, 0);
    rig.ppu.write_register(gb::kRegWx, 7);
    rig.ppu.write_vram(0x1C00, 1);
    // only the bank 1 copy of tile 1 row 7 carries data
    rig.set_tile_row(0x0010, 0, 0x00, 0x00);
    rig.set_tile_row(0x0010, 7, 0x00, 0x00);
    rig.set_tile_row_bank1(0x0010, 7, 0xF0, 0x00);
    rig.set_bg_color(6, 1, 0x03E0);
    // palette 6, bank 1, x flip, y flip
    rig.set_bg_attr(0x1C00, 0x6E);
    rig.render_line0();
    REQUIRE(rig.ppu.framebuffer()[0] == 0);
    REQUIRE(rig.ppu.framebuffer()[4] == 1);
    REQUIRE(rig.ppu.framebuffer_color()[4] == 0x03E0);
}

TEST_CASE("cgb_shade_buffer_carries_the_raw_color_index") {
    Rig rig;
    rig.cgb_bg_setup();
    // a non-identity bgp that must be ignored on cgb
    rig.ppu.write_register(gb::kRegBgp, 0x0C);
    rig.ppu.write_vram(0x1800, 1);
    rig.set_tile_row(0x0010, 0, 0xFF, 0x00);
    rig.render_line0();
    REQUIRE(rig.ppu.framebuffer()[0] == 1);
}

TEST_CASE("palette_ram_survives_a_savestate_roundtrip") {
    Rig rig;
    rig.ppu.set_cgb_mode(true);
    rig.set_bg_color(2, 3, 0x1234);
    rig.ppu.write_register(gb::kRegOcps, 0x80);
    rig.ppu.write_register(gb::kRegOcpd, 0x9A);
    std::vector<uint8_t> blob;
    gb::StateWriter w(blob);
    rig.ppu.save_state(w);
    REQUIRE(blob.size() == gb::Ppu::kStateSize);

    Rig back;
    back.ppu.set_cgb_mode(true);
    gb::StateReader r(blob);
    back.ppu.load_state(r);
    // the spec registers roundtrip with their autoincrement bits and advanced indices
    REQUIRE(back.ppu.read_register(gb::kRegBcps) == 0xD8);
    REQUIRE(back.ppu.read_register(gb::kRegOcps) == 0xC1);
    REQUIRE(back.read_bg_palette(2 * 8 + 3 * 2) == 0x34);
    REQUIRE(back.read_bg_palette(2 * 8 + 3 * 2 + 1) == 0x12);
    back.ppu.write_register(gb::kRegOcps, 0x00);
    REQUIRE(back.ppu.read_register(gb::kRegOcpd) == 0x9A);
}

TEST_CASE("state_load_masks_a_tampered_palette_index") {
    Rig rig;
    rig.ppu.set_cgb_mode(true);
    rig.ppu.write_register(gb::kRegBcps, 0x3F);
    rig.ppu.write_register(gb::kRegBcpd, 0x5A);
    std::vector<uint8_t> blob;
    gb::StateWriter w(blob);
    rig.ppu.save_state(w);
    // bcps and ocps are the last two bytes of the section
    blob[blob.size() - 2] = 0x7F;
    blob[blob.size() - 1] = 0xFF;

    Rig back;
    back.ppu.set_cgb_mode(true);
    gb::StateReader r(blob);
    back.ppu.load_state(r);
    // an unmasked 0x7f index would read past the 64 bytes
    REQUIRE(back.ppu.read_register(gb::kRegBcpd) == 0x5A);
    REQUIRE(back.ppu.read_register(gb::kRegBcps) == 0x7F);
    REQUIRE(back.ppu.read_register(gb::kRegOcps) == 0xFF);
}

TEST_CASE("dmg_state_load_ignores_a_tampered_vbk") {
    Rig cgb;
    cgb.ppu.set_cgb_mode(true);
    cgb.ppu.write_register(gb::kRegVbk, 0x01);
    cgb.ppu.write_vram(0x0000, 0x22);
    std::vector<uint8_t> blob;
    gb::StateWriter w(blob);
    cgb.ppu.save_state(w);
    REQUIRE(blob.size() == gb::Ppu::kStateSize);

    Rig dmg;
    gb::StateReader r(blob);
    dmg.ppu.load_state(r);
    REQUIRE(dmg.ppu.vram_bank() == 0);
    // bank 1 bytes still roundtrip, they are just not cpu-visible
    REQUIRE(dmg.ppu.read_vram(0x0000) == 0x00);

    Rig back;
    back.ppu.set_cgb_mode(true);
    gb::StateReader r2(blob);
    back.ppu.load_state(r2);
    REQUIRE(back.ppu.vram_bank() == 1);
    REQUIRE(back.ppu.read_vram(0x0000) == 0x22);
}
