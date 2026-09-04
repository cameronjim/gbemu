#ifndef MAP_ART_HOST_HPP
#define MAP_ART_HOST_HPP

// the generated world map art, compiled straight into the host test the same way the title's and
// the file select's are: the expected frame is then the bytes the rom loads rather than a second
// transcription of the png

// palette_color_t at global scope, so whichever of the art headers lands first defines it once for
// all of them rather than burying it in its own namespace
#include <gb/cgb.h>
#include <stdint.h>

namespace map_art {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#include "gen/map_glyphs.c"
#include "gen/map_screen.c"
#include "gen/map_water_frames.c"
#pragma GCC diagnostic pop
} // namespace map_art

#endif
