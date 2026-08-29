#ifndef CAMERA_H
#define CAMERA_H

#include <gb/gb.h>
#include <stdint.h>

// the smbd play camera. horizontal follow with backward scrolling allowed, a select-held look-ahead
// that slides mario's screen anchor forward, and a manually panned scy that returns to the level's
// default band. it owns no vram: the caller feeds its output to terrain_set_scroll_x/_pan_y.

// snaps the camera onto mario, anchor at kCamFollowX and scy at his default band
void camera_init(uint16_t mario_x, int16_t feet_y) BANKED;

// one frame of camera rules. feet_y is the bottom of mario's collision box, so growing does not move
// the band; on_ground and standing come from the player (standing means grounded with no horizontal
// speed, the only state that may pan)
void camera_update(uint16_t mario_x, int16_t feet_y, uint8_t on_ground, uint8_t standing,
                   uint8_t keys) BANKED;

// the camera's left edge in world px and its scy, published as ram: the frame's draw pass reads
// them eleven times and the module itself is a trampoline away in bank 5
extern uint16_t camera_pos_x;
extern uint8_t camera_pos_y;

// mario's current screen anchor in px; kCamFollowX at rest, sliding to kCamLookAheadX under select
extern uint8_t camera_pos_anchor;

#endif
