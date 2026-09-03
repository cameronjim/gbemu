#ifndef WELL_H
#define WELL_H

#include <stdint.h>

// clears the board and repaints the whole playfield; call with the lcd off
void well_init(void);

// 1 when any cell of the placement leaves the well or hits a locked cell
uint8_t well_blocked(uint8_t piece, uint8_t rot, int8_t px, int8_t py);

void well_lock(uint8_t piece, uint8_t rot, int8_t px, int8_t py);

// flags the full rows and returns how many there are
uint8_t well_mark_full(void);

// paints a few flagged rows white per call; 1 when every one is painted
uint8_t well_flash_step(void);

// drops the rows above the flagged ones and queues their repaint
void well_collapse(void);

// repaints a few queued rows per call; 1 when the board matches the board array
uint8_t well_redraw_step(void);

#endif
