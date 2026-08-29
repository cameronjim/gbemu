#ifndef CAMERA_H
#define CAMERA_H

#include <stdint.h>

// the smbd play camera. horizontal follow with backward scrolling allowed, a select-held look-ahead
// that slides mario's screen anchor forward, and a manually panned scy that returns to the level's
// default band. it owns no vram: the caller feeds its output to terrain_set_scroll_x/_pan_y.

// snaps the camera onto mario, anchor at kCamFollowX and scy at his default band
void camera_init(uint16_t mario_x, int16_t box_top);

// one frame of camera rules. box_top is the top of mario collision box; on_ground and standing come from
// the player (standing means grounded with no horizontal speed, the only state that may pan)
void camera_update(uint16_t mario_x, int16_t box_top, uint8_t on_ground, uint8_t standing, uint8_t keys);

// the camera's left edge in world px, clamped to the level's ends
uint16_t camera_x(void);

// the camera's scy, inside [0, kScyMax]
uint8_t camera_y(void);

// mario's current screen anchor in px; kCamFollowX at rest, sliding to kCamLookAheadX under select
uint8_t camera_anchor(void);

#endif
