#ifndef SAVE_H
#define SAVE_H

#include <stdint.h>

void save_init(void);
uint32_t save_best(void);
void save_record(uint32_t score);

#endif
