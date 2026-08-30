#ifndef SFX_H
#define SFX_H

#include <stdint.h>

void sfx_init(void);
void sfx_rotate(void);
void sfx_lock(void);

// n rows cleared; four of them gets the higher, longer tetris chime
void sfx_clear(uint8_t n);

void sfx_level(void);
void sfx_over(void);

#endif
