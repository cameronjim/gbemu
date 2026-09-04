#ifndef TITLE_H
#define TITLE_H

#include <gb/gb.h>

// paints the smbd title frame with the lcd off - generated bg art plus the "Deluxe" script's
// sprites, no text and no prompt. banked: it runs at boot and after a game over, never inside a
// frame of play
void title_reset(void) BANKED;

// what one title frame decided; the game loop owns the lcd-off paint or level load each answer needs
#define kTitleStay 0U
#define kTitlePlay 1U
#define kTitleCamera 2U
#define kTitleFile 3U

// one frame of the title, the level select and the labs. start/a flashes the wordmark for
// kTitleLeaveFrames and only then asks for the file select, ignoring the pad while it runs; the
// debug entries arm a run outright and write the level to load through `level`
uint8_t title_frame(uint8_t pressed, uint8_t* level) BANKED;

// title.c's card machinery, shared with mapscreen.c. both live in bank 5, so none of these needs a
// trampoline - and none may be called from another bank, because a `const char*` argument would
// point into the caller's own banked rodata and read as whatever bank 5 has at that address
void card_begin(uint8_t heading_row);
void card_end(void);
void card_clear_map(void);
void card_paint_band(uint8_t y0, uint8_t rows, uint8_t palette);
void card_print_centered(uint8_t y, const char* text);
void card_print_value(uint8_t y, const char* label, uint16_t value, uint8_t digits, uint8_t trailing);

// the between-states cards, all painted the same lcd-off way the title is: everything freezes
// while one is up, so the bg map is theirs to overwrite and flow_resume_from_card puts it back
void card_pause(uint8_t level) BANKED;

// one frame of the pause card's RESUME/QUIT menu: up/down move the cursor and repaint, start or a
// confirm. quitting hands back to the world map without recording anything, which is main.c's job
#define kPauseStay 0U
#define kPauseResume 1U
#define kPauseQuit 2U
uint8_t pause_frame(uint8_t pressed) BANKED;
// the rest are flow.c's, which shares bank 5 with them, so none needs a trampoline
void card_game_over(void);
// the clear card, redrawn each frame while the countdown converts into points
void card_clear(void);
void card_clear_refresh(void);

// the m2 debug camera: no player, no physics, just d-pad scroll and pan over the compiled terrain.
// banked for the same reason the title card is - it is never entered from a frame of play
void debug_camera_enter(uint8_t level) BANKED;
void debug_camera_frame(uint8_t keys) BANKED;

#endif
