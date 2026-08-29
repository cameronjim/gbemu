#include "bus.hpp"

#include "interrupts.hpp"
#include "mapper_rom.hpp"
#include "serial.hpp"
#include "timer.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <utility>
#include <vector>

namespace {

struct Rig {
    gb::Serial serial;
    gb::InterruptLine irq;
    gb::Timer timer{irq};
    gb::Ppu ppu{irq};
    gb::Apu apu;
    gb::Joypad joypad;
    gb::Bus bus{serial, timer, ppu, apu, joypad, irq};
};

// stops on the ppu's next hblank entry; assertions use peek8 so verifying costs no cycles
void tick_to_next_hblank(Rig& rig) {
    while (rig.ppu.mode() == gb::PpuMode::HBlank) {
        rig.bus.tick_components(4);
    }
    while (rig.ppu.mode() != gb::PpuMode::HBlank) {
        rig.bus.tick_components(4);
    }
}

uint32_t cycles_to_vblank(Rig& rig) {
    uint32_t cpu = 0;
    while (rig.ppu.frame_count() == 0) {
        rig.bus.tick_components(4);
        cpu += 4;
    }
    return cpu;
}

void arm_hdma(Rig& rig, uint8_t src_hi, uint8_t dst_hi, uint8_t hdma5) {
    rig.bus.write8(gb::kRegHdma1, src_hi);
    rig.bus.write8(gb::kRegHdma2, 0x00);
    rig.bus.write8(gb::kRegHdma3, dst_hi);
    rig.bus.write8(gb::kRegHdma4, 0x00);
    rig.bus.write8(gb::kRegHdma5, hdma5);
}

} // namespace

TEST_CASE("timer_registers_route_through_bus") {
    Rig rig;
    rig.bus.write8(gb::kRegTma, 0x42);
    REQUIRE(rig.bus.read8(gb::kRegTma) == 0x42);
    rig.bus.write8(gb::kRegTac, 0x05);
    REQUIRE(rig.bus.read8(gb::kRegTac) == 0xFD);
    rig.bus.write8(gb::kRegDiv, 0x99);
    REQUIRE(rig.bus.read8(gb::kRegDiv) == 0x00);
}

TEST_CASE("echo_ram_mirrors_wram_both_directions") {
    Rig rig;
    rig.bus.write8(0xC123, 0x42);
    REQUIRE(rig.bus.read8(0xE123) == 0x42);
    rig.bus.write8(0xF000, 0x99);
    REQUIRE(rig.bus.read8(0xD000) == 0x99);
}

TEST_CASE("unusable_region_reads_ff_ignores_writes") {
    Rig rig;
    rig.bus.write8(0xFEA0, 0x12);
    REQUIRE(rig.bus.read8(0xFEA0) == 0xFF);
    REQUIRE(rig.bus.read8(0xFEFF) == 0xFF);
}

TEST_CASE("unmapped_io_reads_ff") {
    Rig rig;
    REQUIRE(rig.bus.read8(0xFF03) == 0xFF);
    REQUIRE(rig.bus.read8(0xFF4C) == 0xFF);
    REQUIRE(rig.bus.read8(0xFF7F) == 0xFF);
}

TEST_CASE("if_upper_bits_read_ones") {
    Rig rig;
    // pandocs power-up value
    REQUIRE(rig.bus.read8(gb::kRegIf) == 0xE1);
    rig.bus.write8(gb::kRegIf, 0x00);
    REQUIRE(rig.bus.read8(gb::kRegIf) == 0xE0);
    rig.bus.write8(gb::kRegIf, 0xFF);
    REQUIRE(rig.bus.read8(gb::kRegIf) == 0xFF);
}

TEST_CASE("serial_sink_receives_bytes_on_sc_81") {
    Rig rig;
    std::vector<uint8_t> got;
    rig.serial.set_sink([&got](uint8_t b) { got.push_back(b); });
    rig.bus.write8(gb::kRegSb, 'H');
    rig.bus.write8(gb::kRegSc, 0x81);
    rig.bus.write8(gb::kRegSb, 'i');
    rig.bus.write8(gb::kRegSc, 0x80);
    REQUIRE(got == std::vector<uint8_t>{'H'});
}

TEST_CASE("oam_dma_copies_160_bytes") {
    Rig rig;
    for (uint16_t i = 0; i < 0xA0; ++i) {
        rig.bus.write8(static_cast<uint16_t>(0xC000 + i), static_cast<uint8_t>(i + 1));
    }
    rig.bus.write8(0xFF46, 0xC0);
    for (uint16_t i = 0; i < 0xA0; ++i) {
        REQUIRE(rig.bus.read8(static_cast<uint16_t>(0xFE00 + i)) == static_cast<uint8_t>(i + 1));
    }
    REQUIRE(rig.bus.read8(0xFF46) == 0xC0);
}

TEST_CASE("read16_is_little_endian") {
    Rig rig;
    rig.bus.write8(0xC000, 0x34);
    rig.bus.write8(0xC001, 0x12);
    REQUIRE(rig.bus.read16(0xC000) == 0x1234);
    rig.bus.write16(0xC100, 0xBEEF);
    REQUIRE(rig.bus.read8(0xC100) == 0xEF);
    REQUIRE(rig.bus.read8(0xC101) == 0xBE);
}

TEST_CASE("rom_region_routes_through_mapper") {
    Rig rig;
    REQUIRE(rig.bus.read8(0x0000) == 0xFF);
    std::vector<uint8_t> rom(0x8000, 0);
    rom[0x0000] = 0x11;
    rom[0x7FFF] = 0x22;
    gb::MapperRom mapper(std::move(rom));
    rig.bus.attach_mapper(mapper);
    REQUIRE(rig.bus.read8(0x0000) == 0x11);
    REQUIRE(rig.bus.read8(0x7FFF) == 0x22);
    REQUIRE(rig.bus.read8(0xA000) == 0xFF);
}

TEST_CASE("hram_and_ie_are_readable_writable") {
    Rig rig;
    rig.bus.write8(0xFF80, 0x5A);
    REQUIRE(rig.bus.read8(0xFF80) == 0x5A);
    rig.bus.write8(0xFFFE, 0xA5);
    REQUIRE(rig.bus.read8(0xFFFE) == 0xA5);
    rig.bus.write8(gb::kRegIe, 0x1F);
    REQUIRE(rig.bus.read8(gb::kRegIe) == 0x1F);
}

TEST_CASE("svbk_switches_the_d000_window_only") {
    Rig rig;
    rig.bus.set_cgb_mode(true);
    rig.bus.write8(0xC000, 0x11);
    rig.bus.write8(0xD000, 0x22);
    rig.bus.write8(gb::kRegSvbk, 0x02);
    REQUIRE(rig.bus.read8(0xD000) == 0x00);
    rig.bus.write8(0xD000, 0x33);
    // c000 is always bank 0, never follows svbk
    REQUIRE(rig.bus.read8(0xC000) == 0x11);
    rig.bus.write8(gb::kRegSvbk, 0x01);
    REQUIRE(rig.bus.read8(0xD000) == 0x22);
    rig.bus.write8(gb::kRegSvbk, 0x02);
    REQUIRE(rig.bus.read8(0xD000) == 0x33);
}

TEST_CASE("svbk_masks_to_three_bits_and_zero_banks_as_one") {
    Rig rig;
    rig.bus.set_cgb_mode(true);
    rig.bus.write8(gb::kRegSvbk, 0x01);
    rig.bus.write8(0xD000, 0x5A);
    rig.bus.write8(gb::kRegSvbk, 0x00);
    REQUIRE(rig.bus.read8(0xD000) == 0x5A);
    // upper bits are dropped: 0xFF selects bank 7
    rig.bus.write8(gb::kRegSvbk, 0xFF);
    REQUIRE(rig.bus.read8(gb::kRegSvbk) == 0xFF);
    rig.bus.write8(0xD000, 0x77);
    rig.bus.write8(gb::kRegSvbk, 0x07);
    REQUIRE(rig.bus.read8(0xD000) == 0x77);
}

TEST_CASE("svbk_reads_back_with_unused_bits_set") {
    Rig rig;
    rig.bus.set_cgb_mode(true);
    REQUIRE(rig.bus.read8(gb::kRegSvbk) == 0xF8);
    rig.bus.write8(gb::kRegSvbk, 0x03);
    REQUIRE(rig.bus.read8(gb::kRegSvbk) == 0xFB);
}

TEST_CASE("echo_ram_follows_the_switched_bank") {
    Rig rig;
    rig.bus.set_cgb_mode(true);
    rig.bus.write8(gb::kRegSvbk, 0x04);
    rig.bus.write8(0xD100, 0x42);
    REQUIRE(rig.bus.read8(0xF100) == 0x42);
    rig.bus.write8(gb::kRegSvbk, 0x05);
    REQUIRE(rig.bus.read8(0xF100) == 0x00);
    // the e000 half still mirrors fixed bank 0
    rig.bus.write8(0xC200, 0x99);
    REQUIRE(rig.bus.read8(0xE200) == 0x99);
}

TEST_CASE("oam_dma_sources_the_switched_bank") {
    Rig rig;
    rig.bus.set_cgb_mode(true);
    rig.bus.write8(gb::kRegSvbk, 0x03);
    for (uint16_t i = 0; i < 0xA0; ++i) {
        rig.bus.write8(static_cast<uint16_t>(0xD000 + i), static_cast<uint8_t>(i + 1));
    }
    rig.bus.write8(gb::kRegSvbk, 0x04);
    rig.bus.write8(0xFF46, 0xD0);
    REQUIRE(rig.bus.read8(0xFE00) == 0x00);
    rig.bus.write8(gb::kRegSvbk, 0x03);
    rig.bus.write8(0xFF46, 0xD0);
    for (uint16_t i = 0; i < 0xA0; ++i) {
        REQUIRE(rig.bus.read8(static_cast<uint16_t>(0xFE00 + i)) == static_cast<uint8_t>(i + 1));
    }
}

TEST_CASE("svbk_and_key1_are_dead_in_dmg_mode") {
    Rig rig;
    rig.bus.write8(0xD000, 0x5A);
    rig.bus.write8(gb::kRegSvbk, 0x03);
    REQUIRE(rig.bus.read8(gb::kRegSvbk) == 0xFF);
    REQUIRE(rig.bus.read8(0xD000) == 0x5A);
    REQUIRE(rig.bus.read8(gb::kRegKey1) == 0xFF);
    REQUIRE(rig.bus.read8(gb::kRegVbk) == 0xFF);
}

TEST_CASE("key1_arms_and_stop_commits_the_speed_switch") {
    Rig rig;
    rig.bus.set_cgb_mode(true);
    REQUIRE(rig.bus.read8(gb::kRegKey1) == 0x7E);
    rig.bus.write8(gb::kRegKey1, 0x01);
    REQUIRE(rig.bus.read8(gb::kRegKey1) == 0x7F);
    rig.timer.tick(0x1234);
    REQUIRE(rig.timer.read_div() != 0);
    REQUIRE(rig.bus.commit_speed_switch());
    // pandocs: the speed switch resets the div counter
    REQUIRE(rig.timer.read_div() == 0);
    REQUIRE(rig.bus.double_speed());
    REQUIRE(rig.bus.read8(gb::kRegKey1) == 0xFE);
    rig.bus.write8(gb::kRegKey1, 0x01);
    REQUIRE(rig.bus.read8(gb::kRegKey1) == 0xFF);
    REQUIRE(rig.bus.commit_speed_switch());
    REQUIRE(!rig.bus.double_speed());
    REQUIRE(rig.bus.read8(gb::kRegKey1) == 0x7E);
}

TEST_CASE("stop_without_an_armed_switch_changes_nothing") {
    Rig rig;
    rig.bus.set_cgb_mode(true);
    REQUIRE(!rig.bus.commit_speed_switch());
    REQUIRE(rig.bus.read8(gb::kRegKey1) == 0x7E);
    Rig dmg;
    dmg.bus.write8(gb::kRegKey1, 0x01);
    REQUIRE(!dmg.bus.commit_speed_switch());
    REQUIRE(dmg.bus.read8(gb::kRegKey1) == 0xFF);
}

TEST_CASE("double_speed_doubles_cpu_and_timer_cycles_per_video_frame") {
    Rig slow;
    slow.bus.set_cgb_mode(true);
    slow.bus.write8(gb::kRegKey1, 0x00);
    slow.timer.write_div();
    const uint32_t slow_cycles = cycles_to_vblank(slow);
    // the key1 write already spent one m-cycle of the 144-line scanout
    REQUIRE(slow_cycles == 144u * 456u - 4u);
    REQUIRE(slow.timer.read_div() == static_cast<uint8_t>(slow_cycles >> 8));

    Rig fast;
    fast.bus.set_cgb_mode(true);
    fast.bus.write8(gb::kRegKey1, 0x01);
    REQUIRE(fast.bus.commit_speed_switch());
    fast.timer.write_div();
    const uint32_t fast_cycles = cycles_to_vblank(fast);
    REQUIRE(fast_cycles == 2 * slow_cycles);
    // the timer never halves: it saw twice the cycles across the same video frame
    REQUIRE(fast.timer.read_div() == static_cast<uint8_t>(fast_cycles >> 8));
}

TEST_CASE("double_speed_carries_the_odd_video_cycle") {
    Rig rig;
    rig.bus.set_cgb_mode(true);
    rig.bus.write8(gb::kRegKey1, 0x01);
    REQUIRE(rig.bus.commit_speed_switch());
    // the key1 write left the ppu at dot 4; 451 more dots must not reach the line end
    for (uint32_t i = 0; i < 2 * 451; ++i) {
        rig.bus.tick_components(1);
    }
    REQUIRE(rig.ppu.read_register(gb::kRegLy) == 0);
    rig.bus.tick_components(1);
    rig.bus.tick_components(1);
    REQUIRE(rig.ppu.read_register(gb::kRegLy) == 1);
}

TEST_CASE("gp_dma_copies_through_the_switched_wram_bank") {
    Rig rig;
    rig.bus.set_cgb_mode(true);
    rig.ppu.set_cgb_mode(true);
    rig.bus.write8(gb::kRegSvbk, 0x03);
    for (uint16_t i = 0; i < 0x20; ++i) {
        rig.bus.write8(static_cast<uint16_t>(0xD100 + i), static_cast<uint8_t>(i + 1));
    }
    // bit 7 clear: two 0x10 chunks copied at once
    arm_hdma(rig, 0xD1, 0x03, 0x01);
    for (uint16_t i = 0; i < 0x20; ++i) {
        REQUIRE(rig.ppu.read_vram(static_cast<uint16_t>(0x0300 + i)) == static_cast<uint8_t>(i + 1));
    }
    REQUIRE(rig.bus.peek8(gb::kRegHdma5) == 0xFF);
    // the source and destination registers are write-only
    REQUIRE(rig.bus.peek8(gb::kRegHdma1) == 0xFF);
    REQUIRE(rig.bus.peek8(gb::kRegHdma4) == 0xFF);
    // a bank 3 source read through a bank 4 window would have copied zeroes
    rig.bus.write8(gb::kRegSvbk, 0x04);
    arm_hdma(rig, 0xD1, 0x04, 0x00);
    REQUIRE(rig.ppu.read_vram(0x0400) == 0x00);
}

TEST_CASE("gp_dma_lands_in_the_selected_vram_bank") {
    Rig rig;
    rig.bus.set_cgb_mode(true);
    rig.ppu.set_cgb_mode(true);
    rig.bus.write8(0xC000, 0x5A);
    rig.bus.write8(gb::kRegVbk, 0x01);
    arm_hdma(rig, 0xC0, 0x00, 0x00);
    REQUIRE(rig.ppu.read_vram(0x0000) == 0x5A);
    rig.bus.write8(gb::kRegVbk, 0x00);
    REQUIRE(rig.ppu.read_vram(0x0000) == 0x00);
}

TEST_CASE("hblank_dma_moves_one_chunk_per_hblank_entry") {
    Rig rig;
    rig.bus.set_cgb_mode(true);
    rig.ppu.set_cgb_mode(true);
    for (uint16_t i = 0; i < 0x40; ++i) {
        rig.bus.write8(static_cast<uint16_t>(0xC000 + i), static_cast<uint8_t>(0xA0 + i));
    }
    // bit 7 set: four chunks, one per hblank entry
    arm_hdma(rig, 0xC0, 0x00, 0x83);
    REQUIRE(rig.bus.peek8(gb::kRegHdma5) == 0x03);
    REQUIRE(rig.ppu.read_vram(0x0000) == 0x00);
    tick_to_next_hblank(rig);
    REQUIRE(rig.bus.peek8(gb::kRegHdma5) == 0x02);
    REQUIRE(rig.ppu.read_vram(0x0000) == 0xA0);
    REQUIRE(rig.ppu.read_vram(0x000F) == 0xAF);
    REQUIRE(rig.ppu.read_vram(0x0010) == 0x00);
    tick_to_next_hblank(rig);
    REQUIRE(rig.bus.peek8(gb::kRegHdma5) == 0x01);
    REQUIRE(rig.ppu.read_vram(0x0010) == 0xB0);
    tick_to_next_hblank(rig);
    tick_to_next_hblank(rig);
    // a completed transfer reads 0xff
    REQUIRE(rig.bus.peek8(gb::kRegHdma5) == 0xFF);
    REQUIRE(rig.ppu.read_vram(0x003F) == 0xDF);
    tick_to_next_hblank(rig);
    REQUIRE(rig.ppu.read_vram(0x0040) == 0x00);
}

TEST_CASE("clearing_hdma5_bit7_cancels_an_active_hblank_dma") {
    Rig rig;
    rig.bus.set_cgb_mode(true);
    rig.ppu.set_cgb_mode(true);
    rig.bus.write8(0xC000, 0x11);
    rig.bus.write8(0xC010, 0x22);
    arm_hdma(rig, 0xC0, 0x00, 0x84);
    tick_to_next_hblank(rig);
    REQUIRE(rig.bus.peek8(gb::kRegHdma5) == 0x03);
    rig.bus.write8(gb::kRegHdma5, 0x00);
    // pandocs: a stopped transfer reads bit 7 set with the remaining length still in bits 0-6
    REQUIRE(rig.bus.peek8(gb::kRegHdma5) == 0x83);
    tick_to_next_hblank(rig);
    REQUIRE(rig.ppu.read_vram(0x0000) == 0x11);
    REQUIRE(rig.ppu.read_vram(0x0010) == 0x00);
}

TEST_CASE("hdma_registers_are_dead_in_dmg_mode") {
    Rig rig;
    rig.bus.write8(0xC000, 0x5A);
    REQUIRE(rig.bus.peek8(gb::kRegHdma1) == 0xFF);
    REQUIRE(rig.bus.peek8(gb::kRegHdma5) == 0xFF);
    arm_hdma(rig, 0xC0, 0x00, 0x00);
    REQUIRE(rig.ppu.read_vram(0x0000) == 0x00);
    arm_hdma(rig, 0xC0, 0x00, 0x83);
    tick_to_next_hblank(rig);
    REQUIRE(rig.ppu.read_vram(0x0000) == 0x00);
    REQUIRE(rig.bus.peek8(gb::kRegHdma5) == 0xFF);
}

TEST_CASE("vbk_routes_through_the_bus_in_cgb_mode") {
    Rig rig;
    rig.bus.set_cgb_mode(true);
    rig.ppu.set_cgb_mode(true);
    rig.bus.write8(0x8000, 0x11);
    rig.bus.write8(gb::kRegVbk, 0x01);
    REQUIRE(rig.bus.read8(gb::kRegVbk) == 0xFF);
    rig.bus.write8(0x8000, 0x22);
    REQUIRE(rig.bus.read8(0x8000) == 0x22);
    rig.bus.write8(gb::kRegVbk, 0x00);
    REQUIRE(rig.bus.read8(0x8000) == 0x11);
}

TEST_CASE("dmg_state_load_ignores_a_tampered_svbk") {
    Rig rig;
    Rig cgb;
    cgb.bus.set_cgb_mode(true);
    cgb.bus.write8(gb::kRegSvbk, 0x05);
    std::vector<uint8_t> blob;
    gb::StateWriter w(blob);
    cgb.bus.save_state(w);
    REQUIRE(blob.size() == gb::Bus::kStateSize);
    gb::StateReader r(blob);
    rig.bus.load_state(r);
    // the dmg 0xD000 window must stay on bank 1
    REQUIRE(rig.bus.read8(gb::kRegSvbk) == 0xFF);
    rig.bus.write8(0xD000, 0x42);
    REQUIRE(rig.bus.read8(0xF000) == 0x42);
    cgb.bus.write8(gb::kRegSvbk, 0x01);
    gb::StateReader r2(blob);
    cgb.bus.load_state(r2);
    REQUIRE(cgb.bus.read8(gb::kRegSvbk) == 0xFD);
}
