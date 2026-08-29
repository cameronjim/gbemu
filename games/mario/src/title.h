#ifndef TITLE_H
#define TITLE_H

#include <gb/gb.h>

// paints the cgb title card with the lcd off. banked: it runs at boot and after the last level,
// never inside a frame of play, so bank 0 has no reason to carry it
void title_show(void) BANKED;

// the m2 debug camera: no player, no physics, just d-pad scroll and pan over the compiled terrain.
// banked for the same reason the title card is - it is never entered from a frame of play
void debug_camera_enter(void) BANKED;
void debug_camera_frame(uint8_t keys) BANKED;

#endif
