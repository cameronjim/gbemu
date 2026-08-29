#include "world.h"

#include "assets.h"
#include "flappy.h"

#include <gb/gb.h>
#include <stdint.h>

// 8.8 fixed point px scrolled since the round started; 32 bits so a long run cannot wrap
static uint32_t world_x;
// 8.8 px per frame, raised by the difficulty table
static uint16_t speed;
// world column the streamer writes next; it always runs 31 columns ahead of the left edge
static uint16_t next_col;
// position inside the 12 column pipe cycle
static uint8_t phase;
static uint8_t gen_pipe;
// gap top row and height by pipe index; only three pipes ever live in the ring at once
static uint8_t gap_top[4];
static uint8_t gap_rows[4];
// gap height handed to the next pipe generated
static uint8_t next_gap;
static uint8_t coll_pipe;
static uint32_t coll_pipe_x;
static uint16_t score;
static uint8_t rng_state;
static uint8_t column[kMapRows];

static const uint16_t kDiffScoreTable[kDiffSteps] = kDiffScores;
static const uint16_t kDiffSpeedTable[kDiffSteps] = kDiffSpeeds;
static const uint8_t kDiffGapTable[kDiffSteps] = kDiffGaps;

static uint8_t rng_next(void) {
    rng_state = (uint8_t)(rng_state * kRngMul + kRngAdd);
    return rng_state;
}

// the last table row the score has reached sets speed and the gap for new pipes
static void apply_difficulty(void) {
    uint8_t i;
    for (i = kDiffSteps; i > 0U; --i) {
        if (score >= kDiffScoreTable[i - 1U]) {
            speed = kDiffSpeedTable[i - 1U];
            next_gap = kDiffGapTable[i - 1U];
            return;
        }
    }
}

static void fill_sky(void) {
    uint8_t r;
    for (r = 0; r < kPlayRows; ++r) {
        column[r] = kSkyTileId;
    }
    column[kPlayRows] = kGroundTileId;
    column[kPlayRows + 1U] = kGroundTileId;
}

static void fill_pipe(uint8_t g, uint8_t gap, uint8_t body, uint8_t cap) {
    uint8_t r;
    for (r = 0; r < g; ++r) {
        column[r] = body;
    }
    column[g - 1U] = cap;
    for (r = (uint8_t)(g + gap); r < kPlayRows; ++r) {
        column[r] = body;
    }
    column[g + gap] = cap;
}

// writes one world column into the ring slot that just scrolled off the left edge
static void stream_column(void) {
    uint8_t kind = 0U;
    uint8_t p = gen_pipe;
    uint8_t span;

    if (next_col >= kFirstPipeCol) {
        if (phase == 0U) {
            span = (uint8_t)(kPlayRows - kGapTopSlack - next_gap - kGapTopMin + 1U);
            gap_top[p & 3U] = (uint8_t)(kGapTopMin + rng_next() % span);
            gap_rows[p & 3U] = next_gap;
            kind = 1U;
        } else if (phase < kPipeWidthCols) {
            kind = 2U;
        }
        ++phase;
        if (phase == kPipeSpacingCols) {
            phase = 0U;
            ++gen_pipe;
        }
    }

    fill_sky();
    if (kind == 1U) {
        fill_pipe(gap_top[p & 3U], gap_rows[p & 3U], kPipeBodyLeftTileId, kPipeCapLeftTileId);
    } else if (kind == 2U) {
        fill_pipe(gap_top[p & 3U], gap_rows[p & 3U], kPipeBodyRightTileId, kPipeCapRightTileId);
    }

    set_bkg_tiles((uint8_t)(next_col & (kMapCols - 1U)), 0, 1, kMapRows, column);
    ++next_col;
}

void world_init(uint8_t seed) {
    uint8_t i;

    // seeded at title entry from the free running frame count, so the boot world is fixed
    rng_state = seed;
    world_x = 0;
    next_col = 0;
    phase = 0;
    gen_pipe = 0;
    coll_pipe = 0;
    coll_pipe_x = kFirstPipeWorldX;
    score = 0;
    apply_difficulty();

    set_bkg_data(kPipeBodyLeftTileId, 4, kPipeTiles);
    set_bkg_data(kGroundTileId, 1, kGroundTile);

    SCX_REG = 0;
    SCY_REG = 0;
    for (i = 0; i < kMapCols; ++i) {
        stream_column();
    }
}

// tile-align the frozen scene so popup text lands exactly on its grid centers
void world_snap_scroll(void) {
    world_x &= ~(uint32_t)((1UL << (kFixedShift + 3)) - 1U);
    SCX_REG = (uint8_t)(world_x >> kFixedShift);
}

void world_scroll(void) {
    uint16_t before = (uint16_t)(world_x >> 11);

    world_x += speed;
    SCX_REG = (uint8_t)(world_x >> kFixedShift);
    // speed never reaches a whole column, so at most one new column per frame
    if ((uint16_t)(world_x >> 11) != before) {
        stream_column();
    }
}

uint8_t world_kills(uint8_t bird_top_px) {
    uint32_t bx0;
    uint32_t bx1;
    uint8_t bird_bottom;
    uint8_t gtop;

    bird_bottom = (uint8_t)(bird_top_px + kBirdHitPx);
    if (bird_bottom >= kGroundTopPx) {
        return 1U;
    }

    bx0 = (world_x >> kFixedShift) + kBirdScreenX;
    bx1 = bx0 + kBirdSizePx - 1U;
    if (bx0 > coll_pipe_x + kPipeWidthPx - 1U) {
        coll_pipe_x += kPipeSpacingPx;
        ++coll_pipe;
        // the bird's left edge just cleared the pipe's right edge
        ++score;
        apply_difficulty();
    }
    if (bx1 < coll_pipe_x) {
        return 0U;
    }

    gtop = (uint8_t)(gap_top[coll_pipe & 3U] << 3);
    if (bird_top_px < gtop) {
        return 1U;
    }
    if (bird_bottom > (uint8_t)(gtop + (uint8_t)(gap_rows[coll_pipe & 3U] << 3))) {
        return 1U;
    }
    return 0U;
}

uint16_t world_score(void) {
    return score;
}
