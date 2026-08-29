#ifndef TITLE_H
#define TITLE_H

#include <gb/gb.h>

// paints the cgb title card with the lcd off, rereading the battery slot first: progress in it
// turns the prompt into the two-entry menu up and down move between. banked: it runs at boot and
// after the last level, never inside a frame of play
void title_reset(void) BANKED;

// what one title frame decided; the game loop owns the lcd-off level load either answer needs
#define kTitleStay 0U
#define kTitlePlay 1U
#define kTitleCamera 2U

// one frame of the menu, the level select and the two labs. every entry it takes arms the run and
// writes the level to load through `level`, so the loop is left with nothing but the load
uint8_t title_frame(uint8_t pressed, uint8_t* level) BANKED;

// the between-states cards, all painted the same lcd-off way the title is: everything freezes
// while one is up, so the bg map is theirs to overwrite and flow_resume_from_card puts it back
void card_pause(uint8_t level) BANKED;
// the rest are flow.c's, which shares bank 5 with them, so none needs a trampoline
void card_game_over(void);
// the clear card, redrawn each frame while the countdown converts into points
void card_clear(void);
void card_clear_refresh(void);

// the m2 debug camera: no player, no physics, just d-pad scroll and pan over the compiled terrain.
// banked for the same reason the title card is - it is never entered from a frame of play
void debug_camera_enter(void) BANKED;
void debug_camera_frame(uint8_t keys) BANKED;

#endif
