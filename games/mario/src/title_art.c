// the generated title art is const rodata in bank 7, and a banked const array only reads correctly
// while its own bank is paged in - so the loader that hands it to vram has to live there too
#pragma bank 7

#include "title_art.h"

#include "gen/title_deluxe.h"
#include "gen/title_screen.h"
#include "mario.h"

#include <gb/cgb.h>
#include <gb/gb.h>
#include <stdint.h>

// bank-0 sprite ids 0x80-0xa7, two tiles per 8x16 sprite; see games/mario/VRAM.md
#define kTitleSpriteFirst 0x80U
#define kTitleOamCount 20U
// the sparkle is the first two entries of the table below, so blinking it is a prefix
#define kTitleSparkleCount 2U

// the sheet's own four colors: transparent, bright gold, gold, brown outline
static const palette_color_t kTitleDeluxePalette[4] = {0x0000, 0x03BF, 0x0255, 0x090B};

// hand-copied from games/mario/art/title/title_deluxe_oam.json in its own order - the sparkle pair
// first, then the script. x/y are oam values, so the json's screen coordinates carry the 8/16 the
// hardware subtracts back off
// clang-format off
static const uint8_t kTitleOam[kTitleOamCount][4] = {
    // x,    y,    tile, prop
    {105U,  74U, 0x80U, 0U},
    {112U,  74U, 0x82U, S_FLIPX},
    { 88U,  79U, 0x84U, 0U},
    { 96U,  79U, 0x86U, 0U},
    {104U,  79U, 0x88U, 0U},
    {112U,  79U, 0x8AU, 0U},
    {121U,  79U, 0x8CU, 0U},
    {129U,  79U, 0x8EU, 0U},
    {147U,  79U, 0x90U, 0U},
    { 88U,  95U, 0x92U, 0U},
    { 96U,  95U, 0x94U, 0U},
    {104U,  95U, 0x96U, 0U},
    {112U,  95U, 0x98U, 0U},
    {120U,  95U, 0x9AU, 0U},
    {128U,  95U, 0x9CU, 0U},
    {136U,  95U, 0x9EU, 0U},
    {144U,  95U, 0xA0U, 0U},
    {152U,  95U, 0xA2U, 0U},
    {160U,  95U, 0xA4U, 0U},
    {136U,  95U, 0xA6U, 0U},
};
// clang-format on

static uint8_t map_row[kRingTileCols];

// only the visible 20x18 is ever on screen - nothing here scrolls - but the ring is left holding a
// level's terrain, so the whole map goes back to the art's own sky cell first
static void fill_sky(void) {
    uint8_t y;
    uint8_t x;

    for (x = 0; x < kRingTileCols; ++x) {
        map_row[x] = kTitleScreenMap[0];
    }
    for (y = 0; y < kBgMapRows; ++y) {
        set_bkg_tiles(0, y, kRingTileCols, 1, map_row);
    }
    for (x = 0; x < kRingTileCols; ++x) {
        map_row[x] = kTitleScreenAttrs[0];
    }
    for (y = 0; y < kBgMapRows; ++y) {
        set_bkg_attributes(0, y, kRingTileCols, 1, map_row);
    }
}

void title_art_load(void) BANKED {
    fill_sky();
    // the attribute bytes already carry bit 3, so every cell of the frame reads bank 1
    VBK_REG = VBK_BANK_1;
    set_bkg_data(kTitleScreenFirstTile, kTitleScreenTileCount, kTitleScreenTiles);
    VBK_REG = VBK_BANK_0;
    set_bkg_palette(0, kTitleScreenPaletteCount, kTitleScreenPalettes);
    set_bkg_tiles(0, 0, kTitleScreenCols, kTitleScreenRows, kTitleScreenMap);
    set_bkg_attributes(0, 0, kTitleScreenCols, kTitleScreenRows, kTitleScreenAttrs);
    set_sprite_data(kTitleSpriteFirst, kTitleDeluxeTileCount, kTitleDeluxeTiles);
    set_sprite_palette(0, 1, kTitleDeluxePalette);
}

void title_art_place_sprites(void) BANKED {
    uint8_t i;

    // the boot path reaches the title before any level has set the sprite height
    SPRITES_8x16;
    for (i = 0; i < kTitleOamCount; ++i) {
        set_sprite_tile(i, kTitleOam[i][2]);
        set_sprite_prop(i, kTitleOam[i][3]);
        move_sprite(i, kTitleOam[i][0], kTitleOam[i][1]);
    }
}

void title_art_sparkle(uint8_t on) BANKED {
    uint8_t i;

    for (i = 0; i < kTitleSparkleCount; ++i) {
        if (on != 0U) {
            move_sprite(i, kTitleOam[i][0], kTitleOam[i][1]);
        } else {
            move_sprite(i, 0, 0);
        }
    }
}
