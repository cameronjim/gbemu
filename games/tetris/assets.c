#include "assets.h"

// low byte then high byte per row; both planes set gives pixel index 3 everywhere
const uint8_t kBorderTile[16] = {
    0xFF, 0xFF, // 33333333
    0xFF, 0xFF, // 33333333
    0xFF, 0xFF, // 33333333
    0xFF, 0xFF, // 33333333
    0xFF, 0xFF, // 33333333
    0xFF, 0xFF, // 33333333
    0xFF, 0xFF, // 33333333
    0xFF, 0xFF, // 33333333
};
