// This game's answers to the headset frontend's questions (gcn-recomp/runtime/vr_game.h),
// and its per-frame work on the guest thread: when to be in stereo, how to reach the
// game's own first-person camera, and the motion telemetry. See docs/dev/vr.md.
//
// Stereo -- the world around the viewer, six degrees of freedom -- for a live race: the
// player's own (not the title's demo), from the grid through the countdown to the end, and
// not paused (race_live in race.h). The theater panel for everything else: menus, the
// course flyover, the pause menu, the retired and results screens. The frontend holds a
// change for two frames before following it.
//
// First person is the game's: its race camera has a cockpit view, D-Up from the default
// view and D-Down back. The right thumbstick click asks for one or the other
// (set_first_person); the guest frame hook compares what was asked with the camera the game
// is in and, in a live race, taps the D-pad until they agree, a tap at a time. It reads the
// game's camera rather than counting taps, so a D-pad press of the player's own, or a race
// that starts in another view, is simply followed; and it never taps outside a live race,
// where the D-pad moves menu cursors. The tap reaches the game through map_pad, which the
// headset frontends call with each controller state they build.
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "gx/render.h"
#include "hw/pad.h"
#include "motion.h"
#include "race.h"
#include "vr_game.h"

namespace {

// The latest answer, for the render thread: decided on the guest thread from one
// consistent frame of the game's memory.
std::atomic<bool> g_stereo{false};

// What the viewer asked for: -1 nothing yet (no headset, or not asked), 0 the game's chase
// camera, 1 first person.
std::atomic<int> g_want_fp{-1};

// A D-pad tap in progress: the button, held until the frame count passes `until`.
std::atomic<uint16_t> g_tap_button{0};
std::atomic<uint32_t> g_tap_until{0};

constexpr uint32_t kTapFrames = 4;      // held this many game frames
constexpr uint32_t kTapSettle = 20;     // and no other until this many after it ends
constexpr int kTapTries = 3;            // taps that change nothing before giving up on a request

// Frames a race must have been live before stereo comes back after it was not. A quit from
// the pause menu unpauses the race for about 30 frames before the race ends, which would
// otherwise blink into stereo on the way out; resuming costs the same wait.
constexpr uint32_t kResumeFrames = 45;

// FZGX_VRLOG=1: each tap and each change of the stereo answer. FZGX_FP=0/1 asks for the
// chase or first-person camera from the start, as the right stick click would: with no
// headset the taps are only logged (the desktop does not call map_pad), which shows what a
// headset would do.
bool log() {
    static const bool on = getenv("FZGX_VRLOG") != nullptr;
    return on;
}

void guest_frame(uint32_t frame) {
    static const bool preset = [] {
        if (const char* e = getenv("FZGX_FP")) g_want_fp.store(atoi(e) ? 1 : 0);
        return true;
    }();
    (void)preset;
    const fzgx::Race race = fzgx::race_read();
    const bool race_live = fzgx::race_live(race);
    // From the grid on a fresh race stereo comes at once; after a pause, only once the race
    // has stayed live for kResumeFrames.
    static uint32_t live_since = 0;
    static bool was_live = false, was_paused = false;
    if (race_live && !was_live) live_since = frame;
    if (!race_live && race.paused) was_paused = true;
    if (!race.player_race) was_paused = false;
    was_live = race_live;
    const bool live = race_live && (!was_paused || frame - live_since >= kResumeFrames);
    if (live) was_paused = false;
    if (log() && live != g_stereo.load(std::memory_order_relaxed))
        fprintf(stderr, "[vr] frame %u: %s (state %d, player race %d, paused %d)\n", frame, live ? "stereo" : "theater",
                race.state, race.player_race, race.paused);
    g_stereo.store(live, std::memory_order_relaxed);

    // Bring the game's camera to what was asked, in a live race only.
    static uint32_t next_tap = 0;
    static int tries = 0, tries_for = -1;
    const int want = g_want_fp.load(std::memory_order_relaxed);
    const bool fp = race.camera == 0;
    if (want != tries_for || (want == 1) == fp) {
        tries = 0;   // a new request, or this one met
        tries_for = want;
    }
    if (live && race.state == fzgx::kRacing && want >= 0 && race.camera >= 0 && frame >= next_tap &&
        tries < kTapTries) {
        if (want == 1 && !fp) {
            g_tap_button.store(PAD_UP, std::memory_order_relaxed);
        } else if (want == 0 && fp) {
            g_tap_button.store(PAD_DOWN, std::memory_order_relaxed);
        } else {
            g_tap_button.store(0, std::memory_order_relaxed);
        }
        if (const uint16_t b = g_tap_button.load(std::memory_order_relaxed)) {
            g_tap_until.store(frame + kTapFrames, std::memory_order_relaxed);
            next_tap = frame + kTapFrames + kTapSettle;
            if (++tries == kTapTries && log()) fprintf(stderr, "[vr] the camera has not followed; giving up\n");
            if (log()) fprintf(stderr, "[vr] frame %u: camera %d, %s asked: D-%s\n", frame, race.camera,
                               want ? "first person" : "chase", b == PAD_UP ? "Up" : "Down");
        }
    }

    fzgx::motion_frame(race);
}

bool wants_stereo(const gx::Batch&) { return g_stereo.load(std::memory_order_relaxed); }

// Called on every change of view: with on=false whenever the view leaves stereo too, which
// a pause does. Nothing is tapped then (not live); the frontend asks again on the way back.
void set_first_person(bool on, const VrConfig&) { g_want_fp.store(on ? 1 : 0, std::memory_order_relaxed); }

void map_pad(PadState& p) {
    const uint16_t b = g_tap_button.load(std::memory_order_relaxed);
    if (b && gx::g_frames_submitted.load(std::memory_order_relaxed) < g_tap_until.load(std::memory_order_relaxed))
        p.buttons |= b;
}

void config_defaults(VrConfig& c) {
    // A game unit is a metre by the speedometer (a unit a second is 3.6 of its km/h), so
    // the world is drawn life-size until the headset says otherwise.
    c.units_per_metre = 1.0f;
}

const bool installed = [] {
    vr::GameHooks h;
    h.config_defaults = config_defaults;
    h.wants_stereo = wants_stereo;
    h.set_first_person = set_first_person;
    h.map_pad = map_pad;
    vr::set_game_hooks(h);
    gx::set_guest_frame_hook(guest_frame);
    return true;
}();

}  // namespace
