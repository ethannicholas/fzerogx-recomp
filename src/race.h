// What this game's own memory says about the race, for the headset hooks (vr_fzerogx.cpp)
// and the motion telemetry (motion.cpp). See docs/dev/vr.md for how each value was found.
//
// Everything here is in fze.main (module 1), which the game links at an address that
// changes from run to run, so each read starts from where its section is now
// (rels_section_address).
#pragma once
#include <cstdint>

namespace fzgx {

// The race's phase, the word at fze.main .bss+0x3C28. Only the values seen so far are
// named; others exist. It is not reset when a race is quit, so it says nothing outside one:
// see Race::player_race.
enum RaceState : int32_t {
    kMachineSettings = 38,
    kIntro = 39,      // the course flyover
    kGrid = 40,       // on the grid, through the countdown
    kRacing = 41,
    kRetired = 43,    // 42 is presumably a finish; not yet seen
};

struct Race {
    bool linked = false;       // fze.main is linked: nothing else here means anything without it
    int32_t state = 0;
    bool player_race = false;  // a race the player drives: not the menus, not the title's demo
    bool paused = false;       // the pause menu, and the retired screen's
    uint32_t machine = 0;      // the player's machine, 0 if none
    int camera = -1;           // the race camera: 0 first person, 1 the default chase, 2-3 further back
};

Race race_read();

// In a race the player is driving, from the grid to the end, and not paused.
inline bool race_live(const Race& r) {
    return r.linked && r.player_race && !r.paused && r.machine && (r.state == kGrid || r.state == kRacing);
}

// The player's machine: its world matrix (rows of a GX 3x4: the columns are its right, up
// and back axes, the fourth its position) and the normal of the track under it, which is the
// machine's own "up" -- gravity in this game points into the track, in a pipe or on a wall.
// Game units, which are metres by the speedometer.
struct MachinePose {
    float m[3][4];
    float normal[3];
};
bool machine_pose(uint32_t machine, MachinePose* out);

}  // namespace fzgx
