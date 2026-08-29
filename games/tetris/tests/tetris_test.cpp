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
#include <map>
#include <set>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {

std::vector<uint8_t> read_tetris_rom() {
    std::ifstream in(TETRIS_ROM_PATH, std::ios::binary);
    REQUIRE(in.good());
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

constexpr uint32_t kBootFrames = 120;

// pandocs 0x143: 0x80 is dual dmg/cgb, 0xc0 is cgb only. this rom targets cgb only.
constexpr size_t kHeaderCgbFlagOffset = 0x143;
constexpr uint8_t kCgbFlagOnly = 0xC0;

// the playfield geometry the rom draws, mirrored from tetris.h
constexpr int kWellOriginCol = 3;
constexpr int kWellOriginRow = 0;
constexpr int kWellCols = 10;
constexpr int kWellRows = 18;
constexpr int kWallLeftCol = 2;
constexpr int kWallRightCol = 13;

// locked cells carry a tile id per piece so the board reads back off the screen
constexpr uint8_t kWellEmptyTileId = 0x61;
constexpr uint8_t kWallTileId = 0x63;
constexpr uint8_t kFlashTileId = 0x64;
constexpr uint8_t kLockTileFirst = 0x70;
constexpr uint8_t kLockTileLast = 0x76;
constexpr uint8_t kPieceSpriteTileId = 0xE0;

// the ibm font's ascii range, mirrored from tetris.h, for reading printed text off the bg
constexpr uint8_t kFontFirstChar = 0x20;
constexpr uint8_t kFontFirstTile = 0x00;
constexpr int kScreenCols = 20;

// title screen rows, mirrored from tetris.h
constexpr int kTitleRow = 7;
constexpr int kPromptRow = 10;
constexpr int kBestRow = 12;

// right panel geometry, mirrored from tetris.h. the panel is six cells, columns 14-19; every
// label, value, and the next box is left aligned on kPanelCol.
constexpr int kPanelCol = 14;
constexpr int kScoreLabelCol = kPanelCol;
constexpr int kScoreLabelRow = 1;
constexpr int kScoreValueRow = 2;
constexpr int kScoreValueCol = kPanelCol;
constexpr int kScoreDigits = 6;
constexpr int kLevelLabelCol = kPanelCol;
constexpr int kLevelLabelRow = 5;
constexpr int kLevelValueRow = 6;
constexpr int kLevelValueCol = kPanelCol;
constexpr int kLevelDigits = 2;
constexpr int kLinesLabelCol = kPanelCol;
constexpr int kLinesLabelRow = 9;
constexpr int kLinesValueRow = 10;
constexpr int kLinesValueCol = kPanelCol;
constexpr int kLinesDigits = 3;
constexpr uint8_t kDigitTileId = 0x80;
constexpr uint8_t kBackdropTileId = 0x62;
// compact panel hud font's letter block, mirrored from tetris.h; order matches panel.c's lookup
constexpr uint8_t kPanelLetterTileId = 0x8A;
constexpr int kPanelLetterCount = 11;
constexpr int kNextLabelCol = kPanelCol;
constexpr int kNextLabelRow = 13;
constexpr int kNextBoxRow = 15;
constexpr int kNextBoxCol = kPanelCol;
constexpr int kNextBoxCols = 4;
constexpr int kNextBoxRows = 2;

// game over popup band, mirrored from tetris.h
constexpr int kPopupTopRow = 5;
constexpr int kPopupRows = 9;
constexpr int kPopupOverRow = 1;
constexpr int kPopupScoreRow = 3;
constexpr int kPopupTopScoreRow = 5;
constexpr int kPopupPromptRow = 7;

// battery sram: 'T','T','R','S' then a u32 best score, little endian
constexpr size_t kSaveBestOffset = 4;

size_t count_lit_pixels(std::span<const uint8_t> fb) {
    size_t lit = 0;
    for (uint8_t shade : fb) {
        if (shade != 0) {
            ++lit;
        }
    }
    return lit;
}

void run(gb::Gameboy& gameboy, uint32_t frames) {
    for (uint32_t i = 0; i < frames; ++i) {
        gameboy.run_frame();
    }
}

void press(gb::Gameboy& gameboy, gb::Button button, uint32_t frames) {
    gameboy.set_button(button, true);
    run(gameboy, frames);
    gameboy.set_button(button, false);
}

// the rom edge triggers, so one held frame and one released frame is exactly one action
void tap(gb::Gameboy& gameboy, gb::Button button) {
    press(gameboy, button, 1);
    gameboy.run_frame();
}

size_t pixel_index(int x, int y) {
    return static_cast<size_t>(y) * gb::kLcdWidth + static_cast<size_t>(x);
}

uint16_t cell_id(const gb::Gameboy& gameboy, int cx, int cy) {
    const int x = (kWellOriginCol + cx) * 8 + 4;
    const int y = (kWellOriginRow + cy) * 8 + 4;
    return gameboy.framebuffer_tiles()[pixel_index(x, y)];
}

// 0 empty, 1..7 a locked piece plus one, 8 a flashing cell
using Grid = std::array<std::array<uint8_t, kWellCols>, kWellRows>;

// the falling piece can never overlap a locked cell, so a sprite always covers empty board
Grid read_grid(const gb::Gameboy& gameboy) {
    Grid grid{};
    for (int y = 0; y < kWellRows; ++y) {
        for (int x = 0; x < kWellCols; ++x) {
            const uint16_t id = cell_id(gameboy, x, y);
            if ((id & 0x100u) != 0) {
                continue;
            }
            const uint8_t tile = static_cast<uint8_t>(id);
            if (tile == kFlashTileId) {
                grid[y][x] = 8;
            } else if (tile >= kLockTileFirst && tile <= kLockTileLast) {
                grid[y][x] = static_cast<uint8_t>(tile - kLockTileFirst + 1);
            }
        }
    }
    return grid;
}

int count_locked(const Grid& grid) {
    int n = 0;
    for (const auto& row : grid) {
        for (uint8_t v : row) {
            if (v != 0 && v != 8) {
                ++n;
            }
        }
    }
    return n;
}

int count_flash_rows(const Grid& grid) {
    int n = 0;
    for (const auto& row : grid) {
        if (std::all_of(row.begin(), row.end(), [](uint8_t v) { return v == 8; })) {
            ++n;
        }
    }
    return n;
}

bool any_flash(const Grid& grid) {
    for (const auto& row : grid) {
        for (uint8_t v : row) {
            if (v == 8) {
                return true;
            }
        }
    }
    return false;
}

using Cell = std::pair<int, int>;

// the four sprites of the falling piece, as well cells
std::vector<Cell> piece_cells(const gb::Gameboy& gameboy) {
    const std::span<const uint16_t> ids = gameboy.framebuffer_tiles();
    std::set<Cell> found;
    for (int y = 0; y < static_cast<int>(gb::kLcdHeight); ++y) {
        for (int x = 0; x < static_cast<int>(gb::kLcdWidth); ++x) {
            const uint16_t id = ids[pixel_index(x, y)];
            if ((id & 0x100u) == 0 || static_cast<uint8_t>(id) != kPieceSpriteTileId) {
                continue;
            }
            found.insert(Cell{x / 8 - kWellOriginCol, y / 8 - kWellOriginRow});
        }
    }
    return std::vector<Cell>(found.begin(), found.end());
}

// a footprint with its top-left corner moved to the origin, so rotations compare by shape alone
struct Shape {
    std::vector<Cell> cells;
    int w = 0;
    int h = 0;

    bool operator==(const Shape& other) const {
        return cells == other.cells;
    }
};

Shape normalize(std::vector<Cell> cells) {
    Shape shape;
    if (cells.empty()) {
        return shape;
    }
    int minx = cells[0].first;
    int miny = cells[0].second;
    int maxx = minx;
    int maxy = miny;
    for (const Cell& c : cells) {
        minx = std::min(minx, c.first);
        miny = std::min(miny, c.second);
        maxx = std::max(maxx, c.first);
        maxy = std::max(maxy, c.second);
    }
    for (Cell& c : cells) {
        c.first -= minx;
        c.second -= miny;
    }
    std::sort(cells.begin(), cells.end());
    shape.cells = std::move(cells);
    shape.w = maxx - minx + 1;
    shape.h = maxy - miny + 1;
    return shape;
}

Shape piece_shape(const gb::Gameboy& gameboy) {
    return normalize(piece_cells(gameboy));
}

int piece_min_x(const gb::Gameboy& gameboy) {
    const std::vector<Cell> cells = piece_cells(gameboy);
    int minx = kWellCols;
    for (const Cell& c : cells) {
        minx = std::min(minx, c.first);
    }
    return minx;
}

int piece_max_x(const gb::Gameboy& gameboy) {
    const std::vector<Cell> cells = piece_cells(gameboy);
    int maxx = -1;
    for (const Cell& c : cells) {
        maxx = std::max(maxx, c.first);
    }
    return maxx;
}

int piece_min_y(const gb::Gameboy& gameboy) {
    const std::vector<Cell> cells = piece_cells(gameboy);
    int miny = kWellRows;
    for (const Cell& c : cells) {
        miny = std::min(miny, c.second);
    }
    return miny;
}

bool wait_for_piece(gb::Gameboy& gameboy, uint32_t limit) {
    for (uint32_t i = 0; i < limit; ++i) {
        if (!piece_cells(gameboy).empty() && !any_flash(read_grid(gameboy))) {
            return true;
        }
        gameboy.run_frame();
    }
    return false;
}

// boots the rom and leaves it on the first falling piece
void start_play(gb::Gameboy& gameboy, const std::vector<uint8_t>& rom) {
    REQUIRE(gameboy.load_rom(rom));
    run(gameboy, kBootFrames);
    press(gameboy, gb::Button::Start, 2);
    REQUIRE(wait_for_piece(gameboy, 120));
}

// the piece is drawn as sprites, so the board under it is still readable as occupancy
Grid occupancy(const Grid& grid) {
    Grid out{};
    for (int y = 0; y < kWellRows; ++y) {
        for (int x = 0; x < kWellCols; ++x) {
            out[y][x] = (grid[y][x] != 0 && grid[y][x] != 8) ? 1 : 0;
        }
    }
    return out;
}

int landing_offset(const Grid& grid, const Shape& shape, int left) {
    auto fits = [&](int dy) {
        for (const Cell& c : shape.cells) {
            const int x = left + c.first;
            const int y = c.second + dy;
            if (x < 0 || x >= kWellCols || y < 0 || y >= kWellRows || grid[y][x] != 0) {
                return false;
            }
        }
        return true;
    };
    if (!fits(0)) {
        return -1;
    }
    int d = 0;
    while (fits(d + 1)) {
        ++d;
    }
    return d;
}

Grid collapse(const Grid& grid, int* lines) {
    Grid out{};
    int dst = kWellRows - 1;
    *lines = 0;
    for (int y = kWellRows - 1; y >= 0; --y) {
        bool full = true;
        for (int x = 0; x < kWellCols; ++x) {
            if (grid[y][x] == 0) {
                full = false;
            }
        }
        if (full) {
            ++*lines;
            continue;
        }
        out[dst] = grid[y];
        --dst;
    }
    return out;
}

// the standard tetris heuristic: keep the stack low, flat, and free of buried holes
double score_grid(const Grid& grid, int lines) {
    std::array<int, kWellCols> height{};
    int aggregate = 0;
    int holes = 0;
    for (int x = 0; x < kWellCols; ++x) {
        int top = kWellRows;
        for (int y = 0; y < kWellRows; ++y) {
            if (grid[y][x] != 0) {
                top = y;
                break;
            }
        }
        height[x] = kWellRows - top;
        aggregate += height[x];
        for (int y = top + 1; y < kWellRows; ++y) {
            if (grid[y][x] == 0) {
                ++holes;
            }
        }
    }
    int bumpiness = 0;
    for (int x = 0; x + 1 < kWellCols; ++x) {
        bumpiness += std::abs(height[x] - height[x + 1]);
    }
    return -0.510066 * aggregate + 0.760666 * lines - 0.35663 * holes - 0.184483 * bumpiness;
}

struct Choice {
    int rot = -1;
    int left = 0;
};

// min_lines 2 makes the solver refuse singles and keep the last column open for a multi clear
Choice choose_placement(const Grid& grid, const std::array<Shape, 4>& shapes, int min_lines) {
    for (int relax = 0; relax < 3; ++relax) {
        Choice best;
        double best_score = 0;
        for (int rot = 0; rot < 4; ++rot) {
            const Shape& shape = shapes[rot];
            if (shape.cells.size() != 4) {
                continue;
            }
            for (int left = 0; left + shape.w <= kWellCols; ++left) {
                const int drop = landing_offset(grid, shape, left);
                if (drop < 0) {
                    continue;
                }
                Grid placed = grid;
                bool touches_last = false;
                for (const Cell& c : shape.cells) {
                    placed[c.second + drop][left + c.first] = 1;
                    touches_last = touches_last || left + c.first == kWellCols - 1;
                }
                int lines = 0;
                const Grid after = collapse(placed, &lines);
                if (min_lines >= 2 && relax < 2 && lines == 1) {
                    continue;
                }
                if (min_lines >= 2 && relax < 1 && lines < 2 && touches_last) {
                    continue;
                }
                const double score = score_grid(after, lines);
                if (best.rot < 0 || score > best_score) {
                    best_score = score;
                    best = Choice{rot, left};
                }
            }
        }
        if (best.rot >= 0) {
            return best;
        }
    }
    return Choice{};
}

// rotations with the same footprint place the same, so matching on shape cannot desync
void rotate_to(gb::Gameboy& gameboy, const Shape& want) {
    for (int i = 0; i < 4; ++i) {
        if (piece_shape(gameboy) == want) {
            return;
        }
        tap(gameboy, gb::Button::A);
    }
}

void steer_to(gb::Gameboy& gameboy, int left) {
    int stalled = 0;
    for (int i = 0; i < 24 && stalled < 2; ++i) {
        const int minx = piece_min_x(gameboy);
        if (minx == left || minx == kWellCols) {
            return;
        }
        tap(gameboy, minx > left ? gb::Button::Left : gb::Button::Right);
        stalled = piece_min_x(gameboy) == minx ? stalled + 1 : 0;
    }
}

// probes all four rotations off the screen, then leaves the piece back where it started
std::array<Shape, 4> probe_rotations(gb::Gameboy& gameboy) {
    std::array<Shape, 4> shapes;
    shapes[0] = piece_shape(gameboy);
    for (int rot = 1; rot < 4; ++rot) {
        tap(gameboy, gb::Button::A);
        shapes[rot] = piece_shape(gameboy);
    }
    tap(gameboy, gb::Button::A);
    return shapes;
}

struct PieceResult {
    bool ok = false;
    int flash_rows = 0;
    int locked_before = 0;
    int locked_after = 0;
};

// soft drops until the board changes, then reports how many rows lit up
PieceResult drop_and_settle(gb::Gameboy& gameboy, int locked_before) {
    PieceResult result;
    result.locked_before = locked_before;

    gameboy.set_button(gb::Button::Down, true);
    for (int i = 0; i < 300; ++i) {
        gameboy.run_frame();
        const Grid grid = read_grid(gameboy);
        if (count_locked(grid) != locked_before || any_flash(grid)) {
            break;
        }
    }
    gameboy.set_button(gb::Button::Down, false);

    for (int i = 0; i < 8; ++i) {
        gameboy.run_frame();
        result.flash_rows = std::max(result.flash_rows, count_flash_rows(read_grid(gameboy)));
    }
    for (int i = 0; i < 300; ++i) {
        const Grid grid = read_grid(gameboy);
        if (!any_flash(grid) && !piece_cells(gameboy).empty()) {
            break;
        }
        gameboy.run_frame();
    }
    result.locked_after = count_locked(read_grid(gameboy));
    result.ok = true;
    return result;
}

// plays one piece with the solver: probe the rotations, pick a landing, steer, drop
PieceResult play_piece(gb::Gameboy& gameboy, int min_lines) {
    PieceResult result;
    if (!wait_for_piece(gameboy, 300)) {
        return result;
    }
    const std::array<Shape, 4> shapes = probe_rotations(gameboy);
    const Grid grid = occupancy(read_grid(gameboy));
    const Choice choice = choose_placement(grid, shapes, min_lines);
    if (choice.rot < 0) {
        return result;
    }
    rotate_to(gameboy, shapes[choice.rot]);
    steer_to(gameboy, choice.left);
    return drop_and_settle(gameboy, count_locked(read_grid(gameboy)));
}

// like drop_and_settle, but never touches down: gravity alone brings the piece home, so no
// soft-drop points accrue. used where a test needs to predict the exact score of a clear.
PieceResult settle_without_soft_drop(gb::Gameboy& gameboy, int locked_before) {
    PieceResult result;
    result.locked_before = locked_before;

    for (int i = 0; i < 1200; ++i) {
        gameboy.run_frame();
        const Grid grid = read_grid(gameboy);
        if (count_locked(grid) != locked_before || any_flash(grid)) {
            break;
        }
    }
    for (int i = 0; i < 8; ++i) {
        gameboy.run_frame();
        result.flash_rows = std::max(result.flash_rows, count_flash_rows(read_grid(gameboy)));
    }
    for (int i = 0; i < 300; ++i) {
        const Grid grid = read_grid(gameboy);
        if (!any_flash(grid) && !piece_cells(gameboy).empty()) {
            break;
        }
        gameboy.run_frame();
    }
    result.locked_after = count_locked(read_grid(gameboy));
    result.ok = true;
    return result;
}

// plays one piece like play_piece, but settles it with gravity alone (see settle_without_soft_drop)
PieceResult play_piece_no_soft_drop(gb::Gameboy& gameboy, int min_lines) {
    PieceResult result;
    if (!wait_for_piece(gameboy, 300)) {
        return result;
    }
    const std::array<Shape, 4> shapes = probe_rotations(gameboy);
    const Grid grid = occupancy(read_grid(gameboy));
    const Choice choice = choose_placement(grid, shapes, min_lines);
    if (choice.rot < 0) {
        return result;
    }
    rotate_to(gameboy, shapes[choice.rot]);
    steer_to(gameboy, choice.left);
    return settle_without_soft_drop(gameboy, count_locked(read_grid(gameboy)));
}

// soft drops the piece where it stands, used to skip past a piece a test does not want
void drop_in_place(gb::Gameboy& gameboy) {
    REQUIRE(wait_for_piece(gameboy, 300));
    drop_and_settle(gameboy, count_locked(read_grid(gameboy)));
}

// the o piece is the only tetromino whose rotations are all the same footprint
bool is_o_piece(const Shape& shape) {
    return shape.w == 2 && shape.h == 2;
}

void skip_to_non_o_piece(gb::Gameboy& gameboy) {
    for (int i = 0; i < 8; ++i) {
        REQUIRE(wait_for_piece(gameboy, 300));
        if (!is_o_piece(piece_shape(gameboy))) {
            return;
        }
        drop_in_place(gameboy);
    }
    FAIL("no non-o piece spawned");
}

uint16_t cell_color(const gb::Gameboy& gameboy, int cx, int cy) {
    const int x = (kWellOriginCol + cx) * 8 + 4;
    const int y = (kWellOriginRow + cy) * 8 + 4;
    return gameboy.framebuffer_color()[pixel_index(x, y)];
}

// like cell_id/cell_color but against absolute screen tile coordinates, for the panel and popup
uint16_t tile_at(const gb::Gameboy& gameboy, int col, int row) {
    return gameboy.framebuffer_tiles()[pixel_index(col * 8 + 4, row * 8 + 4)];
}

uint16_t color_at(const gb::Gameboy& gameboy, int col, int row, int dx, int dy) {
    return gameboy.framebuffer_color()[pixel_index(col * 8 + dx, row * 8 + dy)];
}

constexpr uint8_t font_tile(char c) {
    return static_cast<uint8_t>(kFontFirstTile + static_cast<uint8_t>(c) - kFontFirstChar);
}

// mirrors panel.c's glyph lookup: the compact hud font only covers the panel's own labels
uint8_t panel_letter_tile(char c) {
    static constexpr char kPanelLetters[] = "CEILNORSTVX";
    for (int i = 0; i < kPanelLetterCount; ++i) {
        if (kPanelLetters[i] == c) {
            return static_cast<uint8_t>(kPanelLetterTileId + i);
        }
    }
    FAIL("letter not in the panel's compact hud font: " << c);
    return 0;
}

bool row_has_tile(const gb::Gameboy& gameboy, int row, uint8_t tile) {
    const std::span<const uint16_t> ids = gameboy.framebuffer_tiles();
    for (int y = row * 8; y < row * 8 + 8; ++y) {
        for (int x = 0; x < static_cast<int>(gb::kLcdWidth); ++x) {
            const uint16_t id = ids[static_cast<size_t>(y) * gb::kLcdWidth + static_cast<size_t>(x)];
            if ((id & 0x100u) == 0 && static_cast<uint8_t>(id) == tile) {
                return true;
            }
        }
    }
    return false;
}

// reads every dedicated digit tile (kDigitTileId + d) found in a row, left to right, as one number
uint32_t read_panel_number(const gb::Gameboy& gameboy, int col, int row, int width) {
    uint32_t value = 0;
    for (int i = 0; i < width; ++i) {
        const uint16_t id = tile_at(gameboy, col + i, row);
        REQUIRE((id & 0x100u) == 0);
        const uint8_t tile = static_cast<uint8_t>(id);
        REQUIRE(tile >= kDigitTileId);
        REQUIRE(tile < kDigitTileId + 10);
        value = value * 10 + (tile - kDigitTileId);
    }
    return value;
}

uint32_t read_score(const gb::Gameboy& gameboy) {
    return read_panel_number(gameboy, kScoreValueCol, kScoreValueRow, kScoreDigits);
}

uint32_t read_level(const gb::Gameboy& gameboy) {
    return read_panel_number(gameboy, kLevelValueCol, kLevelValueRow, kLevelDigits);
}

uint32_t read_lines(const gb::Gameboy& gameboy) {
    return read_panel_number(gameboy, kLinesValueCol, kLinesValueRow, kLinesDigits);
}

// concatenates whatever plain font digit tiles sit in a row; used for the title/popup text lines
uint32_t read_number_from_row(const gb::Gameboy& gameboy, int row) {
    uint32_t value = 0;
    bool any = false;
    for (int col = 0; col < static_cast<int>(kScreenCols); ++col) {
        const uint16_t id = tile_at(gameboy, col, row);
        if ((id & 0x100u) != 0) {
            continue;
        }
        const uint8_t tile = static_cast<uint8_t>(id);
        if (tile >= font_tile('0') && tile <= font_tile('9')) {
            value = value * 10 + (tile - font_tile('0'));
            any = true;
        }
    }
    return any ? value : 0;
}

uint32_t sram_best(std::span<const uint8_t> ram) {
    return static_cast<uint32_t>(ram[kSaveBestOffset]) |
           (static_cast<uint32_t>(ram[kSaveBestOffset + 1]) << 8) |
           (static_cast<uint32_t>(ram[kSaveBestOffset + 2]) << 16) |
           (static_cast<uint32_t>(ram[kSaveBestOffset + 3]) << 24);
}

bool sram_has_magic(std::span<const uint8_t> ram) {
    return ram.size() > kSaveBestOffset + 3 && ram[0] == 'T' && ram[1] == 'T' && ram[2] == 'R' &&
           ram[3] == 'S';
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

void drain_audio(gb::Gameboy& gameboy) {
    std::array<int16_t, 8192> drain{};
    while (gameboy.read_audio(drain) != 0) {
    }
}

// runs until the falling piece has descended `rows` well-rows, counting the frames it took
int frames_to_fall_rows(gb::Gameboy& gameboy, int rows) {
    const int start = piece_min_y(gameboy);
    int frames = 0;
    while (piece_min_y(gameboy) - start < rows && frames < 500) {
        gameboy.run_frame();
        ++frames;
    }
    return frames;
}

} // namespace

TEST_CASE("tetris_rom_declares_a_cgb_only_mbc1_battery_cart") {
    const std::vector<uint8_t> rom = read_tetris_rom();
    REQUIRE(rom.size() == 32768u);
    REQUIRE(rom[kHeaderCgbFlagOffset] == kCgbFlagOnly);

    std::string why;
    auto cart = gb::Cartridge::parse(rom, &why);
    REQUIRE(why.empty());
    REQUIRE(cart.has_value());
    REQUIRE(cart->type() == gb::CartType::Mbc1);
    REQUIRE(cart->has_battery());
    REQUIRE(cart->ram_size() > 0u);
    REQUIRE(cart->cgb());
    REQUIRE(cart->title() == "TETRIS");
}

TEST_CASE("tetris_boots_in_cgb_mode") {
    const std::vector<uint8_t> rom = read_tetris_rom();

    gb::Gameboy gameboy;
    REQUIRE(gameboy.load_rom(rom));
    run(gameboy, kBootFrames);
    // the frontend gates its colored-look hack on this; a unit test cannot assert the look itself
    REQUIRE(gameboy.cgb_mode());
}

TEST_CASE("tetris_boot_is_deterministic") {
    const std::vector<uint8_t> rom = read_tetris_rom();

    gb::Gameboy first;
    gb::Gameboy second;
    REQUIRE(first.load_rom(rom));
    REQUIRE(second.load_rom(rom));
    run(first, kBootFrames);
    run(second, kBootFrames);

    const std::span<const uint8_t> a = first.framebuffer();
    const std::span<const uint8_t> b = second.framebuffer();
    REQUIRE(a.size() == b.size());
    REQUIRE(std::equal(a.begin(), a.end(), b.begin()));

    const std::span<const uint16_t> ca = first.framebuffer_color();
    const std::span<const uint16_t> cb = second.framebuffer_color();
    REQUIRE(ca.size() == cb.size());
    REQUIRE(std::equal(ca.begin(), ca.end(), cb.begin()));
}

TEST_CASE("tetris_boots_to_a_non_blank_title_card") {
    const std::vector<uint8_t> rom = read_tetris_rom();

    gb::Gameboy gameboy;
    REQUIRE(gameboy.load_rom(rom));
    run(gameboy, kBootFrames);
    // the shade framebuffer carries raw 2-bit indices in cgb mode; any nonzero index is drawn content
    REQUIRE(count_lit_pixels(gameboy.framebuffer()) > 100u);
}

TEST_CASE("tetris_title_uses_more_than_two_cgb_colors") {
    const std::vector<uint8_t> rom = read_tetris_rom();

    gb::Gameboy gameboy;
    REQUIRE(gameboy.load_rom(rom));
    run(gameboy, kBootFrames);

    // two bg palettes landed (title text, border strip), so more than two distinct rgb555 values
    // must be on screen; a single dmg-style bgp swap could never produce this
    const std::span<const uint16_t> colors = gameboy.framebuffer_color();
    const std::set<uint16_t> distinct(colors.begin(), colors.end());
    REQUIRE(distinct.size() > 2u);
}

TEST_CASE("start_opens_an_empty_well_with_a_piece_at_the_top") {
    const std::vector<uint8_t> rom = read_tetris_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);

    const std::vector<Cell> cells = piece_cells(gameboy);
    REQUIRE(cells.size() == 4u);
    REQUIRE(piece_min_y(gameboy) <= 1);
    REQUIRE(count_locked(read_grid(gameboy)) == 0);

    // the walls frame the ten playable columns on both sides
    const std::span<const uint16_t> ids = gameboy.framebuffer_tiles();
    REQUIRE(static_cast<uint8_t>(ids[pixel_index(kWallLeftCol * 8 + 4, 4)]) == kWallTileId);
    REQUIRE(static_cast<uint8_t>(ids[pixel_index(kWallRightCol * 8 + 4, 4)]) == kWallTileId);
    REQUIRE(static_cast<uint8_t>(ids[pixel_index(kWellOriginCol * 8 + 4, 4)]) == kWellEmptyTileId);
}

TEST_CASE("pressing_a_at_the_title_also_starts_the_game") {
    const std::vector<uint8_t> rom = read_tetris_rom();

    // space is the a button on this frontend; the title must accept it same as start
    gb::Gameboy gameboy;
    REQUIRE(gameboy.load_rom(rom));
    run(gameboy, kBootFrames);
    press(gameboy, gb::Button::A, 2);
    REQUIRE(wait_for_piece(gameboy, 120));
    REQUIRE(count_locked(read_grid(gameboy)) == 0);
}

TEST_CASE("the_piece_falls_without_any_input") {
    const std::vector<uint8_t> rom = read_tetris_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);

    const int start = piece_min_y(gameboy);
    run(gameboy, 130);
    const int later = piece_min_y(gameboy);
    REQUIRE(later > start);
    // level zero gravity is one row every 53 frames, so 130 frames is a handful of rows at most
    REQUIRE(later - start <= 5);
}

TEST_CASE("holding_left_clamps_the_piece_at_the_left_wall") {
    const std::vector<uint8_t> rom = read_tetris_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);

    gameboy.set_button(gb::Button::Left, true);
    for (int i = 0; i < 120; ++i) {
        gameboy.run_frame();
        REQUIRE(piece_min_x(gameboy) >= 0);
    }
    gameboy.set_button(gb::Button::Left, false);
    REQUIRE(piece_min_x(gameboy) == 0);
}

TEST_CASE("holding_right_clamps_the_piece_at_the_right_wall") {
    const std::vector<uint8_t> rom = read_tetris_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);

    gameboy.set_button(gb::Button::Right, true);
    for (int i = 0; i < 120; ++i) {
        gameboy.run_frame();
        REQUIRE(piece_max_x(gameboy) <= kWellCols - 1);
    }
    gameboy.set_button(gb::Button::Right, false);
    REQUIRE(piece_max_x(gameboy) == kWellCols - 1);
}

TEST_CASE("rotation_changes_the_piece_footprint") {
    const std::vector<uint8_t> rom = read_tetris_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);
    skip_to_non_o_piece(gameboy);

    const Shape before = piece_shape(gameboy);
    tap(gameboy, gb::Button::A);
    const Shape after = piece_shape(gameboy);
    REQUIRE_FALSE(after == before);
    REQUIRE(after.cells.size() == 4u);
    // b turns the other way, so it undoes the a press
    tap(gameboy, gb::Button::B);
    REQUIRE(piece_shape(gameboy) == before);
}

TEST_CASE("a_rotation_that_would_leave_the_well_is_refused") {
    const std::vector<uint8_t> rom = read_tetris_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);
    skip_to_non_o_piece(gameboy);

    // upright against the left wall, every tetromino but o needs a column left of the well to turn
    tap(gameboy, gb::Button::A);
    press(gameboy, gb::Button::Left, 90);
    gameboy.run_frame();
    REQUIRE(piece_min_x(gameboy) == 0);

    const Shape before = piece_shape(gameboy);
    tap(gameboy, gb::Button::A);
    REQUIRE(piece_shape(gameboy) == before);
    REQUIRE(piece_min_x(gameboy) == 0);
}

TEST_CASE("soft_drop_falls_faster_than_gravity") {
    const std::vector<uint8_t> rom = read_tetris_rom();

    gb::Gameboy gravity;
    gb::Gameboy soft;
    start_play(gravity, rom);
    start_play(soft, rom);
    REQUIRE(piece_min_y(gravity) == piece_min_y(soft));

    run(gravity, 10);
    press(soft, gb::Button::Down, 10);
    REQUIRE(piece_min_y(soft) > piece_min_y(gravity));
}

TEST_CASE("a_landed_piece_becomes_locked_background_tiles") {
    const std::vector<uint8_t> rom = read_tetris_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);

    const PieceResult first = drop_and_settle(gameboy, 0);
    REQUIRE(first.ok);
    REQUIRE(first.flash_rows == 0);
    REQUIRE(first.locked_after == 4);

    // the four cells sit on the floor, and the new piece is back at the top as sprites
    const Grid grid = read_grid(gameboy);
    int bottom_cells = 0;
    for (uint8_t v : grid[kWellRows - 1]) {
        if (v != 0) {
            ++bottom_cells;
        }
    }
    REQUIRE(bottom_cells > 0);
    REQUIRE(piece_min_y(gameboy) <= 1);
}

TEST_CASE("locked_cells_wear_one_cgb_palette_per_piece") {
    const std::vector<uint8_t> rom = read_tetris_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);
    for (int i = 0; i < 10; ++i) {
        if (!play_piece(gameboy, 1).ok) {
            break;
        }
    }

    // the block art is one bitmap; only the attribute palette can make two cells differ
    const Grid grid = read_grid(gameboy);
    std::map<uint8_t, std::set<uint16_t>> colors;
    for (int y = 0; y < kWellRows; ++y) {
        for (int x = 0; x < kWellCols; ++x) {
            if (grid[y][x] != 0 && grid[y][x] != 8) {
                colors[grid[y][x]].insert(cell_color(gameboy, x, y));
            }
        }
    }
    REQUIRE(colors.size() >= 3u);
    std::set<uint16_t> seen;
    for (const auto& [piece, palette] : colors) {
        REQUIRE(palette.size() == 1u);
        REQUIRE(seen.insert(*palette.begin()).second);
    }
}

TEST_CASE("a_full_row_flashes_and_the_rows_above_collapse") {
    const std::vector<uint8_t> rom = read_tetris_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);

    bool cleared = false;
    for (int i = 0; i < 40 && !cleared; ++i) {
        const PieceResult result = play_piece(gameboy, 1);
        REQUIRE(result.ok);
        if (result.flash_rows == 0) {
            REQUIRE(result.locked_after == result.locked_before + 4);
            continue;
        }
        cleared = true;
        // four cells landed, then every flashed row was carried off the board
        REQUIRE(result.locked_after == result.locked_before + 4 - 10 * result.flash_rows);
    }
    REQUIRE(cleared);
    REQUIRE_FALSE(any_flash(read_grid(gameboy)));
}

TEST_CASE("two_rows_can_clear_together") {
    const std::vector<uint8_t> rom = read_tetris_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);

    int best = 0;
    for (int i = 0; i < 60 && best < 2; ++i) {
        const PieceResult result = play_piece(gameboy, 2);
        if (!result.ok) {
            break;
        }
        best = std::max(best, result.flash_rows);
        REQUIRE(result.locked_after == result.locked_before + 4 - 10 * result.flash_rows);
    }
    REQUIRE(best >= 2);
}

TEST_CASE("a_blocked_spawn_ends_the_game_and_freezes_the_board_under_the_popup") {
    const std::vector<uint8_t> rom = read_tetris_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);

    // stacking every piece into the same columns tops the well out quickly
    bool over = false;
    for (int i = 0; i < 40 && !over; ++i) {
        REQUIRE(wait_for_piece(gameboy, 300));
        steer_to(gameboy, 4);
        drop_and_settle(gameboy, count_locked(read_grid(gameboy)));
        over = piece_cells(gameboy).empty() && !any_flash(read_grid(gameboy));
    }
    REQUIRE(over);

    const Grid frozen = read_grid(gameboy);
    run(gameboy, 180);
    const Grid after = read_grid(gameboy);
    // the game over card stages in over the middle rows; everything outside that band must hold
    for (int y = 0; y < kWellRows; ++y) {
        if (y >= kPopupTopRow && y < kPopupTopRow + kPopupRows) {
            continue;
        }
        REQUIRE(after[static_cast<size_t>(y)] == frozen[static_cast<size_t>(y)]);
    }
    REQUIRE(piece_cells(gameboy).empty());
}

TEST_CASE("start_after_game_over_rebuilds_a_fresh_well") {
    const std::vector<uint8_t> rom = read_tetris_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);

    bool over = false;
    for (int i = 0; i < 40 && !over; ++i) {
        REQUIRE(wait_for_piece(gameboy, 300));
        steer_to(gameboy, 4);
        drop_and_settle(gameboy, count_locked(read_grid(gameboy)));
        over = piece_cells(gameboy).empty() && !any_flash(read_grid(gameboy));
    }
    REQUIRE(over);
    REQUIRE(count_locked(read_grid(gameboy)) > 0);

    run(gameboy, 20);                     // let the popup finish staging before the dismissing press
    press(gameboy, gb::Button::Start, 2); // over -> title, not straight back to play
    run(gameboy, 20);                     // DISPLAY_OFF's own vblank wait plus the whole title repaint
    REQUIRE(piece_cells(gameboy).empty());
    REQUIRE(row_has_tile(gameboy, kTitleRow, font_tile('T')));

    press(gameboy, gb::Button::Start, 2); // title -> play
    REQUIRE(wait_for_piece(gameboy, 120));
    REQUIRE(count_locked(read_grid(gameboy)) == 0);
    REQUIRE(piece_min_y(gameboy) <= 1);
}

TEST_CASE("the_same_input_script_gives_the_same_board") {
    const std::vector<uint8_t> rom = read_tetris_rom();

    gb::Gameboy first;
    gb::Gameboy second;
    REQUIRE(first.load_rom(rom));
    REQUIRE(second.load_rom(rom));

    // one script driven frame by frame; the seed is the frame counter at the start press.
    // rotate only fires once play has actually begun: a is also a title-screen start button now,
    // so an earlier rotate pulse would open the round itself and desync the intended start frame
    int peak_locked = 0;
    for (int i = 0; i < 900; ++i) {
        const bool start = i >= 120 && i < 122;
        const bool left = i % 37 < 6;
        const bool right = i % 53 < 4;
        const bool rotate = i >= 122 && i % 29 < 2;
        const bool down = i % 17 < 9;
        for (gb::Gameboy* g : {&first, &second}) {
            g->set_button(gb::Button::Start, start);
            g->set_button(gb::Button::Left, left);
            g->set_button(gb::Button::Right, right);
            g->set_button(gb::Button::A, rotate);
            g->set_button(gb::Button::Down, down);
            g->run_frame();
        }
        // a is also the over-card dismiss/retry button, so a busy script can top out, bounce back
        // to the title and start a fresh round before frame 900; a running peak survives that
        peak_locked = std::max(peak_locked, count_locked(read_grid(first)));
    }

    REQUIRE(peak_locked > 0);
    REQUIRE(read_grid(first) == read_grid(second));

    const std::span<const uint16_t> a = first.framebuffer_tiles();
    const std::span<const uint16_t> b = second.framebuffer_tiles();
    REQUIRE(std::equal(a.begin(), a.end(), b.begin()));
    const std::span<const uint16_t> ca = first.framebuffer_color();
    const std::span<const uint16_t> cb = second.framebuffer_color();
    REQUIRE(std::equal(ca.begin(), ca.end(), cb.begin()));
}

TEST_CASE("the_panel_digits_start_at_zero") {
    const std::vector<uint8_t> rom = read_tetris_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);

    REQUIRE(read_score(gameboy) == 0u);
    REQUIRE(read_level(gameboy) == 0u);
    REQUIRE(read_lines(gameboy) == 0u);
}

TEST_CASE("every_panel_element_shares_the_panel_left_edge") {
    const std::vector<uint8_t> rom = read_tetris_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);

    // every label is left aligned on kPanelCol, so its first character always lands there
    struct Label {
        const char* text;
        int row;
    };
    static constexpr Label kLabels[] = {
        {"SCORE", kScoreLabelRow},
        {"LEVEL", kLevelLabelRow},
        {"LINES", kLinesLabelRow},
        {"NEXT", kNextLabelRow},
    };
    for (const Label& label : kLabels) {
        for (int i = 0; label.text[i] != '\0'; ++i) {
            REQUIRE(static_cast<uint8_t>(tile_at(gameboy, kPanelCol + i, label.row)) ==
                    panel_letter_tile(label.text[i]));
        }
        // column 19, the panel's last column, is blank for every label but the score's value row
        const uint16_t spare = tile_at(gameboy, 19, label.row);
        REQUIRE((spare & 0x100u) == 0);
        const uint8_t spare_tile = static_cast<uint8_t>(spare);
        REQUIRE((spare_tile < kPanelLetterTileId || spare_tile >= kPanelLetterTileId + kPanelLetterCount));
    }

    // the wall column immediately left of the panel must stay the wall tile: a future off-by-one
    // leftward would otherwise silently start printing over the well's right wall
    REQUIRE(static_cast<uint8_t>(tile_at(gameboy, kPanelCol - 1, kScoreLabelRow)) == kWallTileId);

    // the score, unlike the labels, fills the panel's full six-column width: its last digit sits
    // at column 19, pinning that this containment (not overflow) is deliberate (see design doc)
    const uint16_t last_digit = tile_at(gameboy, kPanelCol + 5, kScoreValueRow);
    REQUIRE((last_digit & 0x100u) == 0);
    const uint8_t last_digit_tile = static_cast<uint8_t>(last_digit);
    REQUIRE(last_digit_tile >= kDigitTileId);
    REQUIRE(last_digit_tile < kDigitTileId + 10);
}

TEST_CASE("the_panel_backdrop_covers_every_column_through_nineteen") {
    const std::vector<uint8_t> rom = read_tetris_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);

    // row 3 sits between the score value (row 2) and the level label (row 5): text-free, so
    // every column here should show bare backdrop, confirming the plate reaches column 19
    for (int col = 14; col <= 19; ++col) {
        REQUIRE(static_cast<uint8_t>(tile_at(gameboy, col, 3)) == kBackdropTileId);
    }
}

TEST_CASE("the_panel_glyphs_use_the_fonts_full_letter_width") {
    const std::vector<uint8_t> rom = read_tetris_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);

    // the rom runs with lcdc bit 4 clear (signed tile addressing), but every panel tile id is
    // >= 0x80, so debug_vram()[id * 16 + n] addresses it directly; do not reuse this formula for
    // the stock font block (ids < 0x80), which lands at 0x1000 + id * 16 instead
    const std::span<const uint8_t> vram = gameboy.debug_vram();
    constexpr uint8_t kPanelI = kPanelLetterTileId + 2; // 'I' is index 2 of "CEILNORSTVX"
    constexpr uint8_t kPanelOne = kDigitTileId + 1;

    // serifed 'I': ink spans the full cell on the top and bottom rows, not just the stem
    REQUIRE(vram[static_cast<size_t>(kPanelI) * 16 + 1] == 0x7E);
    REQUIRE(vram[static_cast<size_t>(kPanelI) * 16 + 13] == 0x7E);
    // recolor convention: background is index 1, so the low plane byte stays 0xFF throughout
    REQUIRE(vram[static_cast<size_t>(kPanelI) * 16 + 0] == 0xFF);

    // serifed '1': a base serif spans the full cell width, matching 0 and 2-9's rhythm
    REQUIRE(vram[static_cast<size_t>(kPanelOne) * 16 + 13] == 0x7E);
}

TEST_CASE("a_line_clear_scores_the_classic_table_times_level_plus_one") {
    const std::vector<uint8_t> rom = read_tetris_rom();
    static constexpr uint32_t kLineScore[4] = {40, 100, 300, 1200};

    gb::Gameboy gameboy;
    start_play(gameboy, rom);

    // no soft drop anywhere, so nothing but the line-clear bonus can land in the score; stop at
    // the very first clear, of whatever size the solver happens to land, so level is still
    // guaranteed 0 and the classic table value goes straight to the score unmultiplied
    int cleared_n = 0;
    for (int i = 0; i < 60 && cleared_n == 0; ++i) {
        const PieceResult result = play_piece_no_soft_drop(gameboy, 1);
        REQUIRE(result.ok);
        cleared_n = result.flash_rows;
    }
    REQUIRE(cleared_n > 0);
    REQUIRE(wait_for_piece(gameboy, 300));
    REQUIRE(read_score(gameboy) == kLineScore[cleared_n - 1]);
}

TEST_CASE("clearing_ten_lines_advances_the_level_and_speeds_up_gravity") {
    const std::vector<uint8_t> rom = read_tetris_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);

    uint32_t total_lines = 0;
    for (int i = 0; i < 200 && total_lines < 10u; ++i) {
        const PieceResult result = play_piece(gameboy, 1);
        if (!result.ok) {
            break;
        }
        total_lines += static_cast<uint32_t>(result.flash_rows);
    }
    REQUIRE(total_lines >= 10u);
    REQUIRE(wait_for_piece(gameboy, 300));
    REQUIRE(read_lines(gameboy) == total_lines);
    REQUIRE(read_level(gameboy) >= 1u);

    // a fresh, unleveled well is the fair baseline for the gravity comparison below
    gb::Gameboy baseline;
    start_play(baseline, rom);

    const int leveled_frames = frames_to_fall_rows(gameboy, 3);
    const int baseline_frames = frames_to_fall_rows(baseline, 3);
    REQUIRE(leveled_frames < baseline_frames);
}

TEST_CASE("the_next_box_shows_the_piece_that_spawns_after_the_current_one") {
    const std::vector<uint8_t> rom = read_tetris_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);

    std::vector<Cell> preview_cells;
    for (int dy = 0; dy < kNextBoxRows; ++dy) {
        for (int dx = 0; dx < kNextBoxCols; ++dx) {
            const uint16_t id = tile_at(gameboy, kNextBoxCol + dx, kNextBoxRow + dy);
            if ((id & 0x100u) != 0) {
                continue;
            }
            const uint8_t tile = static_cast<uint8_t>(id);
            if (tile >= kLockTileFirst && tile <= kLockTileLast) {
                preview_cells.emplace_back(dx, dy);
            }
        }
    }
    REQUIRE(preview_cells.size() == 4u);
    const Shape preview_shape = normalize(preview_cells);
    const uint16_t preview_color =
        color_at(gameboy, kNextBoxCol + preview_cells[0].first, kNextBoxRow + preview_cells[0].second, 4, 4);

    // drop whatever piece is currently falling; the queued preview must become the next spawn
    drop_in_place(gameboy);
    REQUIRE(wait_for_piece(gameboy, 300));

    REQUIRE(piece_shape(gameboy) == preview_shape);
    const Cell spawned = piece_cells(gameboy).front();
    REQUIRE(cell_color(gameboy, spawned.first, spawned.second) == preview_color);
}

TEST_CASE("the_empty_well_wears_the_beveled_grid_not_a_flat_blank") {
    const std::vector<uint8_t> rom = read_tetris_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);

    // clear a line so a repainted (not just freshly initialised) empty cell is on screen
    bool cleared = false;
    for (int i = 0; i < 60 && !cleared; ++i) {
        const PieceResult result = play_piece(gameboy, 1);
        REQUIRE(result.ok);
        cleared = result.flash_rows > 0;
    }
    REQUIRE(cleared);
    REQUIRE(wait_for_piece(gameboy, 300));

    // the bottom row is empty again post-collapse; every cell there must be the grid tile
    bool found_empty = false;
    for (int x = 0; x < kWellCols; ++x) {
        if (read_grid(gameboy)[static_cast<size_t>(kWellRows - 1)][static_cast<size_t>(x)] != 0) {
            continue;
        }
        found_empty = true;
        REQUIRE(static_cast<uint8_t>(cell_id(gameboy, x, kWellRows - 1)) == kWellEmptyTileId);
        // a flat blank tile would shade every pixel the same; the bevel's left edge (light) and
        // right edge (dark) must not, sampled off the top/bottom rows where both edges agree
        const int px = (kWellOriginCol + x) * 8;
        const int py = (kWellOriginRow + (kWellRows - 1)) * 8;
        const uint16_t left_edge = gameboy.framebuffer_color()[pixel_index(px + 0, py + 3)];
        const uint16_t right_edge = gameboy.framebuffer_color()[pixel_index(px + 7, py + 3)];
        REQUIRE(left_edge != right_edge);
    }
    REQUIRE(found_empty);
}

TEST_CASE("the_title_screen_shows_top_zero_before_any_game") {
    const std::vector<uint8_t> rom = read_tetris_rom();

    gb::Gameboy gameboy;
    REQUIRE(gameboy.load_rom(rom));
    run(gameboy, kBootFrames);

    REQUIRE(row_has_tile(gameboy, kBestRow, font_tile('T')));
    REQUIRE(row_has_tile(gameboy, kBestRow, font_tile('O')));
    REQUIRE(row_has_tile(gameboy, kBestRow, font_tile('P')));
    REQUIRE(read_number_from_row(gameboy, kBestRow) == 0u);
}

TEST_CASE("pressing_start_at_game_over_goes_to_the_title_not_straight_to_play") {
    const std::vector<uint8_t> rom = read_tetris_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);

    bool over = false;
    for (int i = 0; i < 40 && !over; ++i) {
        REQUIRE(wait_for_piece(gameboy, 300));
        steer_to(gameboy, 4);
        drop_and_settle(gameboy, count_locked(read_grid(gameboy)));
        over = piece_cells(gameboy).empty() && !any_flash(read_grid(gameboy));
    }
    REQUIRE(over);
    run(gameboy, 20); // let the popup finish staging

    press(gameboy, gb::Button::Start, 2);
    run(gameboy, 20); // DISPLAY_OFF's own vblank wait plus the whole title repaint
    REQUIRE(row_has_tile(gameboy, kTitleRow, font_tile('T')));
    REQUIRE(piece_cells(gameboy).empty());
    // a single press only dismisses the popup; a second one is needed to actually start play
    REQUIRE_FALSE(wait_for_piece(gameboy, 5));
}

TEST_CASE("pressing_a_at_game_over_dismisses_the_card_back_to_the_title") {
    const std::vector<uint8_t> rom = read_tetris_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);

    bool over = false;
    for (int i = 0; i < 40 && !over; ++i) {
        REQUIRE(wait_for_piece(gameboy, 300));
        steer_to(gameboy, 4);
        drop_and_settle(gameboy, count_locked(read_grid(gameboy)));
        over = piece_cells(gameboy).empty() && !any_flash(read_grid(gameboy));
    }
    REQUIRE(over);
    run(gameboy, 20); // let the popup finish staging

    // space is the a button on this frontend; the game-over card must accept it same as start
    press(gameboy, gb::Button::A, 2);
    run(gameboy, 20); // DISPLAY_OFF's own vblank wait plus the whole title repaint
    REQUIRE(row_has_tile(gameboy, kTitleRow, font_tile('T')));
    REQUIRE(piece_cells(gameboy).empty());
    // a single press only dismisses the popup; a second one is needed to actually start play
    REQUIRE_FALSE(wait_for_piece(gameboy, 5));
}

namespace {

// drives at least one clear (so the round banks something), then stacks the well to end the
// game. the stacking phase soft-drops and can incidentally clear more lines, so the score is
// not predicted; it is read straight off the panel, whose score row survives under the popup
// band untouched. returns 0 if no clear (or no game over) happened in time.
uint32_t play_a_scoring_round_to_game_over(gb::Gameboy& gameboy) {
    bool cleared = false;
    for (int i = 0; i < 60 && !cleared; ++i) {
        const PieceResult result = play_piece(gameboy, 1);
        if (!result.ok) {
            break;
        }
        cleared = result.flash_rows > 0;
    }
    if (!cleared) {
        return 0;
    }

    bool over = false;
    for (int i = 0; i < 60 && !over; ++i) {
        if (!wait_for_piece(gameboy, 300)) {
            break;
        }
        steer_to(gameboy, 4);
        drop_and_settle(gameboy, count_locked(read_grid(gameboy)));
        over = piece_cells(gameboy).empty() && !any_flash(read_grid(gameboy));
    }
    return over ? read_score(gameboy) : 0u;
}

} // namespace

TEST_CASE("game_over_popup_shows_game_over_score_and_top") {
    const std::vector<uint8_t> rom = read_tetris_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);
    const uint32_t expected = play_a_scoring_round_to_game_over(gameboy);
    REQUIRE(expected > 0u);
    run(gameboy, 20); // let the popup finish staging

    REQUIRE(row_has_tile(gameboy, kPopupTopRow + kPopupOverRow, font_tile('G')));
    REQUIRE(row_has_tile(gameboy, kPopupTopRow + kPopupOverRow, font_tile('O')));
    REQUIRE(row_has_tile(gameboy, kPopupTopRow + kPopupScoreRow, font_tile('S')));
    REQUIRE(read_number_from_row(gameboy, kPopupTopRow + kPopupScoreRow) == expected);
    REQUIRE(row_has_tile(gameboy, kPopupTopRow + kPopupTopScoreRow, font_tile('T')));
    REQUIRE(read_number_from_row(gameboy, kPopupTopRow + kPopupTopScoreRow) == expected);
    REQUIRE(row_has_tile(gameboy, kPopupTopRow + kPopupPromptRow, font_tile('P')));
}

TEST_CASE("best_score_lands_in_sram_with_the_ttrs_magic") {
    const std::vector<uint8_t> rom = read_tetris_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);
    const uint32_t expected = play_a_scoring_round_to_game_over(gameboy);
    REQUIRE(expected > 0u);
    run(gameboy, 20);

    const std::span<uint8_t> ram = gameboy.external_ram();
    REQUIRE(sram_has_magic(ram));
    REQUIRE(sram_best(ram) == expected);
}

TEST_CASE("best_score_survives_reload_through_a_second_gameboy") {
    const std::vector<uint8_t> rom = read_tetris_rom();

    gb::Gameboy first;
    start_play(first, rom);
    const uint32_t expected = play_a_scoring_round_to_game_over(first);
    REQUIRE(expected > 0u);
    run(first, 20);
    const std::vector<uint8_t> saved(first.external_ram().begin(), first.external_ram().end());
    REQUIRE(sram_best(saved) == expected);

    gb::Gameboy second;
    REQUIRE(second.load_rom(rom));
    const std::span<uint8_t> ram = second.external_ram();
    REQUIRE(ram.size() == saved.size());
    std::copy(saved.begin(), saved.end(), ram.begin());

    // a fresh boot must load the saved best rather than re-initialising it to zero
    run(second, kBootFrames);
    REQUIRE(sram_has_magic(second.external_ram()));
    REQUIRE(sram_best(second.external_ram()) == expected);
    REQUIRE(read_number_from_row(second, kBestRow) == expected);
}

TEST_CASE("best_score_is_unchanged_by_a_loss_and_updated_by_a_win") {
    const std::vector<uint8_t> rom = read_tetris_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);

    // round one: bank a solid baseline, well clear of any plausible incidental noise below
    for (int i = 0; i < 200 && read_score(gameboy) < 200u; ++i) {
        if (!play_piece(gameboy, 1).ok) {
            break;
        }
    }
    const uint32_t first_best = play_a_scoring_round_to_game_over(gameboy);
    REQUIRE(first_best >= 200u);
    run(gameboy, 20);
    REQUIRE(sram_best(gameboy.external_ram()) == first_best);

    // round two: every piece rides gravity alone (no down input, so no soft-drop points) into
    // the same narrow column; a full ten-wide row essentially never completes this way, so the
    // round banks far less than round one and must not move the saved best
    press(gameboy, gb::Button::Start, 2); // over -> title
    run(gameboy, 20);                     // DISPLAY_OFF's own vblank wait plus the whole title repaint
    press(gameboy, gb::Button::Start, 2); // title -> play
    REQUIRE(wait_for_piece(gameboy, 120));
    bool over = false;
    for (int i = 0; i < 40 && !over; ++i) {
        REQUIRE(wait_for_piece(gameboy, 1200));
        steer_to(gameboy, 4);
        const int before = count_locked(read_grid(gameboy));
        for (int f = 0; f < 1200 && count_locked(read_grid(gameboy)) == before; ++f) {
            gameboy.run_frame();
        }
        over = piece_cells(gameboy).empty() && !any_flash(read_grid(gameboy));
    }
    REQUIRE(over);
    run(gameboy, 20);
    REQUIRE(read_score(gameboy) < first_best);
    REQUIRE(sram_best(gameboy.external_ram()) == first_best);

    // round three: soft drop is fine now, since the goal is simply to beat round one
    press(gameboy, gb::Button::Start, 2); // over -> title
    run(gameboy, 20);                     // DISPLAY_OFF's own vblank wait plus the whole title repaint
    press(gameboy, gb::Button::Start, 2); // title -> play
    REQUIRE(wait_for_piece(gameboy, 120));
    uint32_t live_score = 0;
    for (int i = 0; i < 200 && live_score <= first_best; ++i) {
        if (!play_piece(gameboy, 1).ok) {
            break;
        }
        live_score = read_score(gameboy);
    }
    REQUIRE(live_score > first_best);

    over = false;
    for (int i = 0; i < 40 && !over; ++i) {
        REQUIRE(wait_for_piece(gameboy, 300));
        steer_to(gameboy, 4);
        drop_and_settle(gameboy, count_locked(read_grid(gameboy)));
        over = piece_cells(gameboy).empty() && !any_flash(read_grid(gameboy));
    }
    REQUIRE(over);
    run(gameboy, 20);
    const uint32_t third_best = read_score(gameboy);
    REQUIRE(third_best > first_best);
    REQUIRE(sram_best(gameboy.external_ram()) == third_best);
}

TEST_CASE("the_title_screen_makes_no_sound") {
    const std::vector<uint8_t> rom = read_tetris_rom();

    gb::Gameboy gameboy;
    REQUIRE(gameboy.load_rom(rom));
    run(gameboy, kBootFrames);
    drain_audio(gameboy);

    // the apu is powered on at boot, but nothing triggers a channel until play starts
    REQUIRE(audio_swing(gameboy, 30) < 512);
}

TEST_CASE("a_rotation_blips_and_a_falling_piece_does_not") {
    const std::vector<uint8_t> rom = read_tetris_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);
    // the o piece's rotations are all the same footprint, so it can never prove a blip
    skip_to_non_o_piece(gameboy);
    // gravity is 53 frames a row at level 0, so this cannot lock and ring the thud
    run(gameboy, 20);

    const Shape before = piece_shape(gameboy);
    bool rotated = false;
    for (int i = 0; i < 4 && !rotated; ++i) {
        drain_audio(gameboy);
        REQUIRE(audio_swing(gameboy, 4) < 512);
        tap(gameboy, gb::Button::A);
        rotated = !(piece_shape(gameboy) == before);
        if (rotated) {
            // measured locally: the blip swings several thousand against a silent well
            REQUIRE(audio_swing(gameboy, 10) > 1000);
        }
    }
    REQUIRE(rotated);
}

TEST_CASE("a_line_clear_rings_out") {
    const std::vector<uint8_t> rom = read_tetris_rom();

    gb::Gameboy gameboy;
    start_play(gameboy, rom);

    bool cleared = false;
    int32_t swing = 0;
    for (int piece = 0; piece < 60 && !cleared; ++piece) {
        REQUIRE(wait_for_piece(gameboy, 300));
        const std::array<Shape, 4> shapes = probe_rotations(gameboy);
        const Choice choice = choose_placement(occupancy(read_grid(gameboy)), shapes, 1);
        if (choice.rot < 0) {
            break;
        }
        rotate_to(gameboy, shapes[choice.rot]);
        steer_to(gameboy, choice.left);
        // the steering blips must die out before the drop, or they would answer for the chime
        run(gameboy, 20);
        drain_audio(gameboy);

        const int before = count_locked(read_grid(gameboy));
        gameboy.set_button(gb::Button::Down, true);
        for (int f = 0; f < 300; ++f) {
            gameboy.run_frame();
            // the sample ring holds about ten frames, so it has to be emptied as we go
            drain_audio(gameboy);
            if (count_locked(read_grid(gameboy)) != before) {
                break;
            }
        }
        gameboy.set_button(gb::Button::Down, false);
        // the chime fires on the lock frame, but the flash tiles are staged two rows a frame after it
        for (int f = 0; f < 4 && !cleared; ++f) {
            cleared = any_flash(read_grid(gameboy));
            if (!cleared) {
                gameboy.run_frame();
                drain_audio(gameboy);
            }
        }
        if (cleared) {
            swing = audio_swing(gameboy, 8);
            break;
        }
        run(gameboy, 16); // let the lock thud decay before listening to the next piece
    }
    REQUIRE(cleared);
    REQUIRE(swing > 1000);
}
