#ifndef PLAYER_H
#define PLAYER_H

#include <stdint.h>

// what one frame of physics ended in; anything but kPlayerAlive is the caller's cue to change state
#define kPlayerAlive 0U
#define kPlayerFell 1U
#define kPlayerFlag 2U

// the clear sequence starts at the pole for a level with one and at the axe for a castle
#define kClearFromFlag 0U
#define kClearFromAxe 1U

// the level-clear sequence's phases, in the order player_clear_update walks them
#define kClearSlide 0U
#define kClearHop 1U
#define kClearWalk 2U
#define kClearHold 3U

// which way a pipe transition is carrying him
#define kPipeDown 0U
#define kPipeUp 1U

// loads small mario's and the items' art/palettes and parks him on the bible's start cell
void player_init(void);

// drops him standing on top of the given surface row, the way the level start does; the sub-area
// spawn and any future respawn share it
void player_place(uint16_t column, uint8_t surface_row);

// 1 while he stands squarely on top of the two-column pipe whose cap is at (column, top_row)
uint8_t player_over_pipe(uint16_t column, uint8_t top_row);

// arms the death beat from a kDeathFrom* cause, then one frame of it; 1 when he is off the level
void player_begin_death(uint8_t from);
uint8_t player_death_update(void);

// arms the sink-into-a-pipe animation from where he stands, or the rise-out-of-one at a given cap
void player_begin_pipe_down(void);
void player_begin_pipe_up(uint16_t column, uint8_t top_row);

// one frame of whichever pipe animation is armed; 1 when it is over
uint8_t player_pipe_update(void);

// one frame of smb physics from the held-button mask; returns one of the kPlayer* codes above
uint8_t player_update(uint8_t keys);

// snaps mario onto the flag pole and arms the clear sequence; call once on kPlayerFlag. a castle
// has no pole, so kClearFromAxe skips the slide and walks him off from where he stands
void player_begin_clear(uint8_t from);

// one frame of the clear sequence (slide, hop off, walk to the castle, hold); 1 when it is over
uint8_t player_clear_update(void);

// swaps him between the 16x16 and 16x32 bodies with his feet planted; the grow animation calls it
// every few frames, which is what makes the pose alternate
void player_set_big(uint8_t big);

// writes mario's sprites for this frame (two 8x16 small, four big) under the given cgb palette, or
// parks them when he is out of view or the injury blink hides him
void player_draw(uint16_t cam_x, uint8_t cam_y, uint8_t palette);

// the sprite box's left edge in world px; the camera follows this
uint16_t player_x(void);

// the sprite box's top edge in world px; the camera's default band is measured off it
int16_t player_y(void);

// his whole-px vertical speed this frame; the enemy pass reads it to tell a stomp from a side hit
int8_t player_y_speed(void);

// kicks him back up off whatever he just stomped, the bible's own bounce velocity
void player_stomp_bounce(int8_t speed);

// 1 while mario is resting on a surface, which is what lets the camera resample its default band
uint8_t player_on_ground(void);

// 1 while he is grounded and has no horizontal speed at all: the only state the manual pan accepts
uint8_t player_standing(void);

// which way he faces, which is the way a thrown fireball goes
uint8_t player_facing_left(void);

// his collision box's top edge and height: they differ from the sprite box's while he crouches,
// and the item and enemy passes both need the real thing rather than a fixed 16
int16_t player_box_top(void);
uint8_t player_box_height(void);

// where his feet are in world px; the camera's default band is measured off this, not off the box
// top, so growing to 16x32 leaves the view where it was
int16_t player_feet(void);

// the lift slot he is riding, or 0xff. the crossy insight: the deck carries him pixel for pixel,
// applied before his own step so the two never disagree by a frame
uint8_t player_riding(void);

#endif
