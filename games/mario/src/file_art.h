#ifndef FILE_ART_H
#define FILE_ART_H

#include <gb/gb.h>
#include <stdint.h>

// the smbd file select frame: 31 bank-1 bg tiles plus the eight label glyphs, their palettes, and
// the 20x18 map. runs with the lcd off - it is far more vram traffic than a vblank holds
void file_art_load(void) BANKED;

// one slot's four label cells: "NEW" when level_or_new is 0, else "W1*N" with N that level number.
// seven cells inside a vblank, so this one is safe with the lcd on
void file_art_label(uint8_t slot, uint8_t level_or_new) BANKED;

// mario's 16 px column is centred on the pipe he stands over; his feet meet the lip at kFileStandY
#define kFileMarioX 24U
#define kFileMarioStep 48U
#define kFileStandY 64U

// the arc between two pipes: linear in x, parabolic in y, peaking this far above the lip
#define kFileHopFrames 20U
#define kFileHopPeak 24U

#endif
