#ifndef PIECES_H
#define PIECES_H

#include <stdint.h>

// the four cells of one rotation, each packed as (y << 4) | x inside a 4x4 box
const uint8_t* pieces_shape(uint8_t piece, uint8_t rot);

void pieces_seed(uint8_t seed);
uint8_t pieces_next(void);

#endif
