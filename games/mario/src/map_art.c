// the generated world map art is const rodata in bank 7, and a banked const array only reads
// correctly while its own bank is paged in - so the loader that hands it to vram lives there too,
// beside title_art and file_art
#pragma bank 7

#include "map_art.h"

#include "gen/map_glyphs.h"
#include "gen/map_screen.h"
#include "gen/map_sprites.h"
#include "gen/map_water_frames.h"
#include "mario.h"

#include <gb/cgb.h>
#include <gb/gb.h>
#include <stdint.h>

// the glyph run sits straight past the screen's own tiles, all of it in vram bank 1
#define kMapGlyphFirst (uint8_t)(kMapScreenFirstTile + kMapScreenTileCount)
#define kMapGlyphAttr (uint8_t)(kMapPalText | kCamAttrVram1)
// bank 0 sprite 0x92-0x97, the run VRAM.md calls free: three 8x16 markers, two tiles each
#define kTileMapMarkerRed 0x92U
#define kTileMapMarkerBlue 0x94U
#define kTileMapMarkerRim 0x96U
// the map is a card screen, so it borrows the three enemy/fire obj slots; player_init loads the
// real sprite palettes back on the way into a level
#define kPalMarkerRed 5U
#define kPalMarkerBlue 6U
#define kPalMarkerRim 7U

// the water cells the strip animates, as offsets into the generated tile map: the shimmer band
// that runs across the lake, and the foam at the foot of the island's fall
#define kMapWaterCell (uint16_t)(6U * kMapScreenCols + 8U)
#define kMapFallCell (uint16_t)(11U * kMapScreenCols + 11U)

static const palette_color_t kMapTextPalette[4] = {kMapSkyRgb, 0x7FFF, kMapSkyRgb, 0x7FFF};
// gold behind, black on top: the card's call to action, the way the reference bands its own
static const palette_color_t kMapHilitePalette[4] = {RGB(31, 22, 2), 0x0000, RGB(31, 22, 2), 0x0000};
// index 0 is transparent in all three; the body pair carries outline, fill and gloss, the rim only
// the marker's own orange edge
static const palette_color_t kMapRedPalette[4] = {0, 0x0000, RGB(31, 0, 0), 0x7FFF};
static const palette_color_t kMapBluePalette[4] = {0, 0x0000, RGB(13, 9, 31), 0x7FFF};
static const palette_color_t kMapRimPalette[4] = {0, RGB(31, 23, 8), 0, 0};

static uint8_t map_row[kRingTileCols];

// nothing here scrolls, but the ring is left holding a level's terrain, so the whole map goes back
// to the frame's own top-left cell first
static void fill_band(void) {
    uint8_t y;
    uint8_t x;

    for (x = 0; x < kRingTileCols; ++x) {
        map_row[x] = kMapScreenMap[0];
    }
    for (y = 0; y < kBgMapRows; ++y) {
        set_bkg_tiles(0, y, kRingTileCols, 1, map_row);
    }
    for (x = 0; x < kRingTileCols; ++x) {
        map_row[x] = kMapScreenAttrs[0];
    }
    for (y = 0; y < kBgMapRows; ++y) {
        set_bkg_attributes(0, y, kRingTileCols, 1, map_row);
    }
}

static void put_glyph(uint8_t col, uint8_t row, uint8_t glyph) {
    const uint8_t tile = (uint8_t)(kMapGlyphFirst + glyph);
    const uint8_t attr = kMapGlyphAttr;

    set_bkg_tiles(col, row, 1, 1, &tile);
    set_bkg_attributes(col, row, 1, 1, &attr);
}

void map_art_rows(uint8_t row, uint8_t rows) BANKED {
    const uint16_t first = (uint16_t)row * kMapScreenCols;

    set_bkg_tiles(0, row, kMapScreenCols, rows, kMapScreenMap + first);
    set_bkg_attributes(0, row, kMapScreenCols, rows, kMapScreenAttrs + first);
}

void map_art_load(void) BANKED {
    fill_band();
    // the attribute bytes already carry bit 3, so every cell of the frame reads bank 1
    VBK_REG = VBK_BANK_1;
    set_bkg_data(kMapScreenFirstTile, kMapScreenTileCount, kMapScreenTiles);
    set_bkg_data(kMapGlyphFirst, kMapGlyphsTileCount, kMapGlyphsTiles);
    VBK_REG = VBK_BANK_0;
    set_bkg_palette(0, kMapScreenPaletteCount, kMapScreenPalettes);
    set_bkg_palette(kMapPalText, 1, kMapTextPalette);
    set_bkg_palette(kMapPalHilite, 1, kMapHilitePalette);
    set_sprite_data(kTileMapMarkerRed, kMapSpritesTileCount, kMapSpritesTiles);
    set_sprite_palette(kPalMarkerRed, 1, kMapRedPalette);
    set_sprite_palette(kPalMarkerBlue, 1, kMapBluePalette);
    set_sprite_palette(kPalMarkerRim, 1, kMapRimPalette);
    map_art_rows(0, kMapScreenRows);
}

void map_art_world(uint8_t level) BANKED {
    put_glyph(kMapWorldDigitCol, kMapDigitRow, 1);
    put_glyph(kMapLevelDigitCol, kMapDigitRow, (uint8_t)(level + 1U));
}

void map_art_lives(uint8_t lives) BANKED {
    put_glyph(kMapLivesDigitCol, kMapFooterRow, (uint8_t)(lives / 10U));
    put_glyph((uint8_t)(kMapLivesDigitCol + 1U), kMapFooterRow, (uint8_t)(lives % 10U));
}

void map_art_clear_list(uint8_t cleared) BANKED {
    uint8_t i;

    for (i = 0; i < kMapNodeCount; ++i) {
        const uint8_t col = (uint8_t)(kMapListFirstCol + i);

        if (i < cleared) {
            put_glyph(col, kMapFooterRow, 10U);
        } else {
            // the hollow cell is the frame's own art, so it goes back from the generated map
            const uint16_t cell = (uint16_t)(kMapFooterRow * kMapScreenCols + col);
            set_bkg_tiles(col, kMapFooterRow, 1, 1, kMapScreenMap + cell);
            set_bkg_attributes(col, kMapFooterRow, 1, 1, kMapScreenAttrs + cell);
        }
    }
}

void map_art_animate(uint8_t frame) BANKED {
    const uint8_t f = (uint8_t)(frame % kMapWaterFrameCount);

    VBK_REG = VBK_BANK_1;
    set_bkg_data(kMapScreenMap[kMapWaterCell], 1, kMapWaterFramesTiles + (uint16_t)f * 16U);
    set_bkg_data(kMapScreenMap[kMapFallCell], 1,
                 kMapWaterFramesTiles + (uint16_t)(kMapWaterFrameCount + f) * 16U);
    VBK_REG = VBK_BANK_0;
}

void map_art_marker(uint8_t node, uint8_t state) BANKED {
    static const uint8_t node_x[kMapNodeCount] = kMapNodeXs;
    static const uint8_t node_y[kMapNodeCount] = kMapNodeYs;
    const uint8_t body = (uint8_t)(kSpriteMapMarkerFirst + node * 2U);
    const uint8_t rim = (uint8_t)(body + 1U);
    const uint8_t x = (uint8_t)(node_x[node] + kOamXOffset);
    const uint8_t y = (uint8_t)(node_y[node] + kOamYOffset);

    if (state == (uint8_t)kMapMarkerHidden) {
        move_sprite(body, 0, 0);
        move_sprite(rim, 0, 0);
        return;
    }
    // smbd rings every node it has not cleared, locked or not: the lock is the path refusing to
    // walk there, not a marker of its own
    set_sprite_tile(body, state == (uint8_t)kMapMarkerCleared ? (uint8_t)kTileMapMarkerRed
                                                              : (uint8_t)kTileMapMarkerBlue);
    set_sprite_prop(body,
                    state == (uint8_t)kMapMarkerCleared ? (uint8_t)kPalMarkerRed : (uint8_t)kPalMarkerBlue);
    set_sprite_tile(rim, kTileMapMarkerRim);
    set_sprite_prop(rim, kPalMarkerRim);
    move_sprite(body, x, y);
    move_sprite(rim, x, y);
}

void map_art_border(uint8_t col, uint8_t row, uint8_t glyph, uint8_t flip) BANKED {
    const uint8_t tile = (uint8_t)(kMapGlyphFirst + glyph);
    const uint8_t attr = (uint8_t)(kMapGlyphAttr | flip);

    set_bkg_tiles(col, row, 1, 1, &tile);
    set_bkg_attributes(col, row, 1, 1, &attr);
}

void map_art_blank(uint8_t col, uint8_t row, uint8_t width, uint8_t palette) BANKED {
    uint8_t x;

    for (x = 0; x < width; ++x) {
        map_row[x] = kTileSky;
    }
    set_bkg_tiles(col, row, width, 1, map_row);
    for (x = 0; x < width; ++x) {
        map_row[x] = palette;
    }
    set_bkg_attributes(col, row, width, 1, map_row);
}
