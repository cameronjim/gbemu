#ifndef TITLE_ART_HOST_HPP
#define TITLE_ART_HOST_HPP

// the generated title art, compiled straight into the host test: the expected frame is then the
// same bytes the rom loads rather than a second transcription of the png. hostgb/gb/cgb.h stands in
// for gbdk's own header, and the bank pragma the generator emits means nothing to g++

// palette_color_t at global scope, so whichever of the two art headers lands first defines it
// once for both rather than burying it in its own namespace
#include <gb/cgb.h>
#include <stdint.h>

namespace title_art {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#include "gen/title_deluxe.c"
#include "gen/title_screen.c"
#pragma GCC diagnostic pop
} // namespace title_art

#endif
