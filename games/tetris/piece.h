#ifndef PIECE_H
#define PIECE_H

#include <stdint.h>

void piece_spawn(uint8_t id);
// 1 when the spawn footprint is already occupied, which ends the game
uint8_t piece_spawn_blocked(uint8_t id);

// each returns 1 when the move was legal and taken
uint8_t piece_move(int8_t dx);
uint8_t piece_rotate(int8_t dir);
uint8_t piece_fall(void);

void piece_draw(void);
void piece_hide(void);

void piece_bake(void);

#endif
