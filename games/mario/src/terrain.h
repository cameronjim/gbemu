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

// parks the camera's left edge at world_px, clamped the same way terrain_scroll_x clamps; the ring
// is left alone so the caller can write scx/oam first and pay the streaming cost last
void terrain_set_scroll_x(uint16_t world_px);

// brings the ring's streamed columns back in line with the camera, in either scroll direction; the
// frame's heaviest vram work
void terrain_stream_window(void);

// the largest camera left edge the level allows: level_length_px - screen_width, or 0 when the
// level is narrower than the screen
uint16_t terrain_max_camera_x(void);

// parks scy directly; the play camera pins it while m4's manual pan is still unwritten
void terrain_set_pan_y(uint8_t y_px);

// the camera's current left edge in world px
uint16_t terrain_camera_x(void);

// 1 when the block grid cell blocks the player: out of the level's columns counts as a wall, out
// of its rows does not (open sky above, the death pit below)
uint8_t terrain_solid_at(int16_t column, int16_t row);

// writes the camera's current position to scx/scy; call once per frame, after any scroll/pan
void terrain_apply_scroll(void);

#endif
