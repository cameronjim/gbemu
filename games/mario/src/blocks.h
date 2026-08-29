#ifndef BLOCKS_H
#define BLOCKS_H

#include <stdint.h>

// one compiled block's runtime state; a spent block is still solid, a broken one is gone entirely
#define kBlockStateIdle 0U
#define kBlockStateSpent 1U
#define kBlockStateGone 2U

// clears every block, coin and counter; the level load calls it, an area transition does not
void blocks_load_level(void);

// points the module at the kArea* grid about to be streamed and drops the live bump/item/coin
// slots; call before terrain_init, which streams columns through blocks_apply_column
void blocks_enter_area(uint8_t area);

// how many cells the runtime has altered in the grid being played, and how many of those a solidity
// probe could care about: a spent question block is as solid as a lit one and a world coin is as
// passable as sky, so only a materialized hidden block or a broken brick changes what
// terrain_solid_at answers. terrain.c reads both directly rather than calling through
// blocks_kind_override, because the call alone costs more than the frame can spare
extern uint8_t blocks_override_count;
extern uint8_t blocks_solid_edits;

// the kind a cell renders and collides as: `kind` is what the compiled grid holds there
uint8_t blocks_kind_override(int16_t column, int16_t row, uint8_t kind);

// patches one streamed column's 15 row bytes in place, in a single pass over the reaction list
void blocks_apply_column(int16_t column, uint8_t* rows);

// 1 while an unbumped hidden block waits in this cell; it is absent from collision until then
uint8_t blocks_hidden_at(int16_t column, int16_t row);

// a head bump from below landed on this cell: bounce it, and pay out whatever it holds
void blocks_head_bump(int16_t column, int16_t row);

// one frame of the bounce, the loose item and the coin pop, plus world-coin pickup in a sub-area.
// returns the kItem* he picked up this frame, or kItemNone; the effects are powerup.c's business
uint8_t blocks_update(uint16_t player_px, int16_t player_py, uint8_t player_h, uint16_t cam_x);

// writes the item and coin-pop sprites for this frame, or parks them off screen
void blocks_draw(uint16_t cam_x, uint8_t cam_y);

// the internal counters the hud will read in m8; nothing displays them yet
uint16_t blocks_coins(void);
uint8_t blocks_items_taken(void);

// grown mario breaks bricks instead of bouncing them, and a mushroom_fire block pays him a flower
void blocks_set_player_big(uint8_t big);

// debug: the next blocks_load_level turns the compiled hidden block into a second mushroom_fire
// dispenser, which is the only cell in 1-1 where a flower ends up somewhere a test can reach it.
// see kEnemyLab in mario.h; select from the title arms both this and the enemy lab
void blocks_set_lab(uint8_t on);

#endif
