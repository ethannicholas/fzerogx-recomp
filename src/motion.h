// Motion telemetry for this game: the player's machine, sent to a motion platform through
// gcn-recomp's telemetry (runtime/telemetry.h). See motion.cpp.
#pragma once
#include "race.h"

namespace fzgx {

// Once per game frame, on the guest thread, with what race_read() just said.
void motion_frame(const Race& race);

}  // namespace fzgx
