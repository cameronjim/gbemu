#ifndef TITLE_ART_H
#define TITLE_ART_H

#include <gb/gb.h>
#include <stdint.h>

// the smbd title frame: 143 bank-1 bg tiles with their seven cgb palettes, and the gold "Deluxe"
// script's 40 sprite tiles. runs with the lcd off - it is far more vram traffic than a vblank holds
void title_art_load(void) BANKED;

// the twenty 8x16 sprites of the script and its sparkle, into oam slots 0-19
void title_art_place_sprites(void) BANKED;

// the two-sprite sparkle over the wordmark, blinked by title_frame
void title_art_sparkle(uint8_t on) BANKED;

// parks all forty oam slots off screen for the debug exits that skip the map screen
void title_art_park_sprites(void) BANKED;

#endif
