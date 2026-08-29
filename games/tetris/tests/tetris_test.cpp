#include "cartridge.hpp"
#include "gameboy.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <set>
#include <span>
#include <string>
#include <vector>

namespace {

std::vector<uint8_t> read_tetris_rom() {
    std::ifstream in(TETRIS_ROM_PATH, std::ios::binary);
    REQUIRE(in.good());
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

constexpr uint32_t kBootFrames = 120;

// pandocs 0x143: 0x80 is dual dmg/cgb, 0xc0 is cgb only. this rom targets cgb only.
constexpr size_t kHeaderCgbFlagOffset = 0x143;
constexpr uint8_t kCgbFlagOnly = 0xC0;

size_t count_lit_pixels(std::span<const uint8_t> fb) {
    size_t lit = 0;
    for (uint8_t shade : fb) {
        if (shade != 0) {
            ++lit;
        }
    }
    return lit;
}

void run(gb::Gameboy& gameboy, uint32_t frames) {
    for (uint32_t i = 0; i < frames; ++i) {
        gameboy.run_frame();
    }
}

} // namespace

TEST_CASE("tetris_rom_declares_a_cgb_only_mbc1_battery_cart") {
    const std::vector<uint8_t> rom = read_tetris_rom();
    REQUIRE(rom.size() == 32768u);
    REQUIRE(rom[kHeaderCgbFlagOffset] == kCgbFlagOnly);

    std::string why;
    auto cart = gb::Cartridge::parse(rom, &why);
    REQUIRE(why.empty());
    REQUIRE(cart.has_value());
    REQUIRE(cart->type() == gb::CartType::Mbc1);
    REQUIRE(cart->has_battery());
    REQUIRE(cart->ram_size() > 0u);
    REQUIRE(cart->cgb());
    REQUIRE(cart->title() == "TETRIS");
}

TEST_CASE("tetris_boots_in_cgb_mode") {
    const std::vector<uint8_t> rom = read_tetris_rom();

    gb::Gameboy gameboy;
    REQUIRE(gameboy.load_rom(rom));
    run(gameboy, kBootFrames);
    // the frontend gates its colored-look hack on this; a unit test cannot assert the look itself
    REQUIRE(gameboy.cgb_mode());
}

TEST_CASE("tetris_boot_is_deterministic") {
    const std::vector<uint8_t> rom = read_tetris_rom();

    gb::Gameboy first;
    gb::Gameboy second;
    REQUIRE(first.load_rom(rom));
    REQUIRE(second.load_rom(rom));
    run(first, kBootFrames);
    run(second, kBootFrames);

    const std::span<const uint8_t> a = first.framebuffer();
    const std::span<const uint8_t> b = second.framebuffer();
    REQUIRE(a.size() == b.size());
    REQUIRE(std::equal(a.begin(), a.end(), b.begin()));

    const std::span<const uint16_t> ca = first.framebuffer_color();
    const std::span<const uint16_t> cb = second.framebuffer_color();
    REQUIRE(ca.size() == cb.size());
    REQUIRE(std::equal(ca.begin(), ca.end(), cb.begin()));
}

TEST_CASE("tetris_boots_to_a_non_blank_title_card") {
    const std::vector<uint8_t> rom = read_tetris_rom();

    gb::Gameboy gameboy;
    REQUIRE(gameboy.load_rom(rom));
    run(gameboy, kBootFrames);
    // the shade framebuffer carries raw 2-bit indices in cgb mode; any nonzero index is drawn content
    REQUIRE(count_lit_pixels(gameboy.framebuffer()) > 100u);
}

TEST_CASE("tetris_title_uses_more_than_two_cgb_colors") {
    const std::vector<uint8_t> rom = read_tetris_rom();

    gb::Gameboy gameboy;
    REQUIRE(gameboy.load_rom(rom));
    run(gameboy, kBootFrames);

    // two bg palettes landed (title text, border strip), so more than two distinct rgb555 values
    // must be on screen; a single dmg-style bgp swap could never produce this
    const std::span<const uint16_t> colors = gameboy.framebuffer_color();
    const std::set<uint16_t> distinct(colors.begin(), colors.end());
    REQUIRE(distinct.size() > 2u);
}
