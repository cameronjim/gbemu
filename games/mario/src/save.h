#ifndef SAVE_H
#define SAVE_H

#include <gb/gb.h>
#include <stdint.h>

// three independent battery slots; see the layout comment at kSramBase in mario.h. every read
// below answers out of ram - save_init stages the whole 32-byte header once at boot - and every
// write goes straight back to sram so a slot survives a power cycle without a flush step

// reads the three slots, migrating a one-slot "MAR1" save into slot 1, or writes a fresh header
void save_init(void) BANKED;

// 1 when the slot holds a file at all, which is what puts progress rather than NEW on its line
uint8_t save_slot_used(uint8_t slot);

// the slot's furthest UNLOCKED level, 0..kLevelCount (kLevelCount = world one finished)
uint8_t save_slot_level(uint8_t slot);

// and the score standing when that level was reached
uint16_t save_slot_score(uint8_t slot);

// clears one slot back to empty; the other two are not touched
void save_erase(uint8_t slot) BANKED;

// the file the running game records into. kSaveNoSlot while nothing is picked, which is what a
// debug or lab run leaves it at so a lab clear cannot invent a save
void save_select(uint8_t slot);
uint8_t save_current(void);

// marks the current slot in use, writing an empty file if it had none. called when a file is
// picked, so an untouched NEW slot becomes a real one the moment the player commits to it
void save_begin(void) BANKED;

// records reaching `level` in the current slot; an earlier level never overwrites a further one,
// and kSaveNoSlot records nothing at all
void save_record(uint8_t level, uint16_t score);

#endif
