#ifndef DEBRIS_H
#define DEBRIS_H

#include <gb/gb.h>
#include <stdint.h>

// 0 when no fragment is flying, no puff is burning and neither still holds an oam slot: the game
// loop skips the whole module on those frames, which is nearly all of a level
extern uint8_t debris_busy;
// the two live counters, published as plain ram so a level load or an area swap can drop them
// without a trampoline into bank 6 - the next frame parks whatever was still drawn
extern uint8_t debris_timer;
extern uint8_t debris_puff;

// four quarter-brick fragments out of the cell whose top-left corner is (px, py). one break is
// animated at a time, the way smb only ever has one brick coming apart: a second replaces the first
void debris_break(uint16_t px, int16_t py) BANKED;

// the little burst smb leaves where a fireball meets a wall, at the ball's own 8x8 box
void debris_poof(uint16_t px, int16_t py) BANKED;

// one frame of both: steps them, writes their sprites, and parks the slots as they go
void debris_frame(uint16_t cam_x, uint8_t cam_y) BANKED;

#endif
