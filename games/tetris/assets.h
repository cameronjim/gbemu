#ifndef ASSETS_H
#define ASSETS_H

#include <stdint.h>

// one solid 8x8 tile at 0x60, every pixel index 3; the border strip's own cgb palette colours it
extern const uint8_t kBorderTile[16];

#endif
