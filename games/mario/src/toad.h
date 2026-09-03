#ifndef TOAD_H
#define TOAD_H

#include <gb/gb.h>
#include <stdint.h>

// one frame of the beat past 1-4's axe: the mushroom retainer drawn where the bible put him, the
// sign put up over him on the frame tick is 0, and the whole tableau held for kToadHoldFrames.
// answers 1 on the last frame of the hold, which is the clear card's cue.
//
// the caller owns the tick, so this side keeps no state of its own - player.c's clear sequence
// already has a frame counter per phase and bank 0 has no bytes for a second one
uint8_t toad_frame(uint8_t tick) BANKED;

// where the walk off the axe pedestal ends: 0 while it is still going, and otherwise the clear
// phase it runs into - kClearToad on a castle whose bible put a retainer past the axe, kClearDoor
// on one that did not. the arithmetic is here rather than in player.c because bank 0 has no bytes
// left, and the answer only ever changes on the frame he lands on his mark
uint8_t toad_walk_end(void) BANKED;

#endif
