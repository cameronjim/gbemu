#ifndef FLOW_H
#define FLOW_H

#include <gb/gb.h>
#include <stdint.h>

// the pipe and sub-area transitions. they run on a button edge, never inside a frame of play, so
// they ride in bank 5 with the title card; only the game loop's own state machine stays in bank 0

// loads a level from scratch: the grid, its art and palettes, the blocks, hazards, enemies and
// mario on the start cell. returns how many hazards the level has, which is 0 for 1-1
uint8_t flow_enter_level(uint8_t index) BANKED;

// swaps in one of the level's compiled sub-areas and drops mario at its start cell
void flow_enter_sub_area(uint8_t index) BANKED;

// reloads the main grid with its spent blocks intact and starts him rising out of the link pipe
void flow_leave_sub_area(void) BANKED;

// the sub-area index of the enterable pipe mario is standing squarely on, or 0xff
uint8_t flow_pipe_under_player(void) BANKED;

// and the warp room's own pipes, which name a level to load rather than a room to enter
uint8_t flow_warp_under_player(void) BANKED;

// 1 while he stands on the sub-area's exit pipe, the way back up
uint8_t flow_over_exit_pipe(void) BANKED;

// starts a run with three lives and no score; a continue answers with the furthest level the save
// slot reached, anything else with the level the title had selected
uint8_t flow_begin_run(uint8_t entry, uint8_t selected);

// pays the flagpole band his feet touched at; call once, before the slide moves him
void flow_score_flag(int16_t feet) BANKED;

// the death beat is over: takes a life off the run and answers with what comes next. a game over
// paints its own card, which flow_game_over_frame then holds up
#define kAfterDeathRespawn 0U
#define kAfterDeathGameOver 1U
uint8_t flow_after_death(void) BANKED;

// one frame of the game over card; 1 when it has been up long enough to go back to the title
uint8_t flow_game_over_frame(void) BANKED;

// paints the level-clear card, then one frame of it: the countdown converting into points, the
// hold, and finally the level after this one written through `level`
#define kAfterCardStay 0U
#define kAfterCardNext 1U
#define kAfterCardTitle 2U
void flow_clear_card(void) BANKED;
uint8_t flow_clear_frame(uint8_t* level) BANKED;

// puts the level's own bg back after a card overwrote it, without touching a single actor
void flow_resume_from_card(uint8_t area, uint16_t camera_x) BANKED;

#endif
