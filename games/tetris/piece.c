#include "piece.h"

#include "pieces.h"
#include "tetris.h"
#include "well.h"

#include <gb/gb.h>
#include <stdint.h>

static uint8_t id;
static uint8_t rot;
// top-left of the piece's 4x4 box in well cells; the left wall lets it go negative
static int8_t px;
static int8_t py;

void piece_spawn(uint8_t piece) {
    id = piece;
    rot = 0;
    px = kSpawnCol;
    py = kSpawnRow;
}

uint8_t piece_spawn_blocked(uint8_t piece) {
    return well_blocked(piece, 0, kSpawnCol, kSpawnRow);
}

uint8_t piece_move(int8_t dx) {
    if (well_blocked(id, rot, (int8_t)(px + dx), py)) {
        return 0U;
    }
    px = (int8_t)(px + dx);
    return 1U;
}

// classic gb rotation: keep the rotated footprint only if it is clear, never kick off a wall
uint8_t piece_rotate(int8_t dir) {
    uint8_t next = (uint8_t)((rot + (uint8_t)dir) & 3U);

    if (well_blocked(id, next, px, py)) {
        return 0U;
    }
    rot = next;
    return 1U;
}

uint8_t piece_fall(void) {
    if (well_blocked(id, rot, px, (int8_t)(py + 1))) {
        return 0U;
    }
    py = (int8_t)(py + 1);
    return 1U;
}

void piece_draw(void) {
    const uint8_t* shape = pieces_shape(id, rot);
    uint8_t i;
    uint8_t x;
    uint8_t y;

    for (i = 0; i < kPieceSprites; ++i) {
        x = (uint8_t)(kWellOriginCol + px + (int8_t)(shape[i] & 0x0FU));
        y = (uint8_t)(kWellOriginRow + py + (int8_t)(shape[i] >> 4));
        move_sprite(i, (uint8_t)((uint8_t)(x << 3) + kOamXOffset),
                    (uint8_t)((uint8_t)(y << 3) + kOamYOffset));
        set_sprite_tile(i, kPieceSpriteTileId);
        set_sprite_prop(i, id);
    }
}

void piece_hide(void) {
    uint8_t i;
    for (i = 0; i < kPieceSprites; ++i) {
        move_sprite(i, 0, 0);
    }
}

void piece_bake(void) {
    well_lock(id, rot, px, py);
}
