#ifndef ASSETS_H
#define ASSETS_H

#include "mario.h"

#include <gb/gb.h>
#include <stdint.h>

// every tile and palette this game owns lives in games/mario/src/assets_data.c, banked beside the
// enemy code: m7's super mario alone is 512 bytes of art and bank 0 was already full. nothing but
// the loaders below reaches any of it, and each of them runs with the lcd off at a level load

// loads every terrain tile into vram bank 0 at its pinned id: the 0xa0-0xbf block plus the eight
// ids past mario's last sprite frame
void assets_load_bg_tiles(void) BANKED;

// and the castle's own override of that: 1-4's floors, ceilings and walls are the grey masonry of
// kCastleBrickUpperTile/kCastleBrickLowerTile, not the overworld's grass-capped ground, so a castle
// load redraws the six ids of the ground family with whichever course belongs in each quadrant.
// call it after assets_load_bg_tiles, and only for a castle - the plain loader above puts the grass
// back
void assets_load_bg_tiles_castle(void) BANKED;

// and the scenery - the castle, the flag's ball and pennant, the clouds, hills and bushes - into
// vram BANK 1 at 0x20-0x5d. those ids are the font's own glyphs in bank 0 and collide with nothing:
// a cgb bg map attribute picks a tile's bank per cell, and every scenery kind's kBlockPalette entry
// carries kCamAttrVram1. terrain_init calls this beside the loader above
void assets_load_scenery_tiles(void) BANKED;

// loads the eight cgb bg palettes the terrain streamer tags cells with (see kCamPal* in mario.h)
void assets_load_bg_palettes(void) BANKED;

// the same eight slots, darkened and blue-tinted for a pipe sub-area or an underground level, and
// drained to stone with a lava ramp for a castle; bcpd is mode-locked on real hardware, so every
// one of these runs with the lcd off
void assets_load_bg_palettes_underground(void) BANKED;
void assets_load_bg_palettes_castle(void) BANKED;

// loads small mario's six 16x16 frames at 0xe0 and super mario's 32 tiles at 0x60, then the warm
// and fire cgb sprite palettes
void assets_load_sprite_tiles(void) BANKED;
void assets_load_sprite_palettes(void) BANKED;

// loads the item frames at 0xd0 (mushroom, star, 1-up, coin pop, fireball) plus the flower at 0x80,
// then one cgb sprite palette per item kind
void assets_load_item_tiles(void) BANKED;
void assets_load_item_palettes(void) BANKED;

// loads the enemy frames at 0xc0 and the goomba/koopa cgb sprite palettes
void assets_load_enemy_tiles(void) BANKED;
void assets_load_enemy_palettes(void) BANKED;

// m8a's piranha/flame/lift/bowser pairs at 0x84, and the castle re-tint that turns the koopa's
// palette into the fake bowser's
void assets_load_hazard_tiles(void) BANKED;
void assets_load_enemy_palettes_castle(void) BANKED;

// the hud row's glyphs: the resident ibm font re-encoded as white ink on a transparent cell into
// vram bank 1 at kTileHudDigitFirst, plus the row's one gold coin tile. no palette loader beside
// it - the row borrows kCamPalSky, whose color 0 is the level's own backdrop and whose color 1 is
// white in all three level sets
void assets_load_hud_font(void) BANKED;

// the world map screen's own new tile kinds, all in vram bank 1 at ids the scenery run (0x20-0x5d)
// never claimed: water and path, a round node marker (one quadrant tile, stamped four times with
// the cgb flip bits the way a mirrored hill/bush kind already is), and the CLEAR LIST panel's own
// border/checkbox art - a corner and two straight edges, each reused by flip for the other three
// corners/sides, plus a hollow and a filled cell. assets_load_map_tiles loads the pixel data; the
// ids are public so mapscreen.c can stamp bg quads out of them the way put_block stamps a reused
// kind
#define kTileMapWaterTop 0x60U
#define kTileMapWaterBody 0x61U
#define kTileMapPathTop 0x62U
#define kTileMapPathBody 0x63U
// the round marker's one quadrant, circularly symmetric so put_marker gets the other three by
// x/y-flipping this same tile rather than storing them separately
#define kTileMapMarker 0x64U
#define kTileMapListCorner 0x65U
#define kTileMapListHEdge 0x66U
#define kTileMapListVEdge 0x67U
#define kTileMapListCellEmpty 0x68U
#define kTileMapListCellFilled 0x69U
// the map strip's own foliage, replacing the level's 45-degree hill slopes (a first pass tried
// rounded standing clumps and still read as teardrop-shaped creatures with two eyes) with a low
// hedge row: three flat-topped mounds, each wider than it is tall (a top+base pair, the right
// half of the 16x16 quad the mirror of the left the same way a hill peak's right slope is), sized
// tall/medium/low so a run of them reads as one bumpy, uneven canopy rather than a repeated
// silhouette, each meeting the sand path with a scalloped fringe instead of a ruled line. hedges
// scatter across both the strip's block rows, offset from row to row, so the green band reads as
// dense wooded terrain rather than a thin fringe of bumps under a blank field. that field itself -
// the ground showing between hedges - is a lightly speckled top/base pair, not a flat single tile:
// two different speck scatters, so a block's top half never mirrors its own base the way a single
// reused tile would, and (drawn mirrored through put_dome_quad, same as the hedges) a long run of
// it does not tile into a visible grid. see put_dome_quad in mapscreen.c
#define kTileMapHedgeTallTop 0x6AU
#define kTileMapHedgeTallBase 0x6BU
#define kTileMapHedgeMedTop 0x6CU
#define kTileMapHedgeMedBase 0x6DU
#define kTileMapHedgeLowTop 0x6EU
#define kTileMapHedgeLowBase 0x6FU
#define kTileMapFieldFillTop 0x70U
#define kTileMapFieldFillBase 0x71U
// and the strip's own castle icon, which no longer reuses the level's five castle block kinds: a
// flat 2x4 wall of 8px bricks with a door in one corner and a window in the other read as a slab,
// not as a castle. this is one drawn 4x6 tile icon instead - a narrow crenellated tower over a
// wider crenellated keep, a centered arched door standing on the path row and an arched window
// each side of it - and only its left two tile columns are stored: the right two are these
// mirrored with the cgb x-flip bit, the same trick every other mirrored kind here uses. these ten
// take the bottom of vram bank 1, which nothing else in the game has ever loaded (the scenery run
// starts at 0x20 and the map's own kinds above at 0x60), and they are drawn under the map's brick
// palette, whose color 3 is the black the door and windows want. see map_draw_castle in mapscreen.c
#define kTileMapCastleTowerTop 0x00U
#define kTileMapCastleTowerWall 0x01U
#define kTileMapCastleMerlon 0x02U
#define kTileMapCastleTowerBase 0x03U
#define kTileMapCastleWallTop 0x04U
#define kTileMapCastleCornice 0x05U
#define kTileMapCastleWallMid 0x06U
#define kTileMapCastleArch 0x07U
#define kTileMapCastleWallFoot 0x08U
#define kTileMapCastleDoor 0x09U
void assets_load_map_tiles(void) BANKED;

// the map screen's own eight cgb bg palettes - a card screen, never up during play, so it does not
// share the level's. see the comment on the function body for which of the level's eight slots
// each map kind now colors
void assets_load_map_bg_palettes(void) BANKED;

// stages the six per-kind tables below into ram. m18 took the block kinds from eighteen to
// thirty-eight and six tables of that length is 228 bytes bank 0 does not have, so they ride in
// the asset bank and are copied out at a level load - the same trade level.c makes for the level
// table. terrain_init calls this before it streams a single column
void assets_load_block_tables(void) BANKED;

// per block-kind tile ids, indexed by the kBlock* constants in mario.h; one entry per 2x2 corner.
// the streamer reads these on every column it paints, so they live in ram: a plain load, no bank
// switch behind it. read-only in practice - only assets_load_block_tables writes them
extern uint8_t kBlockTileTl[kBlockKindCount];
extern uint8_t kBlockTileTr[kBlockKindCount];
extern uint8_t kBlockTileBl[kBlockKindCount];
extern uint8_t kBlockTileBr[kBlockKindCount];
// the cgb bg map attribute byte per block kind: its palette slot, plus kCamAttrXFlip on the four
// kinds that are another kind's mirror image
extern uint8_t kBlockPalette[kBlockKindCount];

// what a block kind is to a body standing on it: kFloorSolid, kFloorThin, or neither. m8a's four
// new kinds turned terrain_solid_at's compare chain into six tests on a path the engine walks
// twenty times a frame, so the answer is one indexed load again
#define kFloorSolid 1U
#define kFloorThin 2U
extern uint8_t kBlockFloor[kBlockKindCount];

#endif
