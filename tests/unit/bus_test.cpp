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

TEST_CASE("key1_reports_single_speed_in_cgb_mode") {
    Rig rig;
    rig.bus.set_cgb_mode(true);
    REQUIRE(rig.bus.read8(gb::kRegKey1) == 0x7E);
    // read-only stub this milestone: arming the switch is dropped
    rig.bus.write8(gb::kRegKey1, 0x01);
    REQUIRE(rig.bus.read8(gb::kRegKey1) == 0x7E);
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
