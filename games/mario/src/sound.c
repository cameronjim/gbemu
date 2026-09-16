// smb's sound engine (smbdis.asm SoundEngine, 15045-15960), one frame at a time, on the game boy
// apu. the queues, buffers, length counters and music cursors are smb's own; only the register
// writes change shape. BUDGET.md: bank 8 is the first empty bank
#pragma bank 8

#include "sound.h"

#include "gen/smb_audio_data.h"
#include "mario.h"

#include <gb/gb.h>
#include <stdint.h>

static uint8_t sq1_queue;
static uint8_t sq2_queue;
static uint8_t noise_queue;
static uint8_t area_queue;
static uint8_t event_queue;
static uint8_t pause_queue;
static uint8_t sq1_buf;
static uint8_t sq2_buf;
static uint8_t noise_buf;
static uint8_t area_buf;
static uint8_t event_buf;
static uint8_t area_buf_alt;
static uint8_t sq1_len;
static uint8_t sq2_len;
static uint8_t noise_len;
static uint8_t secondary;
static uint8_t pause_mode;
static uint8_t pause_buf;
static uint8_t len_ofs;
static uint8_t len_adder;
static uint8_t ground_ofs;
static uint16_t data_ofs;
static uint8_t ofs_sq2;
static uint8_t ofs_sq1;
static uint8_t ofs_tri;
static uint8_t ofs_noise;
static uint8_t noise_loop;
static uint8_t sq2_note_len;
static uint8_t sq1_note_len;
static uint8_t tri_note_len;
static uint8_t noise_beat_len;
static uint8_t sq2_len_buf;
static uint8_t tri_len_buf;
static uint8_t alt_sweep;
static uint8_t level_music;
// the period each square sits at, so a control-only write can retrigger it in place
static uint16_t sq1_period;
static uint16_t sq2_period;

// the nes triangle, as 32 samples: up 0..f, back down f..0
static const uint8_t kTriangleWave[16] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
                                          0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10};

// nes decay rate n steps every (n+1)/240 s, the apu envelope every p/64 s
static const uint8_t kDecayPeriod[16] = {1, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4};

// nes $4000: DDLC VVVV. constant volume goes straight in; a decay becomes a full-volume envelope
static uint8_t gb_env(uint8_t ctrl) {
    if ((ctrl & 0x10U) != 0U) {
        return (uint8_t)(ctrl << 4);
    }
    return (uint8_t)(0xF0U | kDecayPeriod[ctrl & 0x0FU]);
}

// nes $4001: EPPP NSSS at 120 hz; nr10 runs at 128 hz, and its direction bit means the opposite
static uint8_t gb_sweep(uint8_t s) {
    uint8_t period;

    if ((s & 0x80U) == 0U) {
        return 0;
    }
    period = (uint8_t)(((s >> 4) & 7U) + 1U);
    if (period > 7U) {
        period = 7;
    }
    return (uint8_t)((period << 4) | ((s & 8U) != 0U ? 0U : 8U) | (s & 7U));
}

static uint8_t ch1_note(uint8_t note) {
    const uint16_t p = kNotePeriod[note >> 1];

    if (p == 0U) {
        return 0;
    }
    sq1_period = p;
    NR13_REG = (uint8_t)p;
    NR14_REG = (uint8_t)(0x80U | (p >> 8));
    return 1;
}

static uint8_t ch2_note(uint8_t note) {
    const uint16_t p = kNotePeriod[note >> 1];

    if (p == 0U) {
        return 0;
    }
    sq2_period = p;
    NR23_REG = (uint8_t)p;
    NR24_REG = (uint8_t)(0x80U | (p >> 8));
    return 1;
}

// PlaySqu1Sfx: control regs, then the note
static void play_sq1(uint8_t ctrl, uint8_t sweep, uint8_t note) {
    NR10_REG = gb_sweep(sweep);
    NR11_REG = (uint8_t)(ctrl & 0xC0U);
    NR12_REG = gb_env(ctrl);
    if (ch1_note(note) == 0U) {
        NR14_REG = (uint8_t)(0x80U | (sq1_period >> 8));
    }
}

// Dump_Squ1_Regs mid-sound: the apu only takes a new envelope on a trigger, so the note restarts
static void ctrl_sq1(uint8_t ctrl) {
    NR11_REG = (uint8_t)(ctrl & 0xC0U);
    NR12_REG = gb_env(ctrl);
    NR14_REG = (uint8_t)(0x80U | (sq1_period >> 8));
}

// a low-byte retune, no restart
static void retune_sq1(uint16_t p) {
    sq1_period = p;
    NR13_REG = (uint8_t)p;
    NR14_REG = (uint8_t)(p >> 8);
}

// ch2 has no sweep unit, so smb's square 2 sweeps (the blast, bowser's fall) play straight
static void play_sq2(uint8_t ctrl, uint8_t note) {
    NR21_REG = (uint8_t)(ctrl & 0xC0U);
    NR22_REG = gb_env(ctrl);
    if (ch2_note(note) == 0U) {
        NR24_REG = (uint8_t)(0x80U | (sq2_period >> 8));
    }
}

static void retune_sq2(uint16_t p) {
    sq2_period = p;
    NR23_REG = (uint8_t)p;
    NR24_REG = (uint8_t)(p >> 8);
}

// $400C, $400E, $400F: $400F>>3 indexes nes's length table, 3 is one frame and 11 five; d5 halts it
static void play_noise(uint8_t ctrl, uint8_t period, uint8_t length) {
    NR42_REG = gb_env(ctrl);
    NR43_REG = (uint8_t)(kNoiseNr43[period & 0x0FU] | ((period & 0x80U) != 0U ? 0x08U : 0U));
    NR41_REG = (length == 0x58U) ? 43U : 60U;
    NR44_REG = (ctrl & 0x20U) != 0U ? 0x80U : 0xC0U;
}

// StopSquare1Sfx: clears the buffer and cuts the channel
static void stop_sq1(void) {
    sq1_buf = 0;
    NR12_REG = 0;
}

// StopSquare2Sfx cuts the channel; only DecrementSfx2Length clears the buffer
static void stop_sq2(void) {
    NR22_REG = 0;
}

static void dec_sq1(void) {
    --sq1_len;
    if (sq1_len == 0U) {
        stop_sq1();
    }
}

static void dec_sq2(void) {
    --sq2_len;
    if (sq2_len == 0U) {
        sq2_buf = 0;
        stop_sq2();
    }
}

static void dec_noise(void) {
    --noise_len;
    if (noise_len == 0U) {
        NR42_REG = 0;
        noise_buf = 0;
    }
}

// square 1 sfx

static void cont_jump(void) {
    if (sq1_len == 0x25U) {
        NR10_REG = gb_sweep(0xF6U);
        ctrl_sq1(0x5FU);
    } else if (sq1_len == 0x20U) {
        NR10_REG = gb_sweep(0xBCU);
        ctrl_sq1(0x48U);
    }
    dec_sq1();
}

// small and big jumps share their register contents; only the note differs
static void start_jump(uint8_t note) {
    play_sq1(0x82U, 0xA7U, note);
    sq1_len = 0x28U;
    cont_jump();
}

static void cont_bump(void) {
    if (sq1_len == 0x06U) {
        NR10_REG = gb_sweep(0xBBU);
    }
    dec_sq1();
}

// the bump and the fireball throw share $9e; the sweep and the length differ
static void start_bump(uint8_t length, uint8_t sweep) {
    sq1_len = length;
    play_sq1(0x9EU, sweep, 0x0CU);
    cont_bump();
}

static void cont_stomp(void) {
    ctrl_sq1(kSwimStompEnv[sq1_len - 1U]);
    if (sq1_len == 0x06U) {
        retune_sq1(kStompSecondPeriod);
    }
    dec_sq1();
}

static void start_stomp(void) {
    sq1_len = 0x0EU;
    play_sq1(0x9EU, 0x9CU, 0x26U);
    cont_stomp();
}

static void cont_smack(void) {
    if (sq1_len == 0x08U) {
        retune_sq1(kSmackSecondPeriod);
        ctrl_sq1(0x9FU);
    } else {
        // the spaces that give the smack its noise
        ctrl_sq1(0x90U);
    }
    dec_sq1();
}

static void start_smack(void) {
    sq1_len = 0x0EU;
    play_sq1(0x9FU, 0xCBU, 0x28U);
    dec_sq1();
}

// six writes, on the frames where d3 is set and d1-0 clear
static void cont_pipe(void) {
    if ((sq1_len & 0x03U) == 0U && (sq1_len & 0x08U) != 0U) {
        play_sq1(0x9AU, 0x91U, 0x44U);
    }
    dec_sq1();
}

static void start_pipe(void) {
    sq1_len = 0x2FU;
    cont_pipe();
}

static void start_flagpole(void) {
    sq1_len = 0x40U;
    play_sq1(0x99U, 0xBCU, 0x62U);
    dec_sq1();
}

// Square1SfxHandler: the queue starts a sound in bit order, the buffer carries it on
static void sq1_handler(void) {
    uint8_t q = sq1_queue;

    if (q != 0U) {
        sq1_buf = q;
        if ((q & kSfxSmallJump) != 0U) {
            start_jump(0x26U);
        } else if ((q & kSfxBigJump) != 0U) {
            start_jump(0x18U);
        } else if ((q & kSfxBump) != 0U) {
            start_bump(0x0AU, 0x93U);
        } else if ((q & kSfxEnemyStomp) != 0U) {
            start_stomp();
        } else if ((q & kSfxEnemySmack) != 0U) {
            start_smack();
        } else if ((q & kSfxPipeDownInjury) != 0U) {
            start_pipe();
        } else if ((q & kSfxFireball) != 0U) {
            start_bump(0x05U, 0x99U);
        } else {
            start_flagpole();
        }
        return;
    }
    q = sq1_buf;
    if (q == 0U) {
        return;
    }
    if ((q & (kSfxSmallJump | kSfxBigJump)) != 0U) {
        cont_jump();
    } else if ((q & (kSfxBump | kSfxFireball)) != 0U) {
        cont_bump();
    } else if ((q & kSfxEnemyStomp) != 0U) {
        cont_stomp();
    } else if ((q & kSfxEnemySmack) != 0U) {
        cont_smack();
    } else if ((q & kSfxPipeDownInjury) != 0U) {
        cont_pipe();
    } else {
        dec_sq1();
    }
}

// square 2 sfx

static void cont_coin(void) {
    if (sq2_len == 0x30U) {
        retune_sq2(kCoinSecondPeriod);
    }
    dec_sq2();
}

// the coin and the timer tick share their note; the tick is quieter and six frames long
static void start_coin(uint8_t length, uint8_t ctrl) {
    sq2_len = length;
    play_sq2(ctrl, 0x42U);
    cont_coin();
}

static void cont_blast(void) {
    if (sq2_len == 0x18U) {
        play_sq2(0x9FU, 0x18U);
    }
    dec_sq2();
}

static void start_blast(void) {
    sq2_len = 0x20U;
    play_sq2(0x9FU, 0x5EU);
    dec_sq2();
}

static void cont_powerup_grab(void) {
    if ((sq2_len & 1U) == 0U) {
        play_sq2(0x5DU, kPowerUpGrabFreq[(sq2_len >> 1) - 1U]);
    }
    dec_sq2();
}

static void start_powerup_grab(void) {
    sq2_len = 0x36U;
    cont_powerup_grab();
}

static void cont_bowser_fall(void) {
    if (sq2_len == 0x08U) {
        play_sq2(0x9FU, 0x5AU);
    }
    dec_sq2();
}

static void start_bowser_fall(void) {
    sq2_len = 0x38U;
    play_sq2(0x9FU, 0x18U);
    dec_sq2();
}

// a new tone every eight frames
static void cont_extra_life(void) {
    if ((sq2_len & 0x07U) == 0U) {
        play_sq2(0x82U, kExtraLifeFreq[(sq2_len >> 3) - 1U]);
    }
    dec_sq2();
}

static void start_extra_life(void) {
    sq2_len = 0x30U;
    cont_extra_life();
}

// the reveal and the vine run on their own counter and never decrement the length
static void cont_grow(void) {
    uint8_t y;

    ++secondary;
    y = (uint8_t)(secondary >> 1);
    if (y == sq2_len) {
        sq2_buf = 0;
        stop_sq2();
        return;
    }
    play_sq2(0x9DU, kGrowFreq[y]);
}

static void start_grow(uint8_t length) {
    sq2_len = length;
    secondary = 0;
    cont_grow();
}

// Square2SfxHandler: the 1-up cannot be interrupted
static void sq2_handler(void) {
    uint8_t q;

    if ((sq2_buf & kSfxExtraLife) != 0U) {
        cont_extra_life();
        return;
    }
    q = sq2_queue;
    if (q != 0U) {
        sq2_buf = q;
        if ((q & kSfxBowserFall) != 0U) {
            start_bowser_fall();
        } else if ((q & kSfxCoinGrab) != 0U) {
            start_coin(0x35U, 0x8DU);
        } else if ((q & kSfxGrowPowerUp) != 0U) {
            start_grow(0x10U);
        } else if ((q & kSfxGrowVine) != 0U) {
            start_grow(0x20U);
        } else if ((q & kSfxBlast) != 0U) {
            start_blast();
        } else if ((q & kSfxTimerTick) != 0U) {
            start_coin(0x06U, 0x98U);
        } else if ((q & kSfxPowerUpGrab) != 0U) {
            start_powerup_grab();
        } else {
            start_extra_life();
        }
        return;
    }
    q = sq2_buf;
    if (q == 0U) {
        return;
    }
    if ((q & kSfxBowserFall) != 0U) {
        cont_bowser_fall();
    } else if ((q & (kSfxCoinGrab | kSfxTimerTick)) != 0U) {
        cont_coin();
    } else if ((q & (kSfxGrowPowerUp | kSfxGrowVine)) != 0U) {
        cont_grow();
    } else if ((q & kSfxBlast) != 0U) {
        cont_blast();
    } else if ((q & kSfxPowerUpGrab) != 0U) {
        cont_powerup_grab();
    } else {
        cont_extra_life();
    }
}

// noise sfx

// a burst every other frame
static void cont_brick(void) {
    if ((noise_len & 1U) != 0U) {
        const uint8_t y = (uint8_t)(noise_len >> 1);

        play_noise(kBrickShatterEnv[y], kBrickShatterFreq[y], 0x18U);
    }
    dec_noise();
}

static void cont_flame(void) {
    play_noise(kBowserFlameEnv[(noise_len >> 1) - 1U], 0x0FU, 0x18U);
    dec_noise();
}

static void noise_handler(void) {
    uint8_t q = noise_queue;

    if (q != 0U) {
        noise_buf = q;
        if ((q & kSfxBrickShatter) != 0U) {
            noise_len = 0x20U;
            cont_brick();
        } else {
            noise_len = 0x40U;
            cont_flame();
        }
        return;
    }
    q = noise_buf;
    if (q == 0U) {
        return;
    }
    if ((q & kSfxBrickShatter) != 0U) {
        cont_brick();
    } else {
        cont_flame();
    }
}

// music

// ProcessLengthData: the low three bits, offset by the header and by the hurry-up adder
static uint8_t length_of(uint8_t bits) {
    return kNoteLength[(uint8_t)((bits & 0x07U) + len_ofs + len_adder)];
}

// AlternateLengthHandler: square 1 and noise keep their length in d0, d7, d6
static uint8_t alt_length_of(uint8_t b) {
    return length_of((uint8_t)(((b & 0x01U) << 2) | ((b >> 6) & 0x03U)));
}

// LoadHeader; y is the 1-based MusicHeaderData index
static void load_header(uint8_t y) {
    const uint8_t* h = &kMusicHeaders[(uint8_t)(kMusicHeaderIndex[y - 1U] * kMusicHeaderBytes)];

    len_ofs = h[0];
    data_ofs = (uint16_t)(h[1] | ((uint16_t)h[2] << 8));
    ofs_tri = h[3];
    ofs_sq1 = h[4];
    ofs_noise = h[5];
    noise_loop = h[5];
    sq2_note_len = 1;
    sq1_note_len = 1;
    tri_note_len = 1;
    noise_beat_len = 1;
    ofs_sq2 = 0;
    alt_sweep = 0;
    NR30_REG = 0;
}

// HandleAreaMusicLoopB: the ground theme walks its 33-step layout, the rest replay one header
static void load_area_loop(uint8_t music) {
    uint8_t y;
    uint8_t carry;

    event_buf = 0;
    area_buf = music;
    if (music == kMusicGround) {
        ++ground_ofs;
        if (ground_ofs == kGroundLayoutEnd) {
            // GMLoopB parks the offset on the lead-in and the loop below steps past it
            ground_ofs = (uint8_t)(kGroundLayoutFirst + 1U);
        }
        y = ground_ofs;
    } else {
        // FindEventMusicHeader: one past the lowest set bit
        y = 8;
        do {
            ++y;
            carry = (uint8_t)(music & 0x01U);
            music = (uint8_t)(music >> 1);
        } while (carry == 0U);
    }
    load_header(y);
}

// LoadAreaMusic
static void load_area(uint8_t music) {
    if (music == kMusicUnderground) {
        stop_sq1();
    }
    ground_ofs = (uint8_t)(kGroundLayoutFirst - 1U);
    load_area_loop(music);
}

// LoadEventMusic: the area theme is kept aside for the hurry-up jingle to hand back to
static void load_event(uint8_t music) {
    uint8_t y = 0;
    uint8_t carry;

    event_buf = music;
    if (music == kMusicDeath) {
        stop_sq1();
        stop_sq2();
    }
    area_buf_alt = area_buf;
    len_adder = 0;
    area_buf = 0;
    if (music == kMusicTimeRunningOut) {
        len_adder = 8;
    }
    do {
        ++y;
        carry = (uint8_t)(music & 0x01U);
        music = (uint8_t)(music >> 1);
    } while (carry == 0U);
    load_header(y);
}

// LoadControlRegs and the per-frame envelope tables, as one apu envelope per note. the area
// themes hold 8 for a frame and are gone in eight; the water and event themes sit near 6 for
// forty; the castle win holds; death and the alternate game over keep smb's hardware decay
static uint8_t music_env(void) {
    if ((event_buf & 0x91U) != 0U) {
        return 0xF1U;
    }
    if ((event_buf & kMusicEndOfCastle) != 0U) {
        return 0x90U;
    }
    if ((area_buf & 0x7DU) != 0U) {
        return 0x81U;
    }
    return 0x67U;
}

// EndOfMusicData: 1 when the music went round again, 0 when it stopped
static uint8_t end_of_music(void) {
    if (event_buf == kMusicTimeRunningOut) {
        if (area_buf_alt != 0U) {
            load_area_loop(area_buf_alt);
            return 1;
        }
    } else if ((event_buf & kMusicVictory) != 0U) {
        load_event(kMusicVictory);
        return 1;
    }
    if ((area_buf & 0x5FU) != 0U) {
        load_area_loop(area_buf);
        return 1;
    }
    area_buf = 0;
    event_buf = 0;
    NR30_REG = 0;
    NR12_REG = 0;
    NR22_REG = 0;
    return 0;
}

// Squ2NoteHandler: a sound effect on the channel owns it until it ends
static void sq2_music_note(uint8_t note) {
    uint16_t p;

    if (sq2_buf != 0U) {
        return;
    }
    p = kNotePeriod[note >> 1];
    if (p == 0U) {
        NR22_REG = 0;
        return;
    }
    NR21_REG = 0x80U;
    NR22_REG = music_env();
    sq2_period = p;
    NR23_REG = (uint8_t)p;
    NR24_REG = (uint8_t)(0x80U | (p >> 8));
}

// HandleSquare2Music: square 2 carries the terminator, so it decides when the song ends
static uint8_t handle_sq2(void) {
    uint8_t b;

    --sq2_note_len;
    if (sq2_note_len != 0U) {
        return 1;
    }
    for (;;) {
        b = kMusicData[data_ofs + ofs_sq2];
        ++ofs_sq2;
        if (b != 0U) {
            break;
        }
        if (end_of_music() == 0U) {
            return 0;
        }
    }
    if ((b & 0x80U) != 0U) {
        sq2_len_buf = length_of(b);
        // the byte after a length is a note, whatever it is
        b = kMusicData[data_ofs + ofs_sq2];
        ++ofs_sq2;
    }
    sq2_music_note(b);
    sq2_note_len = sq2_len_buf;
    return 1;
}

// HandleSquare1Music
static void handle_sq1(void) {
    uint8_t b;
    uint16_t p;

    if (ofs_sq1 == 0U) {
        return;
    }
    --sq1_note_len;
    if (sq1_note_len != 0U) {
        return;
    }
    for (;;) {
        b = kMusicData[data_ofs + ofs_sq1];
        ++ofs_sq1;
        if (b != 0U) {
            break;
        }
        // a zero switches square 1 to $83/$94: the downward sweep that gives death its sound
        alt_sweep = 0x94U;
    }
    sq1_note_len = alt_length_of(b);
    if (sq1_buf != 0U) {
        return;
    }
    p = kNotePeriod[(b & 0x3EU) >> 1];
    if (p == 0U) {
        NR12_REG = 0;
        return;
    }
    NR10_REG = gb_sweep(alt_sweep != 0U ? alt_sweep : 0x7FU);
    NR11_REG = 0x80U;
    NR12_REG = music_env();
    sq1_period = p;
    NR13_REG = (uint8_t)p;
    NR14_REG = (uint8_t)(0x80U | (p >> 8));
}

// the triangle's control byte, as a ch3 note: $1f runs eight frames, $0f four, $ff holds
static void ch3_note(uint8_t note, uint8_t ctrl) {
    const uint16_t p = kNotePeriod[note >> 1];

    if (p == 0U) {
        NR30_REG = 0;
        return;
    }
    NR30_REG = 0x80U;
    NR31_REG = (ctrl == 0x0FU) ? 239U : 223U;
    NR33_REG = (uint8_t)p;
    NR34_REG = (uint8_t)((ctrl == 0xFFU ? 0x80U : 0xC0U) | (p >> 8));
}

// HandleTriangleMusic
static void handle_tri(void) {
    uint8_t b;
    uint8_t ctrl = 0x1FU;

    --tri_note_len;
    if (tri_note_len != 0U) {
        return;
    }
    b = kMusicData[data_ofs + ofs_tri];
    ++ofs_tri;
    if (b == 0U) {
        NR30_REG = 0;
        return;
    }
    if ((b & 0x80U) != 0U) {
        tri_len_buf = length_of(b);
        b = kMusicData[data_ofs + ofs_tri];
        ++ofs_tri;
        if (b == 0U) {
            NR30_REG = 0;
            return;
        }
    }
    tri_note_len = tri_len_buf;
    // water, castle and every event but death and the alternate game over let long notes ring
    if ((event_buf & 0x6EU) != 0U || (area_buf & 0x0AU) != 0U) {
        if (tri_len_buf >= 0x12U) {
            ctrl = 0xFFU;
        } else if ((event_buf & kMusicEndOfCastle) != 0U) {
            ctrl = 0x0FU;
        }
    }
    ch3_note(b, ctrl);
}

// HandleNoiseMusic: underground and castle have no beat; the stream loops on its own zero
static void handle_noise(void) {
    uint8_t b;
    uint8_t beat;

    if ((area_buf & 0xF3U) == 0U) {
        return;
    }
    --noise_beat_len;
    if (noise_beat_len != 0U) {
        return;
    }
    for (;;) {
        b = kMusicData[data_ofs + ofs_noise];
        ++ofs_noise;
        if (b != 0U) {
            break;
        }
        ofs_noise = noise_loop;
    }
    noise_beat_len = alt_length_of(b);
    beat = (uint8_t)(b & 0x3EU);
    if (beat == 0x30U) {
        play_noise(0x1CU, 0x03U, 0x58U);
    } else if (beat == 0x20U) {
        play_noise(0x1CU, 0x0CU, 0x18U);
    } else if ((beat & 0x10U) != 0U) {
        play_noise(0x1CU, 0x03U, 0x18U);
    } else {
        NR42_REG = 0;
    }
}

// MusicHandler
static void music_handler(void) {
    if (event_queue != 0U) {
        load_event(event_queue);
    } else if (area_queue != 0U) {
        load_area(area_queue);
    } else if ((event_buf | area_buf) == 0U) {
        return;
    }
    if (handle_sq2() == 0U) {
        return;
    }
    handle_sq1();
    handle_tri();
    handle_noise();
}

static void silence_all(void) {
    NR12_REG = 0;
    NR22_REG = 0;
    NR30_REG = 0;
    NR42_REG = 0;
}

// the pause jingle, two tones twice, with everything else cut and the music held where it was
static void pause_tone(uint8_t note) {
    play_sq1(0x84U, 0x7FU, note);
}

static void in_pause(void) {
    if (pause_buf == 0U) {
        if (pause_queue == 0U) {
            return;
        }
        pause_buf = pause_queue;
        pause_mode = 1;
        silence_all();
        sq1_buf = 0;
        sq2_buf = 0;
        noise_buf = 0;
        sq1_len = 0x2AU;
        pause_tone(0x44U);
    } else if (sq1_len == 0x24U || sq1_len == 0x18U) {
        pause_tone(0x64U);
    } else if (sq1_len == 0x1EU) {
        pause_tone(0x44U);
    }
    --sq1_len;
    if (sq1_len != 0U) {
        return;
    }
    silence_all();
    // a second queue means the unpause jingle: game sounds come back after it
    if (pause_buf == 2U) {
        pause_mode = 0;
    }
    pause_buf = 0;
}

void sound_init(void) BANKED {
    uint8_t i;

    // pandocs: nr52 on before any other register keeps a write; wave ram only with ch3 off
    NR52_REG = 0x80U;
    NR51_REG = 0xFFU;
    NR50_REG = 0x77U;
    NR30_REG = 0;
    for (i = 0; i < 16U; ++i) {
        *(volatile uint8_t*)(0xFF30U + i) = kTriangleWave[i];
    }
    NR32_REG = 0x20U;
    sq1_queue = 0;
    sq2_queue = 0;
    noise_queue = 0;
    area_queue = 0;
    event_queue = 0;
    pause_queue = 0;
    sq1_buf = 0;
    sq2_buf = 0;
    noise_buf = 0;
    area_buf = 0;
    event_buf = 0;
    area_buf_alt = 0;
    pause_mode = 0;
    pause_buf = 0;
    level_music = kMusicGround;
    sq1_period = 0;
    sq2_period = 0;
}

// SoundEngine, once per frame
void sound_frame(void) BANKED {
    if (pause_mode != 0U || pause_queue == 1U) {
        in_pause();
    } else {
        sq1_handler();
        sq2_handler();
        noise_handler();
        music_handler();
        area_queue = 0;
        event_queue = 0;
    }
    sq1_queue = 0;
    sq2_queue = 0;
    noise_queue = 0;
    pause_queue = 0;
}

void sfx_square1(uint8_t bits) BANKED {
    sq1_queue = (uint8_t)(sq1_queue | bits);
}

void sfx_square2(uint8_t bits) BANKED {
    sq2_queue = (uint8_t)(sq2_queue | bits);
}

void sfx_noise(uint8_t bits) BANKED {
    noise_queue = (uint8_t)(noise_queue | bits);
}

void music_area(uint8_t bits) BANKED {
    area_queue = bits;
}

void music_event(uint8_t bits) BANKED {
    event_queue = (uint8_t)(event_queue | bits);
}

void music_level(uint8_t level_type) BANKED {
    uint8_t music = kMusicGround;

    if (level_type == (uint8_t)kLevelTypeUnderground) {
        music = kMusicUnderground;
    } else if (level_type == (uint8_t)kLevelTypeCastle) {
        music = kMusicCastle;
    }
    level_music = music;
    // a card's rebuild syncs the palette too, and that must not start the theme over
    if (area_buf != music) {
        area_queue = music;
    }
}

void music_level_again(void) BANKED {
    area_queue = level_music;
}

void sound_pause(uint8_t on) BANKED {
    pause_queue = on != 0U ? 1U : 2U;
}
