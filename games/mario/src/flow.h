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

// the sub-area index of the enterable pipe mario is standing squarely on, or 0xff. also answers
// for 1-2's entrance/exit pipes - a same-grid segment teleport rather than a sub-area - with
// kJumpAreaFlag set on the low bits; flow_enter_sub_area is what actually tells the two apart, so
// main.c's state machine can carry the value forward exactly like it already carries an area index
uint8_t flow_pipe_under_player(void) BANKED;

// (re)loads the bg palette set for the camera's current column: level.c's segment table (empty on
// every level but 1-2) can make that a different kLevelType than the level's own. lives here,
// banked, rather than in terrain.c, which is bank 0 and has no room to spare for it - terrain_init
// calls it through the banked trampoline once at every load, and flow_enter_sub_area's same-grid
// jump path calls it again after landing in what may be a different segment
void terrain_sync_palette(void) BANKED;

// and the warp room's own pipes, which name a level to load rather than a room to enter
uint8_t flow_warp_under_player(void) BANKED;

// 1 while he stands on the sub-area's exit pipe, the way back up
uint8_t flow_over_exit_pipe(void) BANKED;

// starts a run with three lives and no score at the given level, and answers with it. the file
// select arms a player's run this way; the title's labs arm theirs the same way
uint8_t flow_begin_run(uint8_t selected);

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
// hold, and finally the file's record. m19 hands back to the world map rather than straight into
// the next level, so `level` comes back as the RAW next node - which is kLevelCount once world one
// is finished; front_cleared is what opens that node on the map and clamps this back to a real one
#define kAfterCardStay 0U
#define kAfterCardMap 1U
void flow_clear_card(void) BANKED;
uint8_t flow_clear_frame(uint8_t* level) BANKED;

// puts the level's own bg back after a card overwrote it, without touching a single actor
void flow_resume_from_card(uint8_t area, uint16_t camera_x) BANKED;

#endif
