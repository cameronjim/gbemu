#include "cartridge.hpp"
#include "gameboy.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <limits>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {

std::vector<uint8_t> read_crossy_rom() {
    std::ifstream in(CROSSY_ROM_PATH, std::ios::binary);
    REQUIRE(in.good());
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

size_t count_lit_pixels(std::span<const uint8_t> fb) {
    size_t lit = 0;
    for (uint8_t shade : fb) {
        if (shade != 0) {
            ++lit;
        }
    }
    return lit;
}

constexpr uint32_t kBootFrames = 120;

// the hover banner covers screen rows 1..5: pad, title, prompt, BEST, then the digit sprites
constexpr uint32_t kBannerTopRow = 1;
constexpr uint32_t kBannerTitleRow = 2;
constexpr uint32_t kBannerPromptRow = 3;
constexpr uint32_t kBannerBestRow = 4;
constexpr size_t kBannerTopPx = 8;
constexpr size_t kBannerEndPx = 48;
// the best's digit sprites sit on the band's last row
constexpr size_t kBestDigitTopPx = 40;

// the tile contract: terrain on the bg, chick, cars and digits as sprites
constexpr uint8_t kGrassTileId = 0xA0;
constexpr uint8_t kTreeTileId = 0xA1;
constexpr uint8_t kRoadTileId = 0xA2;
constexpr uint8_t kRoadStripeTileId = 0xA3;
constexpr uint8_t kWaterTileId = 0xA4;
constexpr uint8_t kRailTileId = 0xA5;
constexpr uint8_t kRailWarnTileId = 0xA6;
constexpr uint8_t kGrassAltTileId = 0xA7;
constexpr uint8_t kWaterDarkTileId = 0xA8;
// the train's carriages are bg, not sprites: dmg draws ten sprites a line and the block is 256 px
constexpr uint8_t kTrainBodyUpperTileId = 0xA9;
constexpr uint8_t kTrainBodyLowerTileId = 0xAA;
// every sprite is 8x16, so each family owns an even aligned run of tile pairs
constexpr uint8_t kCarFirstTileId = 0xB0;
constexpr uint8_t kCarLastTileId = 0xB3;
constexpr uint8_t kLogFirstTileId = 0xB4;
constexpr uint8_t kLogLastTileId = 0xB9;
constexpr uint8_t kEagleFirstTileId = 0xBC;
constexpr uint8_t kEagleLastTileId = 0xBF;
constexpr uint8_t kTrainFirstTileId = 0xC0;
constexpr uint8_t kTrainLastTileId = 0xC7;
constexpr uint8_t kDigitTileId = 0xC8;
constexpr uint8_t kDigitLastTileId = 0xDB;
constexpr uint8_t kChickTileId = 0xE0;
constexpr uint8_t kChickHopTileId = 0xE2;
constexpr uint8_t kChickLastTileId = 0xE3;
// a sprite is two tile rows tall, so a mover parked at a lane's top edge covers just that lane
constexpr int kSpriteRows = 16;

// the popup draws from an inverted copy of the font parked at 0x60
constexpr uint8_t kInvFontFirstTile = 0x60;
constexpr uint8_t kInvFontLastTile = 0x9F;

// the popup band covers screen rows 4..13 of 18, a blank row of air between every text line
constexpr size_t kPopupTopPx = 32;
constexpr size_t kPopupEndPx = 112;
constexpr uint32_t kPopupPromptRow = 12;

// input is ignored for this many frames after a death
constexpr uint32_t kLockoutFrames = 20;

// even world lanes take one grass tile, odd lanes the other
bool is_grass_tile(int tile) {
    return tile == kGrassTileId || tile == kGrassAltTileId;
}

bool is_road_tile(int tile) {
    return tile == kRoadTileId || tile == kRoadStripeTileId;
}

// a water lane is one tile across both of its rows; which one is the lane's parity
bool is_water_tile(int tile) {
    return tile == kWaterTileId || tile == kWaterDarkTileId;
}

bool is_track_tile(int tile) {
    return tile == kRailTileId || tile == kRailWarnTileId;
}

bool is_train_body_tile(int tile) {
    return tile == kTrainBodyUpperTileId || tile == kTrainBodyLowerTileId;
}

// a sweep buries its own rails under carriages, so a planner reads either as the same lane
bool is_track_lane_tile(int tile) {
    return is_track_tile(tile) || is_train_body_tile(tile);
}

// map column of the warning light, the one cell of a track lane whose art blinks
constexpr int kWarnTileCol = 10;

// the grid: 10 columns of 16 px, the chick's cell inset 4 px inside its own
constexpr int kGridCols = 10;
constexpr int kCellPx = 16;
constexpr int kCellInset = 4;

// a hop slides 16 px over 8 frames, so ten frames always settles one
constexpr uint32_t kHopFrames = 10;
constexpr uint32_t kSettleFrames = 12;

// leftmost and rightmost bg cell columns on a tile row whose id is in [lo, hi]
std::pair<int, int> glyph_span(const gb::Gameboy& gameboy, uint32_t row, uint8_t lo, uint8_t hi) {
    const std::span<const uint16_t> ids = gameboy.framebuffer_tiles();
    int left = -1;
    int right = -1;
    for (uint32_t cx = 0; cx < 20; ++cx) {
        const uint16_t id = ids[(row * 8 + 3) * gb::kLcdWidth + cx * 8 + 3];
        if ((id & 0x100u) != 0) {
            continue;
        }
        const uint8_t tile = static_cast<uint8_t>(id);
        if (tile >= lo && tile <= hi) {
            if (left < 0) {
                left = static_cast<int>(cx);
            }
            right = static_cast<int>(cx);
        }
    }
    return {left, right};
}

constexpr int kNoTile = -1;

// the first bg tile inside an 8x8 screen cell; sprites over the cell are stepped past
int bg_cell(const gb::Gameboy& gameboy, int cx, int cy) {
    const std::span<const uint16_t> ids = gameboy.framebuffer_tiles();
    if (cy < 0 || cy > 17 || cx < 0 || cx > 19) {
        return kNoTile;
    }
    for (int y = cy * 8; y < cy * 8 + 8; ++y) {
        for (int x = cx * 8; x < cx * 8 + 8; ++x) {
            const uint16_t id = ids[static_cast<size_t>(y) * gb::kLcdWidth + static_cast<size_t>(x)];
            if ((id & 0x100u) == 0) {
                return static_cast<uint8_t>(id);
            }
        }
    }
    return kNoTile;
}

struct Chick {
    int x;
    int y;
    bool found;
    bool hopping;
};

// top-left corner of the chick's lit sprite pixels
Chick chick_at(const gb::Gameboy& gameboy) {
    const std::span<const uint16_t> ids = gameboy.framebuffer_tiles();
    const std::span<const uint8_t> fb = gameboy.framebuffer();
    Chick c{0, 0, false, false};
    for (size_t i = 0; i < ids.size(); ++i) {
        const uint8_t tile = static_cast<uint8_t>(ids[i]);
        if ((ids[i] & 0x100u) == 0 || fb[i] == 0) {
            continue;
        }
        if (tile < kChickTileId || tile > kChickLastTileId) {
            continue;
        }
        const int x = static_cast<int>(i % gb::kLcdWidth);
        const int y = static_cast<int>(i / gb::kLcdWidth);
        if (!c.found || x < c.x) {
            c.x = x;
        }
        if (!c.found || y < c.y) {
            c.y = y;
        }
        c.hopping = c.hopping || tile >= kChickHopTileId;
        c.found = true;
    }
    return c;
}

// top-left corner of the eagle's lit sprite pixels, wherever the pair has reached
Chick eagle_at(const gb::Gameboy& gameboy) {
    const std::span<const uint16_t> ids = gameboy.framebuffer_tiles();
    const std::span<const uint8_t> fb = gameboy.framebuffer();
    Chick e{0, 0, false, false};
    for (size_t i = 0; i < ids.size(); ++i) {
        const uint8_t tile = static_cast<uint8_t>(ids[i]);
        if ((ids[i] & 0x100u) == 0 || fb[i] == 0) {
            continue;
        }
        if (tile < kEagleFirstTileId || tile > kEagleLastTileId) {
            continue;
        }
        const int x = static_cast<int>(i % gb::kLcdWidth);
        const int y = static_cast<int>(i / gb::kLcdWidth);
        if (!e.found || x < e.x) {
            e.x = x;
        }
        if (!e.found || y < e.y) {
            e.y = y;
        }
        e.found = true;
    }
    return e;
}

int chick_col(const Chick& c) {
    return (c.x - kCellInset) / kCellPx;
}

// chick_at reports the leftmost lit pixel, one in from the 8 px sprite's left edge
constexpr int kChickLitInset = 3;

double chick_center(const Chick& c) {
    return c.x + kChickLitInset;
}

// the grid cell the chick's center sits in; a ride leaves it between two columns
int chick_cell(const Chick& c) {
    const int cell = (c.x + kChickLitInset) / kCellPx;
    return cell < 0 ? 0 : (cell >= kGridCols ? kGridCols - 1 : cell);
}

// the bg tile of the grid cell k lanes ahead of the chick, in grid column gcol
int cell_tile(const gb::Gameboy& gameboy, const Chick& c, int k, int gcol) {
    // the chick sits higher in a water lane, so its lane is read off the 16 px band, not its inset
    const int row = (c.y / kCellPx) * 2 - 2 * k;
    for (int dr = 0; dr < 2; ++dr) {
        for (int dc = 0; dc < 2; ++dc) {
            const int v = bg_cell(gameboy, gcol * 2 + dc, row + dr);
            if (v != kNoTile) {
                return v;
            }
        }
    }
    return kNoTile;
}

bool cell_blocked(const gb::Gameboy& gameboy, const Chick& c, int k, int gcol) {
    return cell_tile(gameboy, c, k, gcol) == kTreeTileId;
}

// a cell the chick can simply stand on: neither a tree nor open water
bool cell_landable(const gb::Gameboy& gameboy, const Chick& c, int k, int gcol) {
    const int tile = cell_tile(gameboy, c, k, gcol);
    return tile != kTreeTileId && !is_water_tile(tile);
}

// water needs a log and rails need a timed window, so the dry land planner routes around both
bool cell_impassable(const gb::Gameboy& gameboy, const Chick& c, int k, int gcol) {
    const int tile = cell_tile(gameboy, c, k, gcol);
    return tile == kTreeTileId || is_water_tile(tile) || is_track_lane_tile(tile);
}

// one 8x8 cell of one of a lane's two tile rows, k lanes ahead of the chick
int lane_row_tile(const gb::Gameboy& gameboy, const Chick& c, int k, int dr, int cx) {
    return bg_cell(gameboy, cx, (c.y / kCellPx) * 2 - 2 * k + dr);
}

int stripe_cells_in_row(const gb::Gameboy& gameboy, const Chick& c, int k, int dr) {
    int n = 0;
    for (int cx = 0; cx < 20; ++cx) {
        n += lane_row_tile(gameboy, c, k, dr, cx) == kRoadStripeTileId ? 1 : 0;
    }
    return n;
}

// the lowest run of contiguous road lanes on screen, as its first lane and its length
struct LaneRun {
    int lo;
    int n;
};

LaneRun lane_run_on_screen(const gb::Gameboy& gameboy, const Chick& c, bool water) {
    for (int k = 0; k <= 6; ++k) {
        const int tile = cell_tile(gameboy, c, k, 0);
        if (water ? !is_water_tile(tile) : !is_road_tile(tile)) {
            continue;
        }
        int n = 0;
        while (k + n <= 6) {
            const int ahead = cell_tile(gameboy, c, k + n, 0);
            if (water ? !is_water_tile(ahead) : !is_road_tile(ahead)) {
                break;
            }
            ++n;
        }
        return LaneRun{k, n};
    }
    return LaneRun{-1, 0};
}

// the warning light's own 8x8 cell, k lanes ahead of the chick
int warn_cell(const gb::Gameboy& gameboy, const Chick& c, int k) {
    return bg_cell(gameboy, kWarnTileCol, (c.y / kCellPx) * 2 - 2 * k);
}

enum Move { kLeft = 0, kRight = 1, kUp = 2, kNoMove = -1 };

// the autopilot: breadth first over the visible lanes, then the first step toward the furthest
int plan_move(const gb::Gameboy& gameboy, const Chick& c, int lanes) {
    constexpr int kUnseen = -2;
    constexpr int kStart = -1;
    std::vector<int> first(static_cast<size_t>(lanes * kGridCols), kUnseen);
    std::vector<std::pair<int, int>> queue;
    const int gcol = chick_col(c);
    if (gcol < 0 || gcol >= kGridCols) {
        return kNoMove;
    }
    first[static_cast<size_t>(gcol)] = kStart;
    queue.push_back({0, gcol});

    int best = kNoMove;
    int best_lane = -1;
    for (size_t i = 0; i < queue.size(); ++i) {
        const int k = queue[i].first;
        const int col = queue[i].second;
        const int came = first[static_cast<size_t>(k * kGridCols + col)];
        if (k > best_lane) {
            best_lane = k;
            best = came;
        }
        constexpr int kStepLane[3] = {0, 0, 1};
        constexpr int kStepCol[3] = {-1, 1, 0};
        for (int m = 0; m < 3; ++m) {
            const int nk = k + kStepLane[m];
            const int nc = col + kStepCol[m];
            if (nk >= lanes || nc < 0 || nc >= kGridCols) {
                continue;
            }
            const size_t at = static_cast<size_t>(nk * kGridCols + nc);
            if (first[at] != kUnseen || cell_impassable(gameboy, c, nk, nc)) {
                continue;
            }
            first[at] = (came == kStart) ? m : came;
            queue.push_back({nk, nc});
        }
    }
    return best_lane > 0 ? best : kNoMove;
}

// one drawn hud digit: the glyph tile plus the extent of its sprite pixels
struct DigitRun {
    uint8_t digit;
    int x0;
    int x1;
    size_t pixels;
};

// the badge fills its whole cell, so neighbouring digits touch: a run is cut by the tile id
// changing or by the 8 px pitch running out, never by a blank column between badges
constexpr int kDigitPitchPx = 8;
// the badge is the top tile of an 8x16 pair, opaque end to end
constexpr size_t kDigitBadgePixels = 64;

std::vector<DigitRun> digit_runs(const gb::Gameboy& gameboy, size_t y0, size_t y1) {
    const std::span<const uint16_t> ids = gameboy.framebuffer_tiles();
    const std::span<const uint8_t> fb = gameboy.framebuffer();
    std::vector<DigitRun> runs;
    int open_tile = -1;
    for (size_t x = 0; x < gb::kLcdWidth; ++x) {
        int digit = -1;
        int glyph = -1;
        size_t pixels = 0;
        for (size_t y = y0; y < y1; ++y) {
            const size_t i = y * gb::kLcdWidth + x;
            const uint8_t tile = static_cast<uint8_t>(ids[i]);
            if ((ids[i] & 0x100u) == 0 || tile < kDigitTileId || tile > kDigitLastTileId || fb[i] == 0) {
                continue;
            }
            if (pixels == 0) {
                glyph = tile;
                // the glyph badge is the pair's top tile, so a digit is two tiles on from the last
                digit = (tile - kDigitTileId) / 2;
            }
            ++pixels;
        }
        if (digit < 0) {
            open_tile = -1;
            continue;
        }
        const bool full = !runs.empty() && runs.back().x1 - runs.back().x0 + 1 >= kDigitPitchPx;
        if (open_tile != glyph || full) {
            runs.push_back(
                DigitRun{static_cast<uint8_t>(digit), static_cast<int>(x), static_cast<int>(x), 0});
            open_tile = glyph;
        }
        runs.back().x1 = static_cast<int>(x);
        runs.back().pixels += pixels;
    }
    return runs;
}

// the hud's own band; the hover banner draws its best lower down, on the band's last row
std::vector<DigitRun> hud_digits(const gb::Gameboy& gameboy) {
    return digit_runs(gameboy, 0, kBestDigitTopPx);
}

std::vector<DigitRun> best_digits(const gb::Gameboy& gameboy) {
    return digit_runs(gameboy, kBestDigitTopPx, kBannerEndPx);
}

int digits_value(const std::vector<DigitRun>& runs) {
    int value = 0;
    if (runs.empty()) {
        return -1;
    }
    for (const DigitRun& run : runs) {
        value = value * 10 + run.digit;
    }
    return value;
}

int hud_score(const gb::Gameboy& gameboy) {
    return digits_value(hud_digits(gameboy));
}

// 1 when that screen column carries a digit sprite pixel anywhere in the hud's own band
bool hud_column_lit(const gb::Gameboy& gameboy, int x) {
    const std::span<const uint16_t> ids = gameboy.framebuffer_tiles();
    const std::span<const uint8_t> fb = gameboy.framebuffer();
    for (size_t y = 0; y < kBestDigitTopPx; ++y) {
        const size_t i = y * gb::kLcdWidth + static_cast<size_t>(x);
        const uint8_t tile = static_cast<uint8_t>(ids[i]);
        if ((ids[i] & 0x100u) != 0 && fb[i] != 0 && tile >= kDigitTileId && tile <= kDigitLastTileId) {
            return true;
        }
    }
    return false;
}

// nothing may eat a badge: every drawn digit keeps all 64 px of its cell
void require_hud_intact(const gb::Gameboy& gameboy, size_t want) {
    const std::vector<DigitRun> digits = hud_digits(gameboy);
    REQUIRE(digits.size() == want);
    for (const DigitRun& digit : digits) {
        REQUIRE(digit.pixels == kDigitBadgePixels);
        REQUIRE(digit.x1 - digit.x0 + 1 == kDigitPitchPx);
    }
}

void run(gb::Gameboy& gameboy, uint32_t frames) {
    for (uint32_t i = 0; i < frames; ++i) {
        gameboy.run_frame();
    }
}

// a two frame tap, then long enough for the whole hop to land
void tap(gb::Gameboy& gameboy, gb::Button button) {
    gameboy.set_button(button, true);
    gameboy.run_frame();
    gameboy.run_frame();
    gameboy.set_button(button, false);
    run(gameboy, kSettleFrames);
}

// entering hover blanks the lcd for a few frames, so settle before reading the screen
constexpr uint32_t kEnterPlayFrames = 8;

// the first press only unlocks the hover world; the banner is erased a lane a frame after it
void unlock(gb::Gameboy& gameboy) {
    gameboy.set_button(gb::Button::Start, true);
    gameboy.run_frame();
    gameboy.run_frame();
    gameboy.set_button(gb::Button::Start, false);
    run(gameboy, kEnterPlayFrames);
}

// boot seeds the world from a counter that has not run yet, so this is always the same world
void start_play(gb::Gameboy& gameboy, const std::vector<uint8_t>& rom) {
    REQUIRE(gameboy.load_rom(rom));
    run(gameboy, kBootFrames);
    unlock(gameboy);
}

gb::Button button_for(int move) {
    if (move == kLeft) {
        return gb::Button::Left;
    }
    if (move == kRight) {
        return gb::Button::Right;
    }
    return gb::Button::Up;
}

// a mover is 8 px tall at a 4 px lane inset, so it never crosses a 16 px band edge
struct CarRun {
    int band;
    int x0;
    int x1;
};

constexpr int kBands = 9;
constexpr int kBandPx = 16;

// run_frame spends a frame of cycles, not a whole lcd frame, so the buffer can straddle two of
// them and a moving mover reads at two x down one band. every family is full width across these
// two rows, and a tear almost never falls between two adjacent scanlines
constexpr int kProbeRow = 6;
constexpr int kProbeRows = 2;

// columns of one sprite family lit in a band; bridge spans the hole a rider punches in a log
std::vector<CarRun> runs_of(const gb::Gameboy& gameboy, uint8_t lo, uint8_t hi, int bridge) {
    const std::span<const uint16_t> ids = gameboy.framebuffer_tiles();
    const std::span<const uint8_t> fb = gameboy.framebuffer();
    std::vector<CarRun> runs;
    for (int band = 0; band < kBands; ++band) {
        int open = -1;
        int shut = -1;
        for (int x = 0; x <= static_cast<int>(gb::kLcdWidth) + bridge; ++x) {
            bool lit = false;
            for (int y = band * kBandPx + kProbeRow; y < band * kBandPx + kProbeRow + kProbeRows && !lit;
                 ++y) {
                if (x >= static_cast<int>(gb::kLcdWidth)) {
                    break;
                }
                const size_t i = static_cast<size_t>(y) * gb::kLcdWidth + static_cast<size_t>(x);
                const uint8_t tile = static_cast<uint8_t>(ids[i]);
                lit = (ids[i] & 0x100u) != 0 && fb[i] != 0 && tile >= lo && tile <= hi;
            }
            if (lit) {
                if (open < 0) {
                    open = x;
                }
                shut = x;
            } else if (open >= 0 && x - shut > bridge) {
                runs.push_back(CarRun{band, open, shut});
                open = -1;
            }
        }
    }
    return runs;
}

std::vector<CarRun> car_runs(const gb::Gameboy& gameboy) {
    return runs_of(gameboy, kCarFirstTileId, kCarLastTileId, 0);
}

// the chick's opaque pixels are six wide, so eight blank columns bridge a rider's hole
constexpr int kRiderBridge = 8;

std::vector<CarRun> log_runs(const gb::Gameboy& gameboy) {
    return runs_of(gameboy, kLogFirstTileId, kLogLastTileId, kRiderBridge);
}

// topmost and bottommost lit row of one sprite family inside a band, or {-1,-1}
std::pair<int, int> sprite_rows(const gb::Gameboy& gameboy, uint8_t lo, uint8_t hi, int band) {
    const std::span<const uint16_t> ids = gameboy.framebuffer_tiles();
    const std::span<const uint8_t> fb = gameboy.framebuffer();
    int top = -1;
    int bottom = -1;
    for (int y = band * kBandPx; y < band * kBandPx + kBandPx; ++y) {
        for (int x = 0; x < static_cast<int>(gb::kLcdWidth); ++x) {
            const size_t i = static_cast<size_t>(y) * gb::kLcdWidth + static_cast<size_t>(x);
            const uint8_t tile = static_cast<uint8_t>(ids[i]);
            if ((ids[i] & 0x100u) == 0 || fb[i] == 0 || tile < lo || tile > hi) {
                continue;
            }
            if (top < 0) {
                top = y - band * kBandPx;
            }
            bottom = y - band * kBandPx;
        }
    }
    return {top, bottom};
}

size_t chick_pixels(const gb::Gameboy& gameboy) {
    const std::span<const uint16_t> ids = gameboy.framebuffer_tiles();
    const std::span<const uint8_t> fb = gameboy.framebuffer();
    size_t lit = 0;
    for (size_t i = 0; i < ids.size(); ++i) {
        const uint8_t tile = static_cast<uint8_t>(ids[i]);
        if ((ids[i] & 0x100u) != 0 && fb[i] != 0 && tile >= kChickTileId && tile <= kChickLastTileId) {
            ++lit;
        }
    }
    return lit;
}

// any car, log or train sprite pixel anywhere inside a 16 px band
bool band_has_mover_pixels(const gb::Gameboy& gameboy, int band) {
    const std::span<const uint16_t> ids = gameboy.framebuffer_tiles();
    const std::span<const uint8_t> fb = gameboy.framebuffer();
    for (int y = band * kBandPx; y < band * kBandPx + kBandPx; ++y) {
        for (int x = 0; x < static_cast<int>(gb::kLcdWidth); ++x) {
            const size_t i = static_cast<size_t>(y) * gb::kLcdWidth + static_cast<size_t>(x);
            const uint8_t tile = static_cast<uint8_t>(ids[i]);
            if ((ids[i] & 0x100u) == 0 || fb[i] == 0) {
                continue;
            }
            if ((tile >= kCarFirstTileId && tile <= kLogLastTileId) ||
                (tile >= kTrainFirstTileId && tile <= kTrainLastTileId)) {
                return true;
            }
        }
    }
    return false;
}

// the danger kind a whole band's bg carries, or kNoTile; movers may hide a few of its cells
int band_danger_kind(const gb::Gameboy& gameboy, int band) {
    int road = 0;
    int water = 0;
    for (int cx = 0; cx < 20; ++cx) {
        const int tile = bg_cell(gameboy, cx, band * 2);
        road += is_road_tile(tile) ? 1 : 0;
        water += is_water_tile(tile) ? 1 : 0;
    }
    if (road >= 10) {
        return kRoadTileId;
    }
    return water >= 10 ? kWaterTileId : kNoTile;
}

// the train is one solid block, but a chick standing on the rails punches the same hole a rider does
std::vector<CarRun> train_runs(const gb::Gameboy& gameboy) {
    return runs_of(gameboy, kTrainFirstTileId, kTrainLastTileId, kRiderBridge);
}

// only the block's ends are sprites: 32 px of locomotive at the front, 16 px of rear behind
constexpr int kTrainHeadPx = 32;
constexpr int kTrainTailPx = 16;
// head to rear, longer than the 160 px screen by half again
constexpr int kTrainSpanPx = 256;

// columns of a band carrying a train sprite pixel; a solid block lights every column it covers
int train_columns(const gb::Gameboy& gameboy, int band) {
    const std::span<const uint16_t> ids = gameboy.framebuffer_tiles();
    const std::span<const uint8_t> fb = gameboy.framebuffer();
    int n = 0;
    for (int x = 0; x < static_cast<int>(gb::kLcdWidth); ++x) {
        bool lit = false;
        for (int y = band * kBandPx; y < band * kBandPx + kBandPx && !lit; ++y) {
            const size_t i = static_cast<size_t>(y) * gb::kLcdWidth + static_cast<size_t>(x);
            const uint8_t tile = static_cast<uint8_t>(ids[i]);
            lit =
                (ids[i] & 0x100u) != 0 && fb[i] != 0 && tile >= kTrainFirstTileId && tile <= kTrainLastTileId;
        }
        n += lit ? 1 : 0;
    }
    return n;
}

// the whole train: its two sprite ends plus the carriages terrain lays down as bg between them
std::vector<bool> train_cover(const gb::Gameboy& gameboy, int band) {
    const std::span<const uint16_t> ids = gameboy.framebuffer_tiles();
    const std::span<const uint8_t> fb = gameboy.framebuffer();
    std::vector<bool> cover(gb::kLcdWidth, false);
    for (int x = 0; x < static_cast<int>(gb::kLcdWidth); ++x) {
        for (int y = band * kBandPx + kProbeRow;
             y < band * kBandPx + kProbeRow + kProbeRows && !cover[static_cast<size_t>(x)]; ++y) {
            const size_t i = static_cast<size_t>(y) * gb::kLcdWidth + static_cast<size_t>(x);
            const uint8_t tile = static_cast<uint8_t>(ids[i]);
            if ((ids[i] & 0x100u) != 0) {
                cover[static_cast<size_t>(x)] =
                    fb[i] != 0 && tile >= kTrainFirstTileId && tile <= kTrainLastTileId;
            } else {
                cover[static_cast<size_t>(x)] = is_train_body_tile(tile);
            }
        }
    }
    return cover;
}

// leftmost and rightmost covered column of the band, or {-1,-1} when the train is nowhere near
std::pair<int, int> train_span(const gb::Gameboy& gameboy, int band) {
    const std::vector<bool> cover = train_cover(gameboy, band);
    int x0 = -1;
    int x1 = -1;
    for (int x = 0; x < static_cast<int>(gb::kLcdWidth); ++x) {
        if (!cover[static_cast<size_t>(x)]) {
            continue;
        }
        if (x0 < 0) {
            x0 = x;
        }
        x1 = x;
    }
    return {x0, x1};
}

bool train_in_band(const gb::Gameboy& gameboy, int band) {
    return train_span(gameboy, band).first >= 0;
}

// a camera creep slides the lanes off the 16 px band grid for eight frames, and tracks come singly:
// so a sweep watched over a long crossing is followed across the whole screen, not one named band
std::vector<bool> train_cover_on_screen(const gb::Gameboy& gameboy) {
    std::vector<bool> cover(gb::kLcdWidth, false);
    for (int band = 0; band < kBands; ++band) {
        const std::vector<bool> in_band = train_cover(gameboy, band);
        for (size_t x = 0; x < cover.size(); ++x) {
            cover[x] = cover[x] || in_band[x];
        }
    }
    return cover;
}

// span within one 16 px lane band, so a second visible track cannot bleed in
std::pair<int, int> train_span_in_band(const gb::Gameboy& gameboy, int band) {
    const std::vector<bool> cover = train_cover(gameboy, band);
    int x0 = -1;
    int x1 = -1;
    for (int x = 0; x < static_cast<int>(gb::kLcdWidth); ++x) {
        if (!cover[static_cast<size_t>(x)]) {
            continue;
        }
        if (x0 < 0) {
            x0 = x;
        }
        x1 = x;
    }
    return {x0, x1};
}

std::pair<int, int> train_span_on_screen(const gb::Gameboy& gameboy) {
    const std::vector<bool> cover = train_cover_on_screen(gameboy);
    int x0 = -1;
    int x1 = -1;
    for (int x = 0; x < static_cast<int>(gb::kLcdWidth); ++x) {
        if (!cover[static_cast<size_t>(x)]) {
            continue;
        }
        if (x0 < 0) {
            x0 = x;
        }
        x1 = x;
    }
    return {x0, x1};
}

// run_frame spends a frame of cycles, not a whole lcd frame, so the buffer can straddle two of
// them: a band split that way reads one mover at two x, wider than its own art, and is no sample
bool run_is_torn(const CarRun& run, int width) {
    return run.x1 - run.x0 + 1 > width;
}

bool band_is_torn(const gb::Gameboy& gameboy, uint8_t lo, uint8_t hi, int bridge, int band, int width) {
    for (const CarRun& run : runs_of(gameboy, lo, hi, bridge)) {
        if (run.band == band && run_is_torn(run, width)) {
            return true;
        }
    }
    return false;
}

// the game centers a car on its oam x, so an unclipped run of 16 gives that number back
double car_center(const CarRun& run) {
    if (run.x0 == 0 && run.x1 - run.x0 != 15) {
        return run.x1 - 7;
    }
    return run.x0 + 8;
}

// a log is 24 px centered on its track x; a run clipped at the left edge is read from its right
constexpr int kLogPx = 24;

double log_center(const CarRun& run) {
    if (run.x0 == 0 && run.x1 - run.x0 + 1 < kLogPx) {
        return run.x1 - (kLogPx / 2 - 1);
    }
    return run.x0 + kLogPx / 2;
}

// the two movers of a lane sit half a lap apart, so half a lap describes the whole lane
constexpr double kCarPeriod = 128.0; // cars ride the free 256 px lap
constexpr double kLogPeriod = 96.0;  // logs ride a 192 px one, so they come round sooner

double phase_delta(double a, double b, double period) {
    double d = std::fmod(a - b, period);
    if (d < -period / 2) {
        d += period;
    }
    if (d >= period / 2) {
        d -= period;
    }
    return d;
}

constexpr double kNoPhase = -1000.0;

constexpr int kCarPx = 16;

double lane_phase(const gb::Gameboy& gameboy, int band) {
    if (band_is_torn(gameboy, kCarFirstTileId, kCarLastTileId, 0, band, kCarPx)) {
        return kNoPhase;
    }
    for (const CarRun& run : car_runs(gameboy)) {
        if (run.band == band) {
            return std::fmod(car_center(run), kCarPeriod);
        }
    }
    return kNoPhase;
}

// either log of a lane gives the same phase, so the first one sighted is enough
double log_phase(const gb::Gameboy& gameboy, int band) {
    if (band_is_torn(gameboy, kLogFirstTileId, kLogLastTileId, kRiderBridge, band, kLogPx)) {
        return kNoPhase;
    }
    for (const CarRun& run : log_runs(gameboy)) {
        if (run.band == band) {
            return std::fmod(log_center(run), kLogPeriod);
        }
    }
    return kNoPhase;
}

int chick_band(const Chick& c) {
    return c.y / kBandPx;
}

// contiguous road lanes ahead of the chick, counted from the next lane up
int road_chunk_ahead(const gb::Gameboy& gameboy, const Chick& c, int gcol) {
    int n = 0;
    while (n < 4 && is_road_tile(cell_tile(gameboy, c, n + 1, gcol))) {
        ++n;
    }
    return n;
}

// one tap is two press frames plus the settle, and the hop commits on the first of them
constexpr double kTapFrames = 14.0;
constexpr uint32_t kMeasureFrames = 16;
// the game kills at 10 px; the plan keeps this much clearance through the whole crossing
constexpr double kPlanMargin = 18.0;
// and the target lane is at least this clear at the moment the hop into it starts
constexpr double kHopMargin = 24.0;

struct Traffic {
    double phase;
    double v;
};

bool burst_is_safe(const std::vector<Traffic>& lanes, double x) {
    for (size_t i = 0; i < lanes.size(); ++i) {
        const double enter = static_cast<double>(i) * kTapFrames;
        const double leave = enter + kTapFrames;
        if (std::fabs(phase_delta(lanes[i].phase + lanes[i].v * enter, x, kCarPeriod)) < kHopMargin) {
            return false;
        }
        for (double t = enter - 4; t <= leave + 6; t += 1.0) {
            if (std::fabs(phase_delta(lanes[i].phase + lanes[i].v * t, x, kCarPeriod)) < kPlanMargin) {
                return false;
            }
        }
    }
    return true;
}

// speeds are eighths of a pixel per frame, so the sample snaps back to the exact one
double snap_speed(double v) {
    return std::round(v * 8.0) / 8.0;
}

bool read_traffic(const gb::Gameboy& gameboy, const Chick& c, int n, std::vector<double>& out) {
    out.clear();
    for (int k = 1; k <= n; ++k) {
        const double phase = lane_phase(gameboy, chick_band(c) - k);
        if (phase == kNoPhase) {
            return false;
        }
        out.push_back(phase);
    }
    return true;
}

constexpr int kBurstAttempts = 60;
constexpr uint32_t kBurstWaitFrames = 4;

// crosses a whole road chunk in one burst, so the chick never idles on asphalt
bool try_burst(gb::Gameboy& gameboy, int n) {
    Chick c = chick_at(gameboy);
    std::vector<double> first;
    std::vector<double> second;
    std::vector<Traffic> lanes;
    if (!c.found || !read_traffic(gameboy, c, n, first)) {
        return false;
    }
    // two samples of the same safe grass lane give every road lane its speed
    run(gameboy, kMeasureFrames);
    c = chick_at(gameboy);
    if (!c.found || !read_traffic(gameboy, c, n, second)) {
        return false;
    }
    for (int i = 0; i < n; ++i) {
        lanes.push_back(Traffic{second[static_cast<size_t>(i)],
                                snap_speed(phase_delta(second[static_cast<size_t>(i)],
                                                       first[static_cast<size_t>(i)], kCarPeriod) /
                                           kMeasureFrames)});
    }

    for (int attempt = 0; attempt < kBurstAttempts; ++attempt) {
        c = chick_at(gameboy);
        if (!c.found || !read_traffic(gameboy, c, n, second)) {
            return false;
        }
        for (int i = 0; i < n; ++i) {
            lanes[static_cast<size_t>(i)].phase = second[static_cast<size_t>(i)];
        }
        if (burst_is_safe(lanes, chick_col(c) * kCellPx + 8)) {
            for (int i = 0; i <= n; ++i) {
                tap(gameboy, gb::Button::Up);
            }
            return true;
        }
        run(gameboy, kBurstWaitFrames);
    }
    return false;
}

// the nearest column whose chunk exit is clear, reachable along this lane
int sidestep_move(const gb::Gameboy& gameboy, const Chick& c, int gcol, int n) {
    for (int d = 1; d < kGridCols; ++d) {
        for (int s = 0; s < 2; ++s) {
            const int j = s == 0 ? gcol - d : gcol + d;
            const int step = j > gcol ? 1 : -1;
            bool clear = j >= 0 && j < kGridCols && !cell_impassable(gameboy, c, n + 1, j);
            for (int t = gcol + step; clear && t != j + step; t += step) {
                clear = !cell_blocked(gameboy, c, 0, t);
            }
            if (clear) {
                return step > 0 ? kRight : kLeft;
            }
        }
    }
    return kNoMove;
}

// a step to either side of a safe lane, keeping the chunk's exit column clear
bool shuffle_column(gb::Gameboy& gameboy, const Chick& c, int gcol, int n) {
    for (int s = 0; s < 2; ++s) {
        const int j = s == 0 ? gcol + 1 : gcol - 1;
        if (j < 0 || j >= kGridCols || cell_blocked(gameboy, c, 0, j) ||
            cell_impassable(gameboy, c, n + 1, j)) {
            continue;
        }
        tap(gameboy, j > gcol ? gb::Button::Right : gb::Button::Left);
        return true;
    }
    return false;
}

// logs are slower than cars, so a longer baseline keeps the sampled speed on a whole 32nd
constexpr uint32_t kLogMeasureFrames = 24;
// the hop commits on the tap's first frame and its slide lands eight frames later
constexpr double kHopArrivalFrames = 9.0;
// the game rides within 12 px of a log center; the plan hops with six of those to spare
constexpr double kRideCatchPx = 12.0;
constexpr double kRideMargin = 6.0;
// the game kills a ride that reaches these, so a landing must leave room downstream of it
constexpr double kRideLeftEdge = 4.0;
constexpr double kRideRightEdge = 156.0;
constexpr double kLandRoom = 60.0;
// past these the ride is nearly over, so the chick gives up and heads back
constexpr double kBailLo = 20.0;
constexpr double kBailHi = 140.0;
// the boarding column: upstream of the drift, so the whole screen is left to ride across
constexpr int kBoardColLeft = 3;
constexpr int kBoardColRight = 6;
// one log passes a fixed x every 128/speed frames, so a wait is at most a few hundred
constexpr int kRideAttempts = 800;
constexpr int kRetreatAttempts = 400;

// two samples of the lane give its logs their shared speed, in whole 32nds of a pixel
bool measure_logs(gb::Gameboy& gameboy, int band, double& v) {
    const double before = log_phase(gameboy, band);
    if (before == kNoPhase) {
        return false;
    }
    run(gameboy, kLogMeasureFrames);
    const double after = log_phase(gameboy, band);
    if (after == kNoPhase) {
        return false;
    }
    v = std::round(phase_delta(after, before, kLogPeriod) / kLogMeasureFrames * 32.0) / 32.0;
    return v != 0.0;
}

// water needs room to drift, so the chick lines up in a middle column before it boards
void approach_column(gb::Gameboy& gameboy, int want) {
    for (int guard = 0; guard < kGridCols; ++guard) {
        const Chick c = chick_at(gameboy);
        if (!c.found) {
            return;
        }
        const int gcol = chick_cell(c);
        if (gcol == want) {
            return;
        }
        const int step = want > gcol ? 1 : -1;
        if (cell_blocked(gameboy, c, 0, gcol + step)) {
            return;
        }
        tap(gameboy, step > 0 ? gb::Button::Right : gb::Button::Left);
    }
}

// contiguous water lanes ahead of the chick, counted from the next lane up
int water_chunk_ahead(const gb::Gameboy& gameboy, const Chick& c) {
    int n = 0;
    while (n < 3 && is_water_tile(cell_tile(gameboy, c, n + 1, 0))) {
        ++n;
    }
    return n;
}

// one lane forward (dk 1) or back (dk -1), taken as soon as the landing is a clear column or a log
bool hop_lane(gb::Gameboy& gameboy, int dk, double v, int budget) {
    const Chick from = chick_at(gameboy);
    if (!from.found) {
        return false;
    }
    // the landing lands within a log's width of the chick, so its own column caps the ride's room
    const double reach = v > 0 ? kRideRightEdge - chick_center(from) : chick_center(from) - kRideLeftEdge;
    const double need = std::min(kLandRoom, reach - kRideCatchPx);

    for (int attempt = 0; attempt < budget; ++attempt) {
        const Chick c = chick_at(gameboy);
        if (!c.found) {
            return false;
        }
        const int gcol = chick_cell(c);
        const double cx = chick_center(c);
        bool go = !is_water_tile(cell_tile(gameboy, c, dk, gcol)) && !cell_blocked(gameboy, c, dk, gcol);
        // log motion is linear, so one speed and one sighting place it at the hop's arrival
        for (const CarRun& log : log_runs(gameboy)) {
            if (go || log.band != chick_band(c) - dk) {
                continue;
            }
            const double at = log_center(log) + v * kHopArrivalFrames;
            // and the landing must leave room downstream, or the ride ends at the screen edge
            const double room = v > 0 ? kRideRightEdge - at : at - kRideLeftEdge;
            go = std::fabs(at - cx) <= kRideCatchPx - kRideMargin && room >= need;
        }
        if (go) {
            tap(gameboy, dk > 0 ? gb::Button::Up : gb::Button::Down);
            return true;
        }
        if (is_water_tile(cell_tile(gameboy, c, 0, gcol)) && (cx < kBailLo || cx > kBailHi)) {
            return false;
        }
        gameboy.run_frame();
    }
    return false;
}

// crosses a whole water chunk: every speed read from the bank, then one log at a time
bool water_cross(gb::Gameboy& gameboy) {
    const Chick start = chick_at(gameboy);
    if (!start.found) {
        return false;
    }
    // a failed exit can leave the chick afloat, with no measurement to hand
    if (is_water_tile(cell_tile(gameboy, start, 0, chick_cell(start)))) {
        double afloat_v = 0;
        if (!is_water_tile(cell_tile(gameboy, start, 1, chick_cell(start)))) {
            return hop_lane(gameboy, 1, 0, kRideAttempts);
        }
        return measure_logs(gameboy, chick_band(start) - 1, afloat_v) &&
               hop_lane(gameboy, 1, afloat_v, kRideAttempts);
    }

    const int n = water_chunk_ahead(gameboy, start);
    std::vector<double> v;
    for (int k = 0; k < n; ++k) {
        double lane_v = 0;
        // measured from the bank, where a long baseline costs the chick nothing
        if (!measure_logs(gameboy, chick_band(start) - 1 - k, lane_v)) {
            return false;
        }
        v.push_back(lane_v);
    }
    if (v.empty()) {
        return false;
    }
    approach_column(gameboy, v.front() > 0 ? kBoardColLeft : kBoardColRight);

    for (int k = 0; k < n; ++k) {
        if (hop_lane(gameboy, 1, v[static_cast<size_t>(k)], kRideAttempts)) {
            continue;
        }
        // carried too far with nothing lined up: back to the bank, log by log, and start over
        for (int back = k; back > 0; --back) {
            if (!hop_lane(gameboy, -1, back > 1 ? v[static_cast<size_t>(back - 2)] : 0.0, kRetreatAttempts)) {
                return false;
            }
        }
        return k > 0;
    }
    // dry land ahead, so the exit only waits on a column clear of trees
    return hop_lane(gameboy, 1, 0, kRideAttempts);
}

// the warning blinks every 15 frames, so a light that holds its crossbuck this long is quiet
constexpr uint32_t kQuietProof = 20;
// a whole warning plus a whole sweep is barely a hundred frames, well short of the eagle's patience
constexpr uint32_t kQuietWaitFrames = 320;

bool track_looks_quiet(const gb::Gameboy& gameboy, int k) {
    const Chick c = chick_at(gameboy);
    return c.found && warn_cell(gameboy, c, k) == kRailWarnTileId &&
           !train_in_band(gameboy, chick_band(c) - k);
}

// holds still until the track k lanes ahead has proved itself between sweeps
bool wait_for_quiet(gb::Gameboy& gameboy, int k) {
    uint32_t held = 0;
    for (uint32_t frame = 0; frame < kQuietWaitFrames; ++frame) {
        if (!chick_at(gameboy).found) {
            return false;
        }
        held = track_looks_quiet(gameboy, k) ? held + 1 : 0;
        if (held >= kQuietProof) {
            return true;
        }
        gameboy.run_frame();
    }
    return false;
}

// rails are grass on a timer: crossed only from a proven quiet light, and never stood on after
bool track_cross(gb::Gameboy& gameboy) {
    const Chick c = chick_at(gameboy);
    if (!c.found) {
        return false;
    }
    const int gcol = chick_cell(c);
    // the lane past a track is always grass, so the crossing only needs a column clear of trees
    if (cell_impassable(gameboy, c, 2, gcol)) {
        const int move = sidestep_move(gameboy, c, gcol, 1);
        if (move == kNoMove) {
            return false;
        }
        tap(gameboy, button_for(move));
        return true;
    }
    if (!wait_for_quiet(gameboy, 1)) {
        return false;
    }
    tap(gameboy, gb::Button::Up);
    tap(gameboy, gb::Button::Up);
    return true;
}

// one autopilot action: a plain hop, a sidestep, a road chunk crossed at once, or a river ridden
bool autopilot_step(gb::Gameboy& gameboy) {
    const Chick c = chick_at(gameboy);
    if (!c.found) {
        return false;
    }
    const int gcol = chick_cell(c);
    if (is_track_lane_tile(cell_tile(gameboy, c, 0, gcol))) {
        // standing on rails is never safe, so the exit hop outranks anything the planner wants
        if (cell_landable(gameboy, c, 1, gcol)) {
            tap(gameboy, gb::Button::Up);
            return true;
        }
        const int move = sidestep_move(gameboy, c, gcol, 0);
        if (move == kNoMove) {
            return false;
        }
        tap(gameboy, button_for(move));
        return true;
    }
    if (is_track_lane_tile(cell_tile(gameboy, c, 1, gcol))) {
        return track_cross(gameboy);
    }
    if (is_water_tile(cell_tile(gameboy, c, 0, gcol)) || is_water_tile(cell_tile(gameboy, c, 1, gcol))) {
        return water_cross(gameboy);
    }
    const int n = road_chunk_ahead(gameboy, c, gcol);
    if (n == 0) {
        const int move = plan_move(gameboy, c, 7);
        if (move == kNoMove) {
            return false;
        }
        tap(gameboy, button_for(move));
        return true;
    }
    if (cell_blocked(gameboy, c, n + 1, gcol)) {
        const int move = sidestep_move(gameboy, c, gcol, n);
        if (move == kNoMove) {
            return false;
        }
        tap(gameboy, button_for(move));
        return true;
    }
    if (try_burst(gameboy, n)) {
        return true;
    }
    // no window from this column; another one lines the lanes up differently
    return shuffle_column(gameboy, c, gcol, n);
}

// steers up the guaranteed path until the hud reaches target or the steps run out
int autopilot(gb::Gameboy& gameboy, int target, int steps) {
    for (int i = 0; i < steps; ++i) {
        if (!autopilot_step(gameboy)) {
            break;
        }
        if (hud_score(gameboy) >= target) {
            break;
        }
    }
    return hud_score(gameboy);
}

// autopilots up to the bank, then catches the first log that comes by and stops there
bool board_a_log(gb::Gameboy& gameboy, int steps) {
    for (int i = 0; i < steps; ++i) {
        const Chick c = chick_at(gameboy);
        if (!c.found) {
            return false;
        }
        if (is_water_tile(cell_tile(gameboy, c, 0, chick_cell(c)))) {
            return true;
        }
        if (!is_water_tile(cell_tile(gameboy, c, 1, chick_cell(c)))) {
            if (!autopilot_step(gameboy)) {
                return false;
            }
            continue;
        }
        // water_cross would carry straight on over the whole chunk, so board by hand
        double v = 0;
        if (!measure_logs(gameboy, chick_band(c) - 1, v)) {
            return false;
        }
        approach_column(gameboy, v > 0 ? kBoardColLeft : kBoardColRight);
        if (!hop_lane(gameboy, 1, v, kRideAttempts)) {
            return false;
        }
    }
    return false;
}

// autopilots up to the bank of the first track it meets and stops on the lane below it
bool walk_to_track(gb::Gameboy& gameboy, int steps) {
    for (int i = 0; i < steps; ++i) {
        const Chick c = chick_at(gameboy);
        if (!c.found) {
            return false;
        }
        if (is_track_lane_tile(cell_tile(gameboy, c, 1, chick_cell(c)))) {
            return true;
        }
        if (!autopilot_step(gameboy)) {
            return false;
        }
    }
    return false;
}

// sidesteps along the current lane until the column past the track is one the chick can land on
bool line_up_past_track(gb::Gameboy& gameboy) {
    for (int guard = 0; guard < kGridCols; ++guard) {
        const Chick c = chick_at(gameboy);
        if (!c.found) {
            return false;
        }
        if (cell_landable(gameboy, c, 2, chick_cell(c))) {
            return true;
        }
        const int move = sidestep_move(gameboy, c, chick_cell(c), 1);
        if (move == kNoMove) {
            return false;
        }
        tap(gameboy, button_for(move));
    }
    return false;
}

// how many warning cells the whole two row lane carries; the light is a single cell
int warn_cells_in_lane(const gb::Gameboy& gameboy, const Chick& c, int k) {
    const int row = (c.y / kCellPx) * 2 - 2 * k;
    int n = 0;
    for (int dr = 0; dr < 2; ++dr) {
        for (int cx = 0; cx < 20; ++cx) {
            n += bg_cell(gameboy, cx, row + dr) == kRailWarnTileId ? 1 : 0;
        }
    }
    return n;
}

// holds still until the light k lanes ahead drops its crossbuck: the warning has begun
bool wait_for_blink(gb::Gameboy& gameboy, int k, uint32_t budget) {
    for (uint32_t frame = 0; frame < budget; ++frame) {
        const Chick c = chick_at(gameboy);
        if (!c.found) {
            return false;
        }
        if (warn_cell(gameboy, c, k) == kRailTileId) {
            return true;
        }
        gameboy.run_frame();
    }
    return false;
}

// holds still until the sweep itself is on screen k lanes ahead
bool wait_for_train(gb::Gameboy& gameboy, int k, uint32_t budget) {
    for (uint32_t frame = 0; frame < budget; ++frame) {
        const Chick c = chick_at(gameboy);
        if (!c.found) {
            return false;
        }
        if (train_in_band(gameboy, chick_band(c) - k)) {
            return true;
        }
        gameboy.run_frame();
    }
    return false;
}

// the chick wears its hop frame through every slide, a camera creep's included, and bands only
// line up with lanes between them
bool world_is_settled(const gb::Gameboy& gameboy) {
    const Chick c = chick_at(gameboy);
    return c.found && !c.hopping;
}

// a carriage only ever covers a track lane, so a band carrying one carries nothing else but rails
bool carriages_only_on_rails(const gb::Gameboy& gameboy) {
    if (!world_is_settled(gameboy)) {
        return true;
    }
    for (int band = 0; band < kBands; ++band) {
        bool carriage = false;
        for (int cx = 0; cx < 20 && !carriage; ++cx) {
            carriage = is_train_body_tile(bg_cell(gameboy, cx, band * 2)) ||
                       is_train_body_tile(bg_cell(gameboy, cx, band * 2 + 1));
        }
        if (!carriage) {
            continue;
        }
        for (int cx = 0; cx < 20; ++cx) {
            const int tile = bg_cell(gameboy, cx, band * 2);
            if (tile != kNoTile && !is_train_body_tile(tile) && !is_track_tile(tile)) {
                return false;
            }
        }
    }
    return true;
}

// every cell of the play area, minus the ones a sprite fully covers
bool play_area_is_all_terrain(const gb::Gameboy& gameboy) {
    for (int cy = 0; cy < 18; ++cy) {
        for (int cx = 0; cx < 20; ++cx) {
            const int tile = bg_cell(gameboy, cx, cy);
            if (tile != kNoTile && !is_grass_tile(tile) && tile != kTreeTileId && !is_road_tile(tile) &&
                !is_water_tile(tile) && !is_track_tile(tile) && !is_train_body_tile(tile)) {
                return false;
            }
        }
    }
    return true;
}

// gbdk's ibm font lands ascii 0x20-0x7f on tiles 0x00-0x5f
constexpr uint8_t font_tile(char c) {
    return static_cast<uint8_t>(c - 0x20);
}

// the popup writes the inverted copy of the same glyph
constexpr uint8_t popup_tile(char c) {
    return static_cast<uint8_t>(kInvFontFirstTile + font_tile(c));
}

bool bg_has_tile(const gb::Gameboy& gameboy, uint8_t tile) {
    for (uint16_t id : gameboy.framebuffer_tiles()) {
        if ((id & 0x100u) == 0 && static_cast<uint8_t>(id) == tile) {
            return true;
        }
    }
    return false;
}

bool row_has_tile(const gb::Gameboy& gameboy, uint32_t row, uint8_t tile) {
    const std::span<const uint16_t> ids = gameboy.framebuffer_tiles();
    for (size_t y = row * 8; y < row * 8 + 8; ++y) {
        for (size_t x = 0; x < gb::kLcdWidth; ++x) {
            const uint16_t id = ids[y * gb::kLcdWidth + x];
            if ((id & 0x100u) == 0 && static_cast<uint8_t>(id) == tile) {
                return true;
            }
        }
    }
    return false;
}

// the hover banner and the game over popup are both inverted font bands over the world
bool inverted_band_shown(const gb::Gameboy& gameboy) {
    for (uint16_t id : gameboy.framebuffer_tiles()) {
        if ((id & 0x100u) != 0) {
            continue;
        }
        const uint8_t tile = static_cast<uint8_t>(id);
        if (tile >= kInvFontFirstTile && tile <= kInvFontLastTile) {
            return true;
        }
    }
    return false;
}

// only the popup says GAME OVER, and no banner line carries a v
bool popup_shown(const gb::Gameboy& gameboy) {
    return bg_has_tile(gameboy, popup_tile('V'));
}

bool hover_shown(const gb::Gameboy& gameboy) {
    return inverted_band_shown(gameboy) && !popup_shown(gameboy);
}

// runs frames until the popup lands, or gives up
bool wait_for_death(gb::Gameboy& gameboy, uint32_t budget) {
    for (uint32_t frame = 0; frame < budget; ++frame) {
        if (popup_shown(gameboy)) {
            return true;
        }
        gameboy.run_frame();
    }
    return popup_shown(gameboy);
}

constexpr size_t kSaveBestOffset = 4;

uint16_t sram_best(std::span<const uint8_t> ram) {
    return static_cast<uint16_t>(ram[kSaveBestOffset] | (ram[kSaveBestOffset + 1] << 8));
}

bool sram_has_magic(std::span<const uint8_t> ram) {
    return ram.size() > kSaveBestOffset + 1 && ram[0] == 'C' && ram[1] == 'R' && ram[2] == 'S' &&
           ram[3] == 'Y';
}

void press(gb::Gameboy& gameboy, gb::Button button, uint32_t frames) {
    gameboy.set_button(button, true);
    run(gameboy, frames);
    gameboy.set_button(button, false);
}

// the popup ignores input for a lockout, so wait it out before the dismissing press
void dismiss(gb::Gameboy& gameboy, gb::Button button) {
    run(gameboy, kLockoutFrames + 4);
    press(gameboy, button, 2);
    // twice the usual settle: the lcd is off while the hover world is generated
    run(gameboy, 2 * kEnterPlayFrames);
}

// a blind dash up the boot world, which is fixed, so this dies on the same frame every time
void quick_death(gb::Gameboy& gameboy) {
    for (int step = 0; step < 30 && !popup_shown(gameboy); ++step) {
        tap(gameboy, gb::Button::Up);
    }
    for (uint32_t frame = 0; frame < 1500 && !popup_shown(gameboy); ++frame) {
        gameboy.run_frame();
    }
    REQUIRE(popup_shown(gameboy));
}

// the seed is sampled at hover entry, so a fixed death plus a chosen wait picks the world
void enter_world(gb::Gameboy& gameboy, const std::vector<uint8_t>& rom, uint32_t wait) {
    start_play(gameboy, rom);
    quick_death(gameboy);
    run(gameboy, wait);
    dismiss(gameboy, gb::Button::A);
    unlock(gameboy);
}

// hops onto the next road lane and stands there until the traffic finds the chick
void die_under_a_car(gb::Gameboy& gameboy, int steps) {
    for (int i = 0; i < steps; ++i) {
        const Chick c = chick_at(gameboy);
        REQUIRE(c.found);
        if (road_chunk_ahead(gameboy, c, chick_col(c)) > 0) {
            tap(gameboy, gb::Button::Up);
            break;
        }
        REQUIRE(autopilot_step(gameboy));
    }
    for (uint32_t i = 0; i < 900; ++i) {
        gameboy.run_frame();
        if (popup_shown(gameboy)) {
            run(gameboy, 6);
            return;
        }
    }
    FAIL("no car ever reached the chick");
}

// the base tier tops a car out at a whole pixel a frame; the ramp is what beats it
constexpr double kBaseCarMax = 1.0;
constexpr double kSpeedTol = 0.05;

// the quickest car of any visible road lane, in px per frame; -1 when the camera moved mid sample
double fastest_car(gb::Gameboy& gameboy) {
    std::vector<double> before;
    for (int band = 0; band < kBands; ++band) {
        before.push_back(lane_phase(gameboy, band));
    }
    const Chick from = chick_at(gameboy);
    run(gameboy, kMeasureFrames);
    const Chick to = chick_at(gameboy);
    // a creep mid sample slides every lane down a band, so the pairing would be nonsense
    if (!from.found || !to.found || from.y != to.y) {
        return -1.0;
    }
    double best = 0;
    for (int band = 0; band < kBands; ++band) {
        const double now = lane_phase(gameboy, band);
        const double was = before[static_cast<size_t>(band)];
        if (was == kNoPhase || now == kNoPhase) {
            continue;
        }
        best = std::max(best, std::fabs(snap_speed(phase_delta(now, was, kCarPeriod) / kMeasureFrames)));
    }
    return best;
}

// peak to peak, not peak: a triggered channel at volume zero still sits at its dac floor
int32_t audio_swing(gb::Gameboy& gameboy, uint32_t frames) {
    std::array<int16_t, 8192> buffer{};
    int16_t high = std::numeric_limits<int16_t>::min();
    int16_t low = std::numeric_limits<int16_t>::max();
    for (uint32_t i = 0; i < frames; ++i) {
        gameboy.run_frame();
        const size_t got = gameboy.read_audio(buffer);
        for (size_t s = 0; s < got; ++s) {
            high = std::max(high, buffer[s]);
            low = std::min(low, buffer[s]);
        }
    }
    return high < low ? 0 : static_cast<int32_t>(high) - static_cast<int32_t>(low);
}

// measured locally: a decayed channel holds one dc level, a hop swings past 6000, a hit past 30000
constexpr int32_t kSilentSwing = 512;
constexpr int32_t kHopSwing = 1000;
constexpr int32_t kDeathSwing = 4000;

void drain_audio(gb::Gameboy& gameboy) {
    std::array<int16_t, 8192> buffer{};
    while (gameboy.read_audio(buffer) != 0) {
    }
}

std::vector<int> play_grid(const gb::Gameboy& gameboy) {
    std::vector<int> grid;
    for (int cy = 0; cy < 18; ++cy) {
        for (int cx = 0; cx < 20; ++cx) {
            grid.push_back(bg_cell(gameboy, cx, cy));
        }
    }
    return grid;
}

} // namespace

TEST_CASE("crossy_rom_declares_an_mbc1_battery_cart") {
    const std::vector<uint8_t> rom = read_crossy_rom();
    REQUIRE(rom.size() == 32768u);

    std::string why;
    auto cart = gb::Cartridge::parse(rom, &why);
    REQUIRE(why.empty());
    REQUIRE(cart.has_value());
    REQUIRE(cart->type() == gb::CartType::Mbc1);
    REQUIRE(cart->has_battery());
    REQUIRE(cart->ram_size() > 0u);
    REQUIRE(cart->title() == "CROSSY");
}

TEST_CASE("crossy_boots_to_a_non_blank_title_card") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    gb::Gameboy gameboy;
    REQUIRE(gameboy.load_rom(rom));
    for (uint32_t i = 0; i < kBootFrames; ++i) {
        gameboy.run_frame();
    }
    REQUIRE(count_lit_pixels(gameboy.framebuffer()) > 100u);
}

TEST_CASE("crossy_boot_is_deterministic") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    gb::Gameboy first;
    gb::Gameboy second;
    REQUIRE(first.load_rom(rom));
    REQUIRE(second.load_rom(rom));
    for (uint32_t i = 0; i < kBootFrames; ++i) {
        first.run_frame();
        second.run_frame();
    }

    const std::span<const uint8_t> a = first.framebuffer();
    const std::span<const uint8_t> b = second.framebuffer();
    REQUIRE(a.size() == b.size());
    REQUIRE(std::equal(a.begin(), a.end(), b.begin()));
}

TEST_CASE("crossy_title_text_is_centered") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    gb::Gameboy gameboy;
    REQUIRE(gameboy.load_rom(rom));
    run(gameboy, kBootFrames);
    REQUIRE(hover_shown(gameboy));

    // even-length lines land symmetric around the 20 column grid: left + right == 19
    for (uint32_t row : {kBannerTitleRow, kBannerPromptRow, kBannerBestRow}) {
        const auto [left, right] = glyph_span(gameboy, row, kInvFontFirstTile + 1, kInvFontLastTile);
        REQUIRE(left >= 0);
        REQUIRE(left + right == 19);
    }

    // the whole band is solid: no world shows through the five rows the banner claims
    const std::span<const uint16_t> ids = gameboy.framebuffer_tiles();
    for (size_t y = kBannerTopPx; y < kBannerEndPx; ++y) {
        for (size_t x = 0; x < gb::kLcdWidth; ++x) {
            const uint16_t id = ids[y * gb::kLcdWidth + x];
            if ((id & 0x100u) != 0) {
                // only the best's own digit sprites ever cross the band
                const uint8_t sprite = static_cast<uint8_t>(id);
                REQUIRE(sprite >= kDigitTileId);
                REQUIRE(sprite <= kDigitLastTileId);
                continue;
            }
            REQUIRE(static_cast<uint8_t>(id) >= kInvFontFirstTile);
            REQUIRE(static_cast<uint8_t>(id) <= kInvFontLastTile);
        }
    }
}

// the badge is the whole 8 px cell, so an n digit run is 8n wide and straddles the middle column
TEST_CASE("best_digits_pixel_centered") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    for (int best : {7, 42, 314}) {
        gb::Gameboy gameboy;
        REQUIRE(gameboy.load_rom(rom));
        const std::span<uint8_t> ram = gameboy.external_ram();
        REQUIRE(ram.size() > kSaveBestOffset + 1);
        ram[0] = 'C';
        ram[1] = 'R';
        ram[2] = 'S';
        ram[3] = 'Y';
        ram[kSaveBestOffset] = static_cast<uint8_t>(best);
        ram[kSaveBestOffset + 1] = static_cast<uint8_t>(best >> 8);
        run(gameboy, kBootFrames);

        const std::vector<DigitRun> digits = best_digits(gameboy);
        REQUIRE_FALSE(digits.empty());
        REQUIRE(digits_value(digits) == best);
        // the whole run's lit box, centered on screen x 80 whatever the digit count
        REQUIRE(digits.front().x0 + digits.back().x1 == static_cast<int>(gb::kLcdWidth) - 1);
        REQUIRE(digits.back().x1 - digits.front().x0 + 1 == static_cast<int>(digits.size()) * kDigitPitchPx);
    }
}

TEST_CASE("lane_parity_grass") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);

    // the opening lanes are plain grass and their tiles alternate with the lane index
    const Chick home = chick_at(gameboy);
    REQUIRE(home.found);
    for (int col = 0; col < kGridCols; ++col) {
        REQUIRE(cell_tile(gameboy, home, 0, col) == kGrassTileId);
        REQUIRE(cell_tile(gameboy, home, 1, col) == kGrassAltTileId);
        REQUIRE(cell_tile(gameboy, home, 2, col) == kGrassTileId);
    }

    // and deeper in, where trees and danger lanes break the run up, no two grass lanes ever touch
    size_t boundaries = 0;
    for (int step = 0; step < 24; ++step) {
        const Chick c = chick_at(gameboy);
        if (!c.found || popup_shown(gameboy)) {
            break;
        }
        for (int k = 0; k <= 5; ++k) {
            for (int col = 0; col < kGridCols; ++col) {
                const int here = cell_tile(gameboy, c, k, col);
                const int ahead = cell_tile(gameboy, c, k + 1, col);
                if (!is_grass_tile(here) || !is_grass_tile(ahead)) {
                    continue;
                }
                REQUIRE(here != ahead);
                ++boundaries;
            }
        }
        if (!autopilot_step(gameboy)) {
            break;
        }
    }
    REQUIRE(boundaries > 100u);
}

TEST_CASE("crossy_hover_shows_chick") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    gb::Gameboy gameboy;
    REQUIRE(gameboy.load_rom(rom));
    run(gameboy, kBootFrames);
    const Chick hover = chick_at(gameboy);
    REQUIRE(hover.found);
    REQUIRE_FALSE(hover.hopping);
    // the hover chick already stands on its spawn cell, because that world is the one it will play
    REQUIRE(chick_col(hover) == 4);

    unlock(gameboy);

    const Chick playing = chick_at(gameboy);
    REQUIRE(playing.found);
    REQUIRE(chick_col(playing) == 4);
    REQUIRE(playing.x == hover.x);
    REQUIRE(playing.y == hover.y);
    // nothing but contract terrain is on screen, so the banner went and the world stayed
    REQUIRE(play_area_is_all_terrain(gameboy));
}

TEST_CASE("hover_shows_live_world") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    gb::Gameboy gameboy;
    REQUIRE(gameboy.load_rom(rom));
    run(gameboy, kBootFrames);
    REQUIRE(hover_shown(gameboy));

    // real terrain fills every row the banner does not claim
    for (int cy = static_cast<int>(kBannerEndPx) / 8; cy < 18; ++cy) {
        for (int cx = 0; cx < 20; ++cx) {
            const int tile = bg_cell(gameboy, cx, cy);
            if (tile == kNoTile) {
                continue;
            }
            REQUIRE((is_grass_tile(tile) || tile == kTreeTileId || is_road_tile(tile) ||
                     is_water_tile(tile) || is_track_tile(tile)));
        }
    }

    // and the movers under the banner are running, not frozen
    const std::vector<CarRun> before = car_runs(gameboy);
    REQUIRE_FALSE(before.empty());
    run(gameboy, 30);
    const std::vector<CarRun> after = car_runs(gameboy);
    REQUIRE_FALSE(after.empty());
    REQUIRE(after.front().x0 != before.front().x0);
    // none of them ever crosses the band, which the banner owns whole
    for (const CarRun& moving : after) {
        REQUIRE(moving.band * kBandPx >= static_cast<int>(kBannerEndPx));
    }
    // the chick is standing, the camera has not moved and no timer is running
    const Chick still = chick_at(gameboy);
    REQUIRE(still.found);
    REQUIRE_FALSE(still.hopping);
    REQUIRE(hover_shown(gameboy));
}

TEST_CASE("hover_runs_no_timers") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    gb::Gameboy gameboy;
    REQUIRE(gameboy.load_rom(rom));
    run(gameboy, kBootFrames);
    const Chick before = chick_at(gameboy);
    REQUIRE(before.found);

    // longer than both the eagle's patience and three whole camera creeps
    run(gameboy, 700);
    REQUIRE_FALSE(eagle_at(gameboy).found);
    REQUIRE_FALSE(popup_shown(gameboy));
    REQUIRE(hover_shown(gameboy));
    const Chick after = chick_at(gameboy);
    REQUIRE(after.found);
    REQUIRE(after.x == before.x);
    REQUIRE(after.y == before.y);
}

TEST_CASE("unlock_press_does_not_hop") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    gb::Gameboy gameboy;
    REQUIRE(gameboy.load_rom(rom));
    run(gameboy, kBootFrames);
    const Chick hover = chick_at(gameboy);
    REQUIRE(hover.found);

    // the first press spends itself on the banner
    press(gameboy, gb::Button::A, 2);
    run(gameboy, kEnterPlayFrames + kSettleFrames);
    REQUIRE_FALSE(hover_shown(gameboy));
    REQUIRE(play_area_is_all_terrain(gameboy));
    const Chick unlocked = chick_at(gameboy);
    REQUIRE(unlocked.found);
    REQUIRE(unlocked.x == hover.x);
    REQUIRE(unlocked.y == hover.y);
    REQUIRE(hud_score(gameboy) == 0);

    // and the next one is play input
    tap(gameboy, gb::Button::Right);
    REQUIRE(chick_at(gameboy).x == hover.x + kCellPx);
}

TEST_CASE("crossy_hop_moves_one_cell") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);
    const Chick home = chick_at(gameboy);
    REQUIRE(home.found);

    gameboy.set_button(gb::Button::Right, true);
    gameboy.run_frame();
    gameboy.run_frame();
    gameboy.set_button(gb::Button::Right, false);
    run(gameboy, kHopFrames);
    const Chick right = chick_at(gameboy);
    REQUIRE(right.x == home.x + kCellPx);
    REQUIRE(right.y == home.y);

    // the hop has landed, so more frames must not move the chick any further
    run(gameboy, kSettleFrames);
    REQUIRE(chick_at(gameboy).x == right.x);

    tap(gameboy, gb::Button::Left);
    REQUIRE(chick_at(gameboy).x == home.x);

    // a forward hop slides the world down two tile rows and leaves the chick put
    const std::vector<int> before = play_grid(gameboy);
    tap(gameboy, gb::Button::Up);
    const Chick ahead = chick_at(gameboy);
    REQUIRE(ahead.x == home.x);
    REQUIRE(ahead.y == home.y);

    const std::vector<int> after = play_grid(gameboy);
    size_t compared = 0;
    for (int cy = 2; cy < 18; ++cy) {
        for (int cx = 0; cx < 20; ++cx) {
            const int was = before[static_cast<size_t>((cy - 2) * 20 + cx)];
            const int now = after[static_cast<size_t>(cy * 20 + cx)];
            if (was == kNoTile || now == kNoTile) {
                continue;
            }
            REQUIRE(now == was);
            ++compared;
        }
    }
    REQUIRE(compared > 300u);
}

TEST_CASE("crossy_tree_blocks_the_hop") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);

    bool tested = false;
    for (int step = 0; step < 40 && !tested; ++step) {
        const Chick c = chick_at(gameboy);
        REQUIRE(c.found);
        const int col = chick_col(c);
        for (int side = 0; side < 2 && !tested; ++side) {
            const int next = side == 0 ? col - 1 : col + 1;
            if (next < 0 || next >= kGridCols || !cell_blocked(gameboy, c, 0, next)) {
                continue;
            }
            tap(gameboy, side == 0 ? gb::Button::Left : gb::Button::Right);
            const Chick after = chick_at(gameboy);
            REQUIRE(after.x == c.x);
            REQUIRE(after.y == c.y);
            tested = true;
        }
        if (tested) {
            break;
        }
        // the full autopilot, so a river between here and the next tree is no obstacle
        REQUIRE(autopilot_step(gameboy));
    }
    REQUIRE(tested);
}

TEST_CASE("crossy_camera_streams_new_lanes") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);

    const int reached = autopilot(gameboy, 13, 60);
    REQUIRE(reached >= 12);
    // nothing but contract terrain ever reaches the screen, so no ring slot went stale
    REQUIRE(play_area_is_all_terrain(gameboy));
    REQUIRE(chick_at(gameboy).found);
}

TEST_CASE("crossy_score_counts_furthest_lane") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);
    REQUIRE(hud_score(gameboy) == 0);

    const int reached = autopilot(gameboy, 13, 60);
    REQUIRE(reached >= 12);

    const std::vector<DigitRun> digits = hud_digits(gameboy);
    REQUIRE(digits.size() == 2u);
    for (const DigitRun& digit : digits) {
        // an opaque badge over the whole cell: 8 columns of 8 rows, every one of them lit
        REQUIRE(digit.pixels == kDigitBadgePixels);
        REQUIRE(digit.x1 - digit.x0 + 1 == kDigitPitchPx);
    }

    // the camera never retreats and the score is the furthest lane, so a step back holds it
    tap(gameboy, gb::Button::Down);
    REQUIRE(hud_score(gameboy) == reached);
}

TEST_CASE("roads_appear_and_read_distinct") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);

    bool found = false;
    for (int step = 0; step < 20 && !found; ++step) {
        const Chick c = chick_at(gameboy);
        REQUIRE(c.found);
        for (int k = 0; k <= 6 && !found; ++k) {
            int road_cells = 0;
            int tree_cells = 0;
            for (int col = 0; col < kGridCols; ++col) {
                const int tile = cell_tile(gameboy, c, k, col);
                road_cells += is_road_tile(tile) ? 1 : 0;
                tree_cells += tile == kTreeTileId ? 1 : 0;
            }
            if (road_cells == 0) {
                continue;
            }
            // a road lane is asphalt end to end: always crossable, never a tree
            REQUIRE(road_cells == kGridCols);
            REQUIRE(tree_cells == 0);
            found = true;
        }
        if (!found) {
            REQUIRE(autopilot_step(gameboy));
        }
    }
    REQUIRE(found);
    REQUIRE(bg_has_tile(gameboy, kRoadTileId));
    REQUIRE(bg_has_tile(gameboy, kRoadStripeTileId));
}

// the world that opens with a road chunk; the boot one opens with grass and a lone road lane
constexpr uint32_t kRoadSeedWait = 0;

TEST_CASE("cars_move_and_keep_their_gap") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    gb::Gameboy gameboy;
    enter_world(gameboy, rom, kRoadSeedWait);

    std::vector<CarRun> runs = car_runs(gameboy);
    REQUIRE_FALSE(runs.empty());
    const int band = runs.front().band;

    double phase = lane_phase(gameboy, band);
    REQUIRE(phase != kNoPhase);
    double travelled = 0;
    int sign = 0;
    uint32_t stale = 1;
    uint32_t sampled = 0;
    for (uint32_t frame = 0; frame < 120; ++frame) {
        gameboy.run_frame();
        const double now = lane_phase(gameboy, band);
        // a torn band is no sample; the next clean one is allowed the frames it stands for
        if (now == kNoPhase) {
            ++stale;
            continue;
        }
        const double step = phase_delta(now, phase, kCarPeriod);
        // 8.8 fixed point at under a pixel a frame, so a step is never a jump
        REQUIRE(std::fabs(step) <= 1.0 * stale);
        if (step != 0) {
            if (sign == 0) {
                sign = step > 0 ? 1 : -1;
            }
            REQUIRE((step > 0 ? 1 : -1) == sign);
        }
        travelled += step;
        phase = now;
        stale = 1;
        ++sampled;

        // the two cars share the lane's speed, so their gap can never close
        std::vector<double> centers;
        for (const CarRun& run : car_runs(gameboy)) {
            if (run.band == band) {
                centers.push_back(car_center(run));
            }
        }
        for (size_t i = 1; i < centers.size(); ++i) {
            for (size_t j = 0; j < i; ++j) {
                REQUIRE(std::fabs(centers[i] - centers[j]) >= 40.0);
            }
        }
    }
    REQUIRE(sampled > 100u);
    REQUIRE(std::fabs(travelled) >= 40.0);
}

TEST_CASE("car_kills_the_chick") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);
    die_under_a_car(gameboy, 20);

    REQUIRE(popup_shown(gameboy));
    // the chick may have died right where the popup sits, so it is parked offscreen
    REQUIRE_FALSE(chick_at(gameboy).found);
    REQUIRE(car_runs(gameboy).empty());
    for (char c : std::string("GAMEOVRSCBTPY")) {
        REQUIRE(bg_has_tile(gameboy, popup_tile(c)));
    }
}

TEST_CASE("popup_is_centered_and_solid") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);
    die_under_a_car(gameboy, 20);

    const std::span<const uint16_t> ids = gameboy.framebuffer_tiles();
    for (size_t y = kPopupTopPx; y < kPopupEndPx; ++y) {
        for (size_t x = 0; x < gb::kLcdWidth; ++x) {
            const uint16_t id = ids[y * gb::kLcdWidth + x];
            // the whole band is popup fill or an inverted glyph: no world, no sprite, one colour
            REQUIRE((id & 0x100u) == 0);
            REQUIRE(static_cast<uint8_t>(id) >= kInvFontFirstTile);
            REQUIRE(static_cast<uint8_t>(id) <= kInvFontLastTile);
        }
    }

    // the frozen world still shows above and below the band
    bool above = false;
    bool below = false;
    for (size_t y = 0; y < gb::kLcdHeight; ++y) {
        for (size_t x = 0; x < gb::kLcdWidth; ++x) {
            const uint16_t id = ids[y * gb::kLcdWidth + x];
            const uint8_t tile = static_cast<uint8_t>(id);
            if ((id & 0x100u) != 0 || tile < kGrassTileId || tile > kGrassAltTileId) {
                continue;
            }
            above = above || y < kPopupTopPx;
            below = below || y >= kPopupEndPx;
        }
    }
    REQUIRE(above);
    REQUIRE(below);

    // an even length prompt lands symmetric around the 20 column grid
    const auto [left, right] = glyph_span(gameboy, kPopupPromptRow, kInvFontFirstTile + 1, kInvFontLastTile);
    REQUIRE(left >= 0);
    REQUIRE(left + right == 19);
}

TEST_CASE("best_score_lands_in_sram") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);
    die_under_a_car(gameboy, 20);

    const std::span<uint8_t> ram = gameboy.external_ram();
    REQUIRE(sram_has_magic(ram));
    REQUIRE(sram_best(ram) >= 1u);
}

TEST_CASE("best_survives_reload") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    gb::Gameboy first;
    start_play(first, rom);
    die_under_a_car(first, 20);
    const std::vector<uint8_t> saved(first.external_ram().begin(), first.external_ram().end());
    const uint16_t best = sram_best(saved);
    REQUIRE(best >= 1u);

    gb::Gameboy second;
    REQUIRE(second.load_rom(rom));
    const std::span<uint8_t> ram = second.external_ram();
    REQUIRE(ram.size() == saved.size());
    std::copy(saved.begin(), saved.end(), ram.begin());

    // boot must read the saved best instead of re-initialising it
    run(second, kBootFrames);
    REQUIRE(sram_has_magic(second.external_ram()));
    REQUIRE(sram_best(second.external_ram()) == best);
    REQUIRE(digits_value(best_digits(second)) == static_cast<int>(best));
}

TEST_CASE("retry_flow") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);
    die_under_a_car(gameboy, 20);
    REQUIRE(popup_shown(gameboy));

    const uint16_t best = sram_best(gameboy.external_ram());
    REQUIRE(best >= 1u);

    // an early press is inside the lockout, so the popup stays put
    press(gameboy, gb::Button::A, 2);
    run(gameboy, 2 * kEnterPlayFrames);
    REQUIRE(popup_shown(gameboy));
    REQUIRE_FALSE(hover_shown(gameboy));

    // b is neither the start nor the hop key, and it still clears the popup
    dismiss(gameboy, gb::Button::B);
    REQUIRE_FALSE(popup_shown(gameboy));
    REQUIRE(hover_shown(gameboy));
    REQUIRE(row_has_tile(gameboy, kBannerTitleRow, popup_tile('C')));
    REQUIRE(row_has_tile(gameboy, kBannerTitleRow, popup_tile('Y')));
    REQUIRE(digits_value(best_digits(gameboy)) == static_cast<int>(best));

    // the press that cleared the popup is spent: the hover world just keeps running
    for (uint32_t i = 0; i < 60; ++i) {
        gameboy.run_frame();
        REQUIRE(hover_shown(gameboy));
        REQUIRE(chick_at(gameboy).found);
    }

    press(gameboy, gb::Button::A, 2);
    run(gameboy, 2 * kEnterPlayFrames);
    REQUIRE_FALSE(hover_shown(gameboy));
    REQUIRE_FALSE(popup_shown(gameboy));
    REQUIRE(hud_score(gameboy) == 0);
    const Chick fresh = chick_at(gameboy);
    REQUIRE(fresh.found);
    REQUIRE(chick_col(fresh) == 4);
    REQUIRE(play_area_is_all_terrain(gameboy));
}

TEST_CASE("autopilot_crosses_roads") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    // a handful of fixed worlds whose first danger chunk is a road: not one lucky world
    for (uint32_t world_wait : {0u, 1u, 3u, 13u, 14u, 15u, 17u, 18u}) {
        gb::Gameboy gameboy;
        enter_world(gameboy, rom, world_wait);

        const int reached = autopilot(gameboy, 13, 60);
        REQUIRE(reached >= 12);
        // the whole mixed run happened without a scratch
        REQUIRE(chick_at(gameboy).found);
        REQUIRE_FALSE(popup_shown(gameboy));
        REQUIRE(play_area_is_all_terrain(gameboy));
    }
}

TEST_CASE("water_appears_and_reads_distinct") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);

    bool found = false;
    for (int step = 0; step < 30 && !found; ++step) {
        const Chick c = chick_at(gameboy);
        REQUIRE(c.found);
        for (int k = 0; k <= 6 && !found; ++k) {
            int water_cells = 0;
            int tree_cells = 0;
            for (int col = 0; col < kGridCols; ++col) {
                const int tile = cell_tile(gameboy, c, k, col);
                water_cells += is_water_tile(tile) ? 1 : 0;
                tree_cells += tile == kTreeTileId ? 1 : 0;
            }
            if (water_cells == 0) {
                continue;
            }
            // a water lane is open water end to end: never a tree, never a patch of bank
            REQUIRE(water_cells == kGridCols);
            REQUIRE(tree_cells == 0);
            found = true;
        }
        if (!found) {
            REQUIRE(autopilot_step(gameboy));
        }
    }
    REQUIRE(found);
    REQUIRE((bg_has_tile(gameboy, kWaterTileId) || bg_has_tile(gameboy, kWaterDarkTileId)));
}

// the seeds whose first danger chunk is one water lane, then two of them
// the lone one moved once run_frame returned on the vblank edge: wait 5 now opens with a pair
constexpr uint32_t kLoneWaterWait = 6;
constexpr uint32_t kTwoWaterWait = 7;

TEST_CASE("water_lanes_alternate_shade") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    for (uint32_t wait : {kLoneWaterWait, kTwoWaterWait}) {
        gb::Gameboy gameboy;
        enter_world(gameboy, rom, wait);
        const Chick c = chick_at(gameboy);
        REQUIRE(c.found);

        const LaneRun river = lane_run_on_screen(gameboy, c, true);
        REQUIRE(river.lo > 0);
        REQUIRE(river.n == static_cast<int>(wait == kLoneWaterWait ? 1u : 2u));

        std::vector<int> shade;
        for (int k = river.lo; k < river.lo + river.n; ++k) {
            // one tile fills the whole lane, both rows alike, so no 8 px seam can read as a divider
            int lane_tile = kNoTile;
            int cells = 0;
            for (int dr = 0; dr < 2; ++dr) {
                for (int cx = 0; cx < 20; ++cx) {
                    const int tile = lane_row_tile(gameboy, c, k, dr, cx);
                    if (tile == kNoTile) {
                        continue;
                    }
                    REQUIRE(is_water_tile(tile));
                    if (lane_tile == kNoTile) {
                        lane_tile = tile;
                    }
                    REQUIRE(tile == lane_tile);
                    ++cells;
                }
            }
            // logs cover part of the lower row, never a whole row of either
            REQUIRE(cells > 20);
            shade.push_back(lane_tile);
        }
        // adjacent lanes of a river take opposite shades, so the boundary is a step, not a line
        for (size_t i = 1; i < shade.size(); ++i) {
            REQUIRE(shade[i] != shade[i - 1]);
        }
    }
}

TEST_CASE("logs_are_full_height") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    gb::Gameboy gameboy;
    enter_world(gameboy, rom, kTwoWaterWait);

    const std::vector<CarRun> runs = log_runs(gameboy);
    REQUIRE_FALSE(runs.empty());
    for (const CarRun& log : runs) {
        const auto [top, bottom] = sprite_rows(gameboy, kLogFirstTileId, kLogLastTileId, log.band);
        REQUIRE(top >= 0);
        // a log all but fills its 16 px lane, so the river never reads as a plank in a wide band
        REQUIRE(bottom - top + 1 >= 14);
        REQUIRE(bottom <= kSpriteRows - 1);
    }
}

TEST_CASE("cars_are_16px") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    gb::Gameboy gameboy;
    enter_world(gameboy, rom, kRoadSeedWait);

    int measured = 0;
    for (uint32_t frame = 0; frame < 120 && measured == 0; ++frame) {
        for (const CarRun& car : car_runs(gameboy)) {
            // an edge clips a car, so only one well inside the screen gives its width
            if (car.x0 == 0 || car.x1 == static_cast<int>(gb::kLcdWidth) - 1) {
                continue;
            }
            measured = car.x1 - car.x0 + 1;
            const auto [top, bottom] = sprite_rows(gameboy, kCarFirstTileId, kCarLastTileId, car.band);
            REQUIRE(bottom - top + 1 >= 14);
        }
        gameboy.run_frame();
    }
    REQUIRE(measured == 16);
}

TEST_CASE("chick_visible_while_riding") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    // a lone river and a stacked one, so an upstream lane's logs get their chance too
    for (uint32_t world_wait : {kLoneWaterWait, kTwoWaterWait}) {
        gb::Gameboy gameboy;
        enter_world(gameboy, rom, world_wait);
        const size_t standing = chick_pixels(gameboy);
        REQUIRE(standing > 40u);

        REQUIRE(board_a_log(gameboy, 24));
        REQUIRE_FALSE(chick_at(gameboy).hopping);

        // the rider snaps onto the log's own sprite grid, so an oam x tie hands it every pixel
        size_t checked = 0;
        for (uint32_t frame = 0; frame < 90; ++frame) {
            const Chick riding = chick_at(gameboy);
            // a ride that has drifted to an edge is clipped by the screen, not by its log
            if (riding.found && riding.x > 2 && riding.x < static_cast<int>(gb::kLcdWidth) - 10) {
                REQUIRE(chick_pixels(gameboy) + 2u >= standing);
                ++checked;
            }
            gameboy.run_frame();
        }
        REQUIRE(checked > 40u);
    }
}

// the seeds whose first danger chunk is a road of one, two and three lanes
// all three moved with the vblank edge: 13 now opens with three lanes, 1 with one, 0 with two
constexpr uint32_t kOneRoadWait = 12;
constexpr uint32_t kTwoRoadWait = 14;
constexpr uint32_t kThreeRoadWait = 13;

TEST_CASE("road_dashes_only_between_lanes") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    const std::pair<uint32_t, int> worlds[3] = {{kOneRoadWait, 1}, {kTwoRoadWait, 2}, {kThreeRoadWait, 3}};
    for (const auto& [wait, lanes] : worlds) {
        gb::Gameboy gameboy;
        enter_world(gameboy, rom, wait);
        const Chick c = chick_at(gameboy);
        REQUIRE(c.found);

        const LaneRun road = lane_run_on_screen(gameboy, c, false);
        REQUIRE(road.lo > 0);
        REQUIRE(road.n == lanes);

        int dash_rows = 0;
        for (int k = 0; k <= 6; ++k) {
            for (int dr = 0; dr < 2; ++dr) {
                const int cells = stripe_cells_in_row(gameboy, c, k, dr);
                if (cells == 0) {
                    continue;
                }
                // a whole row of dashes, on the bottom half of the upper lane of an adjacent pair
                REQUIRE(cells == kGridCols);
                REQUIRE(dr == 1);
                REQUIRE(k > road.lo);
                REQUIRE(k < road.lo + road.n);
                ++dash_rows;
            }
        }
        // k lanes of road, k-1 internal boundaries, and never a dash on an outer edge
        REQUIRE(dash_rows == lanes - 1);
    }
}

TEST_CASE("logs_move_and_keep_their_gap") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    gb::Gameboy gameboy;
    enter_world(gameboy, rom, kTwoWaterWait);

    const std::vector<CarRun> runs = log_runs(gameboy);
    REQUIRE_FALSE(runs.empty());
    const int band = runs.front().band;

    double phase = log_phase(gameboy, band);
    REQUIRE(phase != kNoPhase);
    double travelled = 0;
    int sign = 0;
    uint32_t stale = 1;
    uint32_t sampled = 0;
    for (uint32_t frame = 0; frame < 120; ++frame) {
        gameboy.run_frame();
        const double now = log_phase(gameboy, band);
        if (now == kNoPhase) {
            ++stale;
            continue;
        }
        const double step = phase_delta(now, phase, kLogPeriod);
        // 8.8 fixed point at under a pixel a frame, so a step is never a jump
        REQUIRE(std::fabs(step) <= 1.0 * stale);
        if (step != 0) {
            if (sign == 0) {
                sign = step > 0 ? 1 : -1;
            }
            REQUIRE((step > 0 ? 1 : -1) == sign);
        }
        travelled += step;
        phase = now;
        stale = 1;
        ++sampled;

        // the two logs share the lane's speed and sit half a lap apart, so their gap never closes
        std::vector<double> centers;
        for (const CarRun& log : log_runs(gameboy)) {
            if (log.band == band) {
                centers.push_back(log_center(log));
            }
        }
        for (size_t i = 1; i < centers.size(); ++i) {
            for (size_t j = 0; j < i; ++j) {
                REQUIRE(std::fabs(centers[i] - centers[j]) >= 40.0);
            }
        }
    }
    REQUIRE(sampled > 100u);
    REQUIRE(std::fabs(travelled) >= 40.0);
}

TEST_CASE("logs_ride_a_shorter_lap") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    // the wait for a log is the lap between its two halves, so a shorter lap is a shorter wait
    // 9 slid onto a world whose lane hides a log behind the right edge, so 20 stands in for it
    for (uint32_t world_wait : {5u, 7u, 20u}) {
        gb::Gameboy gameboy;
        enter_world(gameboy, rom, world_wait);

        size_t sightings = 0;
        for (uint32_t frame = 0; frame < 240; ++frame) {
            std::vector<double> centers;
            int band = -1;
            for (const CarRun& log : log_runs(gameboy)) {
                if (band < 0) {
                    band = log.band;
                }
                if (log.band == band) {
                    centers.push_back(log_center(log));
                }
            }
            // a torn band reads one of the pair off the newer frame and the other off the older
            if (band >= 0 &&
                band_is_torn(gameboy, kLogFirstTileId, kLogLastTileId, kRiderBridge, band, kLogPx)) {
                centers.clear();
            }
            if (centers.size() == 2u) {
                REQUIRE(std::fabs(centers[1] - centers[0]) == kLogPeriod);
                ++sightings;
            }
            gameboy.run_frame();
        }
        // both logs of the lane are on screen most of the time: only 20 px of the lap hides one
        REQUIRE(sightings > 120u);
    }
}

TEST_CASE("riding_carries_the_chick") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);
    REQUIRE(board_a_log(gameboy, 24));

    Chick c = chick_at(gameboy);
    REQUIRE(c.found);
    const int band = chick_band(c);
    const int from = c.x;
    int dir = 0;
    int prev = c.x;
    for (uint32_t frame = 0; frame < 30; ++frame) {
        gameboy.run_frame();
        c = chick_at(gameboy);
        REQUIRE(c.found);
        const int step = c.x - prev;
        if (step != 0) {
            // under a pixel a frame, and never once against the log
            REQUIRE(std::abs(step) <= 1);
            if (dir == 0) {
                dir = step > 0 ? 1 : -1;
            }
            REQUIRE((step > 0 ? 1 : -1) == dir);
        }
        prev = c.x;

        double nearest = 999;
        for (const CarRun& log : log_runs(gameboy)) {
            if (log.band == band) {
                nearest = std::min(nearest, std::fabs(log_center(log) - (chick_center(c))));
            }
        }
        REQUIRE(nearest <= kRideCatchPx);
    }
    REQUIRE(dir != 0);
    // the slowest log still covers twelve px in thirty frames
    REQUIRE(std::abs(prev - from) >= 8);
}

// no log can drift back under the chick from this far off before the hop lands
constexpr double kOpenWaterPx = 40.0;

TEST_CASE("hopping_into_water_drowns") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);

    bool at_bank = false;
    for (int step = 0; step < 24 && !at_bank; ++step) {
        const Chick c = chick_at(gameboy);
        REQUIRE(c.found);
        if (is_water_tile(cell_tile(gameboy, c, 1, chick_cell(c)))) {
            at_bank = true;
            break;
        }
        REQUIRE(autopilot_step(gameboy));
    }
    REQUIRE(at_bank);

    const int band = chick_band(chick_at(gameboy)) - 1;
    bool jumped = false;
    for (int attempt = 0; attempt < kRideAttempts && !jumped; ++attempt) {
        const Chick c = chick_at(gameboy);
        REQUIRE(c.found);
        double nearest = 999;
        for (const CarRun& log : log_runs(gameboy)) {
            if (log.band == band) {
                nearest = std::min(nearest, std::fabs(log_center(log) - (chick_center(c))));
            }
        }
        if (nearest >= kOpenWaterPx) {
            tap(gameboy, gb::Button::Up);
            jumped = true;
            break;
        }
        gameboy.run_frame();
    }
    REQUIRE(jumped);

    run(gameboy, 6);
    REQUIRE(popup_shown(gameboy));
    REQUIRE_FALSE(chick_at(gameboy).found);
}

TEST_CASE("riding_off_the_edge_kills") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);
    REQUIRE(board_a_log(gameboy, 24));

    // the slowest log needs three hundred odd frames to carry the chick the width of the screen
    bool killed = false;
    for (uint32_t frame = 0; frame < 900 && !killed; ++frame) {
        gameboy.run_frame();
        killed = popup_shown(gameboy);
    }
    REQUIRE(killed);
    run(gameboy, 6);
    REQUIRE_FALSE(chick_at(gameboy).found);
    REQUIRE(log_runs(gameboy).empty());
}

TEST_CASE("autopilot_crosses_rivers") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    // fixed worlds whose first danger chunk is water, so every run is a mixed crossing
    for (uint32_t world_wait : {4u, 5u, 6u, 7u, 8u, 9u}) {
        gb::Gameboy gameboy;
        enter_world(gameboy, rom, world_wait);

        const int reached = autopilot(gameboy, 15, 80);
        REQUIRE(reached >= 14);
        REQUIRE(chick_at(gameboy).found);
        REQUIRE_FALSE(popup_shown(gameboy));
        REQUIRE(play_area_is_all_terrain(gameboy));
    }
}

// the eagle's timer is 600 frames; these bracket the swoop that follows it
constexpr uint32_t kIdleSafeFrames = 560;
constexpr uint32_t kEagleWatchFrames = 140;
constexpr uint32_t kSwoopFrames = 140;

TEST_CASE("idling_summons_the_eagle") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);

    // nine seconds of standing still on the opening grass and the sky is still empty
    run(gameboy, kIdleSafeFrames);
    REQUIRE_FALSE(eagle_at(gameboy).found);
    REQUIRE(chick_at(gameboy).found);
    REQUIRE_FALSE(popup_shown(gameboy));

    Chick eagle{0, 0, false, false};
    for (uint32_t frame = 0; frame < kEagleWatchFrames && !eagle.found; ++frame) {
        gameboy.run_frame();
        eagle = eagle_at(gameboy);
    }
    REQUIRE(eagle.found);
    // the digits step aside for the swoop, so a train's six sprites never make an eleventh on a line
    REQUIRE(hud_digits(gameboy).empty());
    // it comes down the chick's own column, four px a frame
    const Chick chick = chick_at(gameboy);
    REQUIRE(chick.found);
    REQUIRE(std::abs(chick_at(gameboy).x - eagle.x) <= kCellPx);

    run(gameboy, 4);
    const Chick lower = eagle_at(gameboy);
    REQUIRE(lower.found);
    REQUIRE(lower.y > eagle.y);
    REQUIRE(lower.x == eagle.x);

    bool killed = false;
    for (uint32_t frame = 0; frame < kSwoopFrames && !killed; ++frame) {
        gameboy.run_frame();
        killed = popup_shown(gameboy);
    }
    REQUIRE(killed);
    run(gameboy, 6);
    REQUIRE_FALSE(chick_at(gameboy).found);
    REQUIRE_FALSE(eagle_at(gameboy).found);
}

TEST_CASE("eagle_resets_on_progress") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);

    // four long idles, every one broken by a new lane: unreset, the timer would fire in the third
    for (int round = 0; round < 4; ++round) {
        // idle somewhere a plain hop leads on, so the round is a wait and not a river crossing
        for (int step = 0; step < 12; ++step) {
            const Chick c = chick_at(gameboy);
            REQUIRE(c.found);
            if (is_grass_tile(cell_tile(gameboy, c, 1, chick_cell(c)))) {
                break;
            }
            autopilot_step(gameboy);
        }

        const int before = hud_score(gameboy);
        run(gameboy, 200);
        REQUIRE_FALSE(eagle_at(gameboy).found);
        REQUIRE_FALSE(popup_shown(gameboy));

        tap(gameboy, gb::Button::Up);
        REQUIRE(hud_score(gameboy) == before + 1);
    }

    // well past a thousand frames of play and the eagle never had a reason to come
    REQUIRE_FALSE(eagle_at(gameboy).found);
    REQUIRE(chick_at(gameboy).found);
    REQUIRE_FALSE(popup_shown(gameboy));
}

TEST_CASE("camera_creeps") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);
    const Chick before = chick_at(gameboy);
    REQUIRE(before.found);
    const std::vector<int> grid_before = play_grid(gameboy);

    // four seconds of standing still and the camera has taken a lane on its own
    run(gameboy, 250);
    const Chick after = chick_at(gameboy);
    REQUIRE(after.found);
    REQUIRE(after.x == before.x);
    REQUIRE(after.y == before.y + kCellPx);

    // the same world, two tile rows further down the screen
    const std::vector<int> grid_after = play_grid(gameboy);
    size_t compared = 0;
    for (int cy = 2; cy < 18; ++cy) {
        for (int cx = 0; cx < 20; ++cx) {
            const int was = grid_before[static_cast<size_t>((cy - 2) * 20 + cx)];
            const int now = grid_after[static_cast<size_t>(cy * 20 + cx)];
            if (was == kNoTile || now == kNoTile) {
                continue;
            }
            REQUIRE(now == was);
            ++compared;
        }
    }
    REQUIRE(compared > 300u);
}

TEST_CASE("falling_behind_kills") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);
    const Chick home = chick_at(gameboy);
    REQUIRE(home.found);

    // two creeps of drift, then one lane of progress: enough to restart the idle timer, not
    // enough to catch the camera up, so the next creeps push the chick off the bottom first
    run(gameboy, 500);
    REQUIRE_FALSE(eagle_at(gameboy).found);
    const Chick drifted = chick_at(gameboy);
    REQUIRE(drifted.found);
    REQUIRE(drifted.y == home.y + 2 * kCellPx);

    tap(gameboy, gb::Button::Up);
    REQUIRE(hud_score(gameboy) == 1);

    bool swooped = false;
    bool killed = false;
    for (uint32_t frame = 0; frame < 700 && !killed; ++frame) {
        gameboy.run_frame();
        swooped = swooped || eagle_at(gameboy).found;
        killed = popup_shown(gameboy);
    }
    // the eagle took it, and well before the idle timer could have run out
    REQUIRE(swooped);
    REQUIRE(killed);
    run(gameboy, 6);
    REQUIRE_FALSE(chick_at(gameboy).found);
}

TEST_CASE("difficulty_ramps_car_speed") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    // fixed worlds whose deep road lanes roll fast; the opening ones cannot, whatever they roll
    // re-searched again once the seed moved to hover entry: every world changed
    // and once more for the vblank edge: 3 lost its deep road, so 2 takes its place
    // 2 and 14 open at the tier 0 ceiling exactly, so the opening half of the claim stays tight
    for (uint32_t world_wait : {1u, 2u, 14u, 18u}) {
        gb::Gameboy gameboy;
        enter_world(gameboy, rom, world_wait);

        // lanes 0..6 are all the world holds yet, and every one of them is base tier
        const double base = fastest_car(gameboy);
        REQUIRE(base <= kBaseCarMax + kSpeedTol);

        double deep = 0;
        for (int step = 0; step < 60 && hud_score(gameboy) < 20; ++step) {
            if (!autopilot_step(gameboy)) {
                break;
            }
            // past lane 14 every visible lane was generated at 12 or beyond
            if (hud_score(gameboy) >= 14) {
                deep = std::max(deep, fastest_car(gameboy));
            }
        }
        REQUIRE(hud_score(gameboy) >= 14);
        REQUIRE(deep > kBaseCarMax + kSpeedTol);
    }
}

// the whole quiet-warning-sweep loop fits in this, well short of the eagle's patience
constexpr uint32_t kTrackWatchFrames = 500;
// the light first drops its crossbuck 15 frames into a 60 frame warning, so the sweep is 46 away
constexpr int kBlinkToTrainMin = 40;
constexpr int kBlinkToTrainMax = 55;
// 416 px of travel at 5 px a frame: a 256 px train takes well over a screen's worth of frames
constexpr int kSweepMin = 74;
constexpr int kSweepMax = 92;
// the sweep enters at the right edge, so its first sighting reaches within a sprite of it
constexpr int kEnterX = 152;
// the two dings of a warning, kTrackBellGap apart
constexpr int kBellGap = 30;
constexpr int kBellSwing = 4000;

TEST_CASE("tracks_appear") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    // fixed worlds the autopilot walks onto a track in
    // the vblank edge left 17 with no track inside 60 steps, so 18 takes its place
    for (uint32_t world_wait : {2u, 16u, 18u}) {
        gb::Gameboy gameboy;
        enter_world(gameboy, rom, world_wait);

        // nothing is generated past lane 14 until the camera reaches lane 9, so no rail can show yet
        for (int step = 0; step < 20 && hud_score(gameboy) < 7; ++step) {
            REQUIRE_FALSE(bg_has_tile(gameboy, kRailTileId));
            REQUIRE_FALSE(bg_has_tile(gameboy, kRailWarnTileId));
            REQUIRE(autopilot_step(gameboy));
        }

        REQUIRE(walk_to_track(gameboy, 60));
        REQUIRE(wait_for_quiet(gameboy, 1));
        const Chick c = chick_at(gameboy);
        REQUIRE(c.found);

        // rails end to end, and the lane the chick stands on is not one of them
        for (int col = 0; col < kGridCols; ++col) {
            REQUIRE(is_track_tile(cell_tile(gameboy, c, 1, col)));
        }
        REQUIRE_FALSE(is_track_tile(cell_tile(gameboy, c, 0, chick_cell(c))));
        // tracks come singly and a danger chunk is always followed by grass
        for (int col = 0; col < kGridCols; ++col) {
            REQUIRE_FALSE(is_track_tile(cell_tile(gameboy, c, 2, col)));
            REQUIRE_FALSE(is_water_tile(cell_tile(gameboy, c, 2, col)));
            REQUIRE_FALSE(is_road_tile(cell_tile(gameboy, c, 2, col)));
        }
        // one warning cell in the whole lane, near its center, and lit between sweeps
        REQUIRE(warn_cells_in_lane(gameboy, c, 1) == 1);
        REQUIRE(warn_cell(gameboy, c, 1) == kRailWarnTileId);
        REQUIRE(play_area_is_all_terrain(gameboy));
    }
}

TEST_CASE("warning_precedes_train") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    // fixed worlds whose first track is quiet on arrival, so the whole cycle is watched from the start
    for (uint32_t world_wait : {6u, 7u, 10u, 34u}) {
        gb::Gameboy gameboy;
        enter_world(gameboy, rom, world_wait);
        REQUIRE(walk_to_track(gameboy, 60));
        REQUIRE(wait_for_quiet(gameboy, 1));

        int blink = -1;
        int train = -1;
        int gone = -1;
        int toggles = 0;
        int entered = -1;
        int leftmost = static_cast<int>(gb::kLcdWidth);
        int jumps = 0;
        bool off = false;
        bool leftward = true;
        for (uint32_t frame = 0; frame < kTrackWatchFrames && gone < 0; ++frame) {
            const Chick c = chick_at(gameboy);
            REQUIRE(c.found);
            const bool now_off = warn_cell(gameboy, c, 1) == kRailTileId;
            if (now_off != off) {
                off = now_off;
                ++toggles;
            }
            if (now_off && blink < 0) {
                blink = static_cast<int>(frame);
            }

            // head sprites, bg carriages or the rear pair: any of them is the sweep still
            // crossing. scoped to the watched lane so another visible track cannot bleed in
            const auto [x0, x1] = train_span_in_band(gameboy, c.y / kBandPx - 1);
            if (x0 >= 0) {
                if (train < 0) {
                    train = static_cast<int>(frame);
                    entered = x1;
                }
                // the vblank-edge snapshot can catch the head parking one frame
                // before the column it vacated shows its carriage, a one-off
                // 8 px pop; anything larger or repeated is a real reversal
                if (x0 > leftmost) {
                    ++jumps;
                    leftward = leftward && x0 - leftmost <= 8 && jumps <= 2;
                } else {
                    leftmost = x0;
                }
            } else if (train >= 0) {
                gone = static_cast<int>(frame);
            }
            gameboy.run_frame();
        }

        // no sweep ever came without the light blinking a whole warning ahead of it
        REQUIRE(blink >= 0);
        REQUIRE(train > blink);
        REQUIRE(train - blink >= kBlinkToTrainMin);
        REQUIRE(train - blink <= kBlinkToTrainMax);
        // the light swaps art four times over the warning, so at least three land before the train
        REQUIRE(toggles >= 3);

        // right to left: in at the screen's right edge, off at the left, never once back
        REQUIRE(entered >= kEnterX);
        REQUIRE(leftward);
        REQUIRE(leftmost == 0);
        REQUIRE(gone > train);
        REQUIRE(gone - train >= kSweepMin);
        REQUIRE(gone - train <= kSweepMax);
        REQUIRE_FALSE(popup_shown(gameboy));
    }
}

TEST_CASE("train_head_is_32px") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    for (uint32_t world_wait : {7u, 34u}) {
        gb::Gameboy gameboy;
        enter_world(gameboy, rom, world_wait);
        REQUIRE(walk_to_track(gameboy, 60));
        REQUIRE(wait_for_train(gameboy, 1, kTrackWatchFrames));

        const Chick c = chick_at(gameboy);
        REQUIRE(c.found);
        const int band = chick_band(c) - 1;
        int measured = 0;
        for (uint32_t frame = 0; frame < 90 && measured == 0; ++frame) {
            for (const CarRun& sweep : train_runs(gameboy)) {
                // an edge clips the block, and the rear pair is its own run further back
                if (sweep.band != band || sweep.x0 == 0 || sweep.x1 == static_cast<int>(gb::kLcdWidth) - 1) {
                    continue;
                }
                if (sweep.x1 - sweep.x0 + 1 != kTrainHeadPx) {
                    continue;
                }
                measured = sweep.x1 - sweep.x0 + 1;
                // four 8 px sprites with nothing between them: every column of the run is lit
                REQUIRE(train_columns(gameboy, band) >= measured);
            }
            gameboy.run_frame();
        }
        REQUIRE(measured == kTrainHeadPx);
    }
}

TEST_CASE("train_longer_than_screen") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    // the block is 256 px, so mid crossing it covers all 160 of the screen at once
    for (uint32_t world_wait : {7u, 34u}) {
        gb::Gameboy gameboy;
        enter_world(gameboy, rom, world_wait);
        REQUIRE(walk_to_track(gameboy, 60));
        REQUIRE(wait_for_train(gameboy, 1, kTrackWatchFrames));

        int fullest = 0;
        int full_frames = 0;
        for (uint32_t frame = 0; frame < 120; ++frame) {
            const std::vector<bool> cover = train_cover_on_screen(gameboy);
            int covered = 0;
            for (bool on : cover) {
                covered += on ? 1 : 0;
            }
            fullest = std::max(fullest, covered);
            full_frames += covered == static_cast<int>(gb::kLcdWidth) ? 1 : 0;
            gameboy.run_frame();
        }
        // every column of the play area at once, and for long enough to be no single frame fluke
        REQUIRE(fullest == static_cast<int>(gb::kLcdWidth));
        REQUIRE(full_frames >= 10);
        // and that span is longer than the head and rear sprites could ever reach on their own
        REQUIRE(kTrainSpanPx > static_cast<int>(gb::kLcdWidth));
        REQUIRE(kTrainHeadPx + kTrainTailPx < static_cast<int>(gb::kLcdWidth));
    }
}

TEST_CASE("rails_restored_after_train") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    for (uint32_t world_wait : {7u, 34u}) {
        gb::Gameboy gameboy;
        enter_world(gameboy, rom, world_wait);
        REQUIRE(walk_to_track(gameboy, 60));
        REQUIRE(wait_for_train(gameboy, 1, kTrackWatchFrames));

        // wait the whole block out; the rear lifts the carriages column by column behind it
        bool cleared = false;
        for (uint32_t frame = 0; frame < 200 && !cleared; ++frame) {
            gameboy.run_frame();
            REQUIRE(chick_at(gameboy).found);
            REQUIRE(carriages_only_on_rails(gameboy));
            cleared = train_span_on_screen(gameboy).first < 0;
        }
        REQUIRE(cleared);
        // a creep may have been mid slide when the rear cleared; bands only read on a settled world
        for (uint32_t frame = 0; frame < kSettleFrames && !world_is_settled(gameboy); ++frame) {
            gameboy.run_frame();
        }

        const Chick c = chick_at(gameboy);
        REQUIRE(c.found);
        // rails end to end again, the warning light back among them, and no carriage anywhere
        for (int col = 0; col < kGridCols; ++col) {
            REQUIRE(is_track_tile(cell_tile(gameboy, c, 1, col)));
        }
        REQUIRE(warn_cells_in_lane(gameboy, c, 1) == 1);
        REQUIRE(warn_cell(gameboy, c, 1) == kRailWarnTileId);
        REQUIRE_FALSE(bg_has_tile(gameboy, kTrainBodyUpperTileId));
        REQUIRE_FALSE(bg_has_tile(gameboy, kTrainBodyLowerTileId));
        REQUIRE(play_area_is_all_terrain(gameboy));

        // cross the restored rails and walk on: the lane scrolls away carrying nothing but rails
        REQUIRE(track_cross(gameboy));
        for (int step = 0; step < 6 && chick_at(gameboy).found; ++step) {
            REQUIRE(play_area_is_all_terrain(gameboy));
            REQUIRE(carriages_only_on_rails(gameboy));
            if (!autopilot_step(gameboy)) {
                break;
            }
        }
    }
}

TEST_CASE("warning_rings_the_bell") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    for (uint32_t world_wait : {6u, 34u}) {
        gb::Gameboy gameboy;
        enter_world(gameboy, rom, world_wait);
        REQUIRE(walk_to_track(gameboy, 60));
        REQUIRE(wait_for_quiet(gameboy, 1));
        drain_audio(gameboy);

        int first = -1;
        int second = -1;
        int blink = -1;
        int32_t peak = 0;
        bool ringing = false;
        for (uint32_t frame = 0; frame < kTrackWatchFrames; ++frame) {
            const Chick c = chick_at(gameboy);
            REQUIRE(c.found);
            if (blink < 0 && warn_cell(gameboy, c, 1) == kRailTileId) {
                blink = static_cast<int>(frame);
            }
            // the chick never moves here, so a swing this wide can only be the bell
            const int32_t swing = audio_swing(gameboy, 1);
            peak = std::max(peak, swing);
            // a ding decays over a dozen frames, so only its leading edge counts as a ring
            const bool loud = swing > kBellSwing;
            if (loud && !ringing) {
                if (first < 0) {
                    first = static_cast<int>(frame);
                } else {
                    second = static_cast<int>(frame);
                    break;
                }
            }
            ringing = loud;
        }
        REQUIRE(first >= 0);
        REQUIRE(second > 0);
        // rung at the warning's start and again halfway through it
        REQUIRE(second - first == kBellGap);
        REQUIRE(peak > kBellSwing);
        // and the first ding beats the first blink, which is a quarter of the warning in
        REQUIRE(blink > first);
    }
}

TEST_CASE("train_kills") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    // standing on the rails through a warning: the sweep takes the chick
    for (uint32_t world_wait : {6u, 10u}) {
        gb::Gameboy gameboy;
        enter_world(gameboy, rom, world_wait);
        REQUIRE(walk_to_track(gameboy, 60));
        REQUIRE(wait_for_blink(gameboy, 1, kTrackWatchFrames));
        tap(gameboy, gb::Button::Up);
        REQUIRE_FALSE(popup_shown(gameboy));

        REQUIRE(wait_for_death(gameboy, 150));
        run(gameboy, 6);
        REQUIRE_FALSE(chick_at(gameboy).found);
        REQUIRE(train_runs(gameboy).empty());
    }

    // and hopping onto rails a sweep is already crossing is no better
    for (uint32_t world_wait : {7u, 34u}) {
        gb::Gameboy gameboy;
        enter_world(gameboy, rom, world_wait);
        REQUIRE(walk_to_track(gameboy, 60));
        REQUIRE(wait_for_train(gameboy, 1, kTrackWatchFrames));
        tap(gameboy, gb::Button::Up);

        REQUIRE(wait_for_death(gameboy, 90));
        run(gameboy, 6);
        REQUIRE_FALSE(chick_at(gameboy).found);
    }
}

TEST_CASE("surviving_the_warning") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    for (uint32_t world_wait : {6u, 10u}) {
        gb::Gameboy gameboy;
        enter_world(gameboy, rom, world_wait);
        REQUIRE(walk_to_track(gameboy, 60));
        REQUIRE(line_up_past_track(gameboy));
        const int score = hud_score(gameboy);

        // onto the rails with the light already blinking, held there, then off in time
        REQUIRE(wait_for_blink(gameboy, 1, kTrackWatchFrames));
        tap(gameboy, gb::Button::Up);
        REQUIRE(hud_score(gameboy) == score + 1);
        REQUIRE_FALSE(popup_shown(gameboy));

        run(gameboy, 15);
        REQUIRE_FALSE(popup_shown(gameboy));
        tap(gameboy, gb::Button::Up);
        REQUIRE(hud_score(gameboy) == score + 2);
        REQUIRE_FALSE(popup_shown(gameboy));

        // the danger was real: the sweep crossed the rails one lane back, the ones just left
        REQUIRE(wait_for_train(gameboy, -1, 90));
        REQUIRE_FALSE(popup_shown(gameboy));
        REQUIRE(chick_at(gameboy).found);
    }
}

TEST_CASE("autopilot_crosses_tracks") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    // fixed worlds whose deep lanes hold tracks, crossed on a proven quiet light every time
    // the vblank edge emptied 3, 4 and 7 of rails inside 20 lanes, so 2, 5 and 6 stand in
    int total = 0;
    for (uint32_t world_wait : {1u, 2u, 5u, 6u, 8u, 10u}) {
        gb::Gameboy gameboy;
        enter_world(gameboy, rom, world_wait);

        int tracks = 0;
        for (int step = 0; step < 80 && hud_score(gameboy) < 20; ++step) {
            const Chick c = chick_at(gameboy);
            REQUIRE(c.found);
            tracks += is_track_lane_tile(cell_tile(gameboy, c, 1, chick_cell(c))) ? 1 : 0;
            // lanes scroll off mid sweep over a run this long; none of them leaves a carriage behind
            REQUIRE(carriages_only_on_rails(gameboy));
            if (!autopilot_step(gameboy)) {
                break;
            }
        }
        REQUIRE(tracks >= 1);
        REQUIRE(hud_score(gameboy) >= 20);
        REQUIRE(chick_at(gameboy).found);
        REQUIRE_FALSE(popup_shown(gameboy));
        REQUIRE(play_area_is_all_terrain(gameboy));
        total += tracks;
    }
    // not one lucky world: the six runs waited out several sweeps between them
    REQUIRE(total >= 6);
}

TEST_CASE("hop_makes_sound") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);
    // the start press is not a hop, so nothing has been triggered yet
    drain_audio(gameboy);

    const int32_t quiet = audio_swing(gameboy, 6);
    REQUIRE(quiet < kSilentSwing);
    gameboy.set_button(gb::Button::Up, true);
    gameboy.run_frame();
    gameboy.set_button(gb::Button::Up, false);
    REQUIRE(audio_swing(gameboy, 10) > kHopSwing);
}

TEST_CASE("death_makes_sound") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);
    for (int step = 0; step < 20; ++step) {
        const Chick c = chick_at(gameboy);
        REQUIRE(c.found);
        if (road_chunk_ahead(gameboy, c, chick_col(c)) > 0) {
            tap(gameboy, gb::Button::Up);
            break;
        }
        REQUIRE(autopilot_step(gameboy));
    }

    // the hop onto the asphalt has to decay before the crash can be measured against silence
    run(gameboy, 30);
    drain_audio(gameboy);
    REQUIRE(audio_swing(gameboy, 6) < kSilentSwing);

    int32_t burst = 0;
    bool died = false;
    for (uint32_t frame = 0; frame < 900 && !died; ++frame) {
        burst = std::max(burst, audio_swing(gameboy, 1));
        died = popup_shown(gameboy);
    }
    REQUIRE(died);
    // the car got the chick while it stood still, so the only sound in that stretch was the hit
    REQUIRE(burst > kDeathSwing);
}

TEST_CASE("crossy_play_is_deterministic") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    gb::Gameboy first;
    gb::Gameboy second;
    start_play(first, rom);
    start_play(second, rom);

    // the same hop script twice: three forward, one sideways, repeat
    for (int step = 0; step < 16; ++step) {
        const gb::Button button = (step % 4 == 3) ? gb::Button::Right : gb::Button::Up;
        tap(first, button);
        tap(second, button);
    }

    const std::span<const uint8_t> a = first.framebuffer();
    const std::span<const uint8_t> b = second.framebuffer();
    REQUIRE(a.size() == b.size());
    REQUIRE(std::equal(a.begin(), a.end(), b.begin()));
    REQUIRE(hud_score(first) == hud_score(second));
}

// the camera creeps a lane of its own after four seconds without one, so a wait advances it by exactly one
constexpr uint32_t kCreepWatchFrames = 320;

TEST_CASE("top_band_movers_are_hidden") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    for (uint32_t world_wait : {kRoadSeedWait, kTwoWaterWait}) {
        gb::Gameboy gameboy;
        enter_world(gameboy, rom, world_wait);

        // walk on until a danger chunk opens in the top band with plain ground still under it
        bool staged = false;
        for (int step = 0; step < 40 && !staged; ++step) {
            staged = world_is_settled(gameboy) && band_danger_kind(gameboy, 0) != kNoTile &&
                     band_danger_kind(gameboy, 1) == kNoTile;
            if (!staged && !autopilot_step(gameboy)) {
                break;
            }
        }
        REQUIRE(staged);

        // the lane is there in full, and nothing rides it: band 0 is the hud's own scanlines
        const int kind = band_danger_kind(gameboy, 0);
        REQUIRE_FALSE(band_has_mover_pixels(gameboy, 0));

        // one creep later the same lane sits in band 1, and its movers are back
        bool crept = false;
        for (uint32_t frame = 0; frame < kCreepWatchFrames && !crept; ++frame) {
            gameboy.run_frame();
            crept = world_is_settled(gameboy) && band_danger_kind(gameboy, 1) == kind;
        }
        REQUIRE(crept);
        REQUIRE(band_has_mover_pixels(gameboy, 1));
        REQUIRE_FALSE(band_has_mover_pixels(gameboy, 0));
    }
}

TEST_CASE("score_never_occluded") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);
    REQUIRE(autopilot(gameboy, 10, 40) >= 10);

    // a two digit score, watched frame by frame while danger lanes stream through the top band
    uint32_t watched = 0;
    uint32_t top_danger = 0;
    for (int step = 0; step < 40 && !popup_shown(gameboy); ++step) {
        for (uint32_t frame = 0; frame < 30; ++frame) {
            gameboy.run_frame();
            if (popup_shown(gameboy) || hud_score(gameboy) >= 100) {
                break;
            }
            require_hud_intact(gameboy, 2);
            REQUIRE_FALSE(band_has_mover_pixels(gameboy, 0));
            top_danger += band_danger_kind(gameboy, 0) != kNoTile ? 1 : 0;
            ++watched;
        }
        if (!autopilot_step(gameboy)) {
            break;
        }
    }
    REQUIRE(watched > 1000u);
    // a mover crosses the whole 160 px in well under this, so the digits' columns were swept clean
    REQUIRE(top_danger > 240u);
}

TEST_CASE("digits_form_one_strip") {
    const std::vector<uint8_t> rom = read_crossy_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);
    REQUIRE(autopilot(gameboy, 10, 40) >= 10);

    const std::vector<DigitRun> digits = hud_digits(gameboy);
    REQUIRE(digits.size() == 2u);
    // the badges abut, so the pair reads as one strip: no separator column between the cells
    REQUIRE(digits[1].x0 == digits[0].x1 + 1);
    for (int x = digits[0].x0; x <= digits[1].x1; ++x) {
        REQUIRE(hud_column_lit(gameboy, x));
    }
}
