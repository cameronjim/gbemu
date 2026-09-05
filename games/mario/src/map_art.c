// the generated world map art is const rodata in bank 7, and a banked const array only reads
// correctly while its own bank is paged in - so the loader that hands it to vram lives there too,
// beside title_art and file_art
#pragma bank 7

#include "map_art.h"

#include "gen/map_glyphs.h"
#include "gen/map_list_fill.h"
#include "gen/map_lives.h"
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
// the lives readout's own six ids - three columns by two rows - whose bytes map_art_lives rewrites,
// then the four clear-list variants, one per dash cell of world one's row
#define kMapLivesFirst (uint8_t)(kMapGlyphFirst + kMapGlyphsTileCount)
#define kMapLivesCols 3U
#define kMapListFillFirst (uint8_t)(kMapLivesFirst + kMapLivesCols * 2U)
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

// one cell handed a bank-1 tile of the map's own, under the white-on-band palette every runtime
// glyph reads
static void put_tile(uint8_t col, uint8_t row, uint8_t tile) {
    const uint8_t attr = kMapGlyphAttr;

    set_bkg_tiles(col, row, 1, 1, &tile);
    set_bkg_attributes(col, row, 1, 1, &attr);
}

static void put_glyph(uint8_t col, uint8_t row, uint8_t glyph) {
    put_tile(col, row, (uint8_t)(kMapGlyphFirst + glyph));
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
    set_bkg_data(kMapListFillFirst, kMapListFillTileCount, kMapListFillTiles);
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

// one digit's pre-shifted quadrants, laid top-left, top-right, bottom-left, bottom-right
static const uint8_t* lives_quad(uint8_t digit, uint8_t quad) {
    return kMapLivesTiles + ((uint16_t)digit * 4U + quad) * 16U;
}

// one cell of the readout. `left` and `right` are the quadrants landing in it: the two never share
// a pixel column, so the middle cell of a two-digit readout is simply the or of both
static void put_lives_cell(uint8_t index, uint8_t half, const uint8_t* left, const uint8_t* right) {
    const uint8_t tile = (uint8_t)(kMapLivesFirst + half * kMapLivesCols + index);
    uint8_t bytes[16];
    uint8_t i;

    for (i = 0; i < 16U; ++i) {
        bytes[i] = (uint8_t)(left[i] | (right != 0 ? right[i] : 0U));
    }
    VBK_REG = VBK_BANK_1;
    set_bkg_data(tile, 1, bytes);
    VBK_REG = VBK_BANK_0;
    put_tile((uint8_t)(kMapLivesDigitCol + index), (uint8_t)(kMapFooterRow + half), tile);
}

void map_art_lives(uint8_t lives) BANKED {
    const uint8_t tens = (uint8_t)(lives / 10U);
    const uint8_t units = (uint8_t)(lives % 10U);
    uint8_t half;

    // top half of the block, then the bottom: a digit spans two cells either way, and a tens digit
    // pushes the units one cell right rather than growing a leading zero
    for (half = 0; half < 2U; ++half) {
        const uint8_t quad = (uint8_t)(half * 2U);

        if (tens == 0U) {
            put_lives_cell(0, half, lives_quad(units, quad), 0);
            put_lives_cell(1, half, lives_quad(units, (uint8_t)(quad + 1U)), 0);
            continue;
        }
        put_lives_cell(0, half, lives_quad(tens, quad), 0);
        put_lives_cell(1, half, lives_quad(tens, (uint8_t)(quad + 1U)), lives_quad(units, quad));
        put_lives_cell(2, half, lives_quad(units, (uint8_t)(quad + 1U)), 0);
    }
}

void map_art_clear_list(uint8_t cleared) BANKED {
    uint8_t i;

    for (i = 0; i < kMapNodeCount; ++i) {
        const uint8_t col = (uint8_t)(kMapListFirstCol + i);
        const uint16_t cell = (uint16_t)(kMapListRow * kMapScreenCols + col);
        // a filled dash is the frame's own cell with its dash solid, so it rides the same
        // attribute byte; a cell still to do is the frame's cell itself
        const uint8_t tile = i < cleared ? (uint8_t)(kMapListFillFirst + i) : kMapScreenMap[cell];

        set_bkg_tiles(col, kMapListRow, 1, 1, &tile);
        set_bkg_attributes(col, kMapListRow, 1, 1, kMapScreenAttrs + cell);
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
