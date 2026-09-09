#ifndef MARIO_POPUP_H
#define MARIO_POPUP_H

#include <gb/gb.h>
#include <stdint.h>

// the floating score, smb's floatey number: a two-sprite label that rises from where the points
// were won and is gone kPopupFrames later. one at a time, as smb keeps one per enemy slot and this
// game's stomps, kills, coins and pole are never two in a frame that matter

// 1 while a label is up, so the one frame call into this bank costs nothing on every other frame
extern uint8_t popup_busy;

// the nine 8x16 columns of gen/popup.c into bank-1 sprite vram, and any label left from the last
// level parked. once per level, beside the enemy art
void popup_art_load(void) BANKED;

// raises a label worth `tens` (the unit hud_score keeps; kPopupOneUp for the 1-up) over the world
// point (wx, wy). a value the table does not carry raises nothing, which is what a broken brick's 50
// gets in smb too
void popup_show(uint16_t wx, int16_t wy, uint16_t tens) BANKED;

// one frame of the rise and the draw. enemies_draw calls it with the first oam slot past the enemy
// pool: the label takes that slot and the next when both sit under the hazards pool's floor, and is
// simply not drawn on a frame that has no room
void popup_frame(uint8_t cam_y, uint8_t first_free) BANKED;

#endif
