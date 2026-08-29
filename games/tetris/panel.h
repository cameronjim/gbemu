#ifndef PANEL_H
#define PANEL_H

#include <stdint.h>

// loads the compact panel hud font (digits and letters) into vram; call once at boot
void panel_build_font(void);

// draws the labels and zeroes score/level/lines; call after well_init with the lcd off
void panel_init(void);

// redraws the next-piece box for the given piece id
void panel_set_next(uint8_t piece);

// +1 for a soft-dropped cell
void panel_add_soft_drop(void);

// scores a clear of n rows (1-4) at the current level, then advances lines/level
void panel_add_lines(uint8_t n);

uint32_t panel_score(void);

uint8_t panel_level(void);

// frames per row at the current level, from the classic gb-style curve
uint8_t panel_gravity_frames(void);

#endif
