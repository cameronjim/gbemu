#ifndef PLAYER_H
#define PLAYER_H

#include <stdint.h>

// what one frame of physics ended in; anything but kPlayerAlive is the caller's cue to change state
#define kPlayerAlive 0U
#define kPlayerFell 1U
#define kPlayerFlag 2U

// the level-clear sequence's phases, in the order player_clear_update walks them
#define kClearSlide 0U
#define kClearHop 1U
#define kClearWalk 2U
#define kClearHold 3U

// loads small mario's art/palette and parks him standing on the bible's start cell
void player_init(void);

// one frame of smb physics from the held-button mask; returns one of the kPlayer* codes above
uint8_t player_update(uint8_t keys);

// snaps mario onto the flag pole and arms the clear sequence; call once on kPlayerFlag
void player_begin_clear(void);

// one frame of the clear sequence (slide, hop off, walk to the castle, hold); 1 when it is over
uint8_t player_clear_update(void);

// writes mario's two 8x16 sprites for this frame, or parks them off screen when he is out of view
void player_draw(uint16_t cam_x, uint8_t cam_y);

// the sprite box's left edge in world px; the camera follows this
uint16_t player_x(void);

// the sprite box's top edge in world px; the camera's default band is measured off it
int16_t player_y(void);

// 1 while mario is resting on a surface, which is what lets the camera resample its default band
uint8_t player_on_ground(void);

// 1 while he is grounded and has no horizontal speed at all: the only state the manual pan accepts
uint8_t player_standing(void);

#endif
