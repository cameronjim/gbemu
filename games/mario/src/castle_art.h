#ifndef MARIO_CASTLE_ART_H
#define MARIO_CASTLE_ART_H

#include <gb/gb.h>

// loads every tile a castle draws that no other level type does, all of it cut off the smbd 1-4
// capture (games/mario/tools/rip_tiles.py --level 1-4): the masonry course pair over the ground
// family and in the bank-1 castle run, the castle's own solid block over the hard block's four ids,
// and the lava, bridge, axe and chain. it lives in bank 6 with its art because bank 4, where every
// other tile array is, has no room left; call it after assets_load_bg_tiles and
// assets_load_scenery_tiles, and only for a castle - the plain loaders put the grass and the bevel
// back for every other type
void castle_art_load(void) BANKED;

#endif
