#ifndef PAUSE_H
#define PAUSE_H

#include <gb/gb.h>
#include <stdint.h>

// smbd's pause is an overlay on the frozen level (systems.md, the manual): the band under the hud
// strip carries the level name and the lives the small-screen hud dropped, and a menu with SAVE
// beside RESUME and QUIT. nothing under it is touched, so resume repaints nothing
void pause_begin(uint8_t level) BANKED;
void pause_end(void) BANKED;

// one frame of the menu: up/down move the cursor, a picks, start always resumes
#define kPauseStay 0U
#define kPauseResume 1U
#define kPauseQuit 2U
uint8_t pause_frame(uint8_t pressed) BANKED;

#endif
