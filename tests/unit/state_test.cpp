#include "gameboy.hpp"

#include "test_rom.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace {

// bg tile plus an endless scx increment so the framebuffer changes every frame
std::vector<uint8_t> scroll_rom() {
    std::vector<uint8_t> rom = make_test_rom();
    const uint8_t code[] = {
        0x3E, 0xFF,       // ld a, 0xff
        0xEA, 0x10, 0x80, // ld (0x8010), a
        0x3E, 0x01,       // ld a, 1
        0xEA, 0x00, 0x98, // ld (0x9800), a
        0x0C,             // loop: inc c
        0x79,             // ld a, c
        0xE0, 0x43,       // ldh (scx), a
        0x18, 0xFA,       // jr loop
    };
    for (size_t i = 0; i < sizeof(code); ++i) {
        rom[0x0100 + i] = code[i];
    }
    rom[0x014D] = test_rom_checksum(rom);
    return rom;
}

uint64_t fb_hash(const gb::Gameboy& gameboy) {
    uint64_t hash = 1469598103934665603ull;
    for (uint8_t b : gameboy.framebuffer()) {
        hash = (hash ^ b) * 1099511628211ull;
    }
    return hash;
}

void run_frames(gb::Gameboy& gameboy, uint32_t n) {
    for (uint32_t i = 0; i < n; ++i) {
        gameboy.run_frame();
    }
}

} // namespace

TEST_CASE("roundtrip_restores_exact_state") {
    gb::Gameboy gameboy;
    REQUIRE(gameboy.load_rom(scroll_rom()));
    run_frames(gameboy, 5);
    std::vector<uint8_t> blob;
    gameboy.save_state(blob);
    run_frames(gameboy, 3);
    const uint64_t h1 = fb_hash(gameboy);
    REQUIRE(gameboy.load_state(blob));
    run_frames(gameboy, 3);
    REQUIRE(fb_hash(gameboy) == h1);
}

TEST_CASE("truncated_state_rejected_machine_untouched") {
    gb::Gameboy g1;
    gb::Gameboy control;
    REQUIRE(g1.load_rom(scroll_rom()));
    REQUIRE(control.load_rom(scroll_rom()));
    run_frames(g1, 5);
    run_frames(control, 5);
    std::vector<uint8_t> blob;
    g1.save_state(blob);
    blob.resize(blob.size() / 2);
    REQUIRE(!g1.load_state(blob));
    run_frames(g1, 3);
    run_frames(control, 3);
    REQUIRE(fb_hash(g1) == fb_hash(control));
}

TEST_CASE("bad_magic_and_version_rejected") {
    gb::Gameboy gameboy;
    REQUIRE(gameboy.load_rom(scroll_rom()));
    run_frames(gameboy, 2);
    std::vector<uint8_t> blob;
    gameboy.save_state(blob);
    std::vector<uint8_t> bad_magic = blob;
    bad_magic[0] ^= 0xFF;
    REQUIRE(!gameboy.load_state(bad_magic));
    std::vector<uint8_t> bad_version = blob;
    bad_version[4] ^= 0xFF;
    REQUIRE(!gameboy.load_state(bad_version));
    REQUIRE(gameboy.load_state(blob));
}

TEST_CASE("out_of_range_values_rejected_or_masked") {
    gb::Gameboy gameboy;
    REQUIRE(gameboy.load_rom(scroll_rom()));
    run_frames(gameboy, 2);
    std::vector<uint8_t> blob;
    gameboy.save_state(blob);
    // cpu f register raw byte: magic(4) + version(4) + section len(4) + a(1)
    blob[13] = 0xFF;
    REQUIRE(gameboy.load_state(blob));
    REQUIRE(gameboy.debug_regs().f == 0xF0);
    gameboy.run_frame();
}

TEST_CASE("nonarchitectural_state_survives") {
    // byte-identical re-save proves timer counter, ppu dot, window line all roundtrip
    gb::Gameboy g1;
    REQUIRE(g1.load_rom(scroll_rom()));
    run_frames(g1, 5);
    std::vector<uint8_t> b1;
    g1.save_state(b1);
    gb::Gameboy g2;
    REQUIRE(g2.load_rom(scroll_rom()));
    REQUIRE(g2.load_state(b1));
    std::vector<uint8_t> b2;
    g2.save_state(b2);
    REQUIRE(b1 == b2);
}

namespace {

// cgb cart that parks data in vram bank 1 and wram bank 3, then scrolls
std::vector<uint8_t> cgb_scroll_rom() {
    std::vector<uint8_t> rom = make_test_rom(0x00, 0x00, 0x8000, 0xC0);
    const uint8_t code[] = {
        0x3E, 0xFF,       // ld a, 0xff
        0xEA, 0x10, 0x80, // ld (0x8010), a
        0x3E, 0x01,       // ld a, 1
        0xEA, 0x00, 0x98, // ld (0x9800), a
        0x3E, 0x01,       // ld a, 1
        0xE0, 0x4F,       // ldh (vbk), a
        0x3E, 0xAA,       // ld a, 0xaa
        0xEA, 0x00, 0x80, // ld (0x8000), a
        0x3E, 0x03,       // ld a, 3
        0xE0, 0x70,       // ldh (svbk), a
        0x3E, 0x5A,       // ld a, 0x5a
        0xEA, 0x00, 0xD0, // ld (0xd000), a
        0x0C,             // loop: inc c
        0x79,             // ld a, c
        0xE0, 0x43,       // ldh (scx), a
        0x18, 0xFA,       // jr loop
    };
    for (size_t i = 0; i < sizeof(code); ++i) {
        rom[0x0100 + i] = code[i];
    }
    rom[0x014D] = test_rom_checksum(rom);
    return rom;
}

} // namespace

TEST_CASE("state_sections_size_the_banked_regions") {
    REQUIRE(gb::Ppu::kStateSize == 16693u);
    REQUIRE(gb::Bus::kStateSize == 32898u);
}

TEST_CASE("version_1_blobs_are_rejected") {
    gb::Gameboy gameboy;
    REQUIRE(gameboy.load_rom(scroll_rom()));
    run_frames(gameboy, 2);
    std::vector<uint8_t> blob;
    gameboy.save_state(blob);
    REQUIRE(blob[4] == 2);
    std::vector<uint8_t> v1 = blob;
    v1[4] = 1;
    REQUIRE(!gameboy.load_state(v1));
    REQUIRE(gameboy.load_state(blob));
}

TEST_CASE("cgb_banked_state_roundtrips") {
    gb::Gameboy g1;
    REQUIRE(g1.load_rom(cgb_scroll_rom()));
    REQUIRE(g1.cgb_mode());
    run_frames(g1, 5);
    std::vector<uint8_t> b1;
    g1.save_state(b1);
    gb::Gameboy g2;
    REQUIRE(g2.load_rom(cgb_scroll_rom()));
    REQUIRE(g2.load_state(b1));
    std::vector<uint8_t> b2;
    g2.save_state(b2);
    // byte-identical re-save proves vram bank 1, wram bank 3, vbk and svbk all survived
    REQUIRE(b1 == b2);
    run_frames(g1, 3);
    run_frames(g2, 3);
    REQUIRE(fb_hash(g1) == fb_hash(g2));
}
