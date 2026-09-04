// the generated file select art is const rodata in bank 7, and a banked const array only reads
// correctly while its own bank is paged in - so the loader that hands it to vram lives there too,
// beside title_art
#pragma bank 7

#include "file_art.h"

#include "gen/file_labels.h"
#include "gen/file_select.h"
#include "mario.h"

#include <gb/cgb.h>
#include <gb/gb.h>
#include <stdint.h>

// the generated screen plans two palettes and neither puts white on color 1, so the labels
// get a slot of their own past them
#define kFilePalLabel kFileSelectPaletteCount
#define kFileLabelRow 5U
// a 24 px word centred over a 32 px pipe needs a 4 px offset a bg cell cannot carry, so the strip
// is cut per label rather than per letter: four tiles each, NEW then 1-1 through 1-4
#define kFileLabelCells 4U

static const palette_color_t kFileLabelPalette[4] = {0x0000, 0x7FFF, 0x0000, 0x0000};
// each slot's label spans its own pipe, starting at that pipe's left column
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
    set_bkg_data(kFileSelectTileCount, kFileLabelsTileCount, kFileLabelsTiles);
    VBK_REG = VBK_BANK_0;
    set_bkg_palette(0, kFileSelectPaletteCount, kFileSelectPalettes);
    set_bkg_palette(kFilePalLabel, 1, kFileLabelPalette);
    set_bkg_tiles(0, 0, kFileSelectCols, kFileSelectRows, kFileSelectMap);
    set_bkg_attributes(0, 0, kFileSelectCols, kFileSelectRows, kFileSelectAttrs);
}

void file_art_label(uint8_t slot, uint8_t level_or_new) BANKED {
    uint8_t tiles[kFileLabelCells];
    uint8_t attrs[kFileLabelCells];
    uint8_t first;
    uint8_t i;

    // world one is all there is, so a used file's label is the level it stands on and index 0 is
    // the one label that is not a level, "NEW"
    first = (uint8_t)(kFileSelectTileCount + level_or_new * kFileLabelCells);
    for (i = 0; i < kFileLabelCells; ++i) {
        tiles[i] = (uint8_t)(first + i);
        attrs[i] = (uint8_t)(kFilePalLabel | kCamAttrVram1);
    }
    set_bkg_tiles(kFileLabelCol[slot], kFileLabelRow, kFileLabelCells, 1, tiles);
    set_bkg_attributes(kFileLabelCol[slot], kFileLabelRow, kFileLabelCells, 1, attrs);
}
