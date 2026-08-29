#ifndef TERRAIN_H
#define TERRAIN_H

#include <stdint.h>

// loads terrain art/palettes, fills the ring for the level's opening view, parks the camera at (0,0)
void terrain_init(void);

// moves the debug camera's left edge by delta_px (+right/-left), streaming ring columns as it crosses
// block boundaries; clamped to [0, level_length_px - screen_width]
void terrain_scroll_x(int8_t delta_px);

// pans the debug camera's scy by delta_px (+down/-up); clamped to [0, kScyMax]
void terrain_pan_y(int8_t delta_px);

// writes the camera's current position to scx/scy; call once per frame, after any scroll/pan
void terrain_apply_scroll(void);

#endif
