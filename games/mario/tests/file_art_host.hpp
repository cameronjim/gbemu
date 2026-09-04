#ifndef FILE_ART_HOST_HPP
#define FILE_ART_HOST_HPP

// the generated file select art, compiled straight into the host test the same way the title's is:
// the expected frame is then the bytes the rom loads rather than a second transcription of the png

// palette_color_t at global scope, so whichever of the two art headers lands first defines it
// once for both rather than burying it in its own namespace
#include <gb/cgb.h>
#include <stdint.h>

namespace file_art {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#include "gen/file_labels.c"
#include "gen/file_select.c"
#pragma GCC diagnostic pop
} // namespace file_art

#endif
