// Reading the race out of fze.main's memory. See race.h, and docs/dev/vr.md for how each
// address was found.
#include "race.h"

#include <cmath>
#include <cstring>

#include "recomp.h"
#include "runtime.h"

namespace fzgx {
namespace {

constexpr uint32_t kMainModule = 1;   // fze.main's REL id
constexpr uint32_t kBss = 6;  // its .bss section

// fze.main .bss
constexpr uint32_t kStateOff = 0x3C28;       // RaceState
constexpr uint32_t kPausedOff = 0x25C44;     // 1 while paused (and on the retired screen)
constexpr uint32_t kPlayerRaceOff = 0x25E88; // 1 from the grid to the end of a player's race
constexpr uint32_t kMachineOff = 0x6D838;    // the player's machine (0x2C48C is set in a Grand Prix only)
constexpr uint32_t kCameraOff = 0x83C24;     // the race camera, in the top byte

// The machine (a heap block the race allocates)
constexpr uint32_t kMachineMatrix = 0x14C;   // 3x4, rows
constexpr uint32_t kMachineNormal = 0x1BC;   // the track normal under it

bool in_ram(uint32_t a, uint32_t size) { return a >= 0x80000000u && a + size <= 0x80000000u + RAM_SIZE && !(a & 3); }

float rd_f(uint32_t a) {
    const uint32_t u = mem_r32(a);
    float f;
    memcpy(&f, &u, 4);
    return f;
}

}  // namespace

Race race_read() {
    Race r;
    const uint32_t bss = rels_section_address(kMainModule, kBss);
    if (!bss) return r;
    r.linked = true;
    r.state = (int32_t)mem_r32(bss + kStateOff);
    r.paused = mem_r32(bss + kPausedOff) != 0;
    r.player_race = mem_r32(bss + kPlayerRaceOff) == 1;
    const uint32_t m = mem_r32(bss + kMachineOff);
    r.machine = in_ram(m, 0x400) ? m : 0;
    r.camera = (int)(mem_r32(bss + kCameraOff) >> 24);
    return r;
}

bool machine_pose(uint32_t machine, MachinePose* out) {
    if (!in_ram(machine, 0x400)) return false;
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 4; j++) out->m[i][j] = rd_f(machine + kMachineMatrix + (i * 4 + j) * 4);
    for (int i = 0; i < 3; i++) out->normal[i] = rd_f(machine + kMachineNormal + i * 4);
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 4; j++)
            if (!std::isfinite(out->m[i][j])) return false;
    const float n = out->normal[0] * out->normal[0] + out->normal[1] * out->normal[1] + out->normal[2] * out->normal[2];
    if (!std::isfinite(n) || fabsf(n - 1.0f) > 0.1f) {   // no track under it: the world's up
        out->normal[0] = 0.0f;
        out->normal[1] = 1.0f;
        out->normal[2] = 0.0f;
    }
    return true;
}

}  // namespace fzgx
