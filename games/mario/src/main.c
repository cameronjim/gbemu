#include "blocks.h"
#include "camera.h"
#include "enemies.h"
#include "flow.h"
#include "hazards.h"
#include "hud.h"
#include "level.h"
#include "mapscreen.h"
#include "mario.h"
#include "physics_constants.h"
#include "player.h"
#include "powerup.h"
#include "save.h"
#include "terrain.h"
#include "title.h"

#include <gb/cgb.h>
#include <gb/gb.h>
#include <gbdk/console.h>
#include <gbdk/font.h>
#include <stdint.h>
#include <stdio.h>

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

// the grid mario is playing in; a pipe swaps it and rebuilds the whole ring with the lcd off
static uint8_t current_area;
// which of world one he is on, and where a pipe he has just entered is taking him
static uint8_t level_number;
static uint8_t pending_area;
static uint8_t pending_warp;
// set the instant a pipe spits him back out while down is still held, so the same frame's held
// check cannot swallow him straight back into the exit pipe he just climbed out of; clears the
// moment down is released
static uint8_t pipe_reentry_lock;
// 0 on a level with nothing hazards.c owns, which is 1-1: the bank-5 module is not entered at all
static uint8_t hazard_active;
// and whether any of them is near enough this frame to be worth entering bank 5 for
static uint8_t hazard_near;
// scx/scy and oam are cheap and must land before scanline 0; the ring stream can outlast vblank on
// a column boundary, so it always goes last
static void present(void) {
    terrain_set_scroll_x(camera_pos_x);
    terrain_set_pan_y(camera_pos_y);
    terrain_apply_scroll();
    // the hud strip is a layer of every frame that draws the level and of no card's; raising it
    // here rather than inside terrain_apply_scroll is what keeps it off the debug camera, whose
    // whole point is an unobstructed look at the terrain
    terrain_bar_on = 1U;
    player_draw(camera_pos_x, camera_pos_y, powerup_prop);
    if (blocks_busy != 0U) {
        blocks_draw(camera_pos_x, camera_pos_y);
    }
    if ((powerup_flags & kPowerFlagDrawn) != 0U) {
        powerup_draw(camera_pos_x, camera_pos_y);
    }
    enemies_draw(camera_pos_x, camera_pos_y);
    // a level with no lift, firebar, bowser or axe near the view never enters bank 5 for them
    if (hazard_near != 0U) {
        hazards_draw(camera_pos_x, camera_pos_y);
    }
    terrain_stream_window();
}

// the level load and the respawn share this: both refill the whole ring, far more vram traffic
// than one vblank holds, so both do it with the lcd off
static void enter_play(void) {
    DISPLAY_OFF;
    current_area = kAreaMain;
    pending_area = 0xFF;
    pending_warp = 0xFF;
    hazard_active = flow_enter_level(level_number);
    hazard_near = hazard_active;
    present();
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
    present();
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
    present();
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
    present();
    // the lcd is off and the first vblank is a frame away, so the camera goes into scx/scy here
    terrain_commit_scroll();
    SHOW_BKG;
    DISPLAY_ON;
}

// every death goes through the same beat, whatever killed him
static uint8_t begin_death(uint8_t from) {
    player_begin_death(from);
    return kStateDeath;
}

void main(void) {
    uint8_t state = kStateFront;
    uint8_t keys = 0;
    uint8_t prev = 0;
    uint8_t pressed = 0;
    uint8_t status = kPlayerAlive;
    uint8_t contact = kEnemyHitNone;
    uint8_t taken = kItemNone;
    uint8_t flags = 0;
    uint8_t target = 0xFF;
    uint8_t down_held = 0;

    font_init();
    font_set(font_load(font_ibm));
    // the vbl handler that lands the camera and the lyc one that clips the hud strip, both before
    // anything turns the lcd on
    terrain_install_isrs();
    powerup_init();
    save_init();
    level_number = 0;
    front_title();

    while (1) {
        vsync();
        prev = keys;
        keys = joypad();
        // edge triggered so holding a button cannot re-enter a state every frame
        pressed = (uint8_t)(keys & (uint8_t)~prev);

        if (state == kStateFront) {
            // the title card, the file select and the world map, all driven by mapscreen.c: bank 0
            // only needs to know that one of them asked for a level or for the debug camera
            const uint8_t action = front_frame(pressed, &level_number);

            if (action == (uint8_t)kFrontPlay) {
                enter_play();
                state = kStatePlay;
            } else if (action == (uint8_t)kFrontCamera) {
                current_area = kAreaMain;
                state = kStateCamera;
            }
            continue;
        }

        if (state == kStatePlay) {
            // smb freezes the whole world for the grow and shrink animations, so the frame stops
            // here: no physics, no camera, no blocks, no enemies, only the pose alternating
            if ((powerup_flags & kPowerFlagFrozen) != 0U) {
                powerup_update(keys, player_x(), player_y(), player_facing_left(), camera_pos_x);
                player_set_big(powerup_pose);
                present();
                continue;
            }
            if ((pressed & J_START) != 0U) {
                card_pause(level_number);
                state = kStatePause;
                continue;
            }
            // the countdown and the coin rollover, and whichever cells of the strip they move
            if (hud_frame() != 0U) {
                state = begin_death(kDeathFromHit);
                present();
                continue;
            }
            // the lifts, firebars and bowser move first, so the deck under him has already gone
            // where it is going by the time his own step carries him with it. the gate is worked
            // out once, off the camera as it stood at the end of the last frame, so the step, the
            // contact test and the draw all agree about whether this frame has hazards in it
            hazard_near = (uint8_t)(hazard_active != 0U &&
                                            (uint16_t)(camera_pos_x + kScreenWidthPx + kHazardMarginPx) >
                                                hazard_min_x &&
                                            camera_pos_x < (uint16_t)(hazard_max_x + kHazardMarginPx)
                                        ? 1U
                                        : 0U);
            if (hazard_near != 0U) {
                hazards_step();
            }
            status = player_update(keys);
            if (status == kPlayerFell) {
                // the pit has already taken him under the level, so the beat only holds
                state = begin_death(kDeathFromPit);
                present();
                continue;
            }
            if (status == kPlayerFlag) {
                flow_score_flag(player_feet());
                player_begin_clear(kClearFromFlag);
                state = kStateClear;
                present();
                continue;
            }
            // down is held rather than edge triggered, so landing on a cap with down already
            // pressed still enters it; the lock (cleared the instant down is released) is the only
            // thing standing between that and immediately re-swallowing him on the way out of one
            if ((keys & J_DOWN) == 0U) {
                pipe_reentry_lock = 0;
            }
            down_held = (uint8_t)(((keys & J_DOWN) != 0U && pipe_reentry_lock == 0U) ? 1U : 0U);
            if (current_area == kAreaMain) {
                // down over a cap, or right into a sideways mouth - which can only be answered
                // while he is grounded and pressed still against something, so the object scan
                // costs nothing on an ordinary frame of walking
                if (down_held != 0U ||
                    (flow_side_pipes != 0U && (keys & J_RIGHT) != 0U && player_standing() != 0U)) {
                    target = flow_pipe_target(down_held);
                    if (target != 0xFF) {
                        pending_area = target;
                        pending_warp = 0xFF;
                        if (flow_pipe_side_armed != 0U) {
                            player_begin_pipe_side();
                        } else {
                            player_begin_pipe_down();
                        }
                        state = kStatePipeDown;
                        present();
                        continue;
                    }
                }
            } else if (down_held != 0U) {
                target = flow_warp_under_player();
                if (target != 0xFF) {
                    pending_warp = target;
                    player_begin_pipe_down();
                    state = kStatePipeDown;
                    present();
                    continue;
                }
                // every other pipe in the game takes down, so the bonus room's own exit should too
                if (flow_over_exit_pipe() != 0U) {
                    pending_warp = 0xFF;
                    player_begin_pipe_down();
                    state = kStatePipeDown;
                    present();
                    continue;
                }
            } else if ((pressed & J_UP) != 0U && flow_over_exit_pipe() != 0U) {
                pending_warp = 0xFF;
                player_begin_pipe_down();
                state = kStatePipeDown;
                present();
                continue;
            }
            camera_update(player_x(), player_feet(), player_on_ground(), player_standing(), keys);
            // the loose grid coins 1-2 is strewn with; no other level in world one has any
            if (flow_grid_coins != 0U) {
                flow_collect_grid_coins();
            }
            taken = blocks_busy != 0U
                        ? blocks_update(player_x(), player_box_top(), player_box_height(), camera_pos_x)
                        : (uint8_t)kItemNone;
            if (taken != kItemNone && powerup_collect(taken) != 0U) {
                present(); // the pickup froze the world; the next frame takes the branch above
                continue;
            }
            // the enemy pass runs last so it sees this frame's camera, and hands mario's reaction
            // back rather than reaching into him
            flags = (uint8_t)((player_on_ground() != 0U ? (uint8_t)kEnemyFlagGrounded : 0U) |
                              ((powerup_flags & kPowerFlagStar) != 0U ? (uint8_t)kEnemyFlagStar : 0U) |
                              ((powerup_flags & kPowerFlagImmune) != 0U ? (uint8_t)kEnemyFlagImmune : 0U));
            contact = enemies_update(player_x(), player_box_top(), player_box_height(), player_y_speed(),
                                     flags, camera_pos_x);
            if (contact == kEnemyHitNone && hazard_near != 0U) {
                const uint8_t hazard =
                    hazards_contact(player_x(), player_box_top(), player_box_height(),
                                    (uint8_t)((flags & (kEnemyFlagStar | kEnemyFlagImmune)) != 0U ? 1U : 0U));

                if (hazard == kHazardAxe) {
                    hazards_drop_bridge();
                    player_begin_clear(kClearFromAxe);
                    state = kStateClear;
                    present();
                    continue;
                }
                if (hazard == kHazardDamage) {
                    contact = kEnemyHitDamage;
                }
            }
            if (contact == kEnemyHitDamage && powerup_damage() != 0U) {
                // small mario has nothing left to lose
                state = begin_death(kDeathFromHit);
                present();
                continue;
            }
            if (contact > kEnemyHitDamage) {
                player_stomp_bounce(contact == kEnemyHitShellStomp ? (int8_t)kEnemyShellBouncePx
                                                                   : (int8_t)kEnemyStompBouncePx);
            }
            // no timer running, no ball in the air and no flower in hand: nothing to step, and the
            // frame that spawns an enemy while the ring streams a column has no room to spare
            if ((powerup_flags & kPowerFlagBusy) != 0U) {
                powerup_update(keys, player_x(), player_y(), player_facing_left(), camera_pos_x);
            }
            player_set_big(powerup_pose);
            blocks_player_big = (uint8_t)((powerup_flags & kPowerFlagBig) != 0U ? 1U : 0U);
            present();
            continue;
        }

        if (state == kStateDeath) {
            // the world is frozen: nothing steps but mario falling out of it
            if (player_death_update() != 0U) {
                if (flow_after_death() != (uint8_t)kAfterDeathRespawn) {
                    state = kStateGameOver;
                } else {
                    // the level reloads whole, spent blocks and all, which is smb's own respawn
                    enter_play();
                    state = kStatePlay;
                }
                continue;
            }
            present();
            continue;
        }

        if (state == kStatePause) {
            if ((pressed & J_START) != 0U) {
                leave_card();
                state = kStatePlay;
            }
            continue;
        }

        if (state == kStateGameOver) {
            if (flow_game_over_frame() != 0U) {
                // a game over ends the run, so the file is let go of: whatever it recorded stands,
                // and the next thing the player picks starts a fresh three lives
                level_number = 0;
                front_title();
                state = kStateFront;
            }
            continue;
        }

        if (state == kStatePipeDown) {
            if (player_pipe_update() != 0U) {
                if (pending_warp != 0xFF) {
                    // flow_warp_under_player() only ever hands back a real kLevels index here: a
                    // pipe whose bible destination is a world we do not have yet compiles to
                    // WARP_UNBUILT (0xFF, see compile_level.py), and the check above already
                    // filtered that case out before pending_warp was ever set to it
                    level_number = pending_warp;
                    enter_play();
                    state = kStatePlay;
                } else if (current_area == kAreaMain) {
                    // a jump onto a pipe cap comes up out of it, so that landing owns a state of
                    // its own; everything else is straight into play
                    state = enter_sub_area(pending_area) != 0U ? kStatePipeUp : kStatePlay;
                } else {
                    leave_sub_area();
                    state = kStatePipeUp;
                }
                continue;
            }
            present();
            continue;
        }

        if (state == kStatePipeUp) {
            if (player_pipe_update() != 0U) {
                state = kStatePlay;
                // he can only just have climbed out of the exit pipe, so if down is still held this
                // is not a new press against it - lock the held check until he lets go of down
                pipe_reentry_lock = (uint8_t)((keys & J_DOWN) != 0U ? 1U : 0U);
            }
            present();
            continue;
        }

        if (state == kStateClear) {
            if (player_clear_update() != 0U) {
                flow_clear_card();
                state = kStateClearCard;
                continue;
            }
            // the sequence owns mario, so the camera tracks him as a supported-but-moving actor
            camera_update(player_x(), player_feet(), 1, 0, 0);
            present();
            continue;
        }

        if (state == kStateClearCard) {
            if (flow_clear_frame(&level_number) == (uint8_t)kAfterCardMap) {
                // back to the map with the next node open and the file written, not straight on
                // into the next level: picking what to play is the map's job now
                front_cleared(&level_number);
                state = kStateFront;
            }
            continue;
        }

#if kDebugCamera
        debug_camera_frame(keys);
#endif
    }
}
