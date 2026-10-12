// Motion telemetry: the player's machine, for a motion platform.
//
// Once per game frame (the guest frame hook in vr_fzerogx.cpp) the machine's world matrix
// is read (race.h). Velocity is differenced from its position, a frame being 1/60 s -- the
// game steps once a frame -- and eased over `velocity_s`, and acceleration differenced from
// that and eased over `accel_s` (telemetry.txt; 0.05 s and 0.15 s by default). Both are then
// put in the machine's own frame: surge along its nose, sway to its right, heave along the
// track's normal.
//
// The angles are the machine's against the track under it, not the world: gravity in this
// game points into the track, along a wall or round a pipe, and what a driver feels as level
// is the track. So pitch is the nose against the track's plane and roll the right side
// against it; on a flat road both are small, and come from the machine leaning into a turn
// or a slope's change. Yaw is the heading in the world, which the telemetry module washes
// out.
//
// Speed is the game's: the speedometer's km/h is a game unit a second times 3.6. That makes
// the accelerations a racing machine's: a boost start goes from 0 to 600 km/h in under a
// second, about 20 g. `accel_scale` (0.1) brings them to a platform's range before the shared
// module's own limits; telemetry.txt tunes it.
//
// Inactive -- the platform settles -- outside a live race (race_live: the grid to the end,
// unpaused). The first frame of a run only sets the position and the second the velocity,
// both sent as a discontinuity; so is a jump of more than kJump units in a frame.
#include "motion.h"

#include <cmath>

#include "telemetry.h"

namespace fzgx {
namespace {

constexpr float kDt = 1.0f / 60.0f;
constexpr float kJump = 60.0f;   // units in a frame: 13000 km/h

struct Track {
    int known = 0;   // 0 nothing, 1 the position, 2 the velocity too, 3 everything
    float pos[3];
    float vel[3];    // units/s, eased
    float acc[3];    // units/s^2, eased
};
Track g_track;

float dot(const float* a, const float* b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

}  // namespace

void motion_frame(const Race& race) {
    if (!telemetry::enabled()) return;
    telemetry::Motion m;
    Track& t = g_track;
    MachinePose p;
    if (!race_live(race) || !machine_pose(race.machine, &p)) {
        // Paused, between races, in a menu: settle, and pick up afresh after. A pause holds
        // the machine still, so a resumed race differences from where it stopped, but the
        // velocity it had is gone; starting again is the safe side.
        t.known = 0;
        telemetry::publish(m);
        return;
    }
    const float pos[3] = {p.m[0][3], p.m[1][3], p.m[2][3]};
    float d[3];
    for (int i = 0; i < 3; i++) d[i] = pos[i] - t.pos[i];
    const bool jump = dot(d, d) > kJump * kJump;
    if (t.known == 0 || jump) {
        for (int i = 0; i < 3; i++) t.vel[i] = t.acc[i] = 0.0f;
        t.known = 1;
        m.discontinuity = true;
    } else if (t.known == 1) {
        for (int i = 0; i < 3; i++) t.vel[i] = d[i] / kDt;
        t.known = 2;
        m.discontinuity = true;
    } else {
        static const float vel_s = telemetry::get("velocity_s", 0.05f);
        static const float acc_s = telemetry::get("accel_s", 0.15f);
        auto rate = [](float tau) { return tau > 0.0f ? 1.0f - expf(-kDt / tau) : 1.0f; };
        for (int i = 0; i < 3; i++) {
            const float prev = t.vel[i];
            t.vel[i] += (d[i] / kDt - t.vel[i]) * rate(vel_s);
            t.acc[i] += ((t.vel[i] - prev) / kDt - t.acc[i]) * rate(acc_s);
        }
        t.known = 3;
    }
    for (int i = 0; i < 3; i++) t.pos[i] = pos[i];

    // The machine's axes are the matrix's columns: right, up, and back.
    const float right[3] = {p.m[0][0], p.m[1][0], p.m[2][0]};
    const float fwd[3] = {-p.m[0][2], -p.m[1][2], -p.m[2][2]};
    const float* n = p.normal;
    constexpr float kDeg = 57.2957795f;
    auto clamp1 = [](float x) { return fmaxf(-1.0f, fminf(1.0f, x)); };
    static const float accel_scale = telemetry::get("accel_scale", 0.1f);

    m.active = true;
    m.speed_kmh = sqrtf(dot(t.vel, t.vel)) * 3.6f;
    m.pitch = asinf(clamp1(dot(fwd, n))) * kDeg;      // nose up is positive
    m.roll = -asinf(clamp1(dot(right, n))) * kDeg;    // right side down is positive
    m.yaw = atan2f(fwd[0], -fwd[2]) * kDeg;           // a heading: turning right raises it
    m.surge = dot(t.acc, fwd) * accel_scale;
    m.sway = dot(t.acc, right) * accel_scale;
    m.heave = dot(t.acc, n) * accel_scale;
    m.sway_velocity = dot(t.vel, right);
    telemetry::publish(m);
}

}  // namespace fzgx
