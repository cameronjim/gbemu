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
#include <gbdk/console.h>
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

// the map strip's backdrop, four block rows (local 0-3, absolute kMapBandFirstRow+0..3) of ten
// columns. every cell is landscape - no cell is left as the band's plain sky-blue backdrop, so no
// sky shows between the black header and the path; the only blue in the strip is water. kBlockEmpty
// is never used here any more (see below); every other value under kBlockKindCount is a kind the
// level itself draws (reused straight through put_block); the four sentinels past it are this
// screen's own new art added by put_new_quad/put_dome_quad - water and path, then the low hedge row
// that replaced the level's 45-degree hill slopes (see kTileMapHedgeTallTop in assets.h): three
// flat-topped mounds at three heights, plus a plain field fill (itself lightly textured, not a flat
// tone - see kFoliageTiles in assets_data.c) for the ground between them. the top block row is not
// left as an unbroken field rectangle: it carries its own scatter of hedges at offset columns and
// heights from the row below, so the two rows together read as dense, uneven foliage rather than a
// thin fringe of bumps under a blank green wall. row by row: a scatter of hedges and field over a
// pipe's lip and the castle's crenels, filling the strip's top edge end to end, offset from the row
// beneath so no column stacks the same height twice; the lower hedge row itself (tall, low, medium -
// no two alike side by side), a bush and the pipe's body under the castle's window; the path mario
// and the four markers stand on, running under the castle's door; and, below that, a low hedge at
// the pond's bank, more path, and the castle's wall
#define W (uint8_t)(kBlockKindCount)
#define P (uint8_t)(kBlockKindCount + 1U)
#define N (uint8_t)(kBlockKindCount + 2U) // plain field fill, textured but shapeless
#define T (uint8_t)(kBlockKindCount + 3U) // hedge, tall
#define E (uint8_t)(kBlockKindCount + 4U) // hedge, medium
#define L (uint8_t)(kBlockKindCount + 5U) // hedge, low
static const uint8_t kMapRows[kMapBandBlockRows][kMapBlockCols] = {
    {E, N, L, T, N, E, kBlockPipeTl, kBlockPipeTr, kBlockCastleCrenel, kBlockCastleCrenel},
    {T, L, E, kBlockBushL, kBlockBushM, kBlockBushR, kBlockPipeBodyL, kBlockPipeBodyR,
     kBlockCastleDoorTop, kBlockCastleWindow},
    {P, P, P, P, P, P, P, P, kBlockCastleDoor, kBlockCastle},
    {L, W, W, P, P, P, P, W, kBlockCastle, kBlockCastle},
};
#undef W
#undef P
#undef N
#undef T
#undef E
#undef L

static uint8_t node_column(uint8_t node) {
    return (uint8_t)(kMapNodeFirstCol + node * kMapNodeStepCol);
}

// three legible states, one round marker shape colored three ways: gray and settled for a level
// that is done, bright and gold for the one still to do, and the castle's own brown for one world
// one has not opened yet - the same three cgb slots the old square markers used (kCamPalSpent,
// kCamPalQuestion, kCamPalBrick), so only the shape changed
static uint8_t marker_palette(uint8_t node) {
    if (node < map_unlocked) {
        return (uint8_t)kCamPalSpent;
    }
    return (node == map_unlocked) ? (uint8_t)kCamPalQuestion : (uint8_t)kCamPalBrick;
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

// the same 16x16 shape as put_block, for the map's own two new kinds: `top` fills the upper tile
// row (water's foam, path's grass edge), `body` the lower (plain water, plain sand), both under one
// attribute that carries kCamAttrVram1 - this screen's new tiles live in bank 1, same as scenery
static void put_new_quad(uint8_t bx, uint8_t by, uint8_t top, uint8_t body, uint8_t attr) {
    uint8_t tiles[2];
    uint8_t attrp[2];
    const uint8_t x = (uint8_t)(bx * kTilesPerBlock);
    const uint8_t y = (uint8_t)(by * kTilesPerBlock);

    attrp[0] = attr;
    attrp[1] = attr;
    tiles[0] = top;
    tiles[1] = top;
    set_bkg_tiles(x, y, 2, 1, tiles);
    set_bkg_attributes(x, y, 2, 1, attrp);
    tiles[0] = body;
    tiles[1] = body;
    set_bkg_tiles(x, (uint8_t)(y + 1U), 2, 1, tiles);
    set_bkg_attributes(x, (uint8_t)(y + 1U), 2, 1, attrp);
}

// the same 16x16 shape again, for the map's hedge kinds: unlike put_new_quad's water/path/field
// (the same tile stamped at both left and right), a hedge mound's silhouette is not left-right
// symmetric per tile, so the right half is the left tile mirrored with the cgb x-flip bit - the
// same trick a mirrored hill slope or bush cap already uses for its other side
static void put_dome_quad(uint8_t bx, uint8_t by, uint8_t top, uint8_t base, uint8_t attr) {
    uint8_t tiles[2];
    uint8_t attrp[2];
    const uint8_t x = (uint8_t)(bx * kTilesPerBlock);
    const uint8_t y = (uint8_t)(by * kTilesPerBlock);

    attrp[0] = attr;
    attrp[1] = (uint8_t)(attr | kCamAttrXFlip);
    tiles[0] = top;
    tiles[1] = top;
    set_bkg_tiles(x, y, 2, 1, tiles);
    set_bkg_attributes(x, y, 2, 1, attrp);
    tiles[0] = base;
    tiles[1] = base;
    set_bkg_tiles(x, (uint8_t)(y + 1U), 2, 1, tiles);
    set_bkg_attributes(x, (uint8_t)(y + 1U), 2, 1, attrp);
}

static void put_cell(uint8_t bx, uint8_t by, uint8_t kind) {
    const uint8_t foliage_attr = (uint8_t)(kCamPalPipe | kCamAttrVram1);

    if (kind == (uint8_t)kBlockEmpty) {
        return; // the band paint underneath is already this cell's answer
    }
    if (kind < (uint8_t)kBlockKindCount) {
        put_block(bx, by, kind);
    } else if (kind == (uint8_t)kBlockKindCount) {
        put_new_quad(bx, by, kTileMapWaterTop, kTileMapWaterBody,
                     (uint8_t)(kCamPalNeutral | kCamAttrVram1));
    } else if (kind == (uint8_t)(kBlockKindCount + 1U)) {
        put_new_quad(bx, by, kTileMapPathTop, kTileMapPathBody,
                     (uint8_t)(kCamPalCoin | kCamAttrVram1));
    } else if (kind == (uint8_t)(kBlockKindCount + 2U)) {
        // x-flipped the same way the hedges are, and top/base are two different speck scatters
        // (not the same tile twice): between the mirror and the mismatched halves, a run of field
        // cells does not line every speckle up into an obvious grid
        put_dome_quad(bx, by, kTileMapFieldFillTop, kTileMapFieldFillBase, foliage_attr);
    } else if (kind == (uint8_t)(kBlockKindCount + 3U)) {
        put_dome_quad(bx, by, kTileMapHedgeTallTop, kTileMapHedgeTallBase, foliage_attr);
    } else if (kind == (uint8_t)(kBlockKindCount + 4U)) {
        put_dome_quad(bx, by, kTileMapHedgeMedTop, kTileMapHedgeMedBase, foliage_attr);
    } else {
        put_dome_quad(bx, by, kTileMapHedgeLowTop, kTileMapHedgeLowBase, foliage_attr);
    }
}

// a round stop on the road, not a square block: one quadrant tile stamped four times with the cgb
// flip bits, the same trick a mirrored hill slope or bush cap already uses for its other half. the
// path drawn under it is left in place (put_cell already ran for this cell), so the marker reads as
// sitting on the road rather than replacing a chunk of it - no more of the old square block's black
// underside floating over the water where nothing sits below it to hide the seam
static void put_marker(uint8_t bx, uint8_t by, uint8_t pal) {
    uint8_t tiles[2];
    uint8_t attr[2];
    const uint8_t x = (uint8_t)(bx * kTilesPerBlock);
    const uint8_t y = (uint8_t)(by * kTilesPerBlock);
    const uint8_t base = (uint8_t)(pal | kCamAttrVram1);

    tiles[0] = kTileMapMarker;
    tiles[1] = kTileMapMarker;
    attr[0] = base;
    attr[1] = (uint8_t)(base | kCamAttrXFlip);
    set_bkg_tiles(x, y, 2, 1, tiles);
    set_bkg_attributes(x, y, 2, 1, attr);
    attr[0] = (uint8_t)(base | kCamAttrYFlip);
    attr[1] = (uint8_t)(base | kCamAttrXFlip | kCamAttrYFlip);
    set_bkg_tiles(x, (uint8_t)(y + 1U), 2, 1, tiles);
    set_bkg_attributes(x, (uint8_t)(y + 1U), 2, 1, attr);
}

// left-aligned text, the console cursor moved first; title.c's own helpers are all centered or
// value-suffixed, neither of which the footer's two side-by-side pieces want
static void puts_at(uint8_t x, uint8_t y, const char* text) {
    uint8_t i;

    gotoxy(x, y);
    for (i = 0; text[i] != '\0'; ++i) {
        putchar(text[i]);
    }
}

// "WORLD" then "1-N", centered the way every other card banner is, tracking whichever node is
// current - the cursor's target the instant a walk starts, so the label answers "what am I about
// to enter" rather than lagging a whole walk behind
static void map_draw_world_label(uint8_t node) {
    const uint8_t lvl = (node < (uint8_t)kLevelCount) ? node : (uint8_t)(kLevelCount - 1U);

    puts_at((uint8_t)((kScreenCols - 5U) / 2U), kMapWorldRow, "WORLD");
    gotoxy((uint8_t)((kScreenCols - 3U) / 2U), kMapLevelRow);
    putchar('1');
    putchar('-');
    putchar((char)('1' + lvl));
}

// "MARIO x NN". a host probe finds mario anywhere on screen by his tile family alone (any sprite
// drawn from kTileMarioFirst's range, not just the walking figure's own oam slots), which a second
// mario-shaped icon sprite here would feed into and break every test that reads his position off
// the map - so the lives readout stays plain text, the word rather than the little head
static void map_draw_lives(void) {
    uint8_t digits[2];

    hud_split(hud_lives, digits, 2);
    puts_at(kMapLivesTextCol, kMapLivesTextRow, "LIVES");
    gotoxy(kMapLivesTextCol, (uint8_t)(kMapLivesTextRow + 1U));
    putchar('x');
    putchar((char)('0' + digits[0]));
    putchar((char)('0' + digits[1]));
}

// the CLEAR LIST panel: drawn tiles rather than font punctuation, the way every other piece of
// this screen is - a corner (reused by flip for the other three) and a straight edge for the top/
// bottom and another for the sides, then a hollow or filled square for each of the four cells.
// every one of these lives in bank 1 alongside the marker (see kTileMapListCorner in assets.h) and
// is drawn under the header/footer's own black-band slot, kCamPalSky, the same white-on-black the
// font itself uses here
#define kMapListAttr (uint8_t)(kCamPalSky | kCamAttrVram1)

static void put_tile(uint8_t x, uint8_t y, uint8_t tile, uint8_t attr) {
    set_bkg_tiles(x, y, 1, 1, &tile);
    set_bkg_attributes(x, y, 1, 1, &attr);
}

// the panel's top or bottom edge: a corner at each end, y-flipped for the bottom so the same
// bracket tile serves all four corners, an h-edge tile filling the run between them
static void map_draw_border(uint8_t row, uint8_t flip_y) {
    uint8_t x;
    const uint8_t attr_l = (uint8_t)(kMapListAttr | (flip_y != 0U ? kCamAttrYFlip : 0U));
    const uint8_t attr_r = (uint8_t)(attr_l | kCamAttrXFlip);
    const uint8_t last = (uint8_t)(kMapListLeftCol + kMapListWidth - 1U);

    put_tile(kMapListLeftCol, row, kTileMapListCorner, attr_l);
    for (x = (uint8_t)(kMapListLeftCol + 1U); x < last; ++x) {
        put_tile(x, row, kTileMapListHEdge, attr_l);
    }
    put_tile(last, row, kTileMapListCorner, attr_r);
}

// one row's worth of the panel's left/right sides, between the top and bottom borders
static void map_draw_sides(uint8_t row) {
    const uint8_t last = (uint8_t)(kMapListLeftCol + kMapListWidth - 1U);

    put_tile(kMapListLeftCol, row, kTileMapListVEdge, kMapListAttr);
    put_tile(last, row, kTileMapListVEdge, (uint8_t)(kMapListAttr | kCamAttrXFlip));
}

// the four cells, one per level of world one: filled once its node is behind the file's furthest,
// hollow while it is still ahead. no save-format change - world one is strictly linear, so "node i
// is cleared" is already exactly what map_unlocked answers (see the comment on kSaveSlots)
static void map_draw_list(uint8_t furthest) {
    uint8_t i;

    map_draw_border(kMapListTopRow, 0);
    map_draw_sides(kMapListHeadRow);
    map_draw_sides(kMapListCellsRow);
    map_draw_border(kMapListBottomRow, 1);
    // the label is still plain font text - a genuine SMB Deluxe UI element spelled out, which the
    // font already draws cleanly; only the border and the cells read as ascii art drawn that way
    puts_at((uint8_t)(kMapListLeftCol + 1U), kMapListHeadRow, "CLEAR LIST");
    for (i = 0; i < (uint8_t)kLevelCount; ++i) {
        const uint8_t col = (uint8_t)(kMapListLeftCol + 2U + i * 3U);
        const uint8_t tile = i < furthest ? (uint8_t)kTileMapListCellFilled : (uint8_t)kTileMapListCellEmpty;
        put_tile(col, kMapListCellsRow, tile, kMapListAttr);
    }
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
    assets_load_map_tiles();
    // a card screen, never up during play: its own eight bg palettes, not the level's - see the
    // comment on the function body for why sharing them left level-blue rectangles behind
    assets_load_map_bg_palettes();
    assets_load_sprite_tiles();
    assets_load_sprite_palettes();
    SPRITES_8x16;
    // card_clear_map blanks all 32x32 cells to the font's space glyph under palette 0 (this
    // screen's black band slot), so the header and footer bands are already answered; only the
    // map strip's rows need repainting to the band's sky-blue before the strip itself is drawn
    card_clear_map();
    font_color(kFontFore, kFontBack);
    card_paint_band((uint8_t)(kMapBandFirstRow * kTilesPerBlock),
                     (uint8_t)(kMapBandBlockRows * kTilesPerBlock), kCamPalGround);
    // the level left its own oam behind - mario's lower row, the five hud digits, items, enemies -
    // and SHOW_SPRITES below would put every one of them back on screen. the map draws four sprites
    // (mario, and the lives icon) and owns all forty, so the rest are parked above the visible area
    for (bx = 0; bx < (uint8_t)kOamSlots; ++bx) {
        move_sprite(bx, 0, 0);
    }
    for (by = 0; by < (uint8_t)kMapBandBlockRows; ++by) {
        for (bx = 0; bx < (uint8_t)kMapBlockCols; ++bx) {
            put_cell(bx, (uint8_t)(kMapBandFirstRow + by), kMapRows[by][bx]);
        }
    }
    for (bx = 0; bx < (uint8_t)kLevelCount; ++bx) {
        put_marker(node_column(bx), kMapMarkerRow, marker_palette(bx));
    }
    map_draw_world_label(map_node);
    map_draw_lives();
    map_draw_list(map_unlocked);
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
    // the label answers for wherever the walk is headed, updated the instant it starts rather than
    // once he arrives - the reference's own header reacts on the first step, not the last
    map_draw_world_label(map_target);
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
