// mario's own sprite pass, out of bank 0: player.c is the physics and cannot leave the home bank,
// but writing his four oam slots is a few dozen bytes of work a frame that any bank can do, and
// the landing grace and smb's vertical integrator (feat/mario-physics) took the last of bank 0's
// room. it reads the physics through player.c's accessors and one packed pose byte, so nothing
// here touches player.c's statics
#pragma bank 6

#include "player.h"

#include "mario.h"

#include <gb/gb.h>
#include <stdint.h>

// the tile and palette each of mario's four slots last carried; a walk frame lasts frames and his
// palette changes twice a level, so most frames owe nothing but the two moves
static uint8_t drawn_tile[4];
static uint8_t drawn_prop[4];

void player_draw_reset(void) BANKED {
    uint8_t i;

    for (i = 0; i < 4U; ++i) {
        // never a real tile or property, so the first draw of a fresh level writes all four
        drawn_tile[i] = 0xFF;
        drawn_prop[i] = 0xFF;
    }
}

// oam y 0 parks a sprite entirely above the screen
static void hide(void) {
    move_sprite(kSpriteMarioL, 0, 0);
    move_sprite(kSpriteMarioR, 0, 0);
    move_sprite(kSpriteMarioLowL, 0, 0);
    move_sprite(kSpriteMarioLowR, 0, 0);
}

// one 16 px sprite row: flipping mirrors each 8x16 half, so the halves also swap sides
static void draw_row(uint8_t slot, uint8_t tile, uint8_t prop, int16_t sx, int16_t sy) {
    const uint8_t flipped = (uint8_t)((prop & (uint8_t)S_FLIPX) != 0U ? 1U : 0U);
    const uint8_t left_tile = flipped != 0U ? (uint8_t)(tile + 2U) : tile;
    const uint8_t right_tile = flipped != 0U ? tile : (uint8_t)(tile + 2U);

    if (drawn_tile[slot] != left_tile || drawn_prop[slot] != prop) {
        drawn_tile[slot] = left_tile;
        drawn_prop[slot] = prop;
        set_sprite_tile(slot, left_tile);
        set_sprite_tile((uint8_t)(slot + 1U), right_tile);
        set_sprite_prop(slot, prop);
        set_sprite_prop((uint8_t)(slot + 1U), prop);
    }
    move_sprite(slot, (uint8_t)(sx + kOamXOffset), (uint8_t)(sy + kOamYOffset));
    move_sprite((uint8_t)(slot + 1U), (uint8_t)(sx + 8 + kOamXOffset), (uint8_t)(sy + kOamYOffset));
}

void player_draw(uint16_t cam_x, uint8_t cam_y, uint8_t palette) BANKED {
    const uint8_t pose = player_pose();
    const uint8_t frame = (uint8_t)(pose & kPoseFrameMask);
    const uint8_t big = (uint8_t)(pose & kPoseBig);
    const uint8_t height = big != 0U ? (uint8_t)kPlayerBigHeightPx : (uint8_t)kPlayerHeightPx;
    const int16_t sx = (int16_t)((int16_t)player_x() - (int16_t)cam_x);
    const int16_t sy = (int16_t)(player_y() - (int16_t)cam_y);
    const uint8_t prop = (uint8_t)(palette | (player_facing_left() != 0U ? (uint8_t)S_FLIPX : 0U) |
                                   ((pose & kPoseBehindBg) != 0U ? (uint8_t)S_PRIORITY : 0U));

    if (palette == (uint8_t)kSpriteHidden || (pose & kPoseGone) != 0U || sy <= -(int16_t)height ||
        sy >= (int16_t)kScreenHeightPx || sx <= -(int16_t)kPlayerWidthPx || sx >= (int16_t)kScreenWidthPx) {
        hide();
        return;
    }
    // the whole of big mario and small mario's climb grip live in vram bank 1, so their rows carry
    // S_BANK in the prop. the slot cache compares (tile, prop) and the prop is what tells the two
    // banks apart, so a pose that shares an id with a bank-0 one still reads as a change
    if (big == 0U) {
        // the small poses are the pinned 0xe0 family in bank 0, except the climb grip and the death
        // pose: the grip rides at the same id in bank 1, and the death pose took four of the ids
        // super mario's old bank-0 block gave back
        uint8_t tile;
        uint8_t small_prop = prop;

        if ((pose & kPoseClimbing) != 0U) {
            tile = (uint8_t)kTileClimbSmall;
            small_prop = (uint8_t)(prop | (uint8_t)S_BANK);
        } else if (frame == (uint8_t)kFrameDeath) {
            tile = (uint8_t)kTileMarioDeath;
        } else {
            tile = (uint8_t)(kTileMarioFirst + (uint8_t)(frame * kMarioTilesPerFrame));
        }
        draw_row(kSpriteMarioL, tile, small_prop, sx, sy);
        move_sprite(kSpriteMarioLowL, 0, 0);
        move_sprite(kSpriteMarioLowR, 0, 0);
        return;
    }
    // every big pose is its own 16x32 box of eight tiles in vram bank 1, so all four slots draw the
    // same way whatever he is doing: the upper row at the box's top, the lower one 16 px under it.
    // the crouch is pose 6 and the flagpole grip pose 7 - the fold's 22-tall art is bottom-aligned
    // inside the same box, so it needs no row parking and no origin of its own, only the art
    {
        const uint8_t big_pose = (pose & kPoseClimbing) != 0U ? (uint8_t)kFrameClimbBig : frame;
        const uint8_t base = (uint8_t)(kTileSuperFirst + (uint8_t)(big_pose * kSuperTilesPerFrame));
        const uint8_t big_prop = (uint8_t)(prop | (uint8_t)S_BANK);

        draw_row(kSpriteMarioL, base, big_prop, sx, sy);
        draw_row(kSpriteMarioLowL, (uint8_t)(base + 4U), big_prop, sx, (int16_t)(sy + kPlayerHeightPx));
    }
}
