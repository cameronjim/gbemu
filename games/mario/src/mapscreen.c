// the SELECT FILE card and world one's map. neither runs inside a frame of play, so both ride in
// bank 5 with the title card - and they have to: they call title.c's card machinery with string
// literals, and a `const char*` handed across a bank boundary would point into the caller's own
// banked rodata and be read as whatever the callee's bank has at that address
#pragma bank 5

#include "mapscreen.h"

#include "assets.h"
#include "blocks.h"
#include "enemies.h"
#include "flow.h"
#include "hud.h"
#include "level.h"
#include "mario.h"
#include "save.h"
#include "title.h"

#include <gb/cgb.h>
#include <gb/gb.h>
#include <stdint.h>

// the three screens' own answers, private to this module now that one dispatcher drives them all
#define kFileStay 0U
#define kFileMap 1U
#define kFileTitle 2U
#define kMapStay 0U
#define kMapPlay 1U
#define kMapBack 2U
// and which of them is on screen
#define kScreenTitle 0U
#define kScreenFile 1U
#define kScreenMap 2U

// one slot line, built here rather than printed piecemeal so card_print_centered can center the
// whole thing; every line is padded to kFileLineWidth so the cursor column never shifts
static char line[kFileLineWidth + 1U];

// the file's furthest node, 0..kLevelCount, and which screen is up. both outlive a single screen,
// so they sit above all three
static uint8_t map_unlocked;
static uint8_t screen;

// --- the confirm/back lockout -------------------------------------------------------------------

// guards every screen against a start/a/b edge landing on the very frame it opens. lock_timer counts
// the window down; lock_held remembers which of the three were already down when the screen went
// live (or got pressed during the window) so a button that is simply held through the window cannot
// confirm the instant the timer runs out either - it has to be seen released first, like every other
// edge. left/right are never gated: walking mario the moment the map shows up plays fine, and gating
// it too would only cost the front end some of its responsiveness for nothing
static uint8_t lock_timer;
static uint8_t lock_held;
#define kLockGuardedKeys (uint8_t)(J_START | J_A | J_B)

// called the instant a screen becomes live, before its first frame is ever dispatched
static void lock_begin(void) {
    lock_timer = (uint8_t)kFrontLockFrames;
    lock_held = (uint8_t)(joypad() & kLockGuardedKeys);
}

// called once a frame, ahead of whichever screen is up, to strip the edges the lockout still owns
static uint8_t lock_gate(uint8_t pressed) {
    const uint8_t keys = joypad();
    uint8_t gated;

    if (lock_timer != 0U) {
        --lock_timer;
        // a press that lands inside the window is voided the same way an already-held button is:
        // added to the release-pending mask rather than let through
        lock_held = (uint8_t)(lock_held | (keys & kLockGuardedKeys));
    }
    gated = (uint8_t)(pressed & (uint8_t)~(lock_held & kLockGuardedKeys));
    // a button lets go of its guard the moment it is actually released, not before
    lock_held = (uint8_t)(lock_held & keys);
    return gated;
}

// --- SELECT FILE -------------------------------------------------------------------------------

static uint8_t cursor;
// 0 while the three slots are up, 1 while the erase confirm is: erase is a card of its own rather
// than a state main.c has to know about, because nothing outside this module can reach it
static uint8_t confirming;

static void put(uint8_t* n, const char* text) {
    uint8_t i;

    for (i = 0; text[i] != '\0'; ++i) {
        line[*n] = text[i];
        ++(*n);
    }
}

// ">1 WORLD 1-2" or ">1 NEW         ", padded to the same width either way
static void slot_line(uint8_t slot) {
    uint8_t n = 0;
    uint8_t level;

    line[n++] = (char)(slot == cursor ? '>' : ' ');
    line[n++] = (char)('1' + slot);
    line[n++] = ' ';
    if (save_slot_used(slot) == 0U) {
        put(&n, "NEW");
    } else {
        // a file that finished world one records kLevelCount, which is one past the last node; the
        // line names the level it stands on, so it clamps the same way the map does
        level = save_slot_level(slot);
        if (level >= (uint8_t)kLevelCount) {
            level = (uint8_t)(kLevelCount - 1U);
        }
        put(&n, "WORLD 1-");
        line[n++] = (char)('1' + level);
    }
    while (n < (uint8_t)kFileLineWidth) {
        line[n++] = ' ';
    }
    line[n] = '\0';
}

static void file_show(void) {
    uint8_t slot;
    uint8_t row;

    card_begin(kFileHeadRow);
    card_print_centered(kFileHeadRow, "SELECT FILE");
    for (slot = 0; slot < (uint8_t)kSaveSlots; ++slot) {
        row = (uint8_t)(kFileFirstRow + slot * kFileRowStep);
        if (slot == cursor) {
            // the lit slot gets the accent band every other card uses to say "this one"
            card_paint_band(row, 1, kPalAccent);
        }
        slot_line(slot);
        card_print_centered(row, line);
        if (save_slot_used(slot) != 0U) {
            card_print_value((uint8_t)(row + 1U), "SCORE ", save_slot_score(slot), 5, 1);
        }
    }
    card_print_centered(kFileHintRow, "UP DOWN PICKS");
    card_print_centered((uint8_t)(kFileHintRow + 1U), "A OPENS  B BACK");
    card_print_centered((uint8_t)(kFileHintRow + 2U), "SELECT ERASES");
    card_end();
}

static void erase_show(void) {
    card_begin(kEraseRow);
    card_print_centered(kEraseRow, "ERASE FILE");
    card_print_value((uint8_t)(kEraseRow + 2U), "FILE ", (uint16_t)(cursor + 1U), 1, 0);
    card_print_centered((uint8_t)(kEraseRow + 5U), "A ERASES IT");
    card_print_centered((uint8_t)(kEraseRow + 7U), "B KEEPS IT");
    card_end();
}

static void file_reset(void) {
    cursor = 0;
    confirming = 0;
    file_show();
    lock_begin();
}

static uint8_t file_frame(uint8_t pressed, uint8_t* level) {
    if (confirming != 0U) {
        // a file the player can never clear is a worse annoyance than the one accidental erase
        // this second press is here to prevent, so erase ships - behind a card of its own
        if ((pressed & (J_START | J_A)) != 0U) {
            save_erase(cursor);
            confirming = 0;
            file_show();
        } else if ((pressed & J_B) != 0U) {
            confirming = 0;
            file_show();
        }
        return kFileStay;
    }
    if ((pressed & (J_START | J_A)) != 0U) {
        save_select(cursor);
        // an untouched NEW slot becomes a real file the moment it is opened, so the player can see
        // which of the three they are in even before a level is cleared
        save_begin();
        map_unlocked = save_slot_level(cursor);
        *level = (map_unlocked >= (uint8_t)kLevelCount) ? (uint8_t)(kLevelCount - 1U) : map_unlocked;
        // a player's run is never a lab run, whatever the last debug entry left armed
        hud_set_short_timer(0);
        enemies_set_lab(0);
        blocks_set_lab(0);
        flow_begin_run(*level);
        return kFileMap;
    }
    if ((pressed & J_B) != 0U) {
        return kFileTitle;
    }
    if ((pressed & J_SELECT) != 0U && save_slot_used(cursor) != 0U) {
        confirming = 1;
        erase_show();
        return kFileStay;
    }
    if ((pressed & J_DOWN) != 0U) {
        cursor = (uint8_t)((cursor + 1U) % (uint8_t)kSaveSlots);
        file_show();
    } else if ((pressed & J_UP) != 0U) {
        cursor = (uint8_t)((cursor + (uint8_t)kSaveSlots - 1U) % (uint8_t)kSaveSlots);
        file_show();
    }
    return kFileStay;
}

// --- the world one map -------------------------------------------------------------------------

// the node mario stands on, and the walk between two of them
static uint8_t map_node;
static uint8_t map_target;
static uint8_t map_walking;
static uint8_t map_facing_left;
static uint8_t map_anim;
static uint8_t map_anim_timer;
static uint16_t map_x;

// the backdrop, one row of ten block kinds per screen row. only the four node markers are left out
// of it, because what they are drawn as depends on how far the file has got.
//
// row 1/2  a cloud over the middle of the world
// row 3    the marker row (columns 1, 3, 5, 7) and the castle's crenels
// row 4    the hill's peak, and the castle's door head and window
// row 5    the hill's base, a bush, the castle's door - and the row mario walks along, in front of
//          every one of them, exactly the way he walks past 1-1's scenery
// row 6    the ground, with a hard block under each node so the four stops read on the path itself
// row 7-8  the fill under it
//
// every kind here is already in the game's own tile set: this map adds no art at all
#define E kBlockEmpty
#define F kBlockGroundFill
static const uint8_t kMapRows[kMapBlockRows][kMapBlockCols] = {
    {E, E, E, E, E, E, E, E, E, E},
    {E, E, E, E, E, kBlockCloudTl, kBlockCloudT, kBlockCloudTr, E, E},
    {E, E, E, E, E, kBlockCloudBl, kBlockCloudB, kBlockCloudBr, E, E},
    {E, E, E, E, E, E, E, E, kBlockCastleCrenel, kBlockCastleCrenel},
    {E, kBlockHillPeak, E, E, E, E, E, E, kBlockCastleDoorTop, kBlockCastleWindow},
    {kBlockHillSlopeL, kBlockHillFill, kBlockHillSlopeR, E, kBlockBushL, kBlockBushM, kBlockBushR, E,
     kBlockCastleDoor, kBlockCastle},
    {kBlockGround, kBlockHard, kBlockGround, kBlockHard, kBlockGround, kBlockHard, kBlockGround,
     kBlockHard, kBlockGround, kBlockGround},
    {F, F, F, F, F, F, F, F, F, F},
    {F, F, F, F, F, F, F, F, F, F},
};
#undef E
#undef F

static uint8_t node_column(uint8_t node) {
    return (uint8_t)(kMapNodeFirstCol + node * kMapNodeStepCol);
}

// three legible states out of three block kinds the game already draws: a spent block for a level
// that is done, a lit question block for the one still to do, and a plain brick for one world one
// has not opened yet
static uint8_t marker_kind(uint8_t node) {
    if (node < map_unlocked) {
        return kBlockSpent;
    }
    return (node == map_unlocked) ? (uint8_t)kBlockQuestion : (uint8_t)kBlockBrick;
}

// one 16x16 block: four tiles under one cgb attribute, the same pairing terrain.c streams a level
// column with, read out of the tables assets_load_block_tables staged into ram
static void put_block(uint8_t bx, uint8_t by, uint8_t kind) {
    uint8_t tiles[2];
    uint8_t attr[2];
    const uint8_t x = (uint8_t)(bx * kTilesPerBlock);
    const uint8_t y = (uint8_t)(by * kTilesPerBlock);

    attr[0] = kBlockPalette[kind];
    attr[1] = attr[0];
    tiles[0] = kBlockTileTl[kind];
    tiles[1] = kBlockTileTr[kind];
    set_bkg_tiles(x, y, 2, 1, tiles);
    set_bkg_attributes(x, y, 2, 1, attr);
    tiles[0] = kBlockTileBl[kind];
    tiles[1] = kBlockTileBr[kind];
    set_bkg_tiles(x, (uint8_t)(y + 1U), 2, 1, tiles);
    set_bkg_attributes(x, (uint8_t)(y + 1U), 2, 1, attr);
}

// small mario, drawn straight rather than through player.c: he has no physics here, so the map
// owns his two 8x16 sprites itself and player.c keeps knowing nothing about this screen
static void draw_mario(void) {
    const uint8_t tile = (uint8_t)(kTileMarioFirst + map_anim * kMarioTilesPerFrame);
    const uint8_t left = map_facing_left != 0U ? (uint8_t)(tile + 2U) : tile;
    const uint8_t right = map_facing_left != 0U ? tile : (uint8_t)(tile + 2U);
    const uint8_t prop =
        (uint8_t)(kPalMario | (map_facing_left != 0U ? (uint8_t)S_FLIPX : 0U));
    const uint8_t x = (uint8_t)(map_x + kOamXOffset);
    const uint8_t y = (uint8_t)(kMapWalkRow * kBlockPx + kOamYOffset);

    set_sprite_tile(kSpriteMarioL, left);
    set_sprite_tile(kSpriteMarioR, right);
    set_sprite_prop(kSpriteMarioL, prop);
    set_sprite_prop(kSpriteMarioR, prop);
    move_sprite(kSpriteMarioL, x, y);
    move_sprite(kSpriteMarioR, (uint8_t)(x + 8U), y);
}

static void map_reset(uint8_t node) {
    // the seven bg palettes whose color 0 is the level's sky. the ground's is a dark green inside
    // its own art, so it is the one slot left alone
    static const uint8_t kSkySlots[7] = {kCamPalSky,     kCamPalBrick,   kCamPalQuestion, kCamPalPipe,
                                         kCamPalNeutral, kCamPalSpent,   kCamPalCoin};
    uint8_t bx;
    uint8_t by;

    map_node = (node < (uint8_t)kLevelCount) ? node : (uint8_t)(kLevelCount - 1U);
    if (map_node > map_unlocked) {
        map_node = map_unlocked;
    }
    map_target = map_node;
    map_walking = 0;
    map_facing_left = 0;
    map_anim = kFrameIdle;
    map_anim_timer = 0;
    map_x = (uint16_t)(node_column(map_node) * kBlockPx);

    // the whole bg map, its palettes and both tile banks are rewritten here, which is far more
    // vram traffic than a vblank holds
    DISPLAY_OFF;
    HIDE_SPRITES;
    SCX_REG = 0;
    SCY_REG = 0;
    assets_load_block_tables();
    assets_load_bg_tiles();
    assets_load_scenery_tiles();
    assets_load_bg_palettes();
    // only color 0 of each, so every other shade of the terrain art is exactly the level's
    for (bx = 0; bx < 7U; ++bx) {
        set_bkg_palette_entry(kSkySlots[bx], 0, kMapSkyRgb);
    }
    assets_load_sprite_tiles();
    assets_load_sprite_palettes();
    SPRITES_8x16;
    // card_clear_map blanks all 32x32 cells to the font's space glyph under palette 0, which is
    // the sky slot here too, so everything the 10x9 layout leaves out is simply sky
    card_clear_map();
    // the level left its own oam behind - mario's lower row, the five hud digits, items, enemies -
    // and SHOW_SPRITES below would put every one of them back on screen. the map draws two sprites
    // and owns all forty, so the rest are parked above the visible area first
    for (bx = 0; bx < (uint8_t)kOamSlots; ++bx) {
        move_sprite(bx, 0, 0);
    }
    for (by = 0; by < (uint8_t)kMapBlockRows; ++by) {
        for (bx = 0; bx < (uint8_t)kMapBlockCols; ++bx) {
            if (kMapRows[by][bx] != (uint8_t)kBlockEmpty) {
                put_block(bx, by, kMapRows[by][bx]);
            }
        }
    }
    for (bx = 0; bx < (uint8_t)kLevelCount; ++bx) {
        put_block(node_column(bx), kMapMarkerRow, marker_kind(bx));
    }
    draw_mario();
    SHOW_BKG;
    SHOW_SPRITES;
    DISPLAY_ON;
    lock_begin();
}

static uint8_t map_frame(uint8_t pressed, uint8_t* level) {
    const uint16_t goal = (uint16_t)(node_column(map_target) * kBlockPx);

    if (map_walking != 0U) {
        if (map_x < goal) {
            map_x = (uint16_t)(map_x + kMapWalkPx);
        } else if (map_x > goal) {
            map_x = (uint16_t)(map_x - kMapWalkPx);
        }
        if (map_x == goal) {
            map_walking = 0;
            map_node = map_target;
            map_anim = kFrameIdle;
        } else if (++map_anim_timer >= (uint8_t)kMapWalkAnimFrames) {
            map_anim_timer = 0;
            // the same three-frame cycle the level walk uses, one step every kMapWalkAnimFrames
            map_anim = (uint8_t)(kFrameWalk0 + (map_anim + 1U - kFrameWalk0) % kWalkFrameCount);
        }
        draw_mario();
        return kMapStay;
    }
    if ((pressed & (J_START | J_A)) != 0U) {
        *level = map_node;
        return kMapPlay;
    }
    if ((pressed & J_B) != 0U) {
        return kMapBack;
    }
    // a node past the file's furthest is not walkable at all: the path simply refuses, which is
    // what the locked brick marker over it has already said
    if ((pressed & J_RIGHT) != 0U && map_node + 1U < (uint8_t)kLevelCount &&
        map_node + 1U <= map_unlocked) {
        map_target = (uint8_t)(map_node + 1U);
        map_facing_left = 0;
    } else if ((pressed & J_LEFT) != 0U && map_node != 0U) {
        map_target = (uint8_t)(map_node - 1U);
        map_facing_left = 1;
    } else {
        return kMapStay;
    }
    map_walking = 1;
    map_anim = kFrameWalk0;
    map_anim_timer = 0;
    draw_mario();
    return kMapStay;
}

// --- the front end -----------------------------------------------------------------------------

// the three screens are one state machine rather than three of main.c's: every transition between
// them is a repaint this module already owns, so bank 0 carries a single dispatch block for all
// three and none of the paints below needs a trampoline out of bank 5

void front_title(void) BANKED {
    // the run is over: the file is let go of, so nothing a lab or a fresh boot does can be
    // recorded over it, and the map opens from the file's own furthest again when one is picked
    save_select(kSaveNoSlot);
    map_unlocked = 0;
    screen = kScreenTitle;
    title_reset();
    lock_begin();
}

void front_cleared(uint8_t* level) BANKED {
    // flow_clear_frame hands back the raw next node, which is kLevelCount once world one is done
    if (*level > map_unlocked) {
        map_unlocked = *level;
    }
    if (*level >= (uint8_t)kLevelCount) {
        *level = (uint8_t)(kLevelCount - 1U);
    }
    screen = kScreenMap;
    map_reset(*level);
}

uint8_t front_frame(uint8_t pressed, uint8_t* level) BANKED {
    uint8_t action;

    // one gate ahead of all three screens: whichever is up, its confirm/back edges are the lockout's
    // to strip until the window has passed and any button that was down when it opened has let go
    pressed = lock_gate(pressed);

    if (screen == (uint8_t)kScreenTitle) {
        action = title_frame(pressed, level);
        if (action == (uint8_t)kTitleFile) {
            screen = kScreenFile;
            file_reset();
        } else if (action == (uint8_t)kTitlePlay) {
            // a lab or the level select, straight past the front end and into a level
            return kFrontPlay;
        } else if (action == (uint8_t)kTitleCamera) {
            return kFrontCamera;
        }
        return kFrontStay;
    }
    if (screen == (uint8_t)kScreenFile) {
        action = file_frame(pressed, level);
        if (action == (uint8_t)kFileMap) {
            screen = kScreenMap;
            map_reset(*level);
        } else if (action == (uint8_t)kFileTitle) {
            front_title();
        }
        return kFrontStay;
    }
    action = map_frame(pressed, level);
    if (action == (uint8_t)kMapPlay) {
        return kFrontPlay;
    }
    if (action == (uint8_t)kMapBack) {
        screen = kScreenFile;
        file_reset();
    }
    return kFrontStay;
}
