#include "palette.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>

TEST_CASE("colorize_background_grid_and_white_text") {
    // shade 0 renders the faint beveled grid in game, ui shade 3 is always white
    REQUIRE(colorize(0x00, 0, 3, 3, 0xFFFF, 0xFFFF) == kGridFace);
    REQUIRE(colorize(0x00, 0, 0, 3, 0xFFFF, 0xFFFF) == kGridLight);
    REQUIRE(colorize(0x00, 0, 3, 7, 0xFFFF, 0xFFFF) == kGridDark);
    REQUIRE(colorize(0x83, 0, 3, 3, 0xFFFF, 0xFFFF) == kGridFace);
    REQUIRE(colorize(0x41, 3, 5, 5, 0xFFFF, 0xFFFF) == kWhite);
    // block-range tiles not flagged as styles render as ui, not colored blocks
    REQUIRE(colorize(0x83, 3, 5, 5, 0x0000, 0x0000) == kWhite);
}

TEST_CASE("colorize_menu_background_is_flat") {
    // no block bank loaded means a menu screen: no grid anywhere
    REQUIRE(colorize(0x00, 0, 3, 3, 0x0000, 0x0000) == kBlack);
    REQUIRE(colorize(0x00, 0, 0, 3, 0x0000, 0x0000) == kBlack);
    REQUIRE(colorize(0x00, 0, 3, 7, 0x0000, 0x0000) == kBlack);
}

TEST_CASE("colorize_blocks_by_slot_identity") {
    // a piece's slot decides its color, independent of position; probe cell centers
    REQUIRE(colorize(0x80, 2, 3, 3, 0xFFFF, 0xFFFF) == kPieceColors[0]);
    REQUIRE(colorize(0x84, 2, 11, 35, 0xFFFF, 0xFFFF) == kPieceColors[4]);
    REQUIRE(colorize(0x86, 2, 3, 3, 0xFFFF, 0xFFFF) == kPieceColors[6]);
    // slots beyond seven wrap
    REQUIRE(colorize(0x87, 2, 3, 3, 0xFFFF, 0xFFFF) == kPieceColors[0]);
}

TEST_CASE("colorize_bevel_edges_within_cell") {
    const uint32_t center = colorize(0x80, 2, 3, 3, 0xFFFF, 0xFFFF);
    const uint32_t top = colorize(0x80, 2, 3, 0, 0xFFFF, 0xFFFF);
    const uint32_t bottom = colorize(0x80, 2, 3, 7, 0xFFFF, 0xFFFF);
    REQUIRE(center == kPieceColors[0]);
    REQUIRE(top != center);
    REQUIRE(bottom != center);
    REQUIRE(top != bottom);
}

TEST_CASE("colorize_falling_piece_by_sprite_tile") {
    // the falling piece's sprite tile mirrors its block slot, so it wears its
    // own color from the first frame and never changes on lock
    REQUIRE(colorize(0x102, 2, 3, 3, 0xFFFF, 0xFFFF) == kPieceColors[2]);
    REQUIRE(colorize(0x100, 2, 3, 3, 0xFFFF, 0xFFFF) == kPieceColors[0]);
    REQUIRE(colorize(0x106, 2, 3, 3, 0xFFFF, 0xFFFF) == kPieceColors[6]);
    // falling and locked color agree for the same slot
    REQUIRE(colorize(0x104, 2, 3, 3, 0xFFFF, 0xFFFF) == colorize(0x84, 2, 3, 3, 0xFFFF, 0xFFFF));
    // sprites outside the block bank render as ui shades
    REQUIRE(colorize(0x142, 3, 3, 3, 0xFFFF, 0xFFFF) == kWhite);
    REQUIRE(colorize(0x102, 3, 3, 3, 0x0000, 0x0000) == kWhite);
    // low-tile ui sprites whose vram slot is not a style keep their own art
    REQUIRE(colorize(0x104, 3, 3, 3, 0xFFFF, 0x0000) == kWhite);
    // a sprite reusing the bg block bank is a block (the editor's tile row)
    REQUIRE(colorize(0x180, 2, 3, 3, 0xFFFF, 0x0000) == kPieceColors[0]);
}

namespace {

uint32_t red_of(uint32_t c) {
    return (c >> 16) & 0xFFu;
}
uint32_t green_of(uint32_t c) {
    return (c >> 8) & 0xFFu;
}
uint32_t blue_of(uint32_t c) {
    return c & 0xFFu;
}

} // namespace

TEST_CASE("colorize_crossy_water_is_blue") {
    // both water lanes read as water at every entry
    for (const uint16_t tile : std::array<uint16_t, 2>{0xA4, 0xA8}) {
        for (uint8_t shade = 0; shade < 4; ++shade) {
            const uint32_t c = colorize_crossy(tile, shade);
            REQUIRE(blue_of(c) > red_of(c));
            REQUIRE(blue_of(c) > green_of(c));
        }
    }
}

TEST_CASE("colorize_crossy_water_lanes_step_by_parity") {
    // the even lane's base is shade 2, the odd lane's shade 1: one clear step deeper, same hue
    const uint32_t shallow = colorize_crossy(0xA4, 2);
    const uint32_t deep = colorize_crossy(0xA8, 1);
    REQUIRE(blue_of(shallow) > blue_of(deep) + 0x20u);
    REQUIRE(red_of(shallow) > red_of(deep));
    // and each lane's own ripple is a lift on its base, never a jump to another lane's shade
    const uint32_t shallow_ripple = colorize_crossy(0xA4, 3);
    const uint32_t deep_ripple = colorize_crossy(0xA8, 2);
    REQUIRE(blue_of(shallow_ripple) > blue_of(shallow));
    REQUIRE(blue_of(deep_ripple) > blue_of(deep));
    REQUIRE(blue_of(deep_ripple) < blue_of(shallow));
}

TEST_CASE("colorize_crossy_road_is_grey_with_white_markings") {
    const uint32_t road = colorize_crossy(0xA2, 2);
    // asphalt is neutral: no channel dominates
    REQUIRE(red_of(road) == 0x6Eu);
    REQUIRE(green_of(road) == 0x72u);
    REQUIRE(blue_of(road) == 0x78u);
    // the stripe tile is the same asphalt, its shade 0 dash the lane marking
    REQUIRE(colorize_crossy(0xA3, 2) == road);
    const uint32_t dash = colorize_crossy(0xA3, 0);
    REQUIRE(red_of(dash) > 0xE0u);
    REQUIRE(green_of(dash) > 0xE0u);
    REQUIRE(blue_of(dash) > 0xE0u);
    // plain asphalt never goes white where the stripe does
    REQUIRE(colorize_crossy(0xA2, 0) != dash);
}

TEST_CASE("colorize_crossy_grass_lanes_and_trees_differ") {
    // the two grasses must read as different lanes, and a tree on either of them
    REQUIRE(colorize_crossy(0xA0, 0) != colorize_crossy(0xA7, 0));
    REQUIRE(green_of(colorize_crossy(0xA0, 0)) > green_of(colorize_crossy(0xA7, 0)));
    for (const uint16_t grass : std::array<uint16_t, 2>{0xA0, 0xA7}) {
        REQUIRE(colorize_crossy(0xA1, 3) != colorize_crossy(grass, 0));
        REQUIRE(green_of(colorize_crossy(0xA1, 3)) < green_of(colorize_crossy(grass, 0)));
    }
}

TEST_CASE("colorize_crossy_warning_carries_a_red_accent") {
    const uint32_t field = colorize_crossy(0xA6, 3);
    REQUIRE(red_of(field) > green_of(field) + 0x40u);
    REQUIRE(red_of(field) > blue_of(field) + 0x40u);
}

TEST_CASE("colorize_crossy_movers_wear_their_own_colors") {
    // cars red, logs brown, train dark slate; every 8x16 pair of a family shares its palette
    const uint32_t car = colorize_crossy(0x1B0, 2);
    REQUIRE(red_of(car) > green_of(car) + 0x60u);
    REQUIRE(red_of(car) > blue_of(car) + 0x60u);
    for (uint16_t tile = 0x1B0; tile <= 0x1B3; ++tile) {
        REQUIRE(colorize_crossy(tile, 2) == car);
    }
    const uint32_t log = colorize_crossy(0x1B4, 2);
    REQUIRE(red_of(log) > green_of(log));
    REQUIRE(green_of(log) > blue_of(log));
    for (uint16_t tile = 0x1B4; tile <= 0x1B9; ++tile) {
        REQUIRE(colorize_crossy(tile, 2) == log);
    }
    const uint32_t train = colorize_crossy(0x1C0, 2);
    REQUIRE(red_of(train) < 0x60u);
    REQUIRE(blue_of(train) > red_of(train));
    for (uint16_t tile = 0x1C0; tile <= 0x1C7; ++tile) {
        REQUIRE(colorize_crossy(tile, 2) == train);
    }
    // the train's windows are its shade 3, so they light up warm
    const uint32_t window = colorize_crossy(0x1C0, 3);
    REQUIRE(red_of(window) > blue_of(window) + 0x60u);
    // the carriages between head and tail are bg tiles, and must not read as a different train
    for (uint8_t shade = 0; shade < 4; ++shade) {
        REQUIRE(colorize_crossy(0xA9, shade) == colorize_crossy(0x1C0, shade));
        REQUIRE(colorize_crossy(0xAA, shade) == colorize_crossy(0x1C0, shade));
    }
}

TEST_CASE("colorize_crossy_chick_pops_against_every_lane") {
    // the visibility guarantee: no chick shade collides with a lane's own color, carriages included
    constexpr std::array<uint16_t, 11> kLaneTiles = {0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5,
                                                     0xA6, 0xA7, 0xA8, 0xA9, 0xAA};
    for (uint8_t shade = 1; shade < 4; ++shade) {
        const uint32_t chick = colorize_crossy(0x1E0, shade);
        for (const uint16_t tile : kLaneTiles) {
            REQUIRE(chick != colorize_crossy(tile, shade));
        }
    }
    // a warm yellow body: no snowman, and never as pale as the log crown or the rail ballast
    const uint32_t body = colorize_crossy(0x1E0, 2);
    REQUIRE(red_of(body) > green_of(body));
    REQUIRE(green_of(body) > blue_of(body) + 0x40u);
    REQUIRE(red_of(body) > 0xE0u);
    REQUIRE(blue_of(body) < 0x80u);
    // beak, feet and wing are a deeper orange than the body, so they read as their own parts
    const uint32_t accent = colorize_crossy(0x1E1, 3);
    REQUIRE(red_of(accent) > green_of(accent));
    REQUIRE(green_of(accent) > blue_of(accent));
    REQUIRE(green_of(accent) + 0x40u < green_of(body));
    // and the outline stays near black against every one of them
    const uint32_t outline = colorize_crossy(0x1E0, 1);
    REQUIRE(red_of(outline) < 0x40u);
    REQUIRE(green_of(outline) < 0x40u);
    REQUIRE(blue_of(outline) < 0x40u);
}

TEST_CASE("colorize_crossy_digits_and_font_stay_gray") {
    for (uint8_t shade = 0; shade < 4; ++shade) {
        // the ten digit pairs keep the white badge they render as today
        for (uint16_t tile = 0x1C8; tile <= 0x1DB; ++tile) {
            REQUIRE(colorize_crossy(tile, shade) == kGrayShades[shade]);
        }
        // the plain font, and any unmapped tile
        REQUIRE(colorize_crossy(0x41, shade) == kGrayShades[shade]);
        REQUIRE(colorize_crossy(0x1BA, shade) == kGrayShades[shade]);
        // the inverted font is the popup band, and both games share one card for it
        REQUIRE(colorize_crossy(0x60, shade) == kPopupCard[shade]);
        REQUIRE(colorize_crossy(0x9F, shade) == kPopupCard[shade]);
        REQUIRE(colorize_crossy(0x60, shade) == colorize_flappy(0x60, shade));
    }
    REQUIRE(kGrayShades[0] == kBlack);
    REQUIRE(kGrayShades[3] == kWhite);
    // white strokes on a dark navy fill, the same look flappy's popup wears
    REQUIRE(colorize_crossy(0x60, 0) == kWhite);
    const uint32_t card = colorize_crossy(0x60, 3);
    REQUIRE(red_of(card) < 0x40u);
    REQUIRE(green_of(card) < 0x60u);
    REQUIRE(blue_of(card) > red_of(card));
    // digits match what the tetris path gives the same shades
    REQUIRE(colorize_crossy(0x1C8, 1) == colorize(0x1C8, 1, 3, 3, 0x0000, 0x0000));
    REQUIRE(colorize_crossy(0x1C8, 2) == colorize(0x1C8, 2, 3, 3, 0x0000, 0x0000));
    REQUIRE(colorize_crossy(0x1C8, 3) == colorize(0x1C8, 3, 3, 3, 0x0000, 0x0000));
}

TEST_CASE("colorize_flappy_sky_is_mario_blue") {
    // every bg tile's shade 0 is the sky, so the whole field reads as one blue
    for (const uint16_t tile : std::array<uint16_t, 4>{0x00, 0xA0, 0xB0, 0x41}) {
        const uint32_t c = colorize_flappy(tile, 0);
        REQUIRE(c == kFlappySkyBlue);
        REQUIRE(blue_of(c) > red_of(c));
        REQUIRE(blue_of(c) > green_of(c));
    }
    // title text is white strokes over that sky
    REQUIRE(colorize_flappy(0x41, 3) == kWhite);
}

TEST_CASE("colorize_flappy_pipes_are_mario_green") {
    // body fill, highlight edge and outline all read as green
    for (const uint16_t tile : std::array<uint16_t, 4>{0xA0, 0xA1, 0xA2, 0xA3}) {
        for (uint8_t shade = 1; shade < 4; ++shade) {
            const uint32_t c = colorize_flappy(tile, shade);
            REQUIRE(green_of(c) > red_of(c));
            REQUIRE(green_of(c) > blue_of(c));
        }
    }
    // the outline is darker than the fill, the highlight lighter
    REQUIRE(green_of(colorize_flappy(0xA0, 3)) < green_of(colorize_flappy(0xA0, 2)));
    REQUIRE(red_of(colorize_flappy(0xA0, 1)) > red_of(colorize_flappy(0xA0, 2)));
}

TEST_CASE("colorize_flappy_ground_is_brick") {
    // brick fill and speckles are warm: red leads green leads blue
    for (uint8_t shade = 1; shade < 3; ++shade) {
        const uint32_t c = colorize_flappy(0xB0, shade);
        REQUIRE(red_of(c) > green_of(c));
        REQUIRE(green_of(c) > blue_of(c));
    }
    // the rim stays dark so the ground line still reads
    REQUIRE(red_of(colorize_flappy(0xB0, 3)) < 0x60u);
}

TEST_CASE("colorize_flappy_bird_is_yellow_with_orange_accents") {
    const uint32_t body = colorize_flappy(0x1E0, 1);
    REQUIRE(red_of(body) > 0xE0u);
    REQUIRE(green_of(body) > 0xC0u);
    REQUIRE(blue_of(body) < 0x40u);
    // all three flap frames wear the same feathers
    REQUIRE(colorize_flappy(0x1E1, 1) == body);
    REQUIRE(colorize_flappy(0x1E2, 1) == body);
    const uint32_t beak = colorize_flappy(0x1E0, 2);
    REQUIRE(red_of(beak) > green_of(beak) + 0x40u);
    // the bird never vanishes into a pipe or the sky
    for (uint8_t shade = 1; shade < 4; ++shade) {
        REQUIRE(colorize_flappy(0x1E0, shade) != colorize_flappy(0xA0, shade));
        REQUIRE(colorize_flappy(0x1E0, shade) != kFlappySkyBlue);
    }
}

TEST_CASE("colorize_flappy_popup_and_digits_stay_legible") {
    // inverted glyphs: white strokes (shade 0) on a dark card (shade 3)
    REQUIRE(colorize_flappy(0x60, 0) == kWhite);
    const uint32_t card = colorize_flappy(0x60, 3);
    REQUIRE(red_of(card) < 0x40u);
    REQUIRE(green_of(card) < 0x60u);
    // hud digit sprites: white glyph in a dark halo
    REQUIRE(colorize_flappy(0x1D0, 1) == kWhite);
    REQUIRE(red_of(colorize_flappy(0x1D0, 3)) < 0x40u);
    // unmapped tiles keep the plain gray look
    REQUIRE(colorize_flappy(0xC0, 2) == kGrayShades[2]);
}
