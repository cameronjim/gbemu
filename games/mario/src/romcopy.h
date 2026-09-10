#ifndef ROMCOPY_H
#define ROMCOPY_H

#include <stdint.h>

// bank 0: the one place the engine switches the rom bank, so nothing banked ever has to
void rom_copy(uint8_t bank, const void* src, void* dst, uint16_t len);

#endif
