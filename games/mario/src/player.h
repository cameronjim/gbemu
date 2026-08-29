#ifndef PLAYER_H
#define PLAYER_H

#include <stdint.h>

// loads small mario's art/palette and parks him standing on the bible's start cell
void player_init(void);

// one frame of smb physics from the held-button mask; returns 1 on the frame mario falls out of
// the level, which is the caller's cue to respawn him
uint8_t player_update(uint8_t keys);

// writes mario's two 8x16 sprites for this frame, or parks them off screen when he is out of view
void player_draw(uint16_t cam_x, uint8_t cam_y);

// the collision box's left edge in world px; the camera follows this
uint16_t player_x(void);

#endif
