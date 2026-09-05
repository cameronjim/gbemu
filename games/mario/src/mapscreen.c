// the SELECT FILE card and world one's map. neither runs inside a frame of play, so both ride in
// bank 5 with the title card - and they have to: they call title.c's card machinery with string
// literals, and a `const char*` handed across a bank boundary would point into the caller's own
// banked rodata and be read as whatever the callee's bank has at that address
#pragma bank 5

#include "mapscreen.h"

#include "assets.h"
#include "blocks.h"
#include "enemies.h"
#include "file_art.h"
#include "flow.h"
#include "hud.h"
#include "level.h"
#include "map_art.h"
#include "mario.h"
#include "save.h"
#include "states.h"
#include "terrain.h"
#include "title.h"

#include <gb/cgb.h>
#include <gb/gb.h>
#include <gbdk/font.h>
#include <stdint.h>
#include <stdio.h>

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

// 0 while he is standing on a pipe, else how far into the hop to the cursor's one he is
static uint8_t hop_frame;
static uint8_t hop_from;

// the slot's label: 0 for NEW, else the level number the file stands on. a file that finished
// world one records kLevelCount, one past the last node, so it clamps the same way the map does
static uint8_t slot_label(uint8_t slot) {
    uint8_t level;

    if (save_slot_used(slot) == 0U) {
        return 0;
    }
    level = save_slot_level(slot);
    if (level >= (uint8_t)kLevelCount) {
        level = (uint8_t)(kLevelCount - 1U);
    }
    return (uint8_t)(level + 1U);
}

static uint8_t mario_pipe_x(uint8_t slot) {
    return (uint8_t)(kFileMarioX + slot * kFileMarioStep);
}

// super mario as two rows of two 8x16 sprites, the same pose pair player.c draws him standing in.
// the jump slab is the one pose that lives in vram bank 1, so its rows carry S_BANK
static void file_draw_mario(uint8_t x, uint8_t y, uint8_t jumping) {
    const uint8_t upper = jumping != 0U ? (uint8_t)kTileSuperJumpUpper : (uint8_t)kTileSuperUpper;
    const uint8_t frame = jumping != 0U ? (uint8_t)kFrameJump : (uint8_t)kFrameIdle;
    const uint8_t lower = (uint8_t)(kTileSuperLowerFirst + frame * kSuperTilesPerFrame);
    const uint8_t prop = (uint8_t)(kPalMario | (jumping != 0U ? (uint8_t)S_BANK : 0U));
    const uint8_t sx = (uint8_t)(x + kOamXOffset);
    const uint8_t sy = (uint8_t)(y + kOamYOffset);

    set_sprite_tile(kSpriteMarioL, upper);
    set_sprite_tile(kSpriteMarioR, (uint8_t)(upper + 2U));
    set_sprite_tile(kSpriteMarioLowL, lower);
    set_sprite_tile(kSpriteMarioLowR, (uint8_t)(lower + 2U));
    set_sprite_prop(kSpriteMarioL, prop);
    set_sprite_prop(kSpriteMarioR, prop);
    set_sprite_prop(kSpriteMarioLowL, (uint8_t)kPalMario);
    set_sprite_prop(kSpriteMarioLowR, (uint8_t)kPalMario);
    move_sprite(kSpriteMarioL, sx, sy);
    move_sprite(kSpriteMarioR, (uint8_t)(sx + 8U), sy);
    move_sprite(kSpriteMarioLowL, sx, (uint8_t)(sy + kPlayerHeightPx));
    move_sprite(kSpriteMarioLowR, (uint8_t)(sx + 8U), (uint8_t)(sy + kPlayerHeightPx));
}

static void file_show(void) {
    uint8_t slot;

    hop_frame = 0;
    // the whole bg map, both tile banks and the sprite set are rewritten here, far more vram
    // traffic than a vblank holds
    DISPLAY_OFF;
    HIDE_SPRITES;
    terrain_park_scroll();
    file_art_load();
    for (slot = 0; slot < (uint8_t)kSaveSlots; ++slot) {
        file_art_label(slot, slot_label(slot));
    }
    assets_load_sprite_tiles();
    assets_load_sprite_palettes();
    SPRITES_8x16;
    // a level or the title left its own oam behind, and SHOW_SPRITES would put every one of it
    // back on screen; this screen draws four and owns all forty
    for (slot = 0; slot < (uint8_t)kOamSlots; ++slot) {
        move_sprite(slot, 0, 0);
    }
    file_draw_mario(mario_pipe_x(cursor), (uint8_t)kFileStandY, 0);
    SHOW_BKG;
    SHOW_SPRITES;
    DISPLAY_ON;
}

// one frame of the arc: linear in x, and a parabola in y that is zero at both ends
static void file_hop_step(void) {
    const int16_t from = (int16_t)mario_pipe_x(hop_from);
    const int16_t to = (int16_t)mario_pipe_x(cursor);
    const uint16_t t = hop_frame;
    uint16_t arc;
    int16_t x;

    if (t >= (uint16_t)kFileHopFrames) {
        hop_frame = 0;
        file_draw_mario((uint8_t)to, (uint8_t)kFileStandY, 0);
        return;
    }
    x = (int16_t)(from + (int16_t)((int16_t)(to - from) * (int16_t)t / (int16_t)kFileHopFrames));
    arc = (uint16_t)((4U * kFileHopPeak * t * ((uint16_t)kFileHopFrames - t)) /
                     ((uint16_t)kFileHopFrames * (uint16_t)kFileHopFrames));
    ++hop_frame;
    file_draw_mario((uint8_t)x, (uint8_t)(kFileStandY - (uint8_t)arc), 1);
}

// the cursor becomes the destination the instant the hop starts, so a/b/select answer for the pipe
// he is on his way to rather than the one he left
static void file_hop_begin(uint8_t dest) {
    hop_from = cursor;
    cursor = dest;
    hop_frame = 1;
    file_draw_mario(mario_pipe_x(hop_from), (uint8_t)kFileStandY, 1);
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
        // this second press is here to prevent, so erase ships - behind a card of its own. a is
        // the only confirm on the front end now, and start is one more way to back out: the sdl
        // frontend maps esc to start, and esc is "back" everywhere it is not "pause"
        if ((pressed & J_A) != 0U) {
            save_erase(cursor);
            confirming = 0;
            file_show();
        } else if ((pressed & (J_START | J_B)) != 0U) {
            confirming = 0;
            file_show();
        }
        return kFileStay;
    }
    // a hop owns the screen while it runs: every button is ignored until he lands, so a confirm
    // can never fire for a pipe he is still in the air over
    if (hop_frame != 0U) {
        file_hop_step();
        return kFileStay;
    }
    if ((pressed & J_A) != 0U) {
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
    if ((pressed & (J_START | J_B)) != 0U) {
        return kFileTitle;
    }
    if ((pressed & J_SELECT) != 0U && save_slot_used(cursor) != 0U) {
        confirming = 1;
        erase_show();
        return kFileStay;
    }
    // up and down are kept as aliases of left and right so every path in that predates the pipes
    // still walks the three the same way
    if ((pressed & (uint8_t)(J_RIGHT | J_DOWN)) != 0U) {
        file_hop_begin((uint8_t)((cursor + 1U) % (uint8_t)kSaveSlots));
    } else if ((pressed & (uint8_t)(J_LEFT | J_UP)) != 0U) {
        file_hop_begin((uint8_t)((cursor + (uint8_t)kSaveSlots - 1U) % (uint8_t)kSaveSlots));
    }
    return kFileStay;
}

// --- the world one map -------------------------------------------------------------------------

// the node mario stands on, the one he is walking to, and how far along he is
static uint8_t map_node;
static uint8_t map_target;
static uint8_t map_walking;
static uint8_t map_step;
static uint8_t map_facing_left;
static uint8_t map_anim;
static uint8_t map_anim_timer;
// the water's own eight-frame cycle, ticked whether or not anything else is happening
static uint8_t map_water_timer;
static uint8_t map_water_frame;

// set while the "world 2 is on its way" card sits over the strip; see the kMapPopup* block in
// mario.h
static uint8_t map_popup;
// the popup's own three lines; see the comment on these in mapscreen.h for why they are plain ram
// filled in by states.c rather than string literals here
char map_popup_line1[kMapPopupLineWidth];
char map_popup_line2[kMapPopupLineWidth];
char map_popup_line3[kMapPopupLineWidth];

// the four node cells the generated frame was measured against. bank 5 keeps its own copy rather
// than reading map_art's: a const array only reads correctly under its own rom bank
static const uint8_t kMapNodeX[kMapNodeCount] = kMapNodeXs;
static const uint8_t kMapNodeY[kMapNodeCount] = kMapNodeYs;

// how far apart two neighbouring nodes are in x, which is what a walk between them is measured in
static uint8_t node_span(uint8_t a, uint8_t b) {
    return (uint8_t)(kMapNodeX[a] < kMapNodeX[b] ? kMapNodeX[b] - kMapNodeX[a] : kMapNodeX[a] - kMapNodeX[b]);
}

// where mario's 16x16 art sits this frame: on his node, or interpolated along the straight line to
// the one he is walking to. the four nodes are at four different heights, so a hop carries y as
// well as x - and the two that cross the lake simply run over it, the way the reference's own path
// does between its islands
static void map_mario_pos(uint8_t* px, uint8_t* py) {
    int16_t x = (int16_t)kMapNodeX[map_node];
    int16_t y = (int16_t)kMapNodeY[map_node];

    if (map_node != map_target) {
        const int16_t span = (int16_t)node_span(map_node, map_target);
        const int16_t dx = (int16_t)((int16_t)kMapNodeX[map_target] - x);
        const int16_t dy = (int16_t)((int16_t)kMapNodeY[map_target] - y);

        x = (int16_t)(x + (int16_t)((int16_t)(dx * (int16_t)map_step) / span));
        y = (int16_t)(y + (int16_t)((int16_t)(dy * (int16_t)map_step) / span));
    }
    *px = (uint8_t)(x - (int16_t)kMapMarioDx);
    *py = (uint8_t)(y - (int16_t)kMapMarioDy);
}

// small mario, drawn straight rather than through player.c: he has no physics here, so the map
// owns his two 8x16 sprites itself and player.c keeps knowing nothing about this screen
static void draw_mario(void) {
    const uint8_t tile = (uint8_t)(kTileMarioFirst + map_anim * kMarioTilesPerFrame);
    const uint8_t left = map_facing_left != 0U ? (uint8_t)(tile + 2U) : tile;
    const uint8_t right = map_facing_left != 0U ? tile : (uint8_t)(tile + 2U);
    const uint8_t prop = (uint8_t)(kPalMario | (map_facing_left != 0U ? (uint8_t)S_FLIPX : 0U));
    uint8_t mx;
    uint8_t my;

    map_mario_pos(&mx, &my);
    set_sprite_tile(kSpriteMarioL, left);
    set_sprite_tile(kSpriteMarioR, right);
    set_sprite_prop(kSpriteMarioL, prop);
    set_sprite_prop(kSpriteMarioR, prop);
    move_sprite(kSpriteMarioL, (uint8_t)(mx + kOamXOffset), (uint8_t)(my + kOamYOffset));
    move_sprite(kSpriteMarioR, (uint8_t)(mx + kOamXOffset + 8U), (uint8_t)(my + kOamYOffset));
}

// red once a level is behind the file's furthest, blue while it is still ahead - smbd rings every
// path node it has not cleared, locked or not, and the lock is the path refusing to walk there.
// the castle carries none: 1-4 is the castle itself
static void map_draw_markers(uint8_t shown) {
    uint8_t i;

    for (i = 0; i < (uint8_t)kMapMarkerCount; ++i) {
        uint8_t state = (uint8_t)kMapMarkerHidden;

        if (shown != 0U) {
            state = i < map_unlocked ? (uint8_t)kMapMarkerCleared : (uint8_t)kMapMarkerOpen;
        }
        map_art_marker(i, state);
    }
}

// a bordered card's top or bottom edge, out of the glyph run's three pieces: a bracket at each end,
// y-flipped for the bottom so one tile serves all four corners, an h-edge filling the run between
static void map_draw_border(uint8_t row, uint8_t flip_y, uint8_t left, uint8_t width) {
    const uint8_t fy = flip_y != 0U ? (uint8_t)kCamAttrYFlip : 0U;
    const uint8_t last = (uint8_t)(left + width - 1U);
    uint8_t x;

    map_art_border(left, row, kMapBorderCorner, fy);
    for (x = (uint8_t)(left + 1U); x < last; ++x) {
        map_art_border(x, row, kMapBorderHEdge, fy);
    }
    map_art_border(last, row, kMapBorderCorner, (uint8_t)(fy | kCamAttrXFlip));
}

static void map_draw_sides(uint8_t row, uint8_t left, uint8_t width) {
    map_art_border(left, row, kMapBorderVEdge, 0);
    map_art_border((uint8_t)(left + width - 1U), row, kMapBorderVEdge, kCamAttrXFlip);
}

// the world-two card: a bordered panel over the strip, its interior blanked to the font's space
// glyph under map_art's white-on-band palette so the art below cannot tint through, with the call
// to action's row banded gold the way the reference's own prompts are
static void map_draw_popup(void) {
    uint8_t row;

    map_draw_border(kMapPopupTopRow, 0, kMapPopupLeftCol, kMapPopupWidth);
    for (row = (uint8_t)(kMapPopupTopRow + 1U); row < kMapPopupBottomRow; ++row) {
        const uint8_t pal = row == (uint8_t)kMapPopupPressRow ? (uint8_t)kMapPalHilite : (uint8_t)kMapPalText;

        map_art_blank((uint8_t)(kMapPopupLeftCol + 1U), row, (uint8_t)(kMapPopupWidth - 2U), pal);
        map_draw_sides(row, kMapPopupLeftCol, kMapPopupWidth);
    }
    map_draw_border(kMapPopupBottomRow, 1, kMapPopupLeftCol, kMapPopupWidth);
    card_print_centered(kMapPopupWorldRow, map_popup_line1);
    card_print_centered(kMapPopupWayRow, map_popup_line2);
    card_print_centered(kMapPopupPressRow, map_popup_line3);
}

// undoes every cell the card touched. it spans the strip's rows plus the footer's first padding
// row, and every one of those is generated art, so the whole block goes back from the same map and
// attribute tables map_art_load painted it from
static void map_draw_popup_hide(void) {
    map_art_rows(kMapPopupTopRow, (uint8_t)(kMapPopupBottomRow - kMapPopupTopRow + 1U));
    map_draw_markers(1);
    draw_mario();
}

static void map_reset(uint8_t node) {
    uint8_t i;

    map_node = (node < (uint8_t)kLevelCount) ? node : (uint8_t)(kLevelCount - 1U);
    if (map_node > map_unlocked) {
        map_node = map_unlocked;
    }
    map_target = map_node;
    map_walking = 0;
    map_step = 0;
    map_facing_left = 0;
    map_anim = kFrameIdle;
    map_anim_timer = 0;
    map_water_timer = 0;
    map_water_frame = 0;

    // the whole bg map, its palettes and a tile bank are rewritten here, far more vram traffic
    // than a vblank holds
    DISPLAY_OFF;
    HIDE_SPRITES;
    terrain_park_scroll();
    // the level's own sprite set first: its palette loader would otherwise walk over the three obj
    // slots map_art_load hands the markers
    assets_load_sprite_tiles();
    assets_load_sprite_palettes();
    map_art_load();
    SPRITES_8x16;
    // a level left its own oam behind - mario's lower row, the five hud digits, items, enemies -
    // and SHOW_SPRITES below would put every one of them back on screen. this screen draws ten
    // (mario and the four markers' body/rim pairs) and owns all forty
    for (i = 0; i < (uint8_t)kOamSlots; ++i) {
        move_sprite(i, 0, 0);
    }
    font_color(kFontFore, kFontBack);
    map_art_world(map_node);
    map_art_lives(hud_lives);
    map_art_clear_list(map_unlocked);
    map_draw_markers(1);
    draw_mario();
    // world one is done: every visit to the map from here on gets the card, until it is dismissed.
    // it covers mario's own walk row and three of the four markers, so they are parked off screen
    // the same way the level's leftover oam was above
    map_popup = (uint8_t)(map_unlocked >= (uint8_t)kLevelCount);
    if (map_popup != 0U) {
        // the literal text lives in bank 6 (states.c's map_popup_load); bank 5 is full - see the
        // comment on map_popup_line1 in mapscreen.h
        map_popup_load();
        map_draw_popup();
        move_sprite(kSpriteMarioL, 0, 0);
        move_sprite(kSpriteMarioR, 0, 0);
        map_draw_markers(0);
    }
    SHOW_BKG;
    SHOW_SPRITES;
    DISPLAY_ON;
    lock_begin();
}

static uint8_t map_frame(uint8_t pressed, uint8_t* level) {
    // main.c calls this on the first line of vblank, so the cycle's six tiles land with room to
    // spare - the water drifts whatever else the screen is doing, card included
    if (++map_water_timer >= (uint8_t)kMapWaterTicks) {
        map_water_timer = 0;
        map_water_frame = (uint8_t)((map_water_frame + 1U) % kMapWaterFrameCount);
        map_art_animate(map_water_frame);
    }
    // the popup eats every button until it is dismissed - lock_gate already stripped the edge that
    // was still held from whatever closed the clear card, so this can only fire on a fresh press.
    // map_draw_popup_hide rebuilds everything the card covered - no DISPLAY_OFF needed, the same
    // mid-frame vram write style card_clear_refresh already uses to tick the clear card's timer
    // while the display stays on
    if (map_popup != 0U) {
        if ((pressed & (uint8_t)(J_A | J_START | J_B)) != 0U) {
            map_popup = 0;
            map_draw_popup_hide();
        }
        return kMapStay;
    }
    if (map_walking != 0U) {
        map_step = (uint8_t)(map_step + kMapWalkPx);
        if (map_step >= node_span(map_node, map_target)) {
            map_walking = 0;
            map_node = map_target;
            map_step = 0;
            map_anim = kFrameIdle;
        } else if (++map_anim_timer >= (uint8_t)kMapWalkAnimFrames) {
            map_anim_timer = 0;
            // the same three-frame cycle the level walk uses, one step every kMapWalkAnimFrames
            map_anim = (uint8_t)(kFrameWalk0 + (map_anim + 1U - kFrameWalk0) % kWalkFrameCount);
        }
        draw_mario();
        return kMapStay;
    }
    if ((pressed & J_A) != 0U) {
        *level = map_node;
        return kMapPlay;
    }
    if ((pressed & (J_START | J_B)) != 0U) {
        return kMapBack;
    }
    // a node past the file's furthest is not walkable at all: the path simply refuses, which is
    // what the ring still sitting over it has already said
    if ((pressed & J_RIGHT) != 0U && map_node + 1U < (uint8_t)kLevelCount && map_node + 1U <= map_unlocked) {
        map_target = (uint8_t)(map_node + 1U);
        map_facing_left = 0;
    } else if ((pressed & J_LEFT) != 0U && map_node != 0U) {
        map_target = (uint8_t)(map_node - 1U);
        map_facing_left = 1;
    } else {
        return kMapStay;
    }
    map_walking = 1;
    map_step = 0;
    map_anim = kFrameWalk0;
    map_anim_timer = 0;
    // the label answers for wherever the walk is headed, updated the instant it starts rather than
    // once he arrives - the reference's own header reacts on the first step, not the last
    map_art_world(map_target);
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

void front_map(uint8_t level) BANKED {
    // quitting out of a level, so nothing is opened and nothing is recorded - but a run that
    // started from the title's own level select never opened a file at all, and its map_unlocked
    // is still 0: the node he is standing on has to be walkable, so it counts as reached
    if (level > map_unlocked) {
        map_unlocked = level;
    }
    screen = kScreenMap;
    map_reset(level);
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
