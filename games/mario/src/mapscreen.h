#ifndef MAPSCREEN_H
#define MAPSCREEN_H

#include <gb/gb.h>
#include <stdint.h>

// m19's front end: the title card, the three-slot SELECT FILE screen and world one's map, driven
// as one banked state machine so bank 0 carries a single block for all three rather than one each.
// none of it can happen inside a frame of play, so it rides in bank 5 beside the title card whose
// drawing machinery it reuses (see the card_* block in title.h)

// what one front-end frame decided; the game loop owns the lcd-off level load either answer needs
#define kFrontStay 0U
#define kFrontPlay 1U
#define kFrontCamera 2U

// paints the title card and lets go of any file that was picked: the boot path, and the one a
// game over takes
void front_title(void) BANKED;

// a level was just cleared: opens the node after it, writes the node to stand on back through
// `level` (clamped to the last one when world one is finished) and paints the map
void front_cleared(uint8_t* level) BANKED;

// back to the world map from a level the player quit out of: nothing is opened and nothing is
// recorded, so his lives and score stand and the node he left is the one he is put back on
void front_map(uint8_t level) BANKED;

// one frame of whichever of the three screens is up; a level it enters is written through `level`
uint8_t front_frame(uint8_t pressed, uint8_t* level) BANKED;

// the "world 2 is on its way" popup's own two lines, filled in by states.c's map_popup_load
// (bank 6): bank 5 has no room left for the literals themselves, but plain ram reads correctly
// under any rom bank, so a bank-5 card_print_centered can print straight out of these
#define kMapPopupLineWidth 12U
extern char map_popup_line1[kMapPopupLineWidth];
extern char map_popup_line2[kMapPopupLineWidth];

#endif
