// smb's floatey number (smbdis SetupFloateyNumber 11533, FloateyNumbersRoutine 1283): the points
// an enemy, a coin block, a powerup or the pole just paid, drawn as two 8x8 tiles that rise one
// pixel a frame for 0x30 frames and vanish. here the two tiles are two 8x16 sprites with blank lower
// halves, cut by games/mario/tools/draw_popup.py the way smb's FloateyNumTileData shares tiles:
// five left halves 10 20 40 50 80, two right halves 0 and 00, and the 1-UP's own pair, so eleven
// values are pairs out of nine columns. bank 6 with its art: bank 4 is full, and the frame's one
// call in here happens only while a label is up
#pragma bank 6

#include "popup.h"

#include "camera.h"
#include "gen/popup.h"
#include "mario.h"

#include <gb/gb.h>
#include <stdint.h>

uint8_t popup_busy;

static uint8_t timer;
// smb keeps the number's x on the SCREEN, not in the level: it stays where it appeared while the
// view moves on. y is a world coordinate, because this camera can pan and smb's could not
static uint8_t sx;
static int16_t wy;
static uint8_t left_tile;
static uint8_t right_tile;
// the pair of oam slots the label was last drawn in, or kPopupParked
static uint8_t slot;
#define kPopupParked 0xFFU

// the ten values in tens, in the strip's own order: the five left halves over "0", then over "00"
static const uint16_t kTens[10] = {10, 20, 40, 50, 80, 100, 200, 400, 500, 800};
#define kPopupRightHundreds (kTilePopupFirst + 10U)
#define kPopupRightThousands (kTilePopupFirst + 12U)
#define kPopupOneUpLeft (kTilePopupFirst + 14U)
#define kPopupOneUpRight (kTilePopupFirst + 16U)

static void park(void) {
    if (slot != kPopupParked) {
        move_sprite(slot, 0, 0);
        move_sprite((uint8_t)(slot + 1U), 0, 0);
        slot = kPopupParked;
    }
}

void popup_art_load(void) BANKED {
    VBK_REG = VBK_BANK_1;
    set_sprite_data(kTilePopupFirst, kPopupTileCount, kPopupTiles);
    VBK_REG = VBK_BANK_0;
    // a respawn arrives with the last life's oam still written, so the old slot is not trusted
    slot = kPopupParked;
    timer = 0;
    popup_busy = 0;
}

void popup_show(uint16_t wx, int16_t wy_at, uint16_t tens) BANKED {
    uint8_t i;

    if (tens == (uint16_t)kPopupOneUp) {
        left_tile = (uint8_t)kPopupOneUpLeft;
        right_tile = (uint8_t)kPopupOneUpRight;
    } else {
        i = 0;
        while (i < 10U && kTens[i] != tens) {
            ++i;
        }
        if (i >= 10U) {
            return;
        }
        left_tile = (uint8_t)(kTilePopupFirst + ((i < 5U ? i : (uint8_t)(i - 5U)) << 1));
        right_tile = (uint8_t)(i < 5U ? kPopupRightHundreds : kPopupRightThousands);
    }
    sx = (uint8_t)((int16_t)wx - (int16_t)camera_pos_x);
    // FloateyPart draws the number eight pixels over the point it was set at
    wy = (int16_t)(wy_at - (int16_t)kPopupLiftPx);
    timer = (uint8_t)kPopupFrames;
    popup_busy = 1;
}

void popup_frame(uint8_t cam_y, uint8_t first_free) BANKED {
    int16_t sy;

    if (timer == 0U) {
        park();
        popup_busy = 0;
        return;
    }
    --timer;
    wy = (int16_t)(wy - (int16_t)kPopupRisePx);
    // the pool below has grown or shrunk under the label. a slot it has climbed into it has
    // rewritten itself, so only a slot still above the pool is this module's to park
    if (slot != kPopupParked && slot != first_free) {
        if (slot >= first_free) {
            park();
        } else {
            slot = kPopupParked;
        }
    }
    sy = (int16_t)(wy - (int16_t)cam_y);
    // no room under the hazards floor this frame, or risen out of the picture: not drawn
    if ((uint8_t)(first_free + 2U) > (uint8_t)kHazardPoolFirst || sy <= -(int16_t)kBlockPx ||
        sy >= (int16_t)kScreenHeightPx) {
        park();
        return;
    }
    if (slot != first_free) {
        slot = first_free;
        set_sprite_tile(slot, left_tile);
        set_sprite_tile((uint8_t)(slot + 1U), right_tile);
        set_sprite_prop(slot, (uint8_t)(kPalMushroom | (uint8_t)S_BANK));
        set_sprite_prop((uint8_t)(slot + 1U), (uint8_t)(kPalMushroom | (uint8_t)S_BANK));
    }
    move_sprite(slot, (uint8_t)(sx + kOamXOffset), (uint8_t)(sy + kOamYOffset));
    move_sprite((uint8_t)(slot + 1U), (uint8_t)(sx + 8U + kOamXOffset), (uint8_t)(sy + kOamYOffset));
}
