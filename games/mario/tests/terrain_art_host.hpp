#ifndef TERRAIN_ART_HOST_HPP
#define TERRAIN_ART_HOST_HPP

// the generated terrain art (m23's 1-1 families, m25's 1-2 shaft joint and m26's 1-3 tree),
// compiled straight into the host test the way title_art_host.hpp
// and the sprite_art block in mario_test.cpp do it: the vram pin below then compares what the rom
// actually loaded against the very bytes it was built from, not a second transcription of them.
// the bank pragma the generator emits means nothing to a host compiler

#include <stdint.h>

namespace terrain_art {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#include "gen/axe.c"
#include "gen/brick.c"
#include "gen/brick_underground.c"
#include "gen/bridge.c"
#include "gen/bush.c"
#include "gen/bush_right.c"
#include "gen/castle.c"
#include "gen/castle_brick.c"
#include "gen/castle_crenel_inner.c"
#include "gen/castle_crenel_right.c"
#include "gen/castle_hard.c"
#include "gen/chain.c"
#include "gen/cloud.c"
#include "gen/cloud_right.c"
#include "gen/coin.c"
#include "gen/flag_ball.c"
#include "gen/flag_head.c"
#include "gen/flag_pole.c"
#include "gen/ground.c"
#include "gen/ground_lower.c"
#include "gen/hard.c"
#include "gen/hill.c"
#include "gen/lava.c"
#include "gen/pipe.c"
#include "gen/pipe_joint.c"
#include "gen/pipe_side.c"
#include "gen/question.c"
#include "gen/scen_tail.c"
#include "gen/spent.c"
#include "gen/tree.c"
#include "gen/trunk.c"
#pragma GCC diagnostic pop
} // namespace terrain_art

#endif
