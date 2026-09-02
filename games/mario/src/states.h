#ifndef STATES_H
#define STATES_H

#include <gb/gb.h>
#include <stdint.h>

// title -> select file -> world map -> level -> world map. a game over is the only way back to
// the title from play; b walks the front end back a screen at a time
enum GameState {
    kStateFront,
    kStatePlay,
    kStateClear,
    kStateClearCard,
    kStateDeath,
    kStatePause,
    kStateGameOver,
    kStateCamera,
    kStatePipeDown,
    kStatePipeUp
};

// the game loop's shared state, plain ram so bank 0's play frame and bank 6's other states both
// read it without a call
extern uint8_t current_area;
extern uint8_t level_number;
extern uint8_t pending_area;
extern uint8_t pending_warp;
extern uint8_t pipe_reentry_lock;
extern uint8_t hazard_active;
extern uint8_t hazard_near;

// one frame's draw pass, owned by main.c (bank 0) so the play frame pays no trampoline for it
void main_present(void);

// the lcd-off level load, shared by the front end, a respawn and a warp
void states_enter_play(void) BANKED;

// one frame of any state but kStateFront, kStatePlay and kStateCamera; answers the next state
uint8_t states_off_play(uint8_t state, uint8_t keys, uint8_t pressed) BANKED;

#endif
