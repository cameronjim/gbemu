// the loose item and the coin pop, drawn. blocks.c's reaction logic sits on collision paths the
// engine walks twenty times a frame and cannot leave bank 0, but the sprite pass is two oam slots
// on the frames anything is actually loose - the game loop already skips it on most of a level -
// so it rides in a bank, and bank 0 gets its half kilobyte back. the slot state it reads is plain
// ram (see blocks.h), which is what makes the split cost nothing but the one trampoline
#pragma bank 6

#include "blocks.h"

#include "mario.h"

#include <gb/gb.h>
#include <stdint.h>

// kItem* indexes both tables. the flower lives outside the 0xd0 family and has no palette slot of
// its own, so it borrows the star's white/yellow set - see kPalFire in mario.h
static const uint8_t kItemPalette[kItemKindCount] = {kPalMario, kPalMushroom, kPalStar, kPalOneup, kPalStar};
static const uint8_t kItemTile[kItemKindCount] = {
    kTileItemFirst,
    kTileItemFirst,
    kTileItemFirst + kItemTilesPerKind,
    kTileItemFirst + 2U * kItemTilesPerKind,
    kTileFlowerFirst,
};

// oam y 0 parks a sprite entirely above the screen
static void hide(uint8_t slot) {
    move_sprite(slot, 0, 0);
}

void blocks_draw(uint16_t cam_x, uint8_t cam_y) BANKED {
    int16_t sx;
    int16_t sy;
    uint8_t tile;

    if (blocks_item_kind == kItemNone) {
        if (blocks_item_shown != 0U) {
            blocks_item_shown = 0;
            hide(kSpriteItemL);
            hide(kSpriteItemR);
        }
    } else {
        sx = (int16_t)((int16_t)blocks_item_x - (int16_t)cam_x);
        sy = (int16_t)(blocks_item_y - (int16_t)cam_y);
        if (sy <= -(int16_t)kPlayerHeightPx || sy >= (int16_t)kScreenHeightPx ||
            sx <= -(int16_t)kPlayerWidthPx || sx >= (int16_t)kScreenWidthPx) {
            if (blocks_item_shown != 0U) {
                blocks_item_shown = 0;
                hide(kSpriteItemL);
                hide(kSpriteItemR);
            }
        } else {
            blocks_item_shown = 1;
            tile = kItemTile[blocks_item_kind];
            set_sprite_tile(kSpriteItemL, tile);
            set_sprite_tile(kSpriteItemR, (uint8_t)(tile + 2U));
            set_sprite_prop(kSpriteItemL, kItemPalette[blocks_item_kind]);
            set_sprite_prop(kSpriteItemR, kItemPalette[blocks_item_kind]);
            move_sprite(kSpriteItemL, (uint8_t)(sx + kOamXOffset), (uint8_t)(sy + kOamYOffset));
            move_sprite(kSpriteItemR, (uint8_t)(sx + 8 + kOamXOffset), (uint8_t)(sy + kOamYOffset));
        }
    }

    if (blocks_coin_active == 0U) {
        if (blocks_coin_shown != 0U) {
            blocks_coin_shown = 0;
            hide(kSpriteCoin);
        }
        return;
    }
    sx = (int16_t)((int16_t)blocks_coin_x - (int16_t)cam_x);
    sy = (int16_t)(blocks_coin_y - (int16_t)cam_y);
    if (sy <= -(int16_t)kPlayerHeightPx || sy >= (int16_t)kScreenHeightPx || sx <= -8 ||
        sx >= (int16_t)kScreenWidthPx) {
        if (blocks_coin_shown != 0U) {
            blocks_coin_shown = 0;
            hide(kSpriteCoin);
        }
        return;
    }
    blocks_coin_shown = 1;
    set_sprite_tile(kSpriteCoin, kTileCoinPop);
    set_sprite_prop(kSpriteCoin, kPalCoin);
    move_sprite(kSpriteCoin, (uint8_t)(sx + kOamXOffset), (uint8_t)(sy + kOamYOffset));
}
