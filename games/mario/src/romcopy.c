#include "romcopy.h"

#include <gb/gb.h>
#include <string.h>

// whoever called may itself be banked, and returning into bank 0's window would land them in
// another module's code; the caller's bank goes back exactly as it was
void rom_copy(uint8_t bank, const void* src, void* dst, uint16_t len) {
    const uint8_t caller_bank = CURRENT_BANK;

    SWITCH_ROM_MBC5(bank);
    memcpy(dst, src, len);
    SWITCH_ROM_MBC5(caller_bank);
}
