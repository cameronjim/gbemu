#include "blocks.h"
#include "camera.h"
#include "enemies.h"
#include "flow.h"
#include "hazards.h"
#include "hud.h"
#include "level.h"
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

enum GameState {
    kStateTitle,
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
    SHOW_BKG;
    DISPLAY_ON;
}

// a pipe swaps the whole grid, its palettes and the ring, so it pays the same lcd-off rebuild the
// level load does rather than trying to stream a new area in through vblank; flow.c owns the work
static void enter_sub_area(uint8_t index) {
    DISPLAY_OFF;
    current_area = index;
    flow_enter_sub_area(index);
    present();
    SHOW_BKG;
    DISPLAY_ON;
}

static void leave_sub_area(void) {
    DISPLAY_OFF;
    current_area = kAreaMain;
    flow_leave_sub_area();
    present();
    SHOW_BKG;
    DISPLAY_ON;
}

// every death goes through the same beat, whatever killed him
static uint8_t begin_death(uint8_t from) {
    player_begin_death(from);
    return kStateDeath;
}

void main(void) {
    uint8_t state = kStateTitle;
    uint8_t keys = 0;
    uint8_t prev = 0;
    uint8_t pressed = 0;
    uint8_t status = kPlayerAlive;
    uint8_t contact = kEnemyHitNone;
    uint8_t taken = kItemNone;
    uint8_t flags = 0;
    uint8_t target = 0xFF;

    font_init();
    font_set(font_load(font_ibm));
    powerup_init();
    save_init();
    level_number = 0;
    title_reset();

    while (1) {
        vsync();
        prev = keys;
        keys = joypad();
        // edge triggered so holding a button cannot re-enter a state every frame
        pressed = (uint8_t)(keys & (uint8_t)~prev);

        if (state == kStateTitle) {
            const uint8_t action = title_frame(pressed, &level_number);

            if (action == (uint8_t)kTitlePlay) {
                enter_play();
                state = kStatePlay;
            } else if (action == (uint8_t)kTitleCamera) {
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
            // the countdown and the coin rollover, and the five digit sprites they move
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
            // pipes are edge triggered, so holding the same d-pad direction still pans the camera
            if (current_area == kAreaMain) {
                if ((pressed & J_DOWN) != 0U) {
                    target = flow_pipe_under_player();
                    if (target != 0xFF) {
                        pending_area = target;
                        pending_warp = 0xFF;
                        player_begin_pipe_down();
                        state = kStatePipeDown;
                        present();
                        continue;
                    }
                }
            } else if ((pressed & J_DOWN) != 0U) {
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
                level_number = 0;
                title_reset();
                state = kStateTitle;
            }
            continue;
        }

        if (state == kStatePipeDown) {
            if (player_pipe_update() != 0U) {
                if (pending_warp != 0xFF) {
                    // the bible's warp targets are worlds we do not have yet, so compile_level.py
                    // clamped every one of them to the last level of world one
                    level_number = pending_warp;
                    enter_play();
                    state = kStatePlay;
                } else if (current_area == kAreaMain) {
                    enter_sub_area(pending_area);
                    state = kStatePlay;
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
            const uint8_t after = flow_clear_frame(&level_number);

            if (after == (uint8_t)kAfterCardNext) {
                enter_play();
                state = kStatePlay;
            } else if (after == (uint8_t)kAfterCardTitle) {
                level_number = 0;
                title_reset();
                state = kStateTitle;
            }
            continue;
        }

#if kDebugCamera
        debug_camera_frame(keys);
#endif
    }
}
