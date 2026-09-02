#ifndef HUD_H
#define HUD_H

#include <gb/gb.h>
#include <stdint.h>

// the run's counters, published as plain ram the way blocks_player_big and powerup_flags are:
// blocks.c sits in bank 0 and enemies.c in bank 4, and a banked setter on every coin and every
// stomp would cost a trampoline where a store does
extern uint16_t hud_score;
extern uint8_t hud_coins;
extern uint8_t hud_lives;
// ticks left on the countdown; the digits are kept beside it so a frame never divides
extern uint16_t hud_time;

// smb's score is always a multiple of ten, so hud_score counts tens and the cards print the
// trailing zero. every figure in roster.json's table divides exactly
#define kScoreTens(points) ((uint16_t)((points) / 10U))

// the entry points without BANKED below are only ever called from bank 5's own modules, so they
// take a plain call: a trampoline costs bank 0 the _HOME bytes it ran out of in m8b

// three lives, no coins and no score
void hud_new_game(void);

// arms the level's countdown and paints the whole window strip - its palette attributes, its black
// cells and its labels, the level's own number among them; a respawn shares it. lcd off only
void hud_enter_level(uint16_t ticks, uint8_t level);

// the timer lab: the next hud_enter_level takes kShortTimerTicks instead of the level's own
void hud_set_short_timer(uint8_t on);

// one frame of the countdown, the coin rollover and whichever of the strip's ten digit cells
// changed; 1 on the frame time ran out
uint8_t hud_frame(void) BANKED;

// a 1-up item, and the life every hundredth coin pays; both clamp at kLivesMax
void hud_add_life(void) BANKED;

// spends up to kTimeBonusTicksPerFrame of the remaining countdown at the roster's per-tick rate;
// 1 while there is still time left to convert
uint8_t hud_spend_time_bonus(void);

// writes `count` decimal digits of `value`, most significant first; the cards print with it
void hud_split(uint16_t value, uint8_t* out, uint8_t count);

#endif
