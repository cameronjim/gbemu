// the game loop's states that never run on a frame of play: the pipe transitions, the death beat,
// the pause and clear cards and the game over. every one of them is a handful of frames between
// levels and none of them can happen inside a frame of play, so they ride in bank 6 with blocks_draw
// and hand bank 0 back the room its own hot path needs
#pragma bank 6

#include "states.h"

#include "camera.h"
#include "flow.h"
#include "hazards.h"
#include "hud.h"
#include "mapscreen.h"
#include "mario.h"
#include "player.h"
#include "terrain.h"
#include "title.h"

#include <gb/gb.h>
#include <stdint.h>

// the grid mario is playing in; a pipe swaps it and rebuilds the whole ring with the lcd off
uint8_t current_area;
// which of world one he is on, and where a pipe he has just entered is taking him
uint8_t level_number;
uint8_t pending_area;
uint8_t pending_warp;
// set the instant a pipe spits him back out while down is still held, so the same frame's held
// check cannot swallow him straight back into the exit pipe he just climbed out of; clears the
// moment down is released
uint8_t pipe_reentry_lock;
// 0 on a level with nothing hazards.c owns, which is 1-1: the bank-5 module is not entered at all
uint8_t hazard_active;
// and whether any of them is near enough this frame to be worth entering bank 5 for
uint8_t hazard_near;
// the level load and the respawn share this: both refill the whole ring, far more vram traffic
// than one vblank holds, so both do it with the lcd off
void states_enter_play(void) BANKED {
    DISPLAY_OFF;
    current_area = kAreaMain;
    pending_area = 0xFF;
    pending_warp = 0xFF;
    hazard_active = flow_enter_level(level_number);
    hazard_near = hazard_active;
    main_present();
    // the lcd is off and the first vblank is a frame away, so the camera goes into scx/scy here
    terrain_commit_scroll();
    SHOW_BKG;
    SHOW_SPRITES;
    DISPLAY_ON;
}

// a card overwrote the bg map and every actor is frozen where it stood: the ring is repainted
// where the camera already is, which is the same lcd-off refill a level load pays
static void leave_card(void) {
    DISPLAY_OFF;
    flow_resume_from_card(current_area, camera_pos_x);
    main_present();
    // the lcd is off and the first vblank is a frame away, so the camera goes into scx/scy here
    terrain_commit_scroll();
    SHOW_BKG;
    DISPLAY_ON;
}

// a pipe swaps the whole grid, its palettes and the ring, so it pays the same lcd-off rebuild the
// level load does rather than trying to stream a new area in through vblank; flow.c owns the work.
// answers 1 when the landing left him rising out of a pipe rather than standing in play.
//
// a same-grid jump (kJumpAreaFlag) is not an area at all: he is on the main grid when it is over,
// and recording that is what lets the next pipe of any kind trigger - the gate below only looks
// under him while current_area is kAreaMain, and before this it kept the flagged pseudo-value
static uint8_t enter_sub_area(uint8_t index) {
    uint8_t rising;

    DISPLAY_OFF;
    current_area = ((index & (uint8_t)kJumpAreaFlag) != 0U) ? (uint8_t)kAreaMain : index;
    rising = flow_enter_sub_area(index);
    main_present();
    // the lcd is off and the first vblank is a frame away, so the camera goes into scx/scy here
    terrain_commit_scroll();
    SHOW_BKG;
    DISPLAY_ON;
    return rising;
}

static void leave_sub_area(void) {
    DISPLAY_OFF;
    current_area = kAreaMain;
    flow_leave_sub_area();
    main_present();
    // the lcd is off and the first vblank is a frame away, so the camera goes into scx/scy here
    terrain_commit_scroll();
    SHOW_BKG;
    DISPLAY_ON;
}

uint8_t states_off_play(uint8_t state, uint8_t keys, uint8_t pressed) BANKED {
    if (state == kStateDeath) {
        // the world is frozen: nothing steps but mario falling out of it
        if (player_death_update() != 0U) {
            if (flow_after_death() != (uint8_t)kAfterDeathRespawn) {
                state = kStateGameOver;
            } else {
                // the level reloads whole, spent blocks and all, which is smb's own respawn
                states_enter_play();
                state = kStatePlay;
            }
            return state;
        }
        main_present();
        return state;
    }

    if (state == kStatePause) {
        const uint8_t choice = pause_frame(pressed);

        if (choice == (uint8_t)kPauseResume) {
            leave_card();
            state = kStatePlay;
        } else if (choice == (uint8_t)kPauseQuit) {
            // the run is abandoned, not cleared: nothing is recorded and the lives and score
            // stand. the next level entry reloads everything through enter_play, so whatever
            // sub-area or segment he quit from leaves nothing behind
            front_map(level_number);
            state = kStateFront;
        }
        return state;
    }

    if (state == kStateGameOver) {
        if (flow_game_over_frame() != 0U) {
            // a game over ends the run, so the file is let go of: whatever it recorded stands,
            // and the next thing the player picks starts a fresh three lives
            level_number = 0;
            front_title();
            state = kStateFront;
        }
        return state;
    }

    if (state == kStatePipeDown) {
        if (player_pipe_update() != 0U) {
            if (pending_warp != 0xFF) {
                // flow_warp_under_player() only ever hands back a real kLevels index here: a
                // pipe whose bible destination is a world we do not have yet compiles to
                // WARP_UNBUILT (0xFF, see compile_level.py), and the check above already
                // filtered that case out before pending_warp was ever set to it
                level_number = pending_warp;
                states_enter_play();
                state = kStatePlay;
            } else if (current_area == kAreaMain) {
                // a jump onto a pipe cap comes up out of it, so that landing owns a state of
                // its own; everything else is straight into play
                state = enter_sub_area(pending_area) != 0U ? kStatePipeUp : kStatePlay;
            } else {
                leave_sub_area();
                state = kStatePipeUp;
            }
            return state;
        }
        main_present();
        return state;
    }

    if (state == kStatePipeUp) {
        if (player_pipe_update() != 0U) {
            state = kStatePlay;
            // he can only just have climbed out of the exit pipe, so if down is still held this
            // is not a new press against it - lock the held check until he lets go of down
            pipe_reentry_lock = (uint8_t)((keys & J_DOWN) != 0U ? 1U : 0U);
        }
        main_present();
        return state;
    }

    if (state == kStateClear) {
        // the bridge is still coming apart a cell at a time behind him and bowser is still on his
        // way into the lava; the play loop that stepped bank 5 is over, so this frame does it
        if (hazard_clear_busy != 0U) {
            hazards_clear_step();
        }
        if (player_clear_update() != 0U) {
            flow_clear_card();
            state = kStateClearCard;
            return state;
        }
        // the sequence owns mario, so the camera tracks him as a supported-but-moving actor
        camera_update(player_x(), player_feet(), 1, 0, 0);
        main_present();
        return state;
    }

    if (state == kStateClearCard) {
        if (flow_clear_frame(&level_number) == (uint8_t)kAfterCardMap) {
            // back to the map with the next node open and the file written, not straight on
            // into the next level: picking what to play is the map's job now
            front_cleared(&level_number);
            state = kStateFront;
        }
        return state;
    }
    return state;
}
