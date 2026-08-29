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

#endif
