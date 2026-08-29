#include "gameboy.hpp"

#include "test_rom.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <string>
#include <vector>

TEST_CASE("rom_string_reaches_serial_sink") {
    std::vector<uint8_t> rom = make_test_rom();
    // entry: write 'O','K' through sb/sc, then halt
    const uint8_t code[] = {
        0x3E, 'O',  // ld a, 'o'
        0xE0, 0x01, // ldh (sb), a
        0x3E, 0x81, // ld a, 0x81
        0xE0, 0x02, // ldh (sc), a
        0x3E, 'K',  // ld a, 'k'
        0xE0, 0x01, // ldh (sb), a
        0x3E, 0x81, // ld a, 0x81
        0xE0, 0x02, // ldh (sc), a
        0x76,       // halt
    };
    for (size_t i = 0; i < sizeof(code); ++i) {
        rom[0x0100 + i] = code[i];
    }
    rom[0x014D] = test_rom_checksum(rom);

    gb::Gameboy gameboy;
    std::string got;
    gameboy.set_serial_sink([&got](uint8_t b) { got.push_back(static_cast<char>(b)); });
    REQUIRE(gameboy.load_rom(rom));
    gameboy.run_frame();
    REQUIRE(got == "OK");
    // run_frame stops at the vblank edge; pandocs: vblank begins at line 144, 456 dots per line
    REQUIRE(gameboy.cycles() >= 144u * 456u);
}

TEST_CASE("timer_interrupt_serviced_after_ime_set") {
    std::vector<uint8_t> rom = make_test_rom();
    // entry: arm the fastest timer, enable only the timer interrupt, ei, spin
    const uint8_t code[] = {
        0x3E, 0x05, // ld a, 0x05
        0xE0, 0x07, // ldh (tac), a
        0x3E, 0x04, // ld a, timer bit
        0xE0, 0xFF, // ldh (ie), a
        0x3E, 0xFE, // ld a, 0xfe
        0xE0, 0x05, // ldh (tima), a
        0xAF,       // xor a
        0xE0, 0x0F, // ldh (if), a
        0xFB,       // ei
        0x00,       // nop
        0x18, 0xFE, // jr -2
    };
    for (size_t i = 0; i < sizeof(code); ++i) {
        rom[0x0100 + i] = code[i];
    }
    // timer vector: report by writing 'I' to the serial sink, then spin
    const uint8_t handler[] = {
        0x3E, 'I',  // ld a, 'i'
        0xE0, 0x01, // ldh (sb), a
        0x3E, 0x81, // ld a, 0x81
        0xE0, 0x02, // ldh (sc), a
        0x18, 0xFE, // jr -2
    };
    for (size_t i = 0; i < sizeof(handler); ++i) {
        rom[0x0050 + i] = handler[i];
    }
    rom[0x014D] = test_rom_checksum(rom);

    gb::Gameboy gameboy;
    std::string got;
    gameboy.set_serial_sink([&got](uint8_t b) { got.push_back(static_cast<char>(b)); });
    REQUIRE(gameboy.load_rom(rom));
    gameboy.run_frame();
    REQUIRE(got == "I");
}

TEST_CASE("battery_ram_visible_through_facade") {
    // mbc1+ram+battery cart with 8kb ram
    std::vector<uint8_t> rom = make_test_rom(0x03);
    rom[0x0149] = 0x02;
    // entry: enable ram, write 0x5a to 0xa000, halt
    const uint8_t code[] = {
        0x3E, 0x0A,       // ld a, 0x0a
        0xEA, 0x00, 0x00, // ld (0x0000), a
        0x3E, 0x5A,       // ld a, 0x5a
        0xEA, 0x00, 0xA0, // ld (0xa000), a
        0x76,             // halt
    };
    for (size_t i = 0; i < sizeof(code); ++i) {
        rom[0x0100 + i] = code[i];
    }
    rom[0x014D] = test_rom_checksum(rom);

    gb::Gameboy gameboy;
    REQUIRE(gameboy.load_rom(rom));
    REQUIRE(gameboy.has_battery());
    gameboy.run_frame();
    REQUIRE(gameboy.external_ram().size() == 0x2000);
    REQUIRE(gameboy.external_ram()[0] == 0x5A);
    // frontend restores a save by writing into the same span
    gameboy.external_ram()[1] = 0x77;
    REQUIRE(gameboy.external_ram()[1] == 0x77);
}

TEST_CASE("io_reads_observe_mid_instruction_time") {
    // tetris regression: an exact ly==144 poll must win the race against a
    // vblank isr longer than one scanline, because the in-flight read sees the flip
    std::vector<uint8_t> rom = make_test_rom();
    const uint8_t handler[] = {
        0x06, 0x3C, // ld b, 60: waste over one scanline
        0x05,       // dec b
        0x20, 0xFD, // jr nz, -3
        0xD9,       // reti
    };
    for (size_t i = 0; i < sizeof(handler); ++i) {
        rom[0x0040 + i] = handler[i];
    }
    const uint8_t code[] = {
        0x3E, 0x01, // ld a, 1
        0xE0, 0xFF, // ldh (ie), a
        0xAF,       // xor a
        0xE0, 0x0F, // ldh (if), a
        0xFB,       // ei
        0xF0, 0x44, // ldh a, (ly)
        0xFE, 0x90, // cp 0x90
        0x20, 0xFA, // jr nz, -6
        0x3E, 'V',  // ld a, 'v'
        0xE0, 0x01, // ldh (sb), a
        0x3E, 0x81, // ld a, 0x81
        0xE0, 0x02, // ldh (sc), a
        0x18, 0xFE, // jr -2
    };
    for (size_t i = 0; i < sizeof(code); ++i) {
        rom[0x0100 + i] = code[i];
    }
    rom[0x014D] = test_rom_checksum(rom);

    gb::Gameboy gameboy;
    std::string got;
    gameboy.set_serial_sink([&got](uint8_t b) { got.push_back(static_cast<char>(b)); });
    REQUIRE(gameboy.load_rom(rom));
    for (int i = 0; i < 5 && got.empty(); ++i) {
        gameboy.run_frame();
    }
    REQUIRE(got == "V");
}

TEST_CASE("load_rom_rejects_invalid_bytes") {
    gb::Gameboy gameboy;
    const std::vector<uint8_t> junk = {0x01, 0x02, 0x03};
    REQUIRE(!gameboy.load_rom(junk));
    // a gameboy without a cartridge still renders the test pattern
    gameboy.run_frame();
    REQUIRE(gameboy.cycles() == 0);
    REQUIRE(gameboy.framebuffer().size() == 23040u);
}

TEST_CASE("cgb_mode_follows_the_cart_header") {
    gb::Gameboy dmg;
    REQUIRE(dmg.load_rom(make_test_rom()));
    REQUIRE(!dmg.cgb_mode());

    gb::Gameboy dual;
    REQUIRE(dual.load_rom(make_test_rom(0x00, 0x00, 0x8000, 0x80)));
    REQUIRE(dual.cgb_mode());

    gb::Gameboy only;
    REQUIRE(only.load_rom(make_test_rom(0x00, 0x00, 0x8000, 0xC0)));
    REQUIRE(only.cgb_mode());
}

namespace {

// cgb cart that writes arm_bit to key1, runs stop, then spins forever
std::vector<uint8_t> speed_rom(uint8_t arm_bit) {
    std::vector<uint8_t> rom = make_test_rom(0x00, 0x00, 0x8000, 0xC0);
    const uint8_t code[] = {
        0x3E, arm_bit, // ld a, arm_bit
        0xE0, 0x4D,    // ldh (key1), a
        0x10, 0x00,    // stop
        0x18, 0xFE,    // jr -2
    };
    for (size_t i = 0; i < sizeof(code); ++i) {
        rom[0x0100 + i] = code[i];
    }
    rom[0x014D] = test_rom_checksum(rom);
    return rom;
}

} // namespace

TEST_CASE("run_frame_spans_one_video_frame_in_both_speeds") {
    gb::Gameboy slow;
    REQUIRE(slow.load_rom(speed_rom(0x00)));
    gb::Gameboy fast;
    REQUIRE(fast.load_rom(speed_rom(0x01)));
    // the first call starts mid-frame; measure the steady state after it
    slow.run_frame();
    fast.run_frame();
    uint64_t slow_mark = slow.cycles();
    uint64_t fast_mark = fast.cycles();
    for (int i = 0; i < 3; ++i) {
        slow.run_frame();
        fast.run_frame();
        const uint64_t slow_frame = slow.cycles() - slow_mark;
        const uint64_t fast_frame = fast.cycles() - fast_mark;
        // one call is one video frame of 70224 dots, spent twice as fast in double speed
        REQUIRE(slow_frame >= 70224u - 32u);
        REQUIRE(slow_frame <= 70224u + 32u);
        REQUIRE(fast_frame >= 2u * 70224u - 64u);
        REQUIRE(fast_frame <= 2u * 70224u + 64u);
        slow_mark = slow.cycles();
        fast_mark = fast.cycles();
    }
}

TEST_CASE("boot_registers_match_the_machine_mode") {
    gb::Gameboy dmg;
    REQUIRE(dmg.load_rom(make_test_rom()));
    const gb::CpuRegs& d = dmg.debug_regs();
    REQUIRE(d.a == 0x01);
    REQUIRE(d.f == 0xB0);
    REQUIRE(d.b == 0x00);
    REQUIRE(d.c == 0x13);
    REQUIRE(d.d == 0x00);
    REQUIRE(d.e == 0xD8);
    REQUIRE(d.h == 0x01);
    REQUIRE(d.l == 0x4D);

    gb::Gameboy cgb;
    REQUIRE(cgb.load_rom(make_test_rom(0x00, 0x00, 0x8000, 0xC0)));
    const gb::CpuRegs& c = cgb.debug_regs();
    // games detect a cgb by a == 0x11 at entry
    REQUIRE(c.a == 0x11);
    REQUIRE(c.f == 0x80);
    REQUIRE(c.b == 0x00);
    REQUIRE(c.c == 0x00);
    REQUIRE(c.d == 0xFF);
    REQUIRE(c.e == 0x56);
    REQUIRE(c.h == 0x00);
    REQUIRE(c.l == 0x0D);
    REQUIRE(c.sp == 0xFFFE);
    REQUIRE(c.pc == 0x0100);
}
