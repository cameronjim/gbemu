#ifndef FLOW_H
#define FLOW_H

#include <gb/gb.h>
#include <stdint.h>

// the pipe and sub-area transitions. they run on a button edge, never inside a frame of play, so
// they ride in bank 5 with the title card; only the game loop's own state machine stays in bank 0

// loads a level from scratch: the grid, its art and palettes, the blocks, hazards, enemies and
// mario on the start cell. returns how many hazards the level has, which is 0 for 1-1
uint8_t flow_enter_level(uint8_t index) BANKED;

// swaps in one of the level's compiled sub-areas and drops mario at its start cell. 1 when the
// landing armed a rise out of a pipe rather than putting him straight into play, which only a
// same-grid jump onto a pipe cap does: the caller has to hold its pipe-up state open for it
uint8_t flow_enter_sub_area(uint8_t index) BANKED;

// reloads the main grid with its spent blocks intact and starts him rising out of the link pipe
void flow_leave_sub_area(void) BANKED;

// the pipe mario can enter from where he stands this frame, or 0xff. with down held that is a cap
// he is squarely on top of; with down released it is a sideways mouth his shoulder is against, and
// the caller must already have established that he is grounded, still and pushing right - the scan
// walks the object list, which is not something to pay for on every frame of a walk.
//
// the answer is a sub-area index, or - for 1-2's entrance/exit pipes, a same-grid segment teleport
// rather than a sub-area - kJumpAreaFlag set over the low bits; flow_enter_sub_area is what
// actually tells the two apart, so main.c's state machine can carry the value forward exactly like
// it already carries an area index
uint8_t flow_pipe_target(uint8_t down_held) BANKED;

// 1 while the level being played has a sideways pipe mouth in it at all. the game loop must test
// this before it looks at his stance or calls the scan above: only a walk-in trigger has no button
// edge to hang off, and asking on every frame of every walk is more than 1-1's frames can spare
extern uint8_t flow_side_pipes;

// 1 when the target flow_pipe_target just answered with is a sideways mouth, which is walked into
// rather than sunk down; a plain ram byte so bank 0 pays a load rather than a second banked call
extern uint8_t flow_pipe_side_armed;

// 1 while the main grid being played has loose coin cells in it at all (1-2 does, 1-1 does not).
// the game loop reads it to decide whether this level owes the pickup pass below a frame at all
extern uint8_t flow_grid_coins;

// picks up any loose grid coin the player's box is standing in, paying the same score and coin as
// a sub-area coin; call once per frame of play, on the main grid, while flow_grid_coins is set
void flow_collect_grid_coins(void) BANKED;

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

// the pennant coming down the pole beside mario during the clear. it is repainted bg cells rather
// than a sprite because oam is full (40/40), so it moves one 16 px cell at a time down the column
// left of the shaft - where apply_flag_head stamped it. flow_flag_arm parks it at the top, and
// flow_flag_step runs one frame of the descent and answers 1 once it has reached the pole's base.
// both live here rather than in player.c because bank 0 has no room left for them
void flow_flag_arm(void) BANKED;
uint8_t flow_flag_step(void) BANKED;

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
