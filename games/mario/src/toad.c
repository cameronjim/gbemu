// smb does not end a castle on the axe. the bridge goes, bowser goes into the lava with it, and
// mario walks right off the pedestal the axe stood on, drops into the room past it and stops in
// front of the mushroom retainer while the sign's three lines go up over him - and only then does
// the course-clear card take the screen. player.c's clear sequence owns the walk, because that is the
// physics pass; everything the walk ends in is here, in bank 6 with the other draw passes, because
// bank 0 has sixty-odd bytes left in it and bank 5 seventeen
#pragma bank 6

#include "toad.h"

#include "assets.h"
#include "camera.h"
#include "level.h"
#include "mario.h"
#include "player.h"

#include <gb/gb.h>
#include <stdint.h>

// gbdk's default bg map, the one the level's ring paints - terrain.c's kBgMapBase, and the sign
// goes straight into it for the same reason put_face does: a glyph never needs the wrapping and
// the rectangle setup set_bkg_tiles pays for
#define kBgMapBase ((uint8_t*)0x9800U)

static const char kSignChars[] = kSignGlyphChars;

// the ring holds sixteen columns and wraps every one of them, so a world column's tile column is
// its own doubled index masked - exactly what terrain.c's ring_tile_col does
static uint8_t sign_tile_col(void) {
    const uint16_t column = (uint16_t)(level->toad_column - (uint16_t)kToadSignColumnsLeft);

    return (uint8_t)((column * kTilesPerBlock) & (kRingTileCols - 1U));
}

// the re-encoded glyph for one character of the sign. the list is short enough that a scan beats an
// id per ascii code, which is how the hud row does it too; anything not in it - only ever the space
// - takes the hud's blank, whose cell is pure backdrop
static uint8_t sign_glyph(char c) {
    uint8_t i;

    for (i = 0; kSignChars[i] != 0; ++i) {
        if (kSignChars[i] == c) {
            return (uint8_t)(kTileSignFirst + i);
        }
    }
    return kTileHudBlank;
}

// one line of it, left to right from tile_col of the view the camera settles on - which is what
// centres a line inside the twenty columns rather than hanging all three off the room's left lip.
// the tiles go down in one pass and the attributes in another, so the two vram bank switches are
// paid once a line instead of once a cell
static void put_line(const char* text, uint8_t tile_row, uint8_t tile_col) {
    uint8_t* const row = kBgMapBase + ((uint16_t)tile_row << 5);
    const uint8_t first = (uint8_t)(sign_tile_col() + tile_col);
    uint8_t i;

    VBK_REG = VBK_TILES;
    for (i = 0; text[i] != 0; ++i) {
        row[(uint8_t)(first + i) & (kRingTileCols - 1U)] = sign_glyph(text[i]);
    }
    VBK_REG = VBK_ATTRIBUTES;
    for (i = 0; text[i] != 0; ++i) {
        row[(uint8_t)(first + i) & (kRingTileCols - 1U)] = (uint8_t)kToadSignAttr;
    }
    VBK_REG = VBK_TILES;
}

// he is 16x24 out of four 8x16 sprites, and his tiles are column-major: a column's first sprite is
// its own top pair and its second the pair under that, whose lower tile is the transparent rows
// below his feet. slots come off the throwaway animations' five, which nothing in a room reached by
// touching the axe can be using
static void draw_toad(void) {
    const int16_t sx = (int16_t)((int16_t)((uint16_t)level->toad_column << 4) - (int16_t)camera_pos_x);
    // the row the bible names is the one he stands on top of, so his feet are at its top edge
    const int16_t sy = (int16_t)((int16_t)((uint16_t)level->toad_row << 4) - (int16_t)kToadHeightPx -
                                 (int16_t)camera_pos_y);
    const uint8_t prop = (uint8_t)((uint8_t)kPalStar | (uint8_t)S_BANK);
    uint8_t i;

    for (i = 0; i < (uint8_t)kSpriteToadCount; ++i) {
        const uint8_t slot = (uint8_t)(kSpriteToadFirst + i);

        set_sprite_tile(slot, (uint8_t)(kTileToadFirst + (uint8_t)((i >> 1) * kToadTilesPerColumn) +
                                        (uint8_t)((i & 1U) << 1)));
        set_sprite_prop(slot, prop);
        move_sprite(slot, (uint8_t)(sx + (int16_t)((uint16_t)(i >> 1) << 3) + kOamXOffset),
                    (uint8_t)(sy + (int16_t)((uint16_t)(i & 1U) << 4) + kOamYOffset));
    }
}

uint8_t toad_walk_end(void) BANKED {
    // kToadStopBlocks short of the retainer, so he stops beside him facing his way with a block of
    // the room's floor between them. a castle with no retainer keeps the old fixed run along the
    // pedestal, and either is clamped inside the level
    uint16_t column = level->has_toad != 0U ? (uint16_t)(level->toad_column - (uint16_t)kToadStopBlocks)
                                            : (uint16_t)(level->axe_column + kClearWalkBlocks);

    if (column > (uint16_t)(level_columns - 1U)) {
        column = (uint16_t)(level_columns - 1U);
    }
    // and only once his feet are back down: the mark is past the lip of the pedestal, so he is
    // still falling into the room when he first crosses it
    if (player_x() < (uint16_t)(column << 4) || player_on_ground() == 0U) {
        return 0;
    }
    return level->has_toad != 0U ? (uint8_t)kClearToad : (uint8_t)kClearDoor;
}

uint8_t toad_frame(uint8_t tick) BANKED {
    if (tick == 0U) {
        // his art and the sign's glyphs are only ever wanted here, so they are loaded here rather
        // than at every level's load: eight sprite tiles and eighteen bg ones, all in vram bank 1.
        // the three lines go up on this one frame and nothing repaints the ring afterwards: mario
        // is stopped, so the camera's x never moves again and no column is streamed over them
        assets_load_toad_tiles();
        put_line(kToadSignLine0, (uint8_t)kToadSignLine0Row, (uint8_t)kToadSignLine0Col);
        put_line(kToadSignLine1, (uint8_t)kToadSignLine1Row, (uint8_t)kToadSignLine1Col);
        put_line(kToadSignLine2, (uint8_t)kToadSignLine2Row, (uint8_t)kToadSignLine2Col);
    }
    draw_toad();
    return (uint8_t)(tick + 1U >= (uint8_t)kToadHoldFrames);
}
