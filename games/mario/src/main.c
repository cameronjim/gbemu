#include "blocks.h"
#include "camera.h"
#include "enemies.h"
#include "level_1_1.h"
#include "mario.h"
#include "physics_constants.h"
#include "player.h"
#include "powerup.h"
#include "terrain.h"

#include <gb/cgb.h>
#include <gb/gb.h>
#include <gbdk/console.h>
#include <gbdk/font.h>
#include <stdint.h>
#include <stdio.h>

enum GameState { kStateTitle, kStatePlay, kStateClear, kStateCamera, kStatePipeDown, kStatePipeUp };

// the grid mario is playing in; a pipe swaps it and rebuilds the whole ring with the lcd off
static uint8_t current_area;

// one bg map row's worth of tile ids or vram bank 1 attribute bytes, reused row by row
static uint8_t map_row[kRingTileCols];

static uint8_t text_len(const char* text) {
    uint8_t n = 0;
    while (text[n] != '\0') {
        ++n;
    }
    return n;
}

// putchar, not printf: the format parser costs bank 0 about 1.3kb that two fixed strings do not
// need, and bank 0 is where all the engine but enemies.c lives
static void print_centered(uint8_t y, const char* text) {
    uint8_t i;

    gotoxy((uint8_t)((kScreenCols - text_len(text)) / 2U), y);
    for (i = 0; text[i] != '\0'; ++i) {
        putchar(text[i]);
    }
}

// tags a whole bg row with one cgb palette; vram bank 1 holds the attribute map
static void paint_row_palette(uint8_t y, uint8_t palette) {
    uint8_t x;
    for (x = 0; x < kScreenCols; ++x) {
        map_row[x] = palette;
    }
    set_bkg_attributes(0, y, kScreenCols, 1, map_row);
}

// three real cgb palettes: sky backdrop, warm wordmark, green accent
static void load_palettes(void) {
    palette_color_t sky[4] = {RGB(20, 24, 31), RGB(12, 16, 28), RGB(6, 10, 22), RGB(2, 4, 14)};
    palette_color_t wordmark[4] = {RGB(20, 4, 2), RGB(31, 12, 2), RGB(31, 22, 4), RGB(31, 31, 20)};
    palette_color_t accent[4] = {RGB(2, 12, 4), RGB(4, 20, 8), RGB(10, 28, 12), RGB(24, 31, 20)};
    set_bkg_palette(kPalSky, 1, sky);
    set_bkg_palette(kPalWordmark, 1, wordmark);
    set_bkg_palette(kPalAccent, 1, accent);
}

// wipes the whole ring back to blank sky cells; coming back from a level leaves terrain in it
static void clear_map(void) {
    uint8_t y;
    uint8_t x;

    for (x = 0; x < kRingTileCols; ++x) {
        map_row[x] = kTileSky;
    }
    for (y = 0; y < kBgMapRows; ++y) {
        set_bkg_tiles(0, y, kRingTileCols, 1, map_row);
    }
    for (x = 0; x < kRingTileCols; ++x) {
        map_row[x] = kPalSky;
    }
    for (y = 0; y < kBgMapRows; ++y) {
        set_bkg_attributes(0, y, kRingTileCols, 1, map_row);
    }
}

// the whole map is rewritten here, far more vram traffic than a vblank holds, so the lcd is off
static void enter_title(void) {
    DISPLAY_OFF;
    HIDE_SPRITES;
    SCX_REG = 0;
    SCY_REG = 0;
    load_palettes();
    clear_map();
    // the wordmark and prompt each tint their own row; every other cell keeps the sky palette
    paint_row_palette(kTitleRow, kPalWordmark);
    paint_row_palette(kPromptRow, kPalAccent);
    font_color(kFontFore, kFontBack);
    // "!" pads the wordmark to an even glyph span so it lands pixel-centered
    print_centered(kTitleRow, "MARIO!");
    print_centered(kPromptRow, "SPACE TO START");
    SHOW_BKG;
    DISPLAY_ON;
}

// scx/scy and oam are cheap and must land before scanline 0; the ring stream can outlast vblank on
// a column boundary, so it always goes last
static void present(void) {
    terrain_set_scroll_x(camera_x());
    terrain_set_pan_y(camera_y());
    terrain_apply_scroll();
    player_draw(camera_x(), camera_y(), powerup_sprite_prop());
    blocks_draw(camera_x(), camera_y());
    powerup_draw(camera_x(), camera_y());
    enemies_draw(camera_x(), camera_y());
    terrain_stream_window();
}

// the level load and the respawn share this: both refill the whole ring, far more vram traffic
// than one vblank holds, so both do it with the lcd off
static void enter_play(void) {
    DISPLAY_OFF;
    current_area = kAreaMain;
    powerup_reset();
    blocks_load_level();
    blocks_set_player_big(0);
    blocks_enter_area(kAreaMain);
    terrain_init(kAreaMain);
    player_init();
    enemies_load_level();
    camera_init(player_x(), player_feet());
    present();
    SHOW_BKG;
    DISPLAY_ON;
}

// a pipe swaps the whole grid, its palettes and the ring, so it pays the same lcd-off rebuild the
// level load does rather than trying to stream a new area in through vblank
static void enter_bonus_area(void) {
    DISPLAY_OFF;
    current_area = kAreaBonus;
    blocks_enter_area(kAreaBonus);
    enemies_enter_area(kAreaBonus);
    terrain_init(kAreaBonus);
    player_place((uint16_t)LEVEL_1_1_AREA0_START_COLUMN, (uint8_t)LEVEL_1_1_AREA0_START_ROW);
    camera_init(player_x(), player_feet());
    present();
    SHOW_BKG;
    DISPLAY_ON;
}

// and the way back: the main level reloads with its spent blocks intact and mario rises out of the
// pipe the bible's link names
static void leave_bonus_area(void) {
    DISPLAY_OFF;
    current_area = kAreaMain;
    blocks_enter_area(kAreaMain);
    enemies_enter_area(kAreaMain);
    terrain_init(kAreaMain);
    player_begin_pipe_up((uint16_t)LEVEL_1_1_AREA0_RETURN_COLUMN, (uint8_t)LEVEL_1_1_AREA0_RETURN_TOP_ROW);
    // the camera is framed on where he ends up standing, not on the shaft he is still climbing out of
    camera_init((uint16_t)((uint16_t)LEVEL_1_1_AREA0_RETURN_COLUMN << 4),
                (int16_t)((int16_t)LEVEL_1_1_AREA0_RETURN_TOP_ROW << 4));
    present();
    SHOW_BKG;
    DISPLAY_ON;
}

#if kDebugCamera
// bcpd is mode-locked on real hardware: every palette and attribute write lands with the lcd off
static void enter_camera(void) {
    DISPLAY_OFF;
    HIDE_SPRITES;
    current_area = kAreaMain;
    blocks_load_level();
    blocks_enter_area(kAreaMain);
    terrain_init(kAreaMain);
    SHOW_BKG;
    DISPLAY_ON;
}

// the m2 debug camera: no player, no physics, just d-pad scroll/pan over the compiled terrain
static void camera_state_frame(uint8_t keys) {
    if ((keys & J_RIGHT) != 0U) {
        terrain_scroll_x((int8_t)kCamStepPx);
    } else if ((keys & J_LEFT) != 0U) {
        terrain_scroll_x(-(int8_t)kCamStepPx);
    }
    if ((keys & J_UP) != 0U) {
        terrain_pan_y(-(int8_t)kCamStepPx);
    } else if ((keys & J_DOWN) != 0U) {
        terrain_pan_y((int8_t)kCamStepPx);
    }
    // the only bg writes of the camera state happen here, inside vblank
    terrain_apply_scroll();
}
#endif

void main(void) {
    uint8_t state = kStateTitle;
    uint8_t keys = 0;
    uint8_t prev = 0;
    uint8_t pressed = 0;
    uint8_t status = kPlayerAlive;
    uint8_t contact = kEnemyHitNone;
    uint8_t taken = kItemNone;
    uint8_t flags = 0;

    font_init();
    font_set(font_load(font_ibm));
    powerup_init();
    enter_title();

    while (1) {
        vsync();
        prev = keys;
        keys = joypad();
        // edge triggered so holding a button cannot re-enter a state every frame
        pressed = (uint8_t)(keys & (uint8_t)~prev);

        if (state == kStateTitle) {
            if ((pressed & J_START) != 0U) {
                enemies_set_lab(0);
                blocks_set_lab(0);
                enter_play();
                state = kStatePlay;
            }
#if kEnemyLab
            // the same level, seeded with the lab's denser roster and its second dispenser; see
            // kEnemyLab in mario.h
            else if ((pressed & J_SELECT) != 0U) {
                enemies_set_lab(1);
                blocks_set_lab(1);
                enter_play();
                state = kStatePlay;
            }
#endif
#if kDebugCamera
            else if ((pressed & J_B) != 0U) {
                enter_camera();
                state = kStateCamera;
            }
#endif
            continue;
        }

        if (state == kStatePlay) {
            // smb freezes the whole world for the grow and shrink animations, so the frame stops
            // here: no physics, no camera, no blocks, no enemies, only the pose alternating
            if (powerup_frozen() != 0U) {
                powerup_update(keys, player_x(), player_y(), player_facing_left(), camera_x());
                player_set_big(powerup_pose_big());
                present();
                continue;
            }
            status = player_update(keys);
            if (status == kPlayerFell) {
                enter_play();
                continue;
            }
            if (status == kPlayerFlag) {
                player_begin_clear();
                state = kStateClear;
                present();
                continue;
            }
            // pipes are edge triggered, so holding the same d-pad direction still pans the camera
            if (current_area == kAreaMain) {
#if LEVEL_1_1_HAS_PIPE_ENTRY
                if ((pressed & J_DOWN) != 0U &&
                    player_over_pipe((uint16_t)LEVEL_1_1_PIPE_ENTRY_COLUMN,
                                     (uint8_t)LEVEL_1_1_PIPE_ENTRY_TOP_ROW) != 0U) {
                    player_begin_pipe_down();
                    state = kStatePipeDown;
                    present();
                    continue;
                }
#endif
            } else if ((pressed & J_UP) != 0U &&
                       player_over_pipe((uint16_t)LEVEL_1_1_AREA0_EXIT_COLUMN,
                                        (uint8_t)LEVEL_1_1_AREA0_EXIT_TOP_ROW) != 0U) {
                player_begin_pipe_down();
                state = kStatePipeDown;
                present();
                continue;
            }
            camera_update(player_x(), player_feet(), player_on_ground(), player_standing(), keys);
            taken = blocks_update(player_x(), player_box_top(), player_box_height(), camera_x());
            if (taken != kItemNone && powerup_collect(taken) != 0U) {
                present(); // the pickup froze the world; the next frame takes the branch above
                continue;
            }
            // the enemy pass runs last so it sees this frame's camera, and hands mario's reaction
            // back rather than reaching into him
            flags = (uint8_t)((player_on_ground() != 0U ? (uint8_t)kEnemyFlagGrounded : 0U) |
                              (powerup_star() != 0U ? (uint8_t)kEnemyFlagStar : 0U) |
                              (powerup_immune() != 0U ? (uint8_t)kEnemyFlagImmune : 0U));
            contact = enemies_update(player_x(), player_box_top(), player_box_height(), player_y_speed(),
                                     flags, camera_x());
            if (contact == kEnemyHitDamage && powerup_damage() != 0U) {
                // small mario has nothing left to lose; m8 turns this into lives and a death beat
                enter_play();
                continue;
            }
            if (contact > kEnemyHitDamage) {
                player_stomp_bounce(contact == kEnemyHitShellStomp ? (int8_t)kEnemyShellBouncePx
                                                                   : (int8_t)kEnemyStompBouncePx);
            }
            powerup_update(keys, player_x(), player_y(), player_facing_left(), camera_x());
            player_set_big(powerup_pose_big());
            blocks_set_player_big(powerup_big());
            present();
            continue;
        }

        if (state == kStatePipeDown) {
            if (player_pipe_update() != 0U) {
                if (current_area == kAreaMain) {
                    enter_bonus_area();
                    state = kStatePlay;
                } else {
                    leave_bonus_area();
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
                enter_title();
                state = kStateTitle;
                continue;
            }
            // the sequence owns mario, so the camera tracks him as a supported-but-moving actor
            camera_update(player_x(), player_feet(), 1, 0, 0);
            present();
            continue;
        }

#if kDebugCamera
        camera_state_frame(keys);
#endif
    }
}
