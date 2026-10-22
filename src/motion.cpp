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
// or a slope's change.
//
// Yaw is not the heading. Turning a platform with every turn was jarring (found on a rig
// with another game, through the same shared module), and it cannot turn all the way round
// anyway, so it yaws only for what a pilot would feel through the seat (Motion::yaw_heading
// false):
// - in a slide, by the slip angle -- the nose against the way the machine is going, positive
//   with the nose to the right of it -- beyond `slide_deadzone` (10 degrees), times
//   `slide_yaw_gain` (0.4), at most `slide_yaw_max` (12), eased. On a recorded ride the slip
//   was 1 degree at the median and 7 at the 90th percentile, and 20-45 through slide turns;
// - in a spin -- the machine turning faster than `spin_rate` (400 degrees a second; it turns
//   at 190 at most when steered, and 1350-1500 in a spin attack, for about a second) -- a
//   thrash: `spin_yaw` (12) degrees times the sine of how far it has turned since the spin
//   began. `yaw_rate` is raised to 360 degrees a second to let it through.
// The sideways velocity (Motion::sway_velocity) goes with the slide's weight, for a profile
// that takes traction loss from it.
//
// The machine hovers, but a platform that sits dead still on a straight feels like no
// track at all: the shared module's road texture is on at `track_texture` (0.3), off in the
// air -- while the eased acceleration into the track is below -0.6 g, which a jump's fall is
// (about -9 m/s^2) and nothing on the track is.
//
// Speed is the game's: the speedometer's km/h is a game unit a second times 3.6. That makes
// the accelerations a racing machine's: a boost start goes from 0 to 600 km/h in under a
// second, about 20 g. `accel_scale` (0.1) brings them to a platform's range before the shared
// module's own limits; telemetry.txt tunes it. The engine a shaker plays is the speed, from
// `idle_rpm` (1500) to `max_rpm` (9000) at `top_kmh` (1400; a ride's median was 990 and its
// boosts 1400-1780): the machine has no revs of its own to read.
//
// Inactive -- the platform settles -- outside a live race (race_live: the grid to the end,
// unpaused). The first frame of a run only sets the position and the second the velocity,
// both sent as a discontinuity; so is a jump of more than kJump units in a frame.
#include "motion.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "telemetry.h"

namespace fzgx {
namespace {

constexpr float kDt = 1.0f / 60.0f;
constexpr float kJump = 60.0f;   // units in a frame: 13000 km/h
constexpr float kDeg = 57.2957795f;

struct Track {
    int known = 0;   // 0 nothing, 1 the position, 2 the velocity too, 3 everything
    float pos[3];
    float vel[3];    // units/s, eased
    float acc[3];    // units/s^2, eased
};
Track g_track;

// The platform's yaw (see the top of the file).
struct Yaw {
    bool have_fwd = false;
    float fwd_prev[3];
    float slide = 0.0f;     // the slide's yaw, degrees, eased
    float slide_w = 0.0f;   // how far into a slide, 0 to 1
    bool spinning = false;
    float spin_w = 0.0f;    // 0 to 1, eased
    float spun = 0.0f;      // degrees turned since the spin began
    float rev = 0.0f;       // the engine, 0 idle to 1 flat out, eased
};
Yaw g_yaw;

float dot(const float* a, const float* b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

float rate(float tau) { return tau > 0.0f ? 1.0f - expf(-kDt / tau) : 1.0f; }

// FZGX_MOTIONLOG=1: what goes into the telemetry module twice a second, before its fades and
// limits (GCN_TELEMETRY_LOG shows what comes out); FZGX_MOTIONLOG_EVERY=n every nth frame.
int log_every() {
    static const int n = getenv("FZGX_MOTIONLOG") ? (getenv("FZGX_MOTIONLOG_EVERY") ? atoi(getenv("FZGX_MOTIONLOG_EVERY")) : 30) : 0;
    return n;
}

}  // namespace

void motion_frame(const Race& race) {
    if (!telemetry::enabled()) return;
    static const bool defaults = [] {
        telemetry::set_default("yaw_rate", 360.0f);
        return true;
    }();
    (void)defaults;
    telemetry::Motion m;
    Track& t = g_track;
    MachinePose p;
    if (!race_live(race) || !machine_pose(race.machine, &p)) {
        // Paused, between races, in a menu: settle, and pick up afresh after. A pause holds
        // the machine still, so a resumed race differences from where it stopped, but the
        // velocity it had is gone; starting again is the safe side.
        t.known = 0;
        g_yaw = Yaw();
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
    auto clamp1 = [](float x) { return fmaxf(-1.0f, fminf(1.0f, x)); };
    static const float accel_scale = telemetry::get("accel_scale", 0.1f);

    // The slip angle: the nose against the velocity, positive with the nose to its right.
    const float v_fwd = dot(t.vel, fwd), v_right = dot(t.vel, right);
    const float speed = sqrtf(dot(t.vel, t.vel));
    const bool moving = speed * 3.6f > 50.0f;
    const float slip = moving ? atan2f(-v_right, fabsf(v_fwd)) * kDeg : 0.0f;

    // How fast the machine is turning, right positive, and the yaw that follows from it.
    Yaw& y = g_yaw;
    float turn_rate = 0.0f, turned = 0.0f;
    if (y.have_fwd) {
        turned = atan2f(-dot(y.fwd_prev, right), dot(y.fwd_prev, fwd)) * kDeg;
        turn_rate = turned / kDt;
    }
    for (int i = 0; i < 3; i++) y.fwd_prev[i] = fwd[i];
    y.have_fwd = true;
    static const float spin_rate = telemetry::get("spin_rate", 400.0f);
    static const float spin_yaw = telemetry::get("spin_yaw", 12.0f);
    static const float deadzone = telemetry::get("slide_deadzone", 10.0f);
    static const float slide_gain = telemetry::get("slide_yaw_gain", 0.4f);
    static const float slide_max = telemetry::get("slide_yaw_max", 12.0f);
    // A spin starts above spin_rate and lasts until the turning falls below half of it.
    const bool spinning = fabsf(turn_rate) > (y.spinning ? 0.5f : 1.0f) * spin_rate;
    if (spinning && !y.spinning) y.spun = 0.0f;
    if (spinning) y.spun += turned;
    y.spinning = spinning;
    y.spin_w += ((spinning ? 1.0f : 0.0f) - y.spin_w) * rate(0.08f);
    const float beyond = fmaxf(0.0f, fabsf(slip) - deadzone);
    const float slide_target = spinning ? 0.0f : copysignf(fminf(slide_max, beyond * slide_gain), slip);
    y.slide += (slide_target - y.slide) * rate(0.15f);
    y.slide_w += ((!spinning && beyond > 0.0f ? 1.0f : 0.0f) - y.slide_w) * rate(0.25f);

    // The engine: the speed.
    static const float idle_rpm = telemetry::get("idle_rpm", 1500.0f);
    static const float max_rpm = telemetry::get("max_rpm", 9000.0f);
    static const float top_kmh = telemetry::get("top_kmh", 1400.0f);
    y.rev += (fminf(1.0f, speed * 3.6f / top_kmh) - y.rev) * rate(0.1f);

    // In the air: falling, not pressed into the track.
    static const float texture = telemetry::get("track_texture", 0.3f);
    const bool airborne = dot(t.acc, n) < -0.6f * 9.8f;

    m.active = true;
    m.speed_kmh = speed * 3.6f;
    m.pitch = asinf(clamp1(dot(fwd, n))) * kDeg;      // nose up is positive
    m.roll = -asinf(clamp1(dot(right, n))) * kDeg;    // right side down is positive
    m.yaw = y.slide + spin_yaw * sinf(y.spun / kDeg) * y.spin_w;
    m.surge = dot(t.acc, fwd) * accel_scale;
    m.sway = dot(t.acc, right) * accel_scale;
    m.heave = dot(t.acc, n) * accel_scale;
    m.sway_velocity = v_right * fmaxf(y.slide_w, y.spin_w);
    m.rpm = idle_rpm + (max_rpm - idle_rpm) * y.rev;
    m.max_rpm = max_rpm;
    m.roughness = airborne ? 0.0f : texture;
    if (log_every()) {
        static int k;
        if (k++ % log_every() == 0)
            fprintf(stderr, "[motion] %6.1f km/h pitch %+5.1f roll %+5.1f yaw %+6.1f | surge %+6.2f sway %+6.2f heave %+6.2f m/s2 slide %+6.1f m/s | "
                            "slip %+6.1f turning %+7.1f spin %.2f spun %+5.0f | air %d rpm %5.0f\n",
                    m.speed_kmh, m.pitch, m.roll, m.yaw, m.surge, m.sway, m.heave, v_right, slip, turn_rate, y.spin_w, y.spun,
                    (int)airborne, m.rpm);
    }
    telemetry::publish(m);
}

}  // namespace fzgx
