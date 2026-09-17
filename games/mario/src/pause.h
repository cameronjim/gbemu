#ifndef PAUSE_H
#define PAUSE_H

#include <gb/gb.h>
#include <stdint.h>

// smbd's pause screen (a capture): a black card with PAUSE, WORLD 1-x, mario's sprite and his
// lives, and a CONTINUE / END menu. the level comes back through states.c's leave_card
void pause_begin(uint8_t level) BANKED;

// one frame of the menu: up/down move the cursor, a picks, start always continues
#define kPauseStay 0U
#define kPauseResume 1U
#define kPauseQuit 2U
uint8_t pause_frame(uint8_t pressed) BANKED;

#endif
