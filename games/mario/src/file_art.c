// the generated file select art is const rodata in bank 7, and a banked const array only reads
// correctly while its own bank is paged in - so the loader that hands it to vram lives there too,
// beside title_art
#pragma bank 7

#include "file_art.h"

#include "gen/file_glyphs.h"
#include "gen/file_select.h"
#include "mario.h"

#include <gb/cgb.h>
#include <gb/gb.h>
#include <stdint.h>

// the generated screen plans two palettes and neither puts white on color 1, so the label glyphs
// get a slot of their own past them
#define kFilePalGlyph kFileSelectPaletteCount
#define kFileLabelRow 5U
#define kFileLabelCells 3U
// the glyph strip's own cut order: W, 1, dash, N, E, 2, 3, 4. both labels are three cells wide -
// "NEW", or the world-dash-level the file stands on - so the strip carries no blank
#define kGlyphW 0U
#define kGlyphOne 1U
#define kGlyphDash 2U
#define kGlyphN 3U
#define kGlyphE 4U
#define kGlyphDigitFirst 5U

static const palette_color_t kFileGlyphPalette[4] = {0x0000, 0x7FFF, 0x0000, 0x0000};
// each slot's label sits at its own pipe's left column
static const uint8_t kFileLabelCol[kSaveSlots] = {2U, 8U, 14U};

static uint8_t map_row[kRingTileCols];

// nothing here scrolls, but the ring is left holding a level's terrain, so the whole map goes back
// to the art's own black cell first
static void fill_black(void) {
    uint8_t y;
    uint8_t x;

    for (x = 0; x < kRingTileCols; ++x) {
        map_row[x] = kFileSelectMap[0];
    }
    for (y = 0; y < kBgMapRows; ++y) {
        set_bkg_tiles(0, y, kRingTileCols, 1, map_row);
    }
    for (x = 0; x < kRingTileCols; ++x) {
        map_row[x] = kFileSelectAttrs[0];
    }
    for (y = 0; y < kBgMapRows; ++y) {
        set_bkg_attributes(0, y, kRingTileCols, 1, map_row);
    }
}

void file_art_load(void) BANKED {
    fill_black();
    // the attribute bytes already carry bit 3, so every cell of the frame reads bank 1
    VBK_REG = VBK_BANK_1;
    set_bkg_data(kFileSelectFirstTile, kFileSelectTileCount, kFileSelectTiles);
    set_bkg_data(kFileSelectTileCount, kFileGlyphsTileCount, kFileGlyphsTiles);
    VBK_REG = VBK_BANK_0;
    set_bkg_palette(0, kFileSelectPaletteCount, kFileSelectPalettes);
    set_bkg_palette(kFilePalGlyph, 1, kFileGlyphPalette);
    set_bkg_tiles(0, 0, kFileSelectCols, kFileSelectRows, kFileSelectMap);
    set_bkg_attributes(0, 0, kFileSelectCols, kFileSelectRows, kFileSelectAttrs);
}

void file_art_label(uint8_t slot, uint8_t level_or_new) BANKED {
    uint8_t tiles[kFileLabelCells];
    uint8_t attrs[kFileLabelCells];
    uint8_t i;

    if (level_or_new == 0U) {
        tiles[0] = kGlyphN;
        tiles[1] = kGlyphE;
        tiles[2] = kGlyphW;
    } else {
        // world one is all there is, so the label is the world, the dash and the level number -
        // and the strip carries one "1" that the world and level 1 share
        tiles[0] = kGlyphOne;
        tiles[1] = kGlyphDash;
        tiles[2] =
            (level_or_new == 1U) ? (uint8_t)kGlyphOne : (uint8_t)(kGlyphDigitFirst + level_or_new - 2U);
    }
    for (i = 0; i < kFileLabelCells; ++i) {
        tiles[i] = (uint8_t)(kFileSelectTileCount + tiles[i]);
        attrs[i] = (uint8_t)(kFilePalGlyph | kCamAttrVram1);
    }
    set_bkg_tiles(kFileLabelCol[slot], kFileLabelRow, kFileLabelCells, 1, tiles);
    set_bkg_attributes(kFileLabelCol[slot], kFileLabelRow, kFileLabelCells, 1, attrs);
}
