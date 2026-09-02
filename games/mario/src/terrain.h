#ifndef TERRAIN_H
#define TERRAIN_H

#include <stdint.h>

// loads terrain art/palettes for the given kArea* grid, fills the ring for its opening view, and
// parks the camera at (0,0). blocks_init must already have run for the same area
void terrain_init(uint8_t area);

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

// the block kind the cell renders as: the compiled grid, with blocks.c's runtime overrides applied
uint8_t terrain_kind_at(int16_t column, int16_t row);

// 1 when the block grid cell blocks the player: out of the level's columns counts as a wall, out
// of its rows does not (open sky above, the death pit below)
uint8_t terrain_solid_at(int16_t column, int16_t row);

// what the cell is to a body standing on it: kFloorSolid, kFloorThin (1-3's tree tops, which stop
// only feet that crossed the deck line this frame), or 0. one grid probe, one table load
uint8_t terrain_floor_at(int16_t column, int16_t row);

// repaints one block cell from its current kind; the ring slot is left alone when the column has
// scrolled out of the streamed window
void terrain_write_block(int16_t column, int16_t row);

// writes a cell's kind into the ram grid and repaints it: the flag pennant coming down the pole
// moves one cell a step this way, since oam is full and it cannot be a sprite
void terrain_set_cell(int16_t column, int16_t row, uint8_t kind);

// turns a cell to sky in the ram grid and repaints it: the axe dropping 1-4's bridge
void terrain_clear_cell(int16_t column, int16_t row);

// draws the cell one tile row higher for the head-bump bounce, and puts it back again. the bounce
// borrows the cell above, so it is skipped unless that cell is empty sky
void terrain_bump_block(int16_t column, int16_t row);
void terrain_restore_block(int16_t column, int16_t row);

// writes the camera's current position to scx/scy; call once per frame, after any scroll/pan
void terrain_apply_scroll(void);

#endif
