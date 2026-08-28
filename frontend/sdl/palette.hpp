#pragma once

#include <array>
#include <cstdint>

// standard tetris shape colors: i, o, t, s, z, j, l
inline constexpr std::array<uint32_t, 7> kPieceColors = {
    0xFF00BCD4u, 0xFFFFC107u, 0xFF9C27B0u, 0xFF4CAF50u, 0xFFF44336u, 0xFF2196F3u, 0xFFFF9800u,
};
inline constexpr uint32_t kBlack = 0xFF000000u;
inline constexpr uint32_t kWhite = 0xFFFFFFFFu;
// faint beveled grid on the empty background
inline constexpr uint32_t kGridFace = 0xFF101014u;
inline constexpr uint32_t kGridLight = 0xFF1E1E26u;
inline constexpr uint32_t kGridDark = 0xFF050508u;

inline uint32_t scale_rgb(uint32_t c, uint32_t num, uint32_t den) {
    const uint32_t r = ((c >> 16) & 0xFF) * num / den;
    const uint32_t g = ((c >> 8) & 0xFF) * num / den;
    const uint32_t b = (c & 0xFF) * num / den;
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

inline uint32_t lighten_rgb(uint32_t c, uint32_t num, uint32_t den) {
    const uint32_t r = ((c >> 16) & 0xFF) + (255 - ((c >> 16) & 0xFF)) * num / den;
    const uint32_t g = ((c >> 8) & 0xFF) + (255 - ((c >> 8) & 0xFF)) * num / den;
    const uint32_t b = (c & 0xFF) + (255 - (c & 0xFF)) * num / den;
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

// id: low byte tile index, bit 8 sprite; block slots are tiles 0x80-0x8f and the
// slot number is the shape's identity, so a piece keeps one color for its life.
// the falling piece is sprites whose tile index mirrors its slot at 0x00-0x0f;
// sprite_mask says which mirror slots truly hold styles, so ui sprites that
// happen to use low tiles (the piece editor icons) keep their own art.
inline uint32_t colorize(uint16_t id, uint8_t shade, uint32_t x, uint32_t y, uint16_t block_mask,
                         uint16_t sprite_mask) {
    if ((shade & 0x3u) == 0) {
        // menus have no block bank in vram, so they stay flat black
        if (block_mask == 0) {
            return kBlack;
        }
        // in game, empty space shows a faint beveled grid so the play area reads clearly
        const uint32_t cx = x & 7;
        const uint32_t cy = y & 7;
        if (cx == 0 || cy == 0) {
            return kGridLight;
        }
        if (cx == 7 || cy == 7) {
            return kGridDark;
        }
        return kGridFace;
    }
    const uint8_t tile = static_cast<uint8_t>(id & 0xFF);
    const bool sprite = (id & 0x100) != 0;
    uint8_t slot = 0;
    bool is_block = false;
    if (tile >= 0x80 && tile <= 0x8F) {
        // bg blocks, and sprites reusing the bg bank (the editor's tile row)
        slot = static_cast<uint8_t>(tile - 0x80);
        is_block = ((block_mask >> slot) & 1u) != 0;
    } else if (sprite && tile <= 0x0F) {
        slot = tile;
        is_block = ((sprite_mask >> slot) & 1u) != 0;
    }
    if (is_block) {
        const uint32_t c = kPieceColors[slot % kPieceColors.size()];
        // classic block bevel inside each 8x8 cell
        const uint32_t cx = x & 7;
        const uint32_t cy = y & 7;
        if (cx == 0 || cy == 0) {
            return lighten_rgb(c, 4, 9);
        }
        if (cx == 7 || cy == 7) {
            return scale_rgb(c, 5, 9);
        }
        return c;
    }
    switch (shade & 0x3u) {
    case 1:
        return 0xFF4C4C55u;
    case 2:
        return 0xFF9C9CA8u;
    default:
        return kWhite;
    }
}

// four argb entries per shade, the way a game boy color palette works
using Palette4 = std::array<uint32_t, 4>;

// shade 0 is black and shade 3 white here, and the games set bgp to identity
inline constexpr Palette4 kGrayShades = {kBlack, 0xFF4C4C55u, 0xFF9C9CA8u, kWhite};

// both games draw their popup bands from an inverted copy of the font parked at 0x60,
// which flips shade 0 and 3: white strokes on a dark card
inline constexpr Palette4 kPopupCard = {kWhite, 0xFF9C9CA8u, 0xFF4C4C55u, 0xFF102040u};

// crossy road pins every tile id, so each lane kind gets its own palette.
// the art's own shade usage drives which entries matter: grass is shade 0 with
// shade 1 sprigs, road is flat shade 2, water is one flat shade under its ripples.
inline constexpr Palette4 kCrossyGrass = {0xFF74BE45u, 0xFF4C9A2Bu, 0xFF97D46Fu, 0xFFC6EDA4u};
// the alt lane draws its sprigs in shade 2, the even lane's in shade 1
inline constexpr Palette4 kCrossyGrassAlt = {0xFF57972Fu, 0xFF44802Au, 0xFF3D7220u, 0xFF8CC05Cu};
inline constexpr Palette4 kCrossyTree = {0xFF0C2A10u, 0xFF14421Bu, 0xFF0E3616u, 0xFF1A5322u};
inline constexpr Palette4 kCrossyRoad = {0xFF2A2C30u, 0xFF4A4E54u, 0xFF6E7278u, 0xFFB9BEC4u};
// the dash is the tile's shade 0, so it is the one entry that goes white
inline constexpr Palette4 kCrossyRoadStripe = {0xFFF4F4ECu, 0xFF4A4E54u, 0xFF6E7278u, 0xFFB9BEC4u};
// the even lane's water is shade 2 with shade 3 ripples: the shallow blue
inline constexpr Palette4 kCrossyWater = {0xFF0B2A66u, 0xFF2A6FD8u, 0xFF3F86E8u, 0xFF66A9F4u};
// the odd lane's is shade 1 with shade 2 ripples, a clear step deeper and the same hue
inline constexpr Palette4 kCrossyWaterDark = {0xFF06183Fu, 0xFF1F4FA6u, 0xFF3A76CEu, 0xFF4E8BD8u};
inline constexpr Palette4 kCrossyRail = {0xFF2A2018u, 0xFF5A3A22u, 0xFF8A8580u, 0xFFD8DCE0u};
// the crossbuck cell is shade 3, its x shade 0: a red field makes the blink pop
inline constexpr Palette4 kCrossyWarn = {0xFFFFE9A8u, 0xFF8C1E16u, 0xFFB52A20u, 0xFFE23B2Eu};
// the car's outline and wheels are shade 1, its panels shade 2 and its glass shade 3
inline constexpr Palette4 kCrossyCar = {kBlack, 0xFF2A1410u, 0xFFE0342Au, 0xFFCFE6FFu};
// the log's rim is shade 1, its bark shade 2 and its lit crown shade 3
inline constexpr Palette4 kCrossyLog = {kBlack, 0xFF3F2611u, 0xFF9A6534u, 0xFFC08F4Eu};
// the train's windows are its shade 3, so they light up warm against the slate
inline constexpr Palette4 kCrossyTrain = {kBlack, 0xFF1E242Fu, 0xFF5A6473u, 0xFFFFD34Au};
inline constexpr Palette4 kCrossyEagle = {kBlack, 0xFF4A3018u, 0xFFF2EAD8u, 0xFFE0A032u};
// a near black outline round a warm yellow body, so the chick reads on grass, water, asphalt and bark
// its eye is the outline shade and its beak, feet and wing the orange one
inline constexpr Palette4 kCrossyChick = {kBlack, 0xFF2A1A08u, 0xFFF7D14Au, 0xFFFF7A10u};

// id: low byte tile index, bit 8 sprite. crossy's bg and sprite tiles occupy
// disjoint pinned ranges, so the tile index alone picks the palette.
inline uint32_t colorize_crossy(uint16_t id, uint8_t shade) {
    const uint8_t tile = static_cast<uint8_t>(id & 0xFFu);
    const uint8_t s = shade & 0x3u;
    switch (tile) {
    case 0xA0:
        return kCrossyGrass[s];
    case 0xA1:
        return kCrossyTree[s];
    case 0xA2:
        return kCrossyRoad[s];
    case 0xA3:
        return kCrossyRoadStripe[s];
    case 0xA4:
        return kCrossyWater[s];
    case 0xA5:
        return kCrossyRail[s];
    case 0xA6:
        return kCrossyWarn[s];
    case 0xA7:
        return kCrossyGrassAlt[s];
    case 0xA8:
        return kCrossyWaterDark[s];
    // the train's carriages are bg tiles, so they take the same palette its head and tail wear
    case 0xA9:
    case 0xAA:
        return kCrossyTrain[s];
    default:
        break;
    }
    // the hover banner and the game over popup are inverted font cells, so they take the card
    if (tile >= 0x60 && tile <= 0x9F) {
        return kPopupCard[s];
    }
    // every sprite is an 8x16 pair, so each family owns an even aligned run of tiles
    if (tile >= 0xB0 && tile <= 0xB3) {
        return kCrossyCar[s];
    }
    if (tile >= 0xB4 && tile <= 0xB9) {
        return kCrossyLog[s];
    }
    if (tile >= 0xBC && tile <= 0xBF) {
        return kCrossyEagle[s];
    }
    if (tile >= 0xC0 && tile <= 0xC7) {
        return kCrossyTrain[s];
    }
    if (tile >= 0xE0 && tile <= 0xE3) {
        return kCrossyChick[s];
    }
    // font, hud digits and anything unmapped keep the plain gray look
    return kGrayShades[s];
}

// flappy wears super mario colors: sky blue, pipe green, brick ground.
// the rom clears bg cells to the font's space glyph, so bg shade 0 is the sky.
inline constexpr uint32_t kFlappySkyBlue = 0xFF5C94FCu;
// font strokes are shade 3; the title and hud text reads white on the sky
inline constexpr Palette4 kFlappyText = {kFlappySkyBlue, kWhite, kWhite, kWhite};
// pipe art: 1 highlight, 2 fill, 3 outline
inline constexpr Palette4 kFlappyPipe = {kFlappySkyBlue, 0xFF80D010u, 0xFF00A800u, 0xFF0A3806u};
// ground art: 1 brick fill, 2 speckles, 3 the rim row
inline constexpr Palette4 kFlappyGround = {kFlappySkyBlue, 0xFFC84C0Cu, 0xFFFC9838u, 0xFF40140Cu};
// digit sprites: 1 is the glyph, the rest its dark halo over a pipe
inline constexpr Palette4 kFlappyDigit = {kFlappySkyBlue, kWhite, 0xFF201810u, 0xFF201810u};
// bird art: 1 body, 2 wing and beak, 3 outline
inline constexpr Palette4 kFlappyBird = {kFlappySkyBlue, 0xFFFCD800u, 0xFFFC6000u, 0xFF503000u};

// id: low byte tile index, bit 8 sprite. flappy's bg and sprite tiles occupy
// disjoint pinned ranges, so the tile index alone picks the palette.
inline uint32_t colorize_flappy(uint16_t id, uint8_t shade) {
    const uint8_t tile = static_cast<uint8_t>(id & 0xFFu);
    const uint8_t s = shade & 0x3u;
    if (tile <= 0x5F) {
        return kFlappyText[s];
    }
    if (tile <= 0x9F) {
        return kPopupCard[s];
    }
    if (tile >= 0xA0 && tile <= 0xA3) {
        return kFlappyPipe[s];
    }
    if (tile == 0xB0) {
        return kFlappyGround[s];
    }
    if (tile >= 0xD0 && tile <= 0xD9) {
        return kFlappyDigit[s];
    }
    if (tile >= 0xE0 && tile <= 0xE2) {
        return kFlappyBird[s];
    }
    return kGrayShades[s];
}
