#ifndef MAP_ART_H
#define MAP_ART_H

#include <gb/gb.h>
#include <stdint.h>

// the smbd world one map frame: 71 bank-1 bg tiles for the whole 20x18 screen, thirteen glyph
// tiles behind it for the runtime digits and the world-two card's border, six the lives readout
// rewrites, four clear-list variants, and six sprite tiles for the node markers. runs with the lcd
// off - far more vram traffic than a vblank holds
void map_art_load(void) BANKED;

// the header's "1-N", either side of the dash the art carries
void map_art_world(uint8_t level) BANKED;

// the footer's lives readout: one digit under ten, two past it. the reference's own readout sits
// half a tile off the grid in both axes, so a digit ships as four pre-shifted quadrants written
// into six tile ids of the readout's own rather than a glyph dropped into a cell
void map_art_lives(uint8_t lives) BANKED;

// the clear list: world one's row of four dashes, solid for every level behind `cleared` and the
// frame's own hollow dash for the rest
void map_art_clear_list(uint8_t cleared) BANKED;

// one of four sparkle frames into the two water tiles the strip shares
void map_art_animate(uint8_t frame) BANKED;

// one node's marker, in oam slots the map owns. two sprites per node: a marker is a solid 8x8 in
// four colors, one more than an obj palette's three opaque slots, so a body carrying the outline,
// the fill and the gloss rides over a rim that carries only its orange edge
void map_art_marker(uint8_t node, uint8_t state) BANKED;

// one bordered cell of the world-two card, out of the same glyph run. `flip` is the cgb attribute
// bits that turn the top-left bracket into whichever of the four corners this one is
void map_art_border(uint8_t col, uint8_t row, uint8_t glyph, uint8_t flip) BANKED;

// `width` blank cells under one palette: the card's own interior, and the footer row its bottom
// border sits on
void map_art_blank(uint8_t col, uint8_t row, uint8_t width, uint8_t palette) BANKED;

// puts `rows` whole rows of the frame back, the way the card found them
void map_art_rows(uint8_t row, uint8_t rows) BANKED;

// the water's own cycle: four sparkle frames, one every kMapWaterTicks frames. the frames are
// designed rather than ripped - neither reference holds a second frame of this map's water
#define kMapWaterFrameCount 4U
#define kMapWaterTicks 8U

#define kMapMarkerCleared 0U
#define kMapMarkerOpen 1U
#define kMapMarkerHidden 2U

// the glyph run's own three border pieces, as offsets past the ten digits
#define kMapBorderCorner 10U
#define kMapBorderHEdge 11U
#define kMapBorderVEdge 12U

// the two cgb bg palettes map_art appends past the six the screen plans: white on the band's own
// near-black for every runtime glyph and the card's text, and a gold band for its call to action
#define kMapPalText 6U
#define kMapPalHilite 7U

// the four node cells, measured off the reference frame: each is exactly one 8x8, and 1-4's is the
// castle door itself. the tables are macros rather than a banked const array so both bank 5 and
// bank 7 can hold their own copy - a const array only reads right under its own rom bank
#define kMapNodeCount 4U
#define kMapNodeXs {8U, 48U, 88U, 136U}
#define kMapNodeYs {64U, 56U, 72U, 64U}
// only the three path nodes carry a marker: the reference rings none over its castle, which is
// 1-4 all by itself
#define kMapMarkerCount 3U

// mario's 16x16 stands centred on the 8 px node cell with his feet on its bottom edge
#define kMapMarioDx 4U
#define kMapMarioDy 8U

// the header's two digit cells, either side of the art's dash
#define kMapDigitRow 2U
#define kMapWorldDigitCol 3U
#define kMapLevelDigitCol 5U
// the lives readout's top-left cell; the block runs one cell right and one down from there, and
// one further right again once lives reaches ten
#define kMapFooterRow 15U
#define kMapLivesDigitCol 10U
// world one's row of clear-list dashes, one dash per tile cell
#define kMapListRow 14U
#define kMapListFirstCol 15U

#endif
