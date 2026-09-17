#ifndef SOUND_H
#define SOUND_H

#include <gb/gb.h>
#include <stdint.h>

// smb's sound engine (smbdis.asm SoundEngine), ported to the game boy apu in bank 8. the game
// queues bits during a frame and sound_frame plays them at the top of the next, as smb's nmi does

// Square1SoundQueue bits
#define kSfxSmallJump 0x80U
#define kSfxFlagpole 0x40U
#define kSfxFireball 0x20U
#define kSfxPipeDownInjury 0x10U
#define kSfxEnemySmack 0x08U
#define kSfxEnemyStomp 0x04U
#define kSfxBump 0x02U
#define kSfxBigJump 0x01U
// Square2SoundQueue bits
#define kSfxBowserFall 0x80U
#define kSfxExtraLife 0x40U
#define kSfxPowerUpGrab 0x20U
#define kSfxTimerTick 0x10U
#define kSfxBlast 0x08U
#define kSfxGrowVine 0x04U
#define kSfxGrowPowerUp 0x02U
#define kSfxCoinGrab 0x01U
// NoiseSoundQueue bits
#define kSfxBowserFlame 0x02U
#define kSfxBrickShatter 0x01U

// AreaMusicQueue bits
#define kMusicSilence 0x80U
#define kMusicStarPower 0x40U
#define kMusicPipeIntro 0x20U
#define kMusicCloud 0x10U
#define kMusicCastle 0x08U
#define kMusicUnderground 0x04U
#define kMusicWater 0x02U
#define kMusicGround 0x01U
// EventMusicQueue bits
#define kMusicTimeRunningOut 0x40U
#define kMusicEndOfLevel 0x20U
#define kMusicAltGameOver 0x10U
#define kMusicEndOfCastle 0x08U
#define kMusicVictory 0x04U
#define kMusicGameOver 0x02U
#define kMusicDeath 0x01U

void sound_init(void) BANKED;
void sound_frame(void) BANKED;
void sfx_square1(uint8_t bits) BANKED;
void sfx_square2(uint8_t bits) BANKED;
void sfx_noise(uint8_t bits) BANKED;
void music_area(uint8_t bits) BANKED;
void music_event(uint8_t bits) BANKED;
// the level type's own theme, queued only when it is not already the one playing
void music_level(uint8_t level_type) BANKED;
// a front screen's theme: plays now, or once the event jingle that is still going has ended
void music_screen(uint8_t bits) BANKED;
// silence for a card, unless an event jingle is still playing, which ends into silence anyway
void music_quiet(void) BANKED;
// back to the level's theme when the star runs out
void music_level_again(void) BANKED;
void sound_pause(uint8_t on) BANKED;

#endif
