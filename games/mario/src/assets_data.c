// bank 0 was already full before m7, and super mario alone is 512 bytes of art. every tile and
// palette the game owns now rides with the enemy bank; only the lcd-off loaders here reach them,
// and the one table the streamer reads per column (kBlockTile*) stays behind in assets.c
#pragma bank 4

#include "assets.h"
#include "mario.h"

#include <gb/cgb.h>
#include <gb/gb.h>
#include <stdint.h>

// ground: a 16x16 cobble pattern whose two courses are 6/8 and 8/6 wide, so the four
// quadrants all differ and the block tiles seamlessly both ways. the top block is the same
// rubble under two rows of grass, and shares the fill block's lower half (kGroundLowerTiles)
// clang-format off
static const uint8_t kGroundTiles[64] = {
    // ground top left: grass edge with notches over rubble
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x83, 0x1E, // -..+++#-
    0x83, 0x7E, // -+++++#-
    0x83, 0x7E, // -+++++#-
    0xFF, 0xFF, // ########
    0xFF, 0x20, // --#-----
    0x30, 0xEF, // ++#-++++
    // ground top right
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x01, 0xDF, // ++.++++#
    0x01, 0xFF, // +++++++#
    0x01, 0xFF, // +++++++#
    0xFF, 0xFF, // ########
    0xFF, 0x10, // ---#----
    0x18, 0xF7, // +++#-+++
    // rubble upper left
    0x83, 0x7E, // -+++++#-
    0x83, 0x7E, // -+++++#-
    0x83, 0x7E, // -+++++#-
    0x83, 0x7E, // -+++++#-
    0x83, 0x7E, // -+++++#-
    0xFF, 0xFF, // ########
    0xFF, 0x20, // --#-----
    0x30, 0xEF, // ++#-++++
    // rubble upper right
    0x01, 0xFF, // +++++++#
    0x01, 0xFF, // +++++++#
    0x01, 0xFF, // +++++++#
    0x01, 0xFF, // +++++++#
    0x01, 0xFF, // +++++++#
    0xFF, 0xFF, // ########
    0xFF, 0x10, // ---#----
    0x18, 0xF7, // +++#-+++
};
// clang-format on

// the rubble's lower half, which both ground blocks end on
// clang-format off
static const uint8_t kGroundLowerTiles[32] = {
    // rubble lower left, shared by the top and fill blocks
    0x30, 0xEF, // ++#-++++
    0x30, 0xEF, // ++#-++++
    0x30, 0xEF, // ++#-++++
    0x30, 0xEF, // ++#-++++
    0xFF, 0xFF, // ########
    0xFF, 0x02, // ------#-
    0x83, 0x7E, // -+++++#-
    0x83, 0x7E, // -+++++#-
    // rubble lower right
    0x18, 0xF7, // +++#-+++
    0x18, 0xF7, // +++#-+++
    0x18, 0xF7, // +++#-+++
    0x18, 0xF7, // +++#-+++
    0xFF, 0xFF, // ########
    0xFF, 0x01, // -------#
    0x01, 0xFF, // +++++++#
    0x01, 0xFF, // +++++++#
};
// clang-format on

// brick: two 8px courses in running bond, each a black mortar line, a tan top highlight and
// a brown body. the courses' vertical seams sit 4px apart so the wall never reads as a grid
// clang-format off
static const uint8_t kBrickTiles[64] = {
    // brick upper left
    0xFF, 0xFF, // ########
    0xFF, 0x02, // ------#-
    0x02, 0xFF, // ++++++#+
    0x02, 0xFF, // ++++++#+
    0x02, 0xFF, // ++++++#+
    0x02, 0xFF, // ++++++#+
    0x02, 0xFF, // ++++++#+
    0x02, 0xFF, // ++++++#+
    // brick upper right
    0xFF, 0xFF, // ########
    0xFF, 0x01, // -------#
    0x01, 0xFF, // +++++++#
    0x01, 0xFF, // +++++++#
    0x01, 0xFF, // +++++++#
    0x01, 0xFF, // +++++++#
    0x01, 0xFF, // +++++++#
    0x01, 0xFF, // +++++++#
    // brick lower left
    0xFF, 0xFF, // ########
    0xFF, 0x20, // --#-----
    0x20, 0xFF, // ++#+++++
    0x20, 0xFF, // ++#+++++
    0x20, 0xFF, // ++#+++++
    0x20, 0xFF, // ++#+++++
    0x20, 0xFF, // ++#+++++
    0x20, 0xFF, // ++#+++++
    // brick lower right
    0xFF, 0xFF, // ########
    0xFF, 0x10, // ---#----
    0x10, 0xFF, // +++#++++
    0x10, 0xFF, // +++#++++
    0x10, 0xFF, // +++#++++
    0x10, 0xFF, // +++#++++
    0x10, 0xFF, // +++#++++
    0x10, 0xFF, // +++#++++
};
// clang-format on

// question block: black outline, four corner rivets, a white serif ? and a 1px darker inner
// edge down the right and along the bottom, which is what gives the face its bevel
// clang-format off
static const uint8_t kQuestionTiles[64] = {
    // question upper left
    0xFF, 0xFF, // ########
    0xFF, 0x80, // #-------
    0xFF, 0xB0, // #-##----
    0xFF, 0xB0, // #-##----
    0xF8, 0x87, // #----+++
    0xF9, 0x86, // #----++-
    0xF9, 0x86, // #----++-
    0xFF, 0x80, // #-------
    // question upper right
    0xFF, 0xFF, // ########
    0xFF, 0x03, // ------##
    0xFF, 0x0F, // ----####
    0xFF, 0x0F, // ----####
    0x1F, 0xE3, // +++---##
    0x9F, 0x63, // -++---##
    0x9F, 0x63, // -++---##
    0x9F, 0x63, // -++---##
    // question lower left
    0xFF, 0x80, // #-------
    0xFE, 0x81, // #------+
    0xFE, 0x81, // #------+
    0xFF, 0x80, // #-------
    0xFE, 0xB1, // #-##---+
    0xFE, 0xB1, // #-##---+
    0xFF, 0xFF, // ########
    0xFF, 0xFF, // ########
    // question lower right
    0x3F, 0xC3, // ++----##
    0x7F, 0x83, // +-----##
    0x7F, 0x83, // +-----##
    0xFF, 0x03, // ------##
    0x7F, 0x8F, // +---####
    0x7F, 0x8F, // +---####
    0xFF, 0xFF, // ########
    0xFF, 0xFF, // ########
};
// clang-format on

// spent block: the same shell drained of its glyph, so a used block reads as inert
// clang-format off
static const uint8_t kSpentTiles[64] = {
    // spent upper left
    0xFF, 0xFF, // ########
    0xFF, 0x80, // #-------
    0xFF, 0xB0, // #-##----
    0xFF, 0xB0, // #-##----
    0xFF, 0x80, // #-------
    0xFF, 0x80, // #-------
    0xFF, 0x80, // #-------
    0xFF, 0x80, // #-------
    // spent upper right
    0xFF, 0xFF, // ########
    0xFF, 0x03, // ------##
    0xFF, 0x0F, // ----####
    0xFF, 0x0F, // ----####
    0xFF, 0x03, // ------##
    0xFF, 0x03, // ------##
    0xFF, 0x03, // ------##
    0xFF, 0x03, // ------##
    // spent lower left
    0xFF, 0x80, // #-------
    0xFF, 0x80, // #-------
    0xFF, 0x80, // #-------
    0xFF, 0x80, // #-------
    0xFF, 0xB0, // #-##----
    0xFF, 0xB0, // #-##----
    0xFF, 0xFF, // ########
    0xFF, 0xFF, // ########
    // spent lower right
    0xFF, 0x03, // ------##
    0xFF, 0x03, // ------##
    0xFF, 0x03, // ------##
    0xFF, 0x03, // ------##
    0xFF, 0x0F, // ----####
    0xFF, 0x0F, // ----####
    0xFF, 0xFF, // ########
    0xFF, 0xFF, // ########
};
// clang-format on

// hard/stair block: a beveled stone cube - tan top-left face, brown bevel down the right and
// along the bottom, black outline with the four inner corners notched
// clang-format off
static const uint8_t kHardTiles[64] = {
    // stone cube upper left
    0xFF, 0xFF, // ########
    0xFF, 0x80, // #-------
    0xC0, 0xBF, // #-++++++
    0xC0, 0xBF, // #-++++++
    0xC0, 0xBF, // #-++++++
    0xC0, 0xBF, // #-++++++
    0xC0, 0xBF, // #-++++++
    0xC0, 0xBF, // #-++++++
    // stone cube upper right
    0xFF, 0xFF, // ########
    0xFF, 0x01, // -------#
    0x03, 0xFF, // ++++++##
    0x03, 0xFF, // ++++++##
    0x03, 0xFF, // ++++++##
    0x03, 0xFF, // ++++++##
    0x03, 0xFF, // ++++++##
    0x03, 0xFF, // ++++++##
    // stone cube lower left
    0xC0, 0xBF, // #-++++++
    0xC0, 0xBF, // #-++++++
    0xC0, 0xBF, // #-++++++
    0xC0, 0xBF, // #-++++++
    0xC0, 0xBF, // #-++++++
    0xC0, 0xBF, // #-++++++
    0xFF, 0xFF, // ########
    0xFF, 0xFF, // ########
    // stone cube lower right
    0x03, 0xFF, // ++++++##
    0x03, 0xFF, // ++++++##
    0x03, 0xFF, // ++++++##
    0x03, 0xFF, // ++++++##
    0x03, 0xFF, // ++++++##
    0x03, 0xFF, // ++++++##
    0xFF, 0xFF, // ########
    0xFF, 0xFF, // ########
};
// clang-format on

// pipe: a 32px lip over a 28px body inset 2px on each side. the shading runs in columns across
// the whole width - a 4px light band just inside the left outline, dark green all the way to the
// right outline - so the lip's middle 16px is one repeated tile. smb's pipes only ever have those
// two greens: a first pass put a 2px light/dark dither in front of the right outline as a third
// shade, and with the palette's light green being the bright one it read on screen as a checkered
// rib rather than a shadow, which also made the lip's right half look like a plain body segment
// clang-format off
static const uint8_t kPipeTiles[144] = {
    // lip left, upper
    0xFF, 0xFF, // ########
    0xF8, 0x87, // #----+++
    0xF8, 0x87, // #----+++
    0xF8, 0x87, // #----+++
    0xF8, 0x87, // #----+++
    0xF8, 0x87, // #----+++
    0xF8, 0x87, // #----+++
    0xF8, 0x87, // #----+++
    // lip middle, upper
    0xFF, 0xFF, // ########
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    // lip right, upper
    0xFF, 0xFF, // ########
    0x01, 0xFF, // +++++++#
    0x01, 0xFF, // +++++++#
    0x01, 0xFF, // +++++++#
    0x01, 0xFF, // +++++++#
    0x01, 0xFF, // +++++++#
    0x01, 0xFF, // +++++++#
    0x01, 0xFF, // +++++++#
    // lip left, lower
    0xF8, 0x87, // #----+++
    0xF8, 0x87, // #----+++
    0xF8, 0x87, // #----+++
    0xF8, 0x87, // #----+++
    0xF8, 0x87, // #----+++
    0xF8, 0x87, // #----+++
    0xF8, 0x87, // #----+++
    0xFF, 0xFF, // ########
    // lip middle, lower
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    0xFF, 0xFF, // ########
    // lip right, lower
    0x01, 0xFF, // +++++++#
    0x01, 0xFF, // +++++++#
    0x01, 0xFF, // +++++++#
    0x01, 0xFF, // +++++++#
    0x01, 0xFF, // +++++++#
    0x01, 0xFF, // +++++++#
    0x01, 0xFF, // +++++++#
    0xFF, 0xFF, // ########
    // body left edge
    0x3E, 0x21, // ..#----+
    0x3E, 0x21, // ..#----+
    0x3E, 0x21, // ..#----+
    0x3E, 0x21, // ..#----+
    0x3E, 0x21, // ..#----+
    0x3E, 0x21, // ..#----+
    0x3E, 0x21, // ..#----+
    0x3E, 0x21, // ..#----+
    // body middle
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    // body right edge
    0x04, 0xFC, // +++++#..
    0x04, 0xFC, // +++++#..
    0x04, 0xFC, // +++++#..
    0x04, 0xFC, // +++++#..
    0x04, 0xFC, // +++++#..
    0x04, 0xFC, // +++++#..
    0x04, 0xFC, // +++++#..
    0x04, 0xFC, // +++++#..
};
// clang-format on

// and the same lip and body turned on their side, which is the whole of the sideways pipe's art:
// every tile is kPipeTiles' own transposed, pixel (x,y) read at (y,x). that is the turn a pipe
// wants rather than a plain rotation, because it keeps smb's light where smb keeps it - top left.
// the cap's top outline becomes the rim line down the mouth's LEFT edge, its side outlines become
// the pipe's roof and floor, and the 4px highlight that ran down the vertical pipe's left side now
// runs along the TOP of the whole horizontal run, mouth and body alike, and the bottom is plain
// green in to its outline the same way the vertical pipe's right side is. generated from the array
// above rather than drawn again, so the two cannot disagree about a single pixel
// clang-format off
static const uint8_t kPipeSideTiles[144] = {
    // mouth top left: the cap's top outline transposed, so the rim faces left
    0xFF, 0xFF, // ########
    0xFF, 0x80, // #-------
    0xFF, 0x80, // #-------
    0xFF, 0x80, // #-------
    0xFF, 0x80, // #-------
    0x80, 0xFF, // #+++++++
    0x80, 0xFF, // #+++++++
    0x80, 0xFF, // #+++++++
    // mouth top right: the cap's lower half, the inner line the body starts behind
    0xFF, 0xFF, // ########
    0xFF, 0x01, // -------#
    0xFF, 0x01, // -------#
    0xFF, 0x01, // -------#
    0xFF, 0x01, // -------#
    0x01, 0xFF, // +++++++#
    0x01, 0xFF, // +++++++#
    0x01, 0xFF, // +++++++#
    // mouth middle left
    0x80, 0xFF, // #+++++++
    0x80, 0xFF, // #+++++++
    0x80, 0xFF, // #+++++++
    0x80, 0xFF, // #+++++++
    0x80, 0xFF, // #+++++++
    0x80, 0xFF, // #+++++++
    0x80, 0xFF, // #+++++++
    0x80, 0xFF, // #+++++++
    // mouth middle right
    0x01, 0xFF, // +++++++#
    0x01, 0xFF, // +++++++#
    0x01, 0xFF, // +++++++#
    0x01, 0xFF, // +++++++#
    0x01, 0xFF, // +++++++#
    0x01, 0xFF, // +++++++#
    0x01, 0xFF, // +++++++#
    0x01, 0xFF, // +++++++#
    // mouth bottom left
    0x80, 0xFF, // #+++++++
    0x80, 0xFF, // #+++++++
    0x80, 0xFF, // #+++++++
    0x80, 0xFF, // #+++++++
    0x80, 0xFF, // #+++++++
    0x80, 0xFF, // #+++++++
    0x80, 0xFF, // #+++++++
    0xFF, 0xFF, // ########
    // mouth bottom right
    0x01, 0xFF, // +++++++#
    0x01, 0xFF, // +++++++#
    0x01, 0xFF, // +++++++#
    0x01, 0xFF, // +++++++#
    0x01, 0xFF, // +++++++#
    0x01, 0xFF, // +++++++#
    0x01, 0xFF, // +++++++#
    0xFF, 0xFF, // ########
    // horizontal body, top edge: the vertical body's inset and highlight, now its roof
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0xFF, 0xFF, // ########
    0xFF, 0x00, // --------
    0xFF, 0x00, // --------
    0xFF, 0x00, // --------
    0xFF, 0x00, // --------
    0x00, 0xFF, // ++++++++
    // horizontal body, middle
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    // horizontal body, bottom edge: plain green to its outline, then the 2px inset under it
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    0xFF, 0xFF, // ########
    0x00, 0x00, // ........
    0x00, 0x00, // ........
};
// clang-format on

// the flag shaft, which is the only part of the flag inside the pinned terrain block
// clang-format off
static const uint8_t kFlagPoleTile[16] = {
    // pole: two green pixels with a black shadow
    0x70, 0x10, // .--#....
    0x70, 0x10, // .--#....
    0x70, 0x10, // .--#....
    0x70, 0x10, // .--#....
    0x70, 0x10, // .--#....
    0x70, 0x10, // .--#....
    0x70, 0x10, // .--#....
    0x70, 0x10, // .--#....
};
// clang-format on

// the ball and the white pennant that hangs off the pole's left
// clang-format off
static const uint8_t kFlagHeadTiles[80] = {
    // the ball that caps the pole, with the shaft running out of its bottom
    0x00, 0x00, // ........
    0x78, 0x78, // .####...
    0xC4, 0xBC, // #-+++#..
    0xC4, 0xBC, // #-+++#..
    0x84, 0xFC, // #++++#..
    0x84, 0xFC, // #++++#..
    0x78, 0x78, // .####...
    0x70, 0x10, // .--#....
    // pennant upper left
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x03, 0x03, // ......##
    0x0F, 0x0C, // ....##--
    0x3F, 0x30, // ..##----
    // pennant upper right
    0x00, 0x00, // ........
    0x03, 0x03, // ......##
    0x0F, 0x0D, // ....##-#
    0x3F, 0x31, // ..##---#
    0xFF, 0xC1, // ##-----#
    0xFF, 0x01, // -------#
    0xFF, 0x01, // -------#
    0xFF, 0x01, // -------#
    // pennant lower left
    0x3F, 0x30, // ..##----
    0x0F, 0x0C, // ....##--
    0x03, 0x03, // ......##
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    // pennant lower right
    0xFF, 0x01, // -------#
    0xFF, 0x01, // -------#
    0xFF, 0x01, // -------#
    0xFF, 0xC1, // ##-----#
    0x3F, 0x31, // ..##---#
    0x0F, 0x0D, // ....##-#
    0x03, 0x03, // ......##
    0x00, 0x00, // ........
};
// clang-format on

// the castle, five kinds of block: plain wall, crenellation, an arched window, and the two
// halves of the door. the wall tile repeats in all four corners of its block and the merlon
// tile in both upper ones, so the two of them cost one tile each
// clang-format off
static const uint8_t kCastleTiles[224] = {
    // wall: 4px courses of small brick
    0xFF, 0xFF, // ########
    0xFF, 0x11, // ---#---#
    0x11, 0xFF, // +++#+++#
    0x11, 0xFF, // +++#+++#
    0xFF, 0xFF, // ########
    0x44, 0xFF, // +#+++#++
    0x44, 0xFF, // +#+++#++
    0x44, 0xFF, // +#+++#++
    // merlon: a tan-capped tooth with a sky gap beside it
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0xFC, 0x00, // ------..
    0x10, 0xFC, // +++#++..
    0x10, 0xFC, // +++#++..
    0xFC, 0xFC, // ######..
    0x40, 0xFC, // +#++++..
    0x40, 0xFC, // +#++++..
    // window upper left
    0xFF, 0xFF, // ########
    0xFF, 0x11, // ---#---#
    0x17, 0xF8, // +++#+---
    0x1F, 0xF3, // +++#--##
    0xFF, 0xF7, // ####-###
    0x4F, 0xF7, // +#++-###
    0x4F, 0xF7, // +#++-###
    0x4F, 0xF7, // +#++-###
    // window upper right
    0xFF, 0xFF, // ########
    0xFF, 0x11, // ---#---#
    0xF1, 0x1F, // ---#+++#
    0xF1, 0xCF, // ##--+++#
    0xFF, 0xEF, // ###-####
    0xF4, 0xEF, // ###-+#++
    0xF4, 0xEF, // ###-+#++
    0xF4, 0xEF, // ###-+#++
    // window lower left
    0xFF, 0xF7, // ####-###
    0xFF, 0x17, // ---#-###
    0x1F, 0xF7, // +++#-###
    0x1F, 0xF7, // +++#-###
    0xFF, 0xF7, // ####-###
    0x4F, 0xF0, // +#++----
    0x44, 0xFF, // +#+++#++
    0x44, 0xFF, // +#+++#++
    // window lower right
    0xFF, 0xEF, // ###-####
    0xFF, 0xE1, // ###----#
    0xF1, 0xEF, // ###-+++#
    0xF1, 0xEF, // ###-+++#
    0xFF, 0xEF, // ###-####
    0xF4, 0x0F, // ----+#++
    0x44, 0xFF, // +#+++#++
    0x44, 0xFF, // +#+++#++
    // door arch upper left
    0xFF, 0xFF, // ########
    0xFF, 0x11, // ---#---#
    0x17, 0xF8, // +++#+---
    0x1F, 0xF3, // +++#--##
    0xFF, 0xE7, // ###--###
    0x7F, 0xCF, // +#--####
    0x7F, 0xDF, // +#-#####
    0x7F, 0xDF, // +#-#####
    // door arch upper right
    0xFF, 0xFF, // ########
    0xFF, 0x11, // ---#---#
    0xF1, 0x1F, // ---#+++#
    0xF1, 0xCF, // ##--+++#
    0xFF, 0xE7, // ###--###
    0xFC, 0xF3, // ####--++
    0xFC, 0xFB, // #####-++
    0xFC, 0xFB, // #####-++
    // door arch lower left
    0xFF, 0xDF, // ##-#####
    0xFF, 0x1F, // ---#####
    0x3F, 0xDF, // ++-#####
    0x3F, 0xDF, // ++-#####
    0xFF, 0xDF, // ##-#####
    0x7F, 0xDF, // +#-#####
    0x7F, 0xDF, // +#-#####
    0x7F, 0xDF, // +#-#####
    // door arch lower right
    0xFF, 0xFB, // #####-##
    0xFF, 0xF9, // #####--#
    0xFD, 0xFB, // #####-+#
    0xFD, 0xFB, // #####-+#
    0xFF, 0xFB, // #####-##
    0xFC, 0xFB, // #####-++
    0xFC, 0xFB, // #####-++
    0xFC, 0xFB, // #####-++
    // doorway upper left
    0xFF, 0xDF, // ##-#####
    0xFF, 0x1F, // ---#####
    0x3F, 0xDF, // ++-#####
    0x3F, 0xDF, // ++-#####
    0xFF, 0xDF, // ##-#####
    0x7F, 0xDF, // +#-#####
    0x7F, 0xDF, // +#-#####
    0x7F, 0xDF, // +#-#####
    // doorway upper right
    0xFF, 0xFB, // #####-##
    0xFF, 0xF9, // #####--#
    0xFD, 0xFB, // #####-+#
    0xFD, 0xFB, // #####-+#
    0xFF, 0xFB, // #####-##
    0xFC, 0xFB, // #####-++
    0xFC, 0xFB, // #####-++
    0xFC, 0xFB, // #####-++
    // doorway lower left
    0xFF, 0xDF, // ##-#####
    0xFF, 0x1F, // ---#####
    0x3F, 0xDF, // ++-#####
    0x3F, 0xDF, // ++-#####
    0xFF, 0xDF, // ##-#####
    0x7F, 0xDF, // +#-#####
    0x7F, 0xDF, // +#-#####
    0x7F, 0xDF, // +#-#####
    // doorway lower right
    0xFF, 0xFB, // #####-##
    0xFF, 0xF9, // #####--#
    0xFD, 0xFB, // #####-+#
    0xFD, 0xFB, // #####-+#
    0xFF, 0xFB, // #####-##
    0xFC, 0xFB, // #####-++
    0xFC, 0xFB, // #####-++
    0xFC, 0xFB, // #####-++
};
// clang-format on

// the inner merlon, the one the keep's own battlement row is stamped from. the outer merlon leaves
// its notch and the two rows above its cap transparent, so the sky showed through the row the tower
// stands on; here the notch is the castle's black mortar and the tooth runs the full 8px, which
// puts masonry under the tower and keeps the notches reading as crenellation
// clang-format off
static const uint8_t kCastleCrenelInnerTile[16] = {
    0xFF, 0x03, // ------##
    0xFF, 0x13, // ---#--##
    0x13, 0xFF, // +++#++##
    0x13, 0xFF, // +++#++##
    0xFF, 0xFF, // ########
    0x47, 0xFF, // +#+++###
    0x47, 0xFF, // +#+++###
    0x47, 0xFF, // +#+++###
};
// clang-format on

// clouds, two block rows tall: a rounded left cap and a repeatable middle, each drawn as one
// 16x32 mass with three bumps across its top, a black outline and a cloud-blue underside.
// the right cap is the left one mirrored, which the block's own x-flip attribute pays for
// clang-format off
static const uint8_t kCloudTiles[256] = {
    // cap upper left
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x01, 0x01, // .......#
    0x07, 0x06, // .....##-
    0x0F, 0x08, // ....#---
    0x1F, 0x10, // ...#----
    0x3F, 0x20, // ..#-----
    // cap upper right
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x10, 0x10, // ...#....
    0xFF, 0xEF, // ###-####
    0xFF, 0x00, // --------
    0xFF, 0x00, // --------
    0xFF, 0x00, // --------
    0xFF, 0x00, // --------
    // cap lower left
    0x3F, 0x20, // ..#-----
    0x7F, 0x40, // .#------
    0xFF, 0x80, // #-------
    0xFF, 0x80, // #-------
    0xFF, 0x80, // #-------
    0xFC, 0x83, // #-----++
    0xFB, 0x84, // #----+--
    0xFF, 0x80, // #-------
    // cap lower right
    0xFF, 0x00, // --------
    0xFF, 0x00, // --------
    0xFF, 0x00, // --------
    0xFF, 0x00, // --------
    0xFF, 0x00, // --------
    0xFF, 0x00, // --------
    0x7F, 0x80, // +-------
    0xFF, 0x00, // --------
    // middle upper left
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x0F, 0x0F, // ....####
    0x3F, 0x30, // ..##----
    0x7F, 0x40, // .#------
    0xFF, 0x80, // #-------
    0xFF, 0x00, // --------
    // middle upper right
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x80, 0x80, // #.......
    0xF8, 0x78, // -####...
    0xFE, 0x06, // -----##.
    0xFF, 0x01, // -------#
    0xFF, 0x00, // --------
    0xFF, 0x00, // --------
    // middle lower left
    0xFF, 0x00, // --------
    0xFF, 0x00, // --------
    0xFF, 0x00, // --------
    0xFF, 0x00, // --------
    0xFF, 0x00, // --------
    0xFF, 0x00, // --------
    0xFF, 0x00, // --------
    0xFF, 0x00, // --------
    // middle lower right
    0xFF, 0x00, // --------
    0xFF, 0x00, // --------
    0xFF, 0x00, // --------
    0xFF, 0x00, // --------
    0xFF, 0x00, // --------
    0xFF, 0x00, // --------
    0xFF, 0x00, // --------
    0xFF, 0x00, // --------
    // cap bottom-row upper left
    0xFF, 0x80, // #-------
    0xFF, 0x80, // #-------
    0xFF, 0x80, // #-------
    0xFF, 0x80, // #-------
    0xFF, 0x80, // #-------
    0xBF, 0xC0, // #+------
    0xBF, 0xC0, // #+------
    0x5F, 0x60, // .#+-----
    // cap bottom-row upper right
    0xFF, 0x00, // --------
    0xFF, 0x00, // --------
    0xFF, 0x00, // --------
    0xFF, 0x00, // --------
    0xFF, 0x00, // --------
    0xFF, 0x00, // --------
    0xFF, 0x00, // --------
    0xFF, 0x00, // --------
    // cap bottom-row lower left
    0x2F, 0x30, // ..#+----
    0x27, 0x38, // ..#++---
    0x11, 0x1E, // ...#+++-
    0x08, 0x0F, // ....#+++
    0x06, 0x07, // .....##+
    0x01, 0x01, // .......#
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    // cap bottom-row lower right
    0xFF, 0x00, // --------
    0xFF, 0x00, // --------
    0xFF, 0x00, // --------
    0x10, 0xEF, // +++-++++
    0x00, 0xFF, // ++++++++
    0xEF, 0xFF, // ###+####
    0x10, 0x10, // ...#....
    0x00, 0x00, // ........
    // middle bottom-row upper left
    0xFF, 0x00, // --------
    0xFF, 0x00, // --------
    0xF9, 0x06, // -----++-
    0xF6, 0x09, // ----+--+
    0xFF, 0x00, // --------
    0xFF, 0x00, // --------
    0xFF, 0x00, // --------
    0xFF, 0x00, // --------
    // middle bottom-row upper right
    0xFF, 0x00, // --------
    0xFF, 0x00, // --------
    0xFF, 0x00, // --------
    0xFF, 0x00, // --------
    0xFF, 0x00, // --------
    0xFF, 0x00, // --------
    0xFF, 0x00, // --------
    0xFF, 0x00, // --------
    // middle bottom-row lower left
    0x7F, 0x80, // +-------
    0x3F, 0xC0, // ++------
    0x8F, 0xF0, // #+++----
    0x40, 0x7F, // .#++++++
    0x30, 0x3F, // ..##++++
    0x0F, 0x0F, // ....####
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    // middle bottom-row lower right
    0xFF, 0x00, // --------
    0xFE, 0x01, // -------+
    0xF8, 0x07, // -----+++
    0x81, 0x7F, // -++++++#
    0x06, 0xFE, // +++++##.
    0x78, 0xF8, // +####...
    0x80, 0x80, // #.......
    0x00, 0x00, // ........
};
// clang-format on

// background hills: a rounded summit, a 45-degree left slope and a speckled body. the right
// slope is the left one mirrored by its block's x-flip attribute
// clang-format off
static const uint8_t kHillTiles[192] = {
    // peak upper left
    0x0F, 0x0F, // ....####
    0x0C, 0x0B, // ....#-++
    0x18, 0x17, // ...#-+++
    0x18, 0x17, // ...#-+++
    0x18, 0x17, // ...#-+++
    0x30, 0x2F, // ..#-++++
    0x30, 0x2F, // ..#-++++
    0x30, 0x2F, // ..#-++++
    // peak upper right
    0xF0, 0xF0, // ####....
    0x10, 0xF0, // +++#....
    0x08, 0xF8, // ++++#...
    0x08, 0xF8, // ++++#...
    0x08, 0xF8, // ++++#...
    0x04, 0xFC, // +++++#..
    0x04, 0xFC, // +++++#..
    0x04, 0xFC, // +++++#..
    // peak lower left
    0x30, 0x2F, // ..#-++++
    0x60, 0x5F, // .#-+++++
    0x60, 0x5F, // .#-+++++
    0x60, 0x5F, // .#-+++++
    0x60, 0x5F, // .#-+++++
    0xC0, 0xBF, // #-++++++
    0xC0, 0xBF, // #-++++++
    0xC0, 0xBF, // #-++++++
    // peak lower right
    0x04, 0xFC, // +++++#..
    0x02, 0xFE, // ++++++#.
    0x02, 0xFE, // ++++++#.
    0x02, 0xFE, // ++++++#.
    0x02, 0xFE, // ++++++#.
    0x01, 0xFF, // +++++++#
    0x01, 0xFF, // +++++++#
    0x01, 0xFF, // +++++++#
    // left slope upper left
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    // left slope upper right
    0x01, 0x01, // .......#
    0x03, 0x02, // ......#-
    0x06, 0x05, // .....#-+
    0x0C, 0x0B, // ....#-++
    0x18, 0x17, // ...#-+++
    0x30, 0x2F, // ..#-++++
    0x60, 0x5F, // .#-+++++
    0xC0, 0xBF, // #-++++++
    // left slope lower left
    0x01, 0x01, // .......#
    0x03, 0x02, // ......#-
    0x06, 0x05, // .....#-+
    0x0C, 0x0B, // ....#-++
    0x18, 0x17, // ...#-+++
    0x30, 0x2F, // ..#-++++
    0x60, 0x5F, // .#-+++++
    0xC0, 0xBF, // #-++++++
    // left slope lower right
    0x80, 0x7F, // -+++++++
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    // fill upper left
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    0x0C, 0xFF, // ++++##++
    0x0C, 0xFF, // ++++##++
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    // fill upper right
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    0x30, 0xFF, // ++##++++
    // fill lower left
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    0x06, 0xFF, // +++++##+
    0x06, 0xFF, // +++++##+
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    // fill lower right
    0x30, 0xFF, // ++##++++
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
};
// clang-format on

// bushes, one block row tall and sitting straight on the grass: a rounded cap and a repeatable
// middle, light green over a scalloped dark-green underside. the right cap is the cap mirrored
// clang-format off
static const uint8_t kBushTiles[128] = {
    // bush cap upper left
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    // bush cap upper right
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    // bush cap lower left
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x01, 0x01, // .......#
    0x03, 0x02, // ......#-
    0x23, 0x22, // ..#...#-
    0xFF, 0xDC, // ##-###--
    0xFF, 0x00, // --------
    // bush cap lower right
    0x00, 0x00, // ........
    0x08, 0x08, // ....#...
    0x7F, 0x77, // .###-###
    0xFF, 0x80, // #-------
    0xFF, 0x00, // --------
    0xFF, 0x00, // --------
    0xF7, 0x08, // ----+---
    0x80, 0x7F, // -+++++++
    // bush middle upper left
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x0F, 0x0F, // ....####
    0x1F, 0x10, // ...#----
    // bush middle upper right
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x80, 0x80, // #.......
    0xF8, 0x78, // -####...
    0xFC, 0x04, // -----#..
    // bush middle lower left
    0x3F, 0x20, // ..#-----
    0x7F, 0x40, // .#------
    0xFF, 0x80, // #-------
    0xF0, 0x0F, // ----++++
    0xE0, 0x1F, // ---+++++
    0xC0, 0x3F, // --++++++
    0x80, 0x7F, // -+++++++
    0x00, 0xFF, // ++++++++
    // bush middle lower right
    0xFE, 0x02, // ------#.
    0xFF, 0x01, // -------#
    0x7F, 0x80, // +-------
    0x07, 0xF8, // +++++---
    0x03, 0xFC, // ++++++--
    0x01, 0xFE, // +++++++-
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
};
// clang-format on

// a block's four tiles all take one attribute byte, and the ball's block is scenery in vram
// bank 1 - so the shaft under it and the sky beside it need bank-1 copies of their own
// clang-format off
static const uint8_t kScenPoleTiles[32] = {
    // a second copy of the shaft, in vram bank 1
    0x70, 0x10, // .--#....
    0x70, 0x10, // .--#....
    0x70, 0x10, // .--#....
    0x70, 0x10, // .--#....
    0x70, 0x10, // .--#....
    0x70, 0x10, // .--#....
    0x70, 0x10, // .--#....
    0x70, 0x10, // .--#....
    // the empty cell beside the ball
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
};
// clang-format on

// 1-3's thin platform: a four-px plank with sky under it
// clang-format off
static const uint8_t kThinTiles[32] = {
    // thin platform deck
    0xFF, 0xFF, // ########
    0xFF, 0x00, // --------
    0x00, 0xFF, // ++++++++
    0xFF, 0xFF, // ########
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    // the empty cell under it
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
};
// clang-format on

// 1-4's lava, painted over the death plane
// clang-format off
static const uint8_t kLavaTiles[32] = {
    // lava crest
    0x66, 0x00, // .--..--.
    0xFF, 0x00, // --------
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    0x22, 0xFF, // ++#+++#+
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    // lava fill
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    0x10, 0xFF, // +++#++++
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
    0x08, 0xFF, // ++++#+++
    0x00, 0xFF, // ++++++++
    0x00, 0xFF, // ++++++++
};
// clang-format on

// the castle's ending pair
// clang-format off
static const uint8_t kBridgeAxeTiles[32] = {
    // bridge plank
    0xFF, 0xFF, // ########
    0x00, 0xFF, // ++++++++
    0x4A, 0xFF, // +#++#+#+
    0x00, 0xFF, // ++++++++
    0xFF, 0xFF, // ########
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    // the axe on its handle
    0x00, 0x00, // ........
    0x3C, 0x3C, // ..####..
    0x7E, 0x42, // .#----#.
    0x7E, 0x42, // .#----#.
    0x3C, 0x3C, // ..####..
    0x00, 0x18, // ...++...
    0x00, 0x18, // ...++...
    0x00, 0x18, // ...++...
};
// clang-format on

// a world coin: a six-px oval standing in an otherwise empty cell, so its palette's color 0
// has to be the backdrop
// clang-format off
static const uint8_t kCoinTiles[64] = {
    // coin upper left
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x03, 0x03, // ......##
    0x07, 0x04, // .....#--
    0x07, 0x04, // .....#--
    0x07, 0x04, // .....#--
    0x07, 0x04, // .....#--
    0x07, 0x04, // .....#--
    // coin upper right
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0xC0, 0xC0, // ##......
    0x20, 0xE0, // ++#.....
    0x20, 0xE0, // ++#.....
    0x20, 0xE0, // ++#.....
    0x20, 0xE0, // ++#.....
    0x20, 0xE0, // ++#.....
    // coin lower left
    0x07, 0x04, // .....#--
    0x07, 0x04, // .....#--
    0x07, 0x04, // .....#--
    0x03, 0x03, // ......##
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    // coin lower right
    0x20, 0xE0, // ++#.....
    0x20, 0xE0, // ++#.....
    0x20, 0xE0, // ++#.....
    0xC0, 0xC0, // ##......
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
};
// clang-format on

// m18's art pass gives most blocks four distinct quadrants instead of one tile stamped four times,
// which is 99 background tiles where the old placeholder art was 21. bank 0's tile space cannot
// hold that beside the sprites, so the art is split: everything a level's terrain needs stays in
// vram bank 0, at the pinned 0xa0-0xbf block and the eight ids past mario's last sprite frame, and
// the scenery - the castle, the flag's head, and 1-1's clouds, hills and bushes - goes to vram
// bank 1, which nothing else in the game has ever used for tiles. a cgb bg map attribute picks a
// tile's bank per cell (kCamAttrVram1 in mario.h), so the two sets coexist with no id conflict at
// all: the font keeps its own glyph range in bank 0 and never has to be reloaded
void assets_load_bg_tiles(void) BANKED {
    set_bkg_data(kTileGroundTopL, 4, kGroundTiles);
    set_bkg_data(kTileGroundFillBl, 2, kGroundLowerTiles);
    set_bkg_data(kTileBrickTl, 4, kBrickTiles);
    set_bkg_data(kTileQuestionTl, 4, kQuestionTiles);
    set_bkg_data(kTileSpentTl, 4, kSpentTiles);
    set_bkg_data(kTileHardTl, 4, kHardTiles);
    set_bkg_data(kTilePipeLipL, 9, kPipeTiles);
    set_bkg_data(kTileThin, 2, kThinTiles);
    set_bkg_data(kTileFlagPole, 1, kFlagPoleTile);
    set_bkg_data(kTileBridge, 2, kBridgeAxeTiles);
    set_bkg_data(kTileCoinTl, 4, kCoinTiles);
}

// the same call writes vram bank 1 with vbk pointing there, so the scenery lands beside the font
// rather than on top of it. bcpd and vram are both mode-locked on real hardware and terrain_init
// runs with the lcd off, which is where this is called from; vbk goes back before anything else
// touches the map, because set_bkg_tiles would otherwise write tile numbers into the attribute map
void assets_load_scenery_tiles(void) BANKED {
    VBK_REG = VBK_BANK_1;
    set_bkg_data(kTileLavaTop, 2, kLavaTiles);
    set_bkg_data(kTileCastleWall, 14, kCastleTiles);
    set_bkg_data(kTileCastleCrenelInner, 1, kCastleCrenelInnerTile);
    set_bkg_data(kTileFlagBall, 5, kFlagHeadTiles);
    set_bkg_data(kTileCloudCapTl, 16, kCloudTiles);
    set_bkg_data(kTileHillPeakTl, 12, kHillTiles);
    set_bkg_data(kTileBushCapTl, 8, kBushTiles);
    set_bkg_data(kTileScenPole, 2, kScenPoleTiles);
    // the sideways pipe is solid terrain, not scenery, but vram bank 0 has no tile ids left: it
    // rides here with the scenery and reads back through kCamAttrVram1 the same way
    set_bkg_data(kTilePipeSideTl, 9, kPipeSideTiles);
    VBK_REG = VBK_BANK_0;
}

// the overworld's eight cgb bg palettes. every slot but the ground's keeps the sky in color 0,
// because most of these tiles leave part of their cell empty and that empty part is the backdrop.
// the ground's color 0 is the grass instead: its blocks cover their whole 16x16 cell, and the two
// rows of grass along the top of a surface block are the one place it shows
void assets_load_bg_palettes(void) BANKED {
    palette_color_t sky[4] = {kSkyRgb, RGB(31, 31, 31), RGB(6, 20, 31), RGB(0, 0, 0)};
    palette_color_t ground[4] = {RGB(2, 17, 0), RGB(31, 24, 19), RGB(19, 9, 0), RGB(0, 0, 0)};
    palette_color_t brick[4] = {kSkyRgb, RGB(31, 24, 19), RGB(19, 9, 0), RGB(0, 0, 0)};
    palette_color_t question[4] = {kSkyRgb, RGB(31, 20, 8), RGB(31, 31, 31), RGB(0, 0, 0)};
    palette_color_t pipe[4] = {kSkyRgb, RGB(14, 31, 6), RGB(2, 17, 0), RGB(0, 0, 0)};
    palette_color_t neutral[4] = {kSkyRgb, RGB(31, 31, 31), RGB(31, 24, 19), RGB(0, 0, 0)};
    palette_color_t spent[4] = {kSkyRgb, RGB(24, 15, 6), RGB(13, 6, 0), RGB(0, 0, 0)};
    palette_color_t coin[4] = {kSkyRgb, RGB(31, 26, 7), RGB(31, 20, 8), RGB(0, 0, 0)};
    set_bkg_palette(kCamPalSky, 1, sky);
    set_bkg_palette(kCamPalGround, 1, ground);
    set_bkg_palette(kCamPalBrick, 1, brick);
    set_bkg_palette(kCamPalQuestion, 1, question);
    set_bkg_palette(kCamPalPipe, 1, pipe);
    set_bkg_palette(kCamPalNeutral, 1, neutral);
    set_bkg_palette(kCamPalSpent, 1, spent);
    set_bkg_palette(kCamPalCoin, 1, coin);
}

void assets_load_bg_palettes_underground(void) BANKED {
    // the same eight slots and the same art: only the colors say the room is below ground. the
    // backdrop goes to a near-black navy, the masonry from brown to blue and the pipes to teal,
    // and the two warm slots keep their gold so a coin still reads as one
    palette_color_t sky[4] = {kUndergroundRgb, RGB(20, 24, 31), RGB(6, 14, 26), RGB(0, 0, 0)};
    palette_color_t ground[4] = {RGB(8, 14, 28), RGB(14, 20, 31), RGB(5, 9, 22), RGB(0, 0, 0)};
    palette_color_t brick[4] = {kUndergroundRgb, RGB(12, 18, 31), RGB(6, 10, 26), RGB(0, 0, 0)};
    palette_color_t question[4] = {kUndergroundRgb, RGB(31, 20, 8), RGB(31, 31, 31), RGB(0, 0, 0)};
    palette_color_t pipe[4] = {kUndergroundRgb, RGB(8, 31, 24), RGB(0, 20, 16), RGB(0, 0, 0)};
    palette_color_t neutral[4] = {kUndergroundRgb, RGB(24, 26, 31), RGB(12, 18, 31), RGB(0, 0, 0)};
    palette_color_t spent[4] = {kUndergroundRgb, RGB(8, 11, 20), RGB(4, 6, 14), RGB(0, 0, 0)};
    palette_color_t coin[4] = {kUndergroundRgb, RGB(31, 26, 7), RGB(31, 20, 8), RGB(0, 0, 0)};
    set_bkg_palette(kCamPalSky, 1, sky);
    set_bkg_palette(kCamPalGround, 1, ground);
    set_bkg_palette(kCamPalBrick, 1, brick);
    set_bkg_palette(kCamPalQuestion, 1, question);
    set_bkg_palette(kCamPalPipe, 1, pipe);
    set_bkg_palette(kCamPalNeutral, 1, neutral);
    set_bkg_palette(kCamPalSpent, 1, spent);
    set_bkg_palette(kCamPalCoin, 1, coin);
}

void assets_load_bg_palettes_castle(void) BANKED {
    // the same eight slots again, drained to castle stone. lava takes the coin slot, which no
    // castle grid paints a world coin with, so the one warm ramp on screen is the pit
    palette_color_t sky[4] = {kCastleRgb, RGB(20, 20, 22), RGB(11, 11, 13), RGB(0, 0, 0)};
    palette_color_t ground[4] = {RGB(9, 9, 11), RGB(21, 20, 19), RGB(12, 11, 11), RGB(0, 0, 0)};
    palette_color_t brick[4] = {kCastleRgb, RGB(20, 19, 18), RGB(12, 11, 11), RGB(0, 0, 0)};
    palette_color_t question[4] = {kCastleRgb, RGB(31, 20, 8), RGB(31, 31, 31), RGB(0, 0, 0)};
    palette_color_t pipe[4] = {kCastleRgb, RGB(22, 22, 24), RGB(13, 13, 15), RGB(0, 0, 0)};
    palette_color_t neutral[4] = {kCastleRgb, RGB(28, 28, 30), RGB(17, 17, 19), RGB(0, 0, 0)};
    palette_color_t spent[4] = {kCastleRgb, RGB(9, 9, 10), RGB(6, 6, 7), RGB(0, 0, 0)};
    palette_color_t lava[4] = {kCastleRgb, RGB(31, 24, 6), RGB(28, 9, 1), RGB(0, 0, 0)};
    set_bkg_palette(kCamPalSky, 1, sky);
    set_bkg_palette(kCamPalGround, 1, ground);
    set_bkg_palette(kCamPalBrick, 1, brick);
    set_bkg_palette(kCamPalQuestion, 1, question);
    set_bkg_palette(kCamPalPipe, 1, pipe);
    set_bkg_palette(kCamPalNeutral, 1, neutral);
    set_bkg_palette(kCamPalSpent, 1, spent);
    set_bkg_palette(kCamPalCoin, 1, lava);
}

// the world map screen (mapscreen.c) is a card, never up during play, so it gets its own eight
// slots instead of the level's: kCamPalSky becomes the black band behind WORLD/1-1 and the lives/
// CLEAR LIST text (color 3 is the white ink font_color draws with), kCamPalGround becomes the map
// band's sky-blue backdrop, kCamPalBrick/kCamPalQuestion/kCamPalSpent keep coloring the same three
// reused block kinds they always have (the castle, and the locked/current/cleared node markers),
// kCamPalPipe keeps coloring the reused hill/bush greens, and kCamPalNeutral/kCamPalCoin - which no
// kind this screen draws is pinned to - are repurposed for the two brand new kinds, water and path
void assets_load_map_bg_palettes(void) BANKED {
    palette_color_t black[4] = {kMapSkyRgb, kMapSkyRgb, kMapSkyRgb, RGB(31, 31, 31)};
    palette_color_t sky[4] = {RGB(16, 22, 31), RGB(31, 31, 31), RGB(9, 15, 28), RGB(0, 0, 0)};
    palette_color_t brick[4] = {RGB(10, 7, 5), RGB(24, 18, 12), RGB(16, 11, 7), RGB(0, 0, 0)};
    palette_color_t question[4] = {RGB(10, 6, 2), RGB(31, 20, 4), RGB(31, 31, 20), RGB(0, 0, 0)};
    palette_color_t pipe[4] = {RGB(4, 14, 2), RGB(20, 31, 10), RGB(8, 22, 4), RGB(0, 0, 0)};
    palette_color_t water[4] = {RGB(4, 10, 28), RGB(20, 28, 31), RGB(8, 16, 31), RGB(0, 0, 0)};
    palette_color_t spent[4] = {RGB(8, 8, 10), RGB(18, 18, 20), RGB(12, 12, 14), RGB(0, 0, 0)};
    palette_color_t path[4] = {RGB(24, 18, 8), RGB(31, 27, 16), RGB(28, 22, 12), RGB(0, 0, 0)};
    set_bkg_palette(kCamPalSky, 1, black);
    set_bkg_palette(kCamPalGround, 1, sky);
    set_bkg_palette(kCamPalBrick, 1, brick);
    set_bkg_palette(kCamPalQuestion, 1, question);
    set_bkg_palette(kCamPalPipe, 1, pipe);
    set_bkg_palette(kCamPalNeutral, 1, water);
    set_bkg_palette(kCamPalSpent, 1, spent);
    set_bkg_palette(kCamPalCoin, 1, path);
}

// water (foam top edge, then a plain body), path (a dark edge where it meets the grass, then plain
// sand), the round node marker's one quadrant (reused via flip for the other three - see put_marker
// in mapscreen.c, the same trick the hill/bush/cloud kinds already use for their mirrored halves),
// and the CLEAR LIST panel's border (a corner, reused by flip for all four, plus a straight edge
// for the horizontal sides and one for the vertical) and its two checkbox cells. none of this art
// the level ever draws; bank 0's tile table is exactly full (see the kTile* ids in mario.h), so all
// of it rides in bank 1 like the scenery does, at ids nothing in that bank ever claimed (the
// scenery run stops at 0x5a). the ids are declared in assets.h so mapscreen.c can build bg quads
// out of them directly
// clang-format off
static const uint8_t kMapTiles[160] = {
    // water top: a foam line over the first wave of body
    0xFF, 0xB6, 0xB6, 0x49, 0x00, 0xFF, 0x22, 0xDD,
    0x00, 0xFF, 0x44, 0xBB, 0x00, 0xFF, 0x10, 0xEF,
    // water body: a plain sheet with a few darker ripples
    0x10, 0xEF, 0x00, 0xFF, 0x42, 0xBD, 0x00, 0xFF,
    0x08, 0xF7, 0x00, 0xFF, 0x82, 0x7D, 0x00, 0xFF,
    // path top: a dark line against the grass above, then a speckled start to the sand
    0xFF, 0xFF, 0x22, 0xDD, 0x00, 0xFF, 0x88, 0x77,
    0x00, 0xFF, 0x44, 0xBB, 0x00, 0xFF, 0x10, 0xEF,
    // path body: plain sand, speckled the same way
    0x20, 0xDF, 0x02, 0xFD, 0x80, 0x7F, 0x08, 0xF7,
    0x00, 0xFF, 0x10, 0xEF, 0x02, 0xFD, 0x20, 0xDF,
    // marker: the top-left quadrant of a filled, outlined circle with a small highlight - circularly
    // symmetric, so put_marker gets the other three quadrants by x/y-flipping this one tile
    0x00, 0x00, 0x07, 0x07, 0x18, 0x1F, 0x30, 0x3F,
    0x2E, 0x31, 0x4E, 0x71, 0x4E, 0x71, 0x40, 0x7F,
    // list corner: a 2px top-left bracket, flipped for the other three corners
    0xFF, 0xFF, 0xFF, 0xFF, 0xC0, 0xC0, 0xC0, 0xC0,
    0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0,
    // list h-edge: a 2px top line, flipped for the bottom
    0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    // list v-edge: a 2px left line, flipped for the right
    0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0,
    0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0,
    // list cell, hollow: a small square outline
    0x00, 0x00, 0x7E, 0x7E, 0x42, 0x42, 0x42, 0x42,
    0x42, 0x42, 0x42, 0x42, 0x7E, 0x7E, 0x00, 0x00,
    // list cell, filled: the same square, solid
    0x00, 0x00, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E,
    0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x00, 0x00,
};
// clang-format on

// the map's own foliage (see kTileMapHedgeTallTop in assets.h): a low hedge row - three
// flat-topped mounds, each wider than it is tall, at three heights - rather than the level's
// 45-degree hill slopes or the standing-clump first pass that still read as teardrop creatures.
// drawn with kCamPalPipe, the same slot the level's hills and bushes already use on this screen -
// its four colors are exactly a dark green field (also doubling as the underside shading and the
// plain backdrop above the hedge line), a light upper-left highlight, a mid green body and a black
// outline, so no new palette slot is needed. every "top"/"base" pair is the left half of a 16x16
// quad; put_dome_quad gets the right half by x-flipping the same tile, the trick a mirrored hill
// slope or bush cap already uses for its other side. each mound's silhouette widens fast off the
// flat top - never a single-pixel peak - then holds a level plateau before the ragged run near the
// bottom of every base tile breaks the green/sand line into a scalloped fringe instead of a ruled
// one. field fill is a plain, shapeless top/base pair - but not a flat one: each half carries its
// own small, asymmetric scatter of mid-green specks (no two specks in either half share a row or
// column), and the two halves use different scatters so a block's base never mirrors its own top.
// drawn through put_dome_quad exactly like the hedges (mirrored on its right half too), the mix of
// a different top/base and the x-flip keeps a long run of field cells from lining every speck up
// into a visible grid the way one symmetric, unmirrored tile would
// clang-format off
static const uint8_t kFoliageTiles[128] = {
    // hedge tall top: field above, the mound's flat-wide crown emerging low in the tile
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x17, 0x60, 0x5F, 0xC0, 0xBF, 0xC0, 0xBF,
    // hedge tall base: full body, scalloped where it meets the sand
    0xC0, 0xBF, 0xC0, 0xBF, 0xC0, 0xBF, 0x80, 0xBF, 0x80, 0xBF, 0x80, 0xBD, 0x80, 0xA7, 0x80, 0x99,
    // hedge medium top: mostly field, the crown only just emerging at the very bottom
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x17,
    // hedge medium base: full body, a shorter mound overall than tall
    0x40, 0x5F, 0x80, 0xBF, 0x80, 0xBF, 0x80, 0xBF, 0x80, 0xBF, 0x80, 0xBD, 0x80, 0xA7, 0x80, 0x99,
    // hedge low top: all field, this mound never reaches the upper tile
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    // hedge low base: the shortest mound, crown and scalloped fringe both inside one tile
    0x00, 0x00, 0x00, 0x00, 0x10, 0x17, 0x40, 0x5F, 0x80, 0xBF, 0x80, 0xBD, 0x80, 0xA7, 0x80, 0x99,
    // field fill top: a sparse, asymmetric scatter of specks over the plain dark field
    0x00, 0x00, 0x00, 0x20, 0x00, 0x04, 0x00, 0x00, 0x00, 0x80, 0x00, 0x02, 0x00, 0x00, 0x00, 0x10,
    // field fill base: a different scatter, so it never mirrors the top half it sits under
    0x00, 0x08, 0x00, 0x00, 0x00, 0x40, 0x00, 0x02, 0x00, 0x00, 0x00, 0x20, 0x00, 0x04, 0x00, 0x80,
};
// clang-format on

// the map strip's castle icon, left half only: four tile columns wide on screen, of which these
// are the outer and inner left ones, and six tile rows tall. drawn under the map's brick palette,
// so color 0 is its darkest brown (the merlon notches and the shadow inside a crenellation), 1 the
// light brown of the pilaster highlights and the window sills, 2 the stone itself and 3 the black
// of the door, the windows and every outline. the two cells beside the tower are not drawn at all,
// so the band's own sky shows around it and the silhouette steps in at the tower - see
// map_draw_castle in mapscreen.c, which mirrors these for the right half
// clang-format off
static const uint8_t kMapCastleTiles[160] = {
    // tower merlons over the crenellation line
    0xF3, 0xF3, 0xE3, 0x92, 0xE3, 0x92, 0xFF, 0xFF, 0xE0, 0x9F, 0xE0, 0x9F, 0xE0, 0x9F, 0xE0, 0x9F,
    // tower wall with its window
    0xE7, 0x9F, 0xE7, 0x9F, 0xE7, 0x9F, 0xE7, 0x9F, 0xE7, 0x9F, 0xE0, 0x9F, 0xE0, 0x9F, 0xE0, 0x9F,
    // the keep's own merlons, at the shoulder beside the tower
    0xE7, 0xE7, 0xC6, 0xA5, 0xC6, 0xA5, 0xFF, 0xFF, 0xE0, 0x9F, 0xE0, 0x9F, 0xE0, 0x9F, 0xE0, 0x9F,
    // tower wall, the run between the two crenellations
    0xE0, 0x9F, 0xE0, 0x9F, 0xE0, 0x9F, 0xE0, 0x9F, 0xE0, 0x9F, 0xE0, 0x9F, 0xE0, 0x9F, 0xE0, 0x9F,
    // keep wall: the arched top of the window under the cornice
    0xFF, 0xFF, 0xE0, 0x9F, 0xE0, 0x9F, 0xD8, 0xBF, 0xFC, 0xBF, 0xFC, 0xBF, 0xFC, 0xBF, 0xFC, 0xBF,
    // the cornice the tower sinks into, then plain stone
    0xFF, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF,
    // keep wall: the window's lower half and its sill
    0xFC, 0xBF, 0xFC, 0xBF, 0xFC, 0xBF, 0xFC, 0x83, 0xE0, 0x9F, 0xE0, 0x9F, 0xE0, 0x9F, 0xE0, 0x9F,
    // the door's arch
    0x00, 0xFF, 0x03, 0xFF, 0x07, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF,
    // keep wall down to the base line it stands on the path with
    0xE0, 0x9F, 0xE0, 0x9F, 0xE0, 0x9F, 0xE0, 0x9F, 0xE0, 0x9F, 0xE0, 0x9F, 0xE0, 0x9F, 0xFF, 0xFF,
    // the door, open all the way to the path
    0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0xFF, 0xFF,
};
// clang-format on

void assets_load_map_tiles(void) BANKED {
    VBK_REG = VBK_BANK_1;
    set_bkg_data(kTileMapCastleTowerTop, 10, kMapCastleTiles);
    set_bkg_data(kTileMapWaterTop, 10, kMapTiles);
    set_bkg_data(kTileMapHedgeTallTop, 8, kFoliageTiles);
    VBK_REG = VBK_BANK_0;
}

void assets_load_sprite_palettes(void) BANKED {
    // warm family: 1 skin, 2 the cap/shirt/overall red, 3 the dark his hair, shoes, straps and
    // outline accents are drawn in. color 0 is the sprite's transparency
    palette_color_t mario[4] = {RGB(0, 0, 0), RGB(31, 22, 14), RGB(26, 2, 0), RGB(9, 4, 0)};
    // fire mario keeps the art and swaps the two body shades for smb's white-over-red outfit. the
    // dark slot goes deep red rather than the cap's own: it also paints his hair and mustache, and
    // a bright red there reads as a wig against the white outfit
    palette_color_t fire[4] = {RGB(0, 0, 0), RGB(31, 22, 14), RGB(31, 31, 31), RGB(22, 3, 1)};
    set_sprite_palette(kPalMario, 1, mario);
    set_sprite_palette(kPalFire, 1, fire);
}

void assets_load_item_palettes(void) BANKED {
    palette_color_t mushroom[4] = {RGB(0, 0, 0), RGB(31, 26, 19), RGB(28, 4, 2), RGB(0, 0, 0)};
    palette_color_t star[4] = {RGB(0, 0, 0), RGB(31, 31, 24), RGB(31, 26, 4), RGB(12, 8, 0)};
    // roster.json: smbd modernises the 1-up to a green cap, which is what this palette paints
    palette_color_t oneup[4] = {RGB(0, 0, 0), RGB(31, 26, 19), RGB(2, 19, 2), RGB(0, 0, 0)};
    palette_color_t coin[4] = {RGB(0, 0, 0), RGB(31, 26, 7), RGB(31, 17, 3), RGB(0, 0, 0)};
    set_sprite_palette(kPalMushroom, 1, mushroom);
    set_sprite_palette(kPalStar, 1, star);
    set_sprite_palette(kPalOneup, 1, oneup);
    set_sprite_palette(kPalCoin, 1, coin);
}

void assets_load_enemy_palettes(void) BANKED {
    // roster.json: the goomba is gray on the nes and recoloured in smbd, so ours is the mushroom
    // brown the deluxe art reads as; the koopa keeps its green shell over tan skin
    palette_color_t goomba[4] = {RGB(0, 0, 0), RGB(31, 24, 19), RGB(19, 9, 0), RGB(0, 0, 0)};
    palette_color_t koopa[4] = {RGB(0, 0, 0), RGB(31, 26, 16), RGB(2, 19, 2), RGB(0, 0, 0)};
    set_sprite_palette(kPalGoomba, 1, goomba);
    set_sprite_palette(kPalKoopa, 1, koopa);
}

// the items. colors: 1 spots/shine, 2 the body, 3 the outline; color 0 is sprite transparency.
// mushroom and 1-up share a silhouette and differ only by palette, exactly as smb's own do. the
// rows below are the same ascii the terrain arrays carry: . transparent, - color 1, + 2, # 3
// clang-format off
static const uint8_t kItemTiles[256] = {
    // mushroom l top
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x0F, 0x0F, // ....####
    0x30, 0x3F, // ..##++++
    0x60, 0x7F, // .##+++++
    0x4C, 0x73, // .#++--++
    0x9C, 0xE3, // #++---++
    0x9C, 0xE3, // #++---++
    // mushroom l bot
    0x98, 0xE7, // #++--+++
    0x80, 0xFF, // #+++++++
    0x60, 0x7F, // .##+++++
    0x3F, 0x3F, // ..######
    0x1F, 0x10, // ...#----
    0x1F, 0x16, // ...#-##-
    0x1F, 0x16, // ...#-##-
    0x1F, 0x1F, // ...#####
    // mushroom r top
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0xF0, 0xF0, // ####....
    0x0C, 0xFC, // ++++##..
    0x06, 0xFE, // +++++##.
    0x32, 0xCE, // ++--++#.
    0x39, 0xC7, // ++---++#
    0x39, 0xC7, // ++---++#
    // mushroom r bot
    0x19, 0xE7, // +++--++#
    0x01, 0xFF, // +++++++#
    0x06, 0xFE, // +++++##.
    0xFC, 0xFC, // ######..
    0xF8, 0x08, // ----#...
    0xF8, 0x68, // -##-#...
    0xF8, 0x68, // -##-#...
    0xF8, 0xF8, // #####...
    // star l top
    0x01, 0x01, // .......#
    0x02, 0x03, // ......#+
    0x04, 0x07, // .....#++
    0x04, 0x07, // .....#++
    0xF0, 0xFF, // ####++++
    0x80, 0xFF, // #+++++++
    0x80, 0xFF, // #+++++++
    0x40, 0x7F, // .#++++++
    // star l bot
    0x26, 0x3F, // ..#++##+
    0x26, 0x3F, // ..#++##+
    0x20, 0x3F, // ..#+++++
    0x20, 0x3F, // ..#+++++
    0x42, 0x7E, // .#++++#.
    0x44, 0x7C, // .#+++#..
    0x48, 0x78, // .#++#...
    0x78, 0x78, // .####...
    // star r top
    0x80, 0x80, // #.......
    0x40, 0xC0, // +#......
    0x20, 0xE0, // ++#.....
    0x20, 0xE0, // ++#.....
    0x0F, 0xFF, // ++++####
    0x01, 0xFF, // +++++++#
    0x01, 0xFF, // +++++++#
    0x02, 0xFE, // ++++++#.
    // star r bot
    0x64, 0xFC, // +##++#..
    0x64, 0xFC, // +##++#..
    0x04, 0xFC, // +++++#..
    0x04, 0xFC, // +++++#..
    0x42, 0x7E, // .#++++#.
    0x22, 0x3E, // ..#+++#.
    0x12, 0x1E, // ...#++#.
    0x1E, 0x1E, // ...####.
    // oneup l top
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x0F, 0x0F, // ....####
    0x30, 0x3F, // ..##++++
    0x60, 0x7F, // .##+++++
    0x4C, 0x73, // .#++--++
    0x9C, 0xE3, // #++---++
    0x9C, 0xE3, // #++---++
    // oneup l bot
    0x98, 0xE7, // #++--+++
    0x80, 0xFF, // #+++++++
    0x60, 0x7F, // .##+++++
    0x3F, 0x3F, // ..######
    0x1F, 0x10, // ...#----
    0x1F, 0x16, // ...#-##-
    0x1F, 0x16, // ...#-##-
    0x1F, 0x1F, // ...#####
    // oneup r top
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0xF0, 0xF0, // ####....
    0x0C, 0xFC, // ++++##..
    0x06, 0xFE, // +++++##.
    0x32, 0xCE, // ++--++#.
    0x39, 0xC7, // ++---++#
    0x39, 0xC7, // ++---++#
    // oneup r bot
    0x19, 0xE7, // +++--++#
    0x01, 0xFF, // +++++++#
    0x06, 0xFE, // +++++##.
    0xFC, 0xFC, // ######..
    0xF8, 0x08, // ----#...
    0xF8, 0x68, // -##-#...
    0xF8, 0x68, // -##-#...
    0xF8, 0xF8, // #####...
    // coin pop top
    0x3C, 0x3C, // ..####..
    0x62, 0x5E, // .#-+++#.
    0xE1, 0x9F, // #--++++#
    0xE1, 0x9F, // #--++++#
    0xE1, 0x9F, // #--++++#
    0xE1, 0x9F, // #--++++#
    0xE1, 0x9F, // #--++++#
    0xE1, 0x9F, // #--++++#
    // coin pop bottom
    0xE1, 0x9F, // #--++++#
    0xE1, 0x9F, // #--++++#
    0xE1, 0x9F, // #--++++#
    0xE1, 0x9F, // #--++++#
    0xE1, 0x9F, // #--++++#
    0xE1, 0x9F, // #--++++#
    0x62, 0x5E, // .#-+++#.
    0x3C, 0x3C, // ..####..
    // fireball top, blank so the ball is one 8x8
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    // fireball bottom, spin frame A: an orange/red body (color 2), a two-pixel bright gold core
    // (color 1) dead center, and a black outline (color 3) - drawn under kPalCoin, not the star's
    // near-white set, which is the fix for "the fire should be more prominent and not just white"
    0x3C, 0x3C, // ..####..
    0x42, 0x7E, // .#++++#.
    0x81, 0xFF, // #++++++#
    0x99, 0xE7, // #++--++#
    0x99, 0xE7, // #++--++#
    0x81, 0xFF, // #++++++#
    0x42, 0x7E, // .#++++#.
    0x3C, 0x3C, // ..####..
};
// clang-format on

// the fireball's spin frame B, a 45-degree-rotated silhouette of the same body/core/outline
// coloring. it lives at the same tile id as frame A (kTileFireball/+1) but in CGB VRAM bank 1 -
// bank 0's tile table is exactly full end to end (see the kTile* ids in mario.h), so a second
// fireball frame has nowhere to go there. powerup_draw toggles the sprite's S_BANK attribute to
// pick this frame instead, the same way a bg tile picks bank 1 for scenery
// clang-format off
static const uint8_t kFireballFrameBTiles[32] = {
    // top, blank
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    // bottom, rotated
    0x18, 0x18, // ...##...
    0x24, 0x3C, // ..#++#..
    0x42, 0x7E, // .#++++#.
    0x99, 0xE7, // #++--++#
    0x99, 0xE7, // #++--++#
    0x42, 0x7E, // .#++++#.
    0x24, 0x3C, // ..#++#..
    0x18, 0x18, // ...##...
};
// clang-format on

// small mario, drawn as two 8x16 sprites. colors: 1 his skin - face, bare forearms and hands - 2
// the red cap, shirt and overalls, 3 his hair, mustache, shoes and the two overall buttons. stubby
// proportions: cap and face take the top seven rows, the body the next seven, the shoes the last
// two, and the sleeves stop at the elbow so the bare forearm reads against the red.
// every frame lights row 0, row 15, column 1 and column 14 of the 16px box and nothing outside
// columns 1..14, which is what lets the host tests read the sprite's exact top and side edges.
// the run cycle is idle / walk0 open stride / walk1 rear leg lifted / walk2 trailing stride, with
// the hands swinging between rows 8 and 11 so the three silhouettes read apart at speed; skid
// leans the whole head one column back with the front arm thrown out, and jump tucks the rear leg
// clang-format off
static const uint8_t kMarioTiles[384] = {
    // idle l top
    0x00, 0x0F, // ....++++
    0x00, 0x1F, // ...+++++
    0x20, 0x3F, // ..#+++++
    0x3F, 0x31, // ..##---#
    0x3F, 0x21, // ..#----#
    0x3F, 0x20, // ..#-----
    0x1F, 0x00, // ...-----
    0x00, 0x1F, // ...+++++
    // idle l bot
    0x00, 0x3F, // ..++++++
    0x60, 0x1F, // .--+++++
    0x68, 0x1F, // .--+#+++
    0x00, 0x1F, // ...+++++
    0x00, 0x1E, // ...++++.
    0x00, 0x1E, // ...++++.
    0x3E, 0x3E, // ..#####.
    0x3E, 0x3E, // ..#####.
    // idle r top
    0x00, 0xC0, // ++......
    0x00, 0xF8, // +++++...
    0x00, 0xFC, // ++++++..
    0xF8, 0x00, // -----...
    0xFC, 0x00, // ------..
    0xFC, 0xF8, // #####-..
    0xF0, 0x00, // ----....
    0x00, 0xF8, // +++++...
    // idle r bot
    0x00, 0xFC, // ++++++..
    0x06, 0xF8, // +++++--.
    0x16, 0xF8, // +++#+--.
    0x00, 0xF8, // +++++...
    0x00, 0x78, // .++++...
    0x00, 0x78, // .++++...
    0x7C, 0x7C, // .#####..
    0x7C, 0x7C, // .#####..
    // walk0 l top
    0x00, 0x0F, // ....++++
    0x00, 0x1F, // ...+++++
    0x20, 0x3F, // ..#+++++
    0x3F, 0x31, // ..##---#
    0x3F, 0x21, // ..#----#
    0x3F, 0x20, // ..#-----
    0x1F, 0x00, // ...-----
    0x00, 0x1F, // ...+++++
    // walk0 l bot
    0x00, 0x3F, // ..++++++
    0x00, 0x3F, // ..++++++
    0x68, 0x1F, // .--+#+++
    0x60, 0x1F, // .--+++++
    0x00, 0x3C, // ..++++..
    0x00, 0x38, // ..+++...
    0x7C, 0x7C, // .#####..
    0x7C, 0x7C, // .#####..
    // walk0 r top
    0x00, 0xC0, // ++......
    0x00, 0xF8, // +++++...
    0x00, 0xFC, // ++++++..
    0xF8, 0x00, // -----...
    0xFC, 0x00, // ------..
    0xFC, 0xF8, // #####-..
    0xF0, 0x00, // ----....
    0x00, 0xF8, // +++++...
    // walk0 r bot
    0x06, 0xF8, // +++++--.
    0x06, 0xF8, // +++++--.
    0x10, 0xF8, // +++#+...
    0x00, 0xF8, // +++++...
    0x00, 0x78, // .++++...
    0x00, 0x38, // ..+++...
    0x3C, 0x3C, // ..####..
    0x3C, 0x3C, // ..####..
    // walk1 l top
    0x00, 0x0F, // ....++++
    0x00, 0x1F, // ...+++++
    0x20, 0x3F, // ..#+++++
    0x3F, 0x31, // ..##---#
    0x3F, 0x21, // ..#----#
    0x3F, 0x20, // ..#-----
    0x1F, 0x00, // ...-----
    0x00, 0x1F, // ...+++++
    // walk1 l bot
    0x00, 0x3F, // ..++++++
    0x60, 0x1F, // .--+++++
    0x68, 0x1F, // .--+#+++
    0x00, 0x1F, // ...+++++
    0x00, 0x1F, // ...+++++
    0x00, 0x38, // ..+++...
    0x78, 0x78, // .####...
    0x00, 0x00, // ........
    // walk1 r top
    0x00, 0xC0, // ++......
    0x00, 0xF8, // +++++...
    0x00, 0xFC, // ++++++..
    0xF8, 0x00, // -----...
    0xFC, 0x00, // ------..
    0xFC, 0xF8, // #####-..
    0xF0, 0x00, // ----....
    0x00, 0xF8, // +++++...
    // walk1 r bot
    0x00, 0xFC, // ++++++..
    0x06, 0xF8, // +++++--.
    0x16, 0xF8, // +++#+--.
    0x00, 0xF8, // +++++...
    0x00, 0xF0, // ++++....
    0x00, 0x78, // .++++...
    0x00, 0x7C, // .+++++..
    0x7C, 0x7C, // .#####..
    // walk2 l top
    0x00, 0x0F, // ....++++
    0x00, 0x1F, // ...+++++
    0x20, 0x3F, // ..#+++++
    0x3F, 0x31, // ..##---#
    0x3F, 0x21, // ..#----#
    0x3F, 0x20, // ..#-----
    0x1F, 0x00, // ...-----
    0x00, 0x1F, // ...+++++
    // walk2 l bot
    0x60, 0x1F, // .--+++++
    0x60, 0x1F, // .--+++++
    0x08, 0x1F, // ...+#+++
    0x00, 0x1F, // ...+++++
    0x00, 0x1E, // ...++++.
    0x00, 0x1C, // ...+++..
    0x3C, 0x3C, // ..####..
    0x3C, 0x3C, // ..####..
    // walk2 r top
    0x00, 0xC0, // ++......
    0x00, 0xF8, // +++++...
    0x00, 0xFC, // ++++++..
    0xF8, 0x00, // -----...
    0xFC, 0x00, // ------..
    0xFC, 0xF8, // #####-..
    0xF0, 0x00, // ----....
    0x00, 0xF8, // +++++...
    // walk2 r bot
    0x00, 0xFC, // ++++++..
    0x00, 0xFC, // ++++++..
    0x16, 0xF8, // +++#+--.
    0x06, 0xF8, // +++++--.
    0x00, 0x7C, // .+++++..
    0x00, 0x3C, // ..++++..
    0x1E, 0x1E, // ...####.
    0x1E, 0x1E, // ...####.
    // skid l top
    0x00, 0x1F, // ...+++++
    0x00, 0x3F, // ..++++++
    0x40, 0x7F, // .#++++++
    0x7F, 0x62, // .##---#-
    0x7F, 0x42, // .#----#-
    0x7F, 0x41, // .#-----#
    0x3F, 0x00, // ..------
    0x00, 0x3F, // ..++++++
    // skid l bot
    0x60, 0x1F, // .--+++++
    0x08, 0x1F, // ...+#+++
    0x00, 0x1F, // ...+++++
    0x00, 0x1F, // ...+++++
    0x00, 0x1C, // ...+++..
    0x00, 0x3C, // ..++++..
    0x7C, 0x7C, // .#####..
    0x7C, 0x7C, // .#####..
    // skid r top
    0x00, 0x80, // +.......
    0x00, 0xF0, // ++++....
    0x00, 0xF8, // +++++...
    0xF0, 0x00, // ----....
    0xF8, 0x00, // -----...
    0xF8, 0xF0, // ####-...
    0xE0, 0x00, // ---.....
    0x06, 0xF8, // +++++--.
    // skid r bot
    0x00, 0xF8, // +++++...
    0x10, 0xF8, // +++#+...
    0x00, 0xF8, // +++++...
    0x00, 0xF8, // +++++...
    0x00, 0x78, // .++++...
    0x00, 0x7C, // .+++++..
    0x3E, 0x3E, // ..#####.
    0x3E, 0x3E, // ..#####.
    // jump l top
    0x00, 0x1F, // ...+++++
    0x00, 0x3F, // ..++++++
    0x40, 0x7F, // .#++++++
    0x7F, 0x62, // .##---#-
    0x7F, 0x42, // .#----#-
    0x7F, 0x41, // .#-----#
    0x3F, 0x00, // ..------
    0x00, 0x3F, // ..++++++
    // jump l bot
    0x60, 0x1F, // .--+++++
    0x68, 0x1F, // .--+#+++
    0x00, 0x1F, // ...+++++
    0x00, 0x1F, // ...+++++
    0x00, 0x3E, // ..+++++.
    0x00, 0x78, // .++++...
    0x78, 0x78, // .####...
    0x78, 0x78, // .####...
    // jump r top
    0x00, 0x80, // +.......
    0x00, 0xF0, // ++++....
    0x00, 0xF8, // +++++...
    0xF6, 0x00, // ----.--.
    0xFC, 0x00, // ------..
    0xF8, 0xF6, // ####-++.
    0xE0, 0x06, // ---..++.
    0x00, 0xFE, // +++++++.
    // jump r bot
    0x00, 0xFC, // ++++++..
    0x10, 0xF8, // +++#+...
    0x00, 0xF8, // +++++...
    0x00, 0xF8, // +++++...
    0x00, 0x3C, // ..++++..
    0x3C, 0x3C, // ..####..
    0x00, 0x00, // ........
    0x00, 0x00, // ........
};
// clang-format on

// the enemies. colors: 1 the light detail (goomba face, koopa skin and shell rim), 2 the body,
// 3 the black outline, brows and eyes; 0 is sprite transparency. the four symmetric frames come
// first, one 8x16 pair each - only their left half is stored and the right half is that same tile
// drawn flipped - then the koopa's two facing frames in mario's left-top/left-bottom/right-top/
// right-bottom order. roster.json animates the goomba by alternating states, which these two
// frames do; being mirrored halves they cannot put one foot in front of the other, so the walk
// alternates the feet wide and tucked instead, which is the same read at 8px
// clang-format off
static const uint8_t kEnemyTiles[256] = {
    // goomba walk0 top
    0x03, 0x03, // ......##
    0x0F, 0x0F, // ....####
    0x18, 0x1F, // ...##+++
    0x30, 0x3F, // ..##++++
    0x20, 0x3F, // ..#+++++
    0x60, 0x7F, // .##+++++
    0x7C, 0x7F, // .#####++
    0x7E, 0x5F, // .#-####+
    // goomba walk0 bottom
    0x7C, 0x4F, // .#--##++
    0x7C, 0x4F, // .#--##++
    0x78, 0x47, // .#---+++
    0x40, 0x7F, // .#++++++
    0x80, 0xFF, // #+++++++
    0x80, 0xFF, // #+++++++
    0xF0, 0xFF, // ####++++
    0xF0, 0xF0, // ####....
    // goomba walk1 top
    0x03, 0x03, // ......##
    0x0F, 0x0F, // ....####
    0x18, 0x1F, // ...##+++
    0x30, 0x3F, // ..##++++
    0x20, 0x3F, // ..#+++++
    0x60, 0x7F, // .##+++++
    0x7C, 0x7F, // .#####++
    0x7E, 0x5F, // .#-####+
    // goomba walk1 bottom
    0x7C, 0x4F, // .#--##++
    0x7C, 0x4F, // .#--##++
    0x78, 0x47, // .#---+++
    0x40, 0x7F, // .#++++++
    0x80, 0xFF, // #+++++++
    0x80, 0xFF, // #+++++++
    0x3C, 0x3F, // ..####++
    0x3C, 0x3C, // ..####..
    // goomba squash top
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    // goomba squash bottom
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x0F, 0x0F, // ....####
    0x30, 0x3F, // ..##++++
    0x70, 0x4F, // .#--++++
    0x80, 0xFF, // #+++++++
    0x80, 0xFF, // #+++++++
    0xF0, 0xF0, // ####....
    // koopa shell top
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x07, 0x07, // .....###
    0x18, 0x1F, // ...##+++
    0x20, 0x3F, // ..#+++++
    0x40, 0x7F, // .#++++++
    0x4C, 0x7F, // .#++##++
    0x98, 0xFF, // #++##+++
    // koopa shell bottom
    0xB0, 0xFF, // #+##++++
    0xA0, 0xFF, // #+#+++++
    0x98, 0xFF, // #++##+++
    0x86, 0xFF, // #++++##+
    0x80, 0xFF, // #+++++++
    0x7F, 0x7F, // .#######
    0xFF, 0x00, // --------
    0x7F, 0x7F, // .#######
    // koopa walk0 l top
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x01, 0x01, // .......#
    0x01, 0x01, // .......#
    0x01, 0x01, // .......#
    0x39, 0x39, // ..###..#
    0x62, 0x7E, // .##+++#.
    0x41, 0x7F, // .#+++++#
    // koopa walk0 l bot
    0x80, 0xFF, // #+++++++
    0x98, 0xFF, // #++##+++
    0xA4, 0xFF, // #+#++#++
    0x80, 0xFF, // #+++++++
    0x80, 0xFF, // #+++++++
    0x7F, 0x7F, // .#######
    0x38, 0x00, // ..---...
    0x78, 0x00, // .----...
    // koopa walk0 r top
    0x78, 0x78, // .####...
    0xFC, 0x84, // #----#..
    0xFE, 0x32, // --##--#.
    0xFF, 0x31, // --##---#
    0xFF, 0x01, // -------#
    0xFE, 0x02, // ------#.
    0xFE, 0x82, // #-----#.
    0xFC, 0x84, // #----#..
    // koopa walk0 r bot
    0x7C, 0xC4, // +#---#..
    0x3C, 0xE4, // ++#--#..
    0x18, 0xF8, // +++##...
    0x10, 0xF0, // +++#....
    0x10, 0xF0, // +++#....
    0xE0, 0xE0, // ###.....
    0xE0, 0x00, // ---.....
    0xF0, 0x00, // ----....
    // koopa walk1 l top
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x01, 0x01, // .......#
    0x01, 0x01, // .......#
    0x01, 0x01, // .......#
    0x39, 0x39, // ..###..#
    0x62, 0x7E, // .##+++#.
    0x41, 0x7F, // .#+++++#
    // koopa walk1 l bot
    0x80, 0xFF, // #+++++++
    0x98, 0xFF, // #++##+++
    0xA4, 0xFF, // #+#++#++
    0x80, 0xFF, // #+++++++
    0x80, 0xFF, // #+++++++
    0x7F, 0x7F, // .#######
    0x18, 0x00, // ...--...
    0x3D, 0x00, // ..----.-
    // koopa walk1 r top
    0x78, 0x78, // .####...
    0xFC, 0x84, // #----#..
    0xFE, 0x32, // --##--#.
    0xFF, 0x31, // --##---#
    0xFF, 0x01, // -------#
    0xFE, 0x02, // ------#.
    0xFE, 0x82, // #-----#.
    0xFC, 0x84, // #----#..
    // koopa walk1 r bot
    0x7C, 0xC4, // +#---#..
    0x3C, 0xE4, // ++#--#..
    0x18, 0xF8, // +++##...
    0x10, 0xF0, // +++#....
    0x10, 0xF0, // +++#....
    0xE0, 0xE0, // ###.....
    0xC0, 0x00, // --......
    0xF0, 0x00, // ----....
};
// clang-format on

// super mario. the shared upper slab comes first, then one lower slab per pose; the crouch's own
// upper eight rows are blank because the pose draws the shared slab eight px lower and lets it
// show through, which is how a 24 px body costs no extra tiles.
// the slab holds cap, face and chest down to the overall bib's top line, so every pose inherits
// the same torso and only the arms below the elbow, the legs and the shoes swing - which is what
// lets seven poses share it, crouch included
// clang-format off
static const uint8_t kSuperTiles[512] = {
    // shared upper l top
    0x00, 0x0F, // ....++++
    0x00, 0x1F, // ...+++++
    0x00, 0x3F, // ..++++++
    0x20, 0x3F, // ..#+++++
    0x3F, 0x31, // ..##---#
    0x3F, 0x21, // ..#----#
    0x3F, 0x20, // ..#-----
    0x1F, 0x00, // ...-----
    // shared upper l bot
    0x0F, 0x00, // ....----
    0x00, 0x1F, // ...+++++
    0x00, 0x3F, // ..++++++
    0x00, 0x3F, // ..++++++
    0x00, 0x3F, // ..++++++
    0x08, 0x3F, // ..++#+++
    0x00, 0x3F, // ..++++++
    0x00, 0x3F, // ..++++++
    // shared upper r top
    0x00, 0xC0, // ++......
    0x00, 0xF8, // +++++...
    0x00, 0xFC, // ++++++..
    0x04, 0xFC, // +++++#..
    0xF8, 0x00, // -----...
    0xFC, 0x00, // ------..
    0xFC, 0xF8, // #####-..
    0xF0, 0x00, // ----....
    // shared upper r bot
    0xE0, 0x00, // ---.....
    0x00, 0xF8, // +++++...
    0x00, 0xFC, // ++++++..
    0x00, 0xFC, // ++++++..
    0x00, 0xFC, // ++++++..
    0x10, 0xFC, // +++#++..
    0x00, 0xFC, // ++++++..
    0x00, 0xFC, // ++++++..
    // idle lower l top
    0x60, 0x1F, // .--+++++
    0x60, 0x1F, // .--+++++
    0x00, 0x3F, // ..++++++
    0x00, 0x3F, // ..++++++
    0x00, 0x1F, // ...+++++
    0x00, 0x1F, // ...+++++
    0x00, 0x1F, // ...+++++
    0x00, 0x1F, // ...+++++
    // idle lower l bot
    0x00, 0x1E, // ...++++.
    0x00, 0x1E, // ...++++.
    0x00, 0x1E, // ...++++.
    0x00, 0x1E, // ...++++.
    0x00, 0x1E, // ...++++.
    0x3E, 0x3E, // ..#####.
    0x3E, 0x3E, // ..#####.
    0x7E, 0x7E, // .######.
    // idle lower r top
    0x06, 0xF8, // +++++--.
    0x06, 0xF8, // +++++--.
    0x00, 0xFC, // ++++++..
    0x00, 0xFC, // ++++++..
    0x00, 0xF8, // +++++...
    0x00, 0xF8, // +++++...
    0x00, 0xF8, // +++++...
    0x00, 0xF8, // +++++...
    // idle lower r bot
    0x00, 0x78, // .++++...
    0x00, 0x78, // .++++...
    0x00, 0x78, // .++++...
    0x00, 0x78, // .++++...
    0x00, 0x78, // .++++...
    0x7C, 0x7C, // .#####..
    0x7C, 0x7C, // .#####..
    0x7E, 0x7E, // .######.
    // walk0 lower l top
    0x00, 0x3F, // ..++++++
    0x00, 0x3F, // ..++++++
    0x60, 0x1F, // .--+++++
    0x60, 0x1F, // .--+++++
    0x00, 0x1F, // ...+++++
    0x00, 0x1F, // ...+++++
    0x00, 0x1F, // ...+++++
    0x00, 0x3C, // ..++++..
    // walk0 lower l bot
    0x00, 0x3C, // ..++++..
    0x00, 0x38, // ..+++...
    0x00, 0x78, // .++++...
    0x00, 0x78, // .++++...
    0x00, 0x78, // .++++...
    0x78, 0x78, // .####...
    0x78, 0x78, // .####...
    0x78, 0x78, // .####...
    // walk0 lower r top
    0x06, 0xF8, // +++++--.
    0x06, 0xF8, // +++++--.
    0x00, 0xF8, // +++++...
    0x00, 0xF8, // +++++...
    0x00, 0xF8, // +++++...
    0x00, 0xF8, // +++++...
    0x00, 0xF8, // +++++...
    0x00, 0x7C, // .+++++..
    // walk0 lower r bot
    0x00, 0x7C, // .+++++..
    0x00, 0x3C, // ..++++..
    0x00, 0x3C, // ..++++..
    0x00, 0x3E, // ..+++++.
    0x00, 0x3E, // ..+++++.
    0x3E, 0x3E, // ..#####.
    0x3E, 0x3E, // ..#####.
    0x3E, 0x3E, // ..#####.
    // walk1 lower l top
    0x60, 0x1F, // .--+++++
    0x60, 0x1F, // .--+++++
    0x00, 0x3F, // ..++++++
    0x00, 0x3F, // ..++++++
    0x00, 0x1F, // ...+++++
    0x00, 0x1F, // ...+++++
    0x00, 0x1F, // ...+++++
    0x00, 0x1F, // ...+++++
    // walk1 lower l bot
    0x00, 0x0F, // ....++++
    0x00, 0x0F, // ....++++
    0x00, 0x0F, // ....++++
    0x00, 0x0F, // ....++++
    0x00, 0x0F, // ....++++
    0x1F, 0x1F, // ...#####
    0x1F, 0x1F, // ...#####
    0x3F, 0x3F, // ..######
    // walk1 lower r top
    0x06, 0xF8, // +++++--.
    0x06, 0xF8, // +++++--.
    0x00, 0xFC, // ++++++..
    0x00, 0xFC, // ++++++..
    0x00, 0xF8, // +++++...
    0x00, 0xF8, // +++++...
    0x00, 0xF8, // +++++...
    0x00, 0xF8, // +++++...
    // walk1 lower r bot
    0x00, 0xF0, // ++++....
    0x00, 0xF0, // ++++....
    0x00, 0xF0, // ++++....
    0x00, 0xF0, // ++++....
    0x00, 0xF0, // ++++....
    0xF8, 0xF8, // #####...
    0xF8, 0xF8, // #####...
    0xFC, 0xFC, // ######..
    // walk2 lower l top
    0x60, 0x1F, // .--+++++
    0x60, 0x1F, // .--+++++
    0x00, 0x1F, // ...+++++
    0x00, 0x1F, // ...+++++
    0x00, 0x1F, // ...+++++
    0x00, 0x1F, // ...+++++
    0x00, 0x1F, // ...+++++
    0x00, 0x0E, // ....+++.
    // walk2 lower l bot
    0x00, 0x1E, // ...++++.
    0x00, 0x3E, // ..+++++.
    0x3C, 0x3C, // ..####..
    0x3C, 0x3C, // ..####..
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x01, 0x01, // .......#
    // walk2 lower r top
    0x00, 0xFC, // ++++++..
    0x00, 0xFC, // ++++++..
    0x06, 0xF8, // +++++--.
    0x06, 0xF8, // +++++--.
    0x00, 0xF8, // +++++...
    0x00, 0xF8, // +++++...
    0x00, 0xF8, // +++++...
    0x00, 0x78, // .++++...
    // walk2 lower r bot
    0x00, 0x78, // .++++...
    0x00, 0x78, // .++++...
    0x00, 0x78, // .++++...
    0x00, 0x7C, // .+++++..
    0x00, 0x7C, // .+++++..
    0x7E, 0x7E, // .######.
    0xFE, 0xFE, // #######.
    0xFE, 0xFE, // #######.
    // skid lower l top
    0x00, 0x3F, // ..++++++
    0x00, 0x3F, // ..++++++
    0x60, 0x1F, // .--+++++
    0x60, 0x1F, // .--+++++
    0x00, 0x1F, // ...+++++
    0x00, 0x1F, // ...+++++
    0x00, 0x3E, // ..+++++.
    0x00, 0x3C, // ..++++..
    // skid lower l bot
    0x00, 0x78, // .++++...
    0x00, 0x78, // .++++...
    0x00, 0x78, // .++++...
    0x00, 0x78, // .++++...
    0x00, 0x78, // .++++...
    0x78, 0x78, // .####...
    0x78, 0x78, // .####...
    0x78, 0x78, // .####...
    // skid lower r top
    0x06, 0xF8, // +++++--.
    0x06, 0xF8, // +++++--.
    0x00, 0xF8, // +++++...
    0x00, 0xF8, // +++++...
    0x00, 0xF8, // +++++...
    0x00, 0xF8, // +++++...
    0x00, 0x7C, // .+++++..
    0x00, 0x3C, // ..++++..
    // skid lower r bot
    0x00, 0x3C, // ..++++..
    0x00, 0x1E, // ...++++.
    0x00, 0x1E, // ...++++.
    0x00, 0x1E, // ...++++.
    0x00, 0x1E, // ...++++.
    0x1E, 0x1E, // ...####.
    0x1E, 0x1E, // ...####.
    0x1E, 0x1E, // ...####.
    // jump lower l top
    0x00, 0x3F, // ..++++++
    0x00, 0x3F, // ..++++++
    0x60, 0x1F, // .--+++++
    0x60, 0x1F, // .--+++++
    0x00, 0x1F, // ...+++++
    0x00, 0x1F, // ...+++++
    0x00, 0x3E, // ..+++++.
    0x00, 0x3E, // ..+++++.
    // jump lower l bot
    0x7C, 0x7C, // .#####..
    0x7C, 0x7C, // .#####..
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    // jump lower r top
    0x00, 0xF8, // +++++...
    0x00, 0xF8, // +++++...
    0x00, 0xF8, // +++++...
    0x00, 0xF8, // +++++...
    0x00, 0xF8, // +++++...
    0x00, 0xF8, // +++++...
    0x00, 0x7C, // .+++++..
    0x00, 0x7C, // .+++++..
    // jump lower r bot
    0x00, 0x7C, // .+++++..
    0x00, 0x7C, // .+++++..
    0x00, 0x7C, // .+++++..
    0x00, 0x7C, // .+++++..
    0x00, 0x7E, // .++++++.
    0x7E, 0x7E, // .######.
    0x7E, 0x7E, // .######.
    0xFE, 0xFE, // #######.
    // crouch lower l top
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    // crouch lower l bot
    0x00, 0x3F, // ..++++++
    0x60, 0x1F, // .--+++++
    0x60, 0x1F, // .--+++++
    0x00, 0x3F, // ..++++++
    0x00, 0x3F, // ..++++++
    0x78, 0x78, // .####...
    0x78, 0x78, // .####...
    0x7E, 0x7E, // .######.
    // crouch lower r top
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    0x00, 0x00, // ........
    // crouch lower r bot
    0x00, 0xFC, // ++++++..
    0x06, 0xF8, // +++++--.
    0x06, 0xF8, // +++++--.
    0x00, 0xFC, // ++++++..
    0x00, 0xFC, // ++++++..
    0x1E, 0x1E, // ...####.
    0x1E, 0x1E, // ...####.
    0x7E, 0x7E, // .######.
};
// clang-format on

// the fire flower: white petals, a yellow heart and a dark stem, which is what the star's palette
// paints once it is borrowed for the item
// clang-format off
static const uint8_t kFlowerTiles[64] = {
    0x18, 0x18, 0x3C, 0x24, 0x3E, 0x22, 0x1F, 0x11, 0x0C, 0x0B, 0x18, 0x17, 0x18, 0x17, 0x0C, 0x0B, 0x07,
    0x06, 0x01, 0x01, 0x0F, 0x0F, 0x19, 0x19, 0x19, 0x19, 0x0F, 0x0F, 0x01, 0x01, 0x01, 0x01, 0x18, 0x18,
    0x3C, 0x24, 0x7C, 0x44, 0xF8, 0x88, 0x30, 0xD0, 0x18, 0xE8, 0x18, 0xE8, 0x30, 0xD0, 0xE0, 0x60, 0x80,
    0x80, 0xC0, 0xC0, 0xB0, 0xB0, 0xB0, 0xB0, 0xC0, 0xC0, 0x80, 0x80, 0x80, 0x80, // flower l/r
};
// clang-format on

// the three poses vram bank 0 has no tile ids left for, so they live in CGB VRAM BANK 1 and are
// drawn with S_BANK set in the sprite's own prop - the same trick the fireball's second spin frame
// already uses (see kFireballFrameBTiles and powerup_draw). each keeps an id inside the family it
// belongs to, so the tile number alone still names the sprite: the jump slab inside super mario's
// 0x60-0x7f run, the two climb poses inside small mario's own 0xe0-0xf7 run
//
// super mario's jump upper slab. every other pose of his shares one upper slab, which is why his
// jump could not raise an arm: this is that slab redrawn with the head leaning back and the front
// arm thrown up beside the cap, and player_draw swaps to it on kFrameJump alone
// clang-format off
static const uint8_t kSuperJumpUpperTiles[64] = {
    // jump upper l top
    0x00, 0x1F, // ...+++++
    0x00, 0x3F, // ..++++++
    0x00, 0x7F, // .+++++++
    0x40, 0x7F, // .#++++++
    0x7F, 0x62, // .##---#-
    0x7F, 0x42, // .#----#-
    0x7F, 0x41, // .#-----#
    0x3F, 0x00, // ..------
    // jump upper l bot
    0x1F, 0x00, // ...-----
    0x00, 0x3F, // ..++++++
    0x00, 0x7F, // .+++++++
    0x00, 0x7F, // .+++++++
    0x00, 0x7F, // .+++++++
    0x10, 0x7F, // .++#++++
    0x00, 0x7F, // .+++++++
    0x00, 0x7F, // .+++++++
    // jump upper r top
    0x00, 0x80, // +.......
    0x06, 0xF0, // ++++.--.
    0x06, 0xF8, // +++++--.
    0x0E, 0xF8, // ++++#--.
    0xF0, 0x06, // ----.++.
    0xF8, 0x06, // -----++.
    0xF8, 0xF6, // ####-++.
    0xE0, 0x06, // ---..++.
    // jump upper r bot
    0xC0, 0x06, // --...++.
    0x00, 0xFE, // +++++++.
    0x00, 0xF8, // +++++...
    0x00, 0xF8, // +++++...
    0x00, 0xF8, // +++++...
    0x20, 0xF8, // ++#++...
    0x00, 0xF8, // +++++...
    0x00, 0xF8, // +++++...
};
// clang-format on

// small mario's flagpole climb: both hands on the pole at his right, face turned to it. one 16x16
// pose, held for the whole slide and the flip to the pole's far side
// clang-format off
static const uint8_t kClimbSmallTiles[64] = {
    // climb l top
    0x00, 0x0F, // ....++++
    0x00, 0x1F, // ...+++++
    0x20, 0x3F, // ..#+++++
    0x3F, 0x31, // ..##---#
    0x3F, 0x21, // ..#----#
    0x3F, 0x20, // ..#-----
    0x1F, 0x00, // ...-----
    0x00, 0x1F, // ...+++++
    // climb l bot
    0x00, 0x3F, // ..++++++
    0x08, 0x3F, // ..++#+++
    0x00, 0x1F, // ...+++++
    0x00, 0x1F, // ...+++++
    0x00, 0x1F, // ...+++++
    0x00, 0x1F, // ...+++++
    0x7C, 0x7C, // .#####..
    0x7C, 0x7C, // .#####..
    // climb r top
    0x00, 0xC0, // ++......
    0x00, 0xF8, // +++++...
    0x00, 0xFC, // ++++++..
    0xF8, 0x00, // -----...
    0xFC, 0x00, // ------..
    0xFC, 0xF8, // #####-..
    0xF0, 0x00, // ----....
    0x06, 0xF0, // ++++.--.
    // climb r bot
    0x06, 0xF8, // +++++--.
    0x20, 0xF8, // ++#++...
    0x06, 0xF0, // ++++.--.
    0x06, 0xF8, // +++++--.
    0x00, 0xF8, // +++++...
    0x00, 0xF8, // +++++...
    0x7C, 0x7C, // .#####..
    0x7C, 0x7C, // .#####..
};
// clang-format on

// and super mario's, the same grip on a 16x32 body: its own upper slab (the shared one has both
// arms down) over a lower slab with the second hand out and the legs held together
// clang-format off
static const uint8_t kClimbBigTiles[128] = {
    // climb upper l top
    0x00, 0x1F, // ...+++++
    0x00, 0x3F, // ..++++++
    0x00, 0x7F, // .+++++++
    0x40, 0x7F, // .#++++++
    0x7F, 0x62, // .##---#-
    0x7F, 0x42, // .#----#-
    0x7F, 0x41, // .#-----#
    0x3F, 0x00, // ..------
    // climb upper l bot
    0x1F, 0x00, // ...-----
    0x00, 0x3F, // ..++++++
    0x00, 0x7F, // .+++++++
    0x00, 0x7F, // .+++++++
    0x00, 0x7F, // .+++++++
    0x10, 0x7F, // .++#++++
    0x00, 0x7F, // .+++++++
    0x00, 0x7F, // .+++++++
    // climb upper r top
    0x00, 0x80, // +.......
    0x00, 0xF0, // ++++....
    0x00, 0xF8, // +++++...
    0x08, 0xF8, // ++++#...
    0xF0, 0x00, // ----....
    0xF8, 0x00, // -----...
    0xF8, 0xF0, // ####-...
    0xE0, 0x00, // ---.....
    // climb upper r bot
    0xC0, 0x00, // --......
    0x06, 0xF0, // ++++.--.
    0x06, 0xF8, // +++++--.
    0x00, 0xF8, // +++++...
    0x00, 0xF8, // +++++...
    0x20, 0xF8, // ++#++...
    0x00, 0xF8, // +++++...
    0x00, 0xF8, // +++++...
    // climb lower l top
    0x00, 0x3F, // ..++++++
    0x00, 0x3F, // ..++++++
    0x00, 0x1F, // ...+++++
    0x00, 0x1F, // ...+++++
    0x00, 0x1F, // ...+++++
    0x00, 0x1F, // ...+++++
    0x00, 0x1F, // ...+++++
    0x00, 0x1F, // ...+++++
    // climb lower l bot
    0x00, 0x1F, // ...+++++
    0x00, 0x1F, // ...+++++
    0x00, 0x1F, // ...+++++
    0x00, 0x1F, // ...+++++
    0x00, 0x1F, // ...+++++
    0x00, 0x1F, // ...+++++
    0x7C, 0x7C, // .#####..
    0x7C, 0x7C, // .#####..
    // climb lower r top
    0x06, 0xF8, // +++++--.
    0x06, 0xF8, // +++++--.
    0x00, 0xF8, // +++++...
    0x00, 0xF8, // +++++...
    0x00, 0xF8, // +++++...
    0x00, 0xF8, // +++++...
    0x00, 0xF8, // +++++...
    0x00, 0xF8, // +++++...
    // climb lower r bot
    0x00, 0xF8, // +++++...
    0x00, 0xF8, // +++++...
    0x00, 0xF8, // +++++...
    0x00, 0xF8, // +++++...
    0x00, 0xF8, // +++++...
    0x00, 0xF8, // +++++...
    0x7C, 0x7C, // .#####..
    0x7C, 0x7C, // .#####..
};
// clang-format on

void assets_load_sprite_tiles(void) BANKED {
    set_sprite_data(kTileMarioFirst, kMarioTileCount, kMarioTiles);
    // both bodies stay resident, so growing and shrinking never touch vram
    set_sprite_data(kTileSuperFirst, kSuperTileCount, kSuperTiles);
    // the jump slab and the two climb poses ride in vram bank 1: bank 0's tile table is full end
    // to end, and a sprite picks its bank from S_BANK in its own prop (see player_draw)
    VBK_REG = VBK_BANK_1;
    set_sprite_data(kTileSuperJumpUpper, 4, kSuperJumpUpperTiles);
    set_sprite_data(kTileClimbSmall, 4, kClimbSmallTiles);
    set_sprite_data(kTileClimbBigUpper, 8, kClimbBigTiles);
    VBK_REG = VBK_BANK_0;
}

void assets_load_item_tiles(void) BANKED {
    set_sprite_data(kTileItemFirst, kItemTileCount, kItemTiles);
    set_sprite_data(kTileFlowerFirst, kFlowerTileCount, kFlowerTiles);
    // the fireball's second spin frame rides in bank 1 at the same id as the first, since bank 0's
    // tile table has no room left; see the comment on kFireballFrameBTiles
    VBK_REG = VBK_BANK_1;
    set_sprite_data(kTileFireball, 2, kFireballFrameBTiles);
    VBK_REG = VBK_BANK_0;
}

// m8a's four actors, one 8x16 pair each: a piranha head over its stem, a firebar flame in the top
// half of its pair, a lift plank in the top half of its own, and the fake bowser
//
// the piranha is one mirrored 8x16 column (left_tile drawn normal, the same bitmap redrawn
// S_FLIPX for the right half, see enemies.c's draw loop), so column 7 below is the sprite's own
// centreline: it sits adjacent to its own mirror image, not the outer edge. the jaws start wide
// apart at the top (col7 empty = an open gap down the middle), a tooth block ('-') rides the inner
// edge where the gap is still open, then the gap closes into a solid taper that narrows to a thin
// stem and flares back out into two leaf tips at the base, right above the pipe cap
// clang-format off
static const uint8_t kHazardTiles[128] = {
    // piranha top - two scalloped jaw lobes opening around a gap that narrows going down
    0x20, 0x30, // ..#+....
    0x40, 0x78, // .#+++...
    0x80, 0xF8, // #++++...
    0x40, 0x7E, // .#+++++.
    0x80, 0xFC, // #+++++..
    0x44, 0x78, // .#+++-..
    0x82, 0xFC, // #+++++-.
    0x44, 0x78, // .#+++-..
    // piranha bottom - teeth flank the gap as it closes, then the jaws taper to a stem and leaves
    0x82, 0xFD, // #+++++-+
    0x40, 0x7F, // .#++++++
    0x20, 0x3F, // ..#+++++
    0x10, 0x1F, // ...#++++
    0x08, 0x0F, // ....#+++
    0x04, 0x07, // .....#++
    0x42, 0x63, // .#+...#+
    0x85, 0xF7, // #+++.#+#
    0x18, 0x18, 0x3C, 0x24, 0x7E, 0x5A, 0x7E, 0x42,
    0x7E, 0x42, 0x3C, 0x24, 0x18, 0x18, 0x00, 0x00, // flame, upper half of the pair
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // flame lower half, blank
    0xFF, 0xFF, 0xFF, 0x00, 0xDB, 0xFF, 0xFF, 0x00,
    0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // lift deck, upper half of the pair
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // lift lower half, blank
    0x00, 0x00, 0x07, 0x07, 0x1F, 0x18, 0x3F, 0x20,
    0x37, 0x28, 0x3F, 0x20, 0x7F, 0x40, 0x7F, 0x40, // fake bowser top
    0x7F, 0x58, 0x7F, 0x40, 0x3F, 0x20, 0x3F, 0x30,
    0x1F, 0x1F, 0x3B, 0x38, 0x33, 0x30, 0x00, 0x00, // fake bowser bottom
};
// clang-format on

void assets_load_enemy_tiles(void) BANKED {
    set_sprite_data(kTileEnemyFirst, kEnemyTileCount, kEnemyTiles);
}

void assets_load_hazard_tiles(void) BANKED {
    set_sprite_data(kTileHazardFirst, kHazardTileCount, kHazardTiles);
}

// the bar's coin: a gold (color 1) oval with a white (color 2) slot down the middle, on the same
// color 3 cell the glyphs sit on. gold is the low plane alone and white the high plane alone, so
// the rows read lo = everything but the slot, hi = everything but the body
// clang-format off
static const uint8_t kHudCoinTile[16] = {
    0xFF, 0xFF, // ........
    0xFF, 0xC3, // ..oooo..
    0xE7, 0x99, // .oo##oo.
    0xEF, 0x91, // .oo#ooo.
    0xEF, 0x91, // .oo#ooo.
    0xE7, 0x99, // .oo##oo.
    0xFF, 0xC3, // ..oooo..
    0xFF, 0xFF, // ........
};
// clang-format on

// the hud bar's own copy of the ibm font, in vram bank 1. the resident glyphs at 0x00-0x5f draw
// their ink on color 3 over color 0, which no level palette set can turn into white-on-black
// (color 3 is the art's black outline in all eight slots and color 0 of the sky slot is the sky
// itself). so each glyph is read back out of bank 0 and re-encoded: ink on color 2, cell on color
// 3, which under kCamPalQuestion is white on black in every set. a 2bpp row is (lo, hi), so a cell
// pixel is both planes set and an ink pixel is the hi plane only - and taking the ink mask as
// lo|hi keeps this right whatever shade the font itself was expanded with. the space glyph carries
// no ink at all, which is what makes a blank cell of the bar solid black
static void hud_glyph(uint8_t id, char c) {
    // gbdk leaves lcdc bit 4 clear, so a bg tile id is signed indexing from 0x9000 (pan docs,
    // "bg and window tile data"): the font's own 0x00-0x5f sit at 0x9000 up, which is the half of
    // vram the sprites at 0x8000-0x8fff cannot reach and the reason both fit at all
    const uint8_t* src = (const uint8_t*)(0x9000U + ((uint16_t)((uint8_t)c - kFontFirstChar) << 4));
    uint8_t glyph[16];
    uint8_t r;

    VBK_REG = VBK_BANK_0;
    for (r = 0; r < 16U; r = (uint8_t)(r + 2U)) {
        const uint8_t ink = (uint8_t)(src[r] | src[r + 1U]);

        glyph[r] = (uint8_t)~ink;
        glyph[r + 1U] = 0xFFU;
    }
    VBK_REG = VBK_BANK_1;
    set_bkg_data(id, 1, glyph);
}

void assets_load_hud_font(void) BANKED {
    // the same list hud.c maps a character to an id with, expanded here so the copy order and the
    // ids cannot drift apart
    static const char kLetters[] = kHudGlyphChars;
    uint8_t i;

    for (i = 0; i < 10U; ++i) {
        hud_glyph((uint8_t)(kTileHudDigitFirst + i), (char)('0' + i));
    }
    hud_glyph(kTileHudBlank, ' ');
    for (i = 0; kLetters[i] != '\0'; ++i) {
        hud_glyph((uint8_t)(kTileHudLetterFirst + i), kLetters[i]);
    }
    VBK_REG = VBK_BANK_1;
    set_bkg_data(kTileHudCoin, 1, kHudCoinTile);
    VBK_REG = VBK_BANK_0;
}

void assets_load_enemy_palettes_castle(void) BANKED {
    // no koopa and no piranha stands in a castle, so the koopa slot is re-tinted for the fake
    // bowser: roster.json calls the nes original grayish-blue with yellow hair. m9's real bowser
    // gets art of his own
    palette_color_t goomba[4] = {RGB(0, 0, 0), RGB(31, 24, 19), RGB(19, 9, 0), RGB(0, 0, 0)};
    palette_color_t bowser[4] = {RGB(0, 0, 0), RGB(30, 28, 8), RGB(9, 14, 12), RGB(3, 5, 6)};
    set_sprite_palette(kPalGoomba, 1, goomba);
    set_sprite_palette(kPalKoopa, 1, bowser);
}
