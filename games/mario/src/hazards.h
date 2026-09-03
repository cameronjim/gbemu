#ifndef HAZARDS_H
#define HAZARDS_H

#include "mario.h"

#include <gb/gb.h>
#include <stdint.h>

// what hazards_contact reports back to main.c
#define kHazardNone 0U
#define kHazardDamage 1U
#define kHazardAxe 2U

// the lift decks. hazards.c runs banked, but these live in ram like every other variable, so
// player.c can probe them on every collide without paying a banked call for it
extern uint16_t hazard_lift_x[kLiftSlots];
extern int16_t hazard_lift_y[kLiftSlots];
extern int8_t hazard_lift_dx[kLiftSlots];
extern int8_t hazard_lift_dy[kLiftSlots];
extern uint8_t hazard_lift_count;
// the world x span every lift, bar, bowser and axe of the level falls inside, published so the
// game loop can skip the whole module without a trampoline to ask
extern uint16_t hazard_min_x;
extern uint16_t hazard_max_x;

// reads the level's object list into the pools and resets the spin; call with the lcd off
void hazards_load_level(void) BANKED;

// how many lifts, bars, bowsers and axes the loaded level has. 1-1 has none, and the game loop
// gates every call below on this so that level never pays a trampoline into bank 5 at all
uint8_t hazards_count(void) BANKED;

// one frame of motion for every lift, firebar and the fake bowser. runs before the player's own
// step, so the deck he is standing on has already moved when his carry is applied
void hazards_step(void) BANKED;

// kHazard*: what the player's box touched this frame
uint8_t hazards_contact(uint16_t player_px, int16_t player_py, uint8_t player_h, uint8_t immune) BANKED;

// arms the collapse of 1-4's bridge: smb pulls its cells one at a time from the axe end back
// toward the far side, so this clears nothing itself - hazards_clear_step walks it
void hazards_drop_bridge(void) BANKED;

// one frame of that collapse and of bowser's fall into the lava, for the frames after the axe when
// the play loop has handed over to the clear sequence and nothing steps this module any more
void hazards_clear_step(void) BANKED;

// 1 while the collapse still has a cell to take or bowser is still on his way down. the clear
// sequence reads it as ram rather than through a banked call, the way the lift decks are read
extern uint8_t hazard_clear_busy;

// a fireball's box against bowser's; 1 when the ball landed on him, which spends it either way.
// the fifth hit pays kBowserKillPoints and drops him
uint8_t hazards_fireball_hit(uint16_t px, int16_t py) BANKED;

void hazards_draw(uint16_t cam_x, uint8_t cam_y) BANKED;

// the firebar's angle step, 0..kFirebarSteps-1; the rotation test reads it through the flames
uint8_t hazards_spin_step(void) BANKED;

// 1 while the fake bowser is still on the bridge
uint8_t hazards_bowser_live(void) BANKED;

// how many fireballs have landed on him, 0..kBowserFireballHits
uint8_t hazards_bowser_hits(void) BANKED;

#endif
