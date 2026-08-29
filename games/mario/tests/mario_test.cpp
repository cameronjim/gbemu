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
#include <utility>
#include <vector>

namespace {

std::vector<uint8_t> read_mario_rom() {
    std::ifstream in(MARIO_ROM_PATH, std::ios::binary);
    REQUIRE(in.good());
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

constexpr uint32_t kBootFrames = 120;

// the rows main.c tints and prints to, per games/mario/src/mario.h
constexpr uint32_t kTitleRow = 6;
constexpr uint32_t kPromptRow = 10;

// gbdk's ibm font lands ascii 0x20-0x7f on tiles 0x00-0x5f; space (tile 0) is excluded so a
// span only ever measures real glyphs
constexpr uint8_t kFontFirstTile = 0x00;
constexpr uint8_t kFontLastTile = 0x5F;

void run(gb::Gameboy& gameboy, uint32_t frames) {
    for (uint32_t i = 0; i < frames; ++i) {
        gameboy.run_frame();
    }
}

// leftmost and rightmost bg cell columns on a tile row whose id is in [lo, hi]
std::pair<int, int> glyph_span(const gb::Gameboy& gameboy, uint32_t row, uint8_t lo, uint8_t hi) {
    const std::span<const uint16_t> ids = gameboy.framebuffer_tiles();
    int left = -1;
    int right = -1;
    for (uint32_t cx = 0; cx < 20; ++cx) {
        const uint16_t id = ids[(row * 8 + 3) * gb::kLcdWidth + cx * 8 + 3];
        if ((id & 0x100u) != 0) {
            continue;
        }
        const uint8_t tile = static_cast<uint8_t>(id);
        if (tile >= lo && tile <= hi) {
            if (left < 0) {
                left = static_cast<int>(cx);
            }
            right = static_cast<int>(cx);
        }
    }
    return {left, right};
}

// a 5-bit channel that is not the same as the other two makes the color read as non-gray
bool is_non_gray(uint16_t rgb555) {
    const uint16_t r = rgb555 & 0x1Fu;
    const uint16_t g = (rgb555 >> 5) & 0x1Fu;
    const uint16_t b = (rgb555 >> 10) & 0x1Fu;
    return !(r == g && g == b);
}

} // namespace

TEST_CASE("mario_rom_declares_a_cgb_mbc5_cart") {
    const std::vector<uint8_t> rom = read_mario_rom();
    REQUIRE(rom.size() == 131072u);

    std::string why;
    auto cart = gb::Cartridge::parse(rom, &why);
    REQUIRE(why.empty());
    REQUIRE(cart.has_value());
    REQUIRE(cart->type() == gb::CartType::Mbc5);
    REQUIRE(cart->has_battery());
    REQUIRE(cart->ram_size() > 0u);
    REQUIRE(cart->cgb());
    REQUIRE(cart->title() == "MARIO");
}

TEST_CASE("mario_boots_to_a_colored_title") {
    const std::vector<uint8_t> rom = read_mario_rom();

    gb::Gameboy gameboy;
    REQUIRE(gameboy.load_rom(rom));
    run(gameboy, kBootFrames);
    REQUIRE(gameboy.cgb_mode());

    const std::span<const uint16_t> colors = gameboy.framebuffer_color();
    std::set<uint16_t> distinct(colors.begin(), colors.end());
    REQUIRE(distinct.size() >= 4u);

    size_t non_gray = 0;
    for (uint16_t c : distinct) {
        if (is_non_gray(c)) {
            ++non_gray;
        }
    }
    REQUIRE(non_gray >= 2u);
}

TEST_CASE("mario_title_text_is_centered") {
    const std::vector<uint8_t> rom = read_mario_rom();

    gb::Gameboy gameboy;
    REQUIRE(gameboy.load_rom(rom));
    run(gameboy, kBootFrames);

    // even-length lines land symmetric around the 20 column grid: left + right == 19
    for (uint32_t row : {kTitleRow, kPromptRow}) {
        const auto [left, right] = glyph_span(gameboy, row, kFontFirstTile + 1, kFontLastTile);
        REQUIRE(left >= 0);
        REQUIRE(left + right == 19);
    }
}

TEST_CASE("mario_boot_is_deterministic") {
    const std::vector<uint8_t> rom = read_mario_rom();

    gb::Gameboy first;
    gb::Gameboy second;
    REQUIRE(first.load_rom(rom));
    REQUIRE(second.load_rom(rom));
    run(first, kBootFrames);
    run(second, kBootFrames);

    const std::span<const uint16_t> a = first.framebuffer_color();
    const std::span<const uint16_t> b = second.framebuffer_color();
    REQUIRE(a.size() == b.size());
    REQUIRE(std::equal(a.begin(), a.end(), b.begin()));
}
