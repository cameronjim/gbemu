#ifndef SAVE_H
#define SAVE_H

#include <gb/gb.h>
#include <stdint.h>

// reads the battery slot, or writes a fresh one when the magic is missing
void save_init(void) BANKED;

// 1 when a level past 1-1 has been reached, which is what puts CONTINUE on the title
uint8_t save_has_progress(void);

// the furthest level reached
uint8_t save_level(void);

// records reaching `level`; an earlier level never overwrites a further one
void save_record(uint8_t level, uint16_t score);

#endif
