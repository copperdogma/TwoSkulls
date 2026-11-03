#pragma once

#include <cstdint>
#include "esp_err.h"

namespace skull::actuators {
struct Stats {
    uint32_t deferred_updates;
    uint64_t max_pending_us;
    uint8_t pending_channels; // Bit 0 jaw, bit 1 left eye, bit 2 right eye.
    bool stuck; // At least one currently pending channel observed for >=100 ms.
};
// Same diagnostic-task owner as tick(); no cross-task synchronization.
Stats stats();
enum class JawMode { automatic, manual, disabled };
struct JawState {
    JawMode mode;
    unsigned requested_us; // Desired PWM command, not measured shaft position.
    unsigned applied_us; // Last successfully applied command; 0 means no pulses.
    unsigned closed_us;
    unsigned open_us;
    unsigned minimum_us; // Fixed manual-session exploration envelope.
    unsigned maximum_us;
    bool enabled; // Last applied PWM duty is nonzero, not physical feedback.
    bool faulted; // Any driver error latches motion off until reboot.
};
// All jaw APIs have the same single control-task owner as tick().
// Validate/configure before first motion. Initial activation uses the configured
// closed pulse; later reconfiguration preserves manual/disabled command state.
bool configure_jaw(unsigned closed_us, unsigned open_us);
JawState jaw_state();
// Caller must establish show/audio/startup/tone quiescence and prevent new work.
// Entry holds the last applied command, cancels automatic gestures, and starts
// a 90 s lease. No automatic restart follows timeout; it disables pulses.
bool jaw_manual_enter();
// Every accepted command is <=25 us from both the last applied pulse and the
// last request, within the session's saved-endpoint envelope plus at most 50 us.
// Fixed electrical bounds 544..2400 us do not establish linkage-safe travel.
bool jaw_manual_target(unsigned pulse_us);
bool jaw_manual_step(int delta_us);
// Immediate best-effort zero duty, retried by tick if LEDC is currently pending.
// Driver faults also latch disabled and retain these retries (even for eye-only
// faults); init/manual/resume cannot reactivate the servo before a reboot.
// Disabling pulses is not physical signal disconnection or assured torque off.
void jaw_disable();
// Explicit normal-motion restart only. First return slews <=10 us per tick.
bool jaw_resume();
// Initializes jaw GPIO23 (50 Hz) with ZERO duty, eyes GPIO32/33 (5 kHz) dim.
// Call configure_jaw only after validated SD configuration is available.
esp_err_t init();
// Diagnostic task only, approximately every 20 ms. RMS uses signed 16-bit PCM
// units. Speech/audio_busy cancel the boot check and idle breathing. Idle jaw
// breathing opens to 30 degrees over 2 s, holds 100 ms, closes over 2 s, and
// starts every 7 s (first interval begins at the first tick). Eyes remain dim.
void tick(uint16_t rms, bool speaking, bool audio_busy = false);
// Same owner as tick(). Feed actual A2DP connection, not stream readiness.
// Warn after 30 s boot age without a speaker, or on the first tick after an
// observed connection drops. Reconnection clears it. Existing diagnostics win.
void speaker_connection(bool connected);
// Repeat the gentle check; invoke from the same task as tick(), while idle.
void test();
// Same control-task owner as tick(); may also be set during boot before that
// task starts. Overrides only eye brightness: count flashes, 200 ms on/200 ms
// off each, optionally repeating after an additional 2000 ms dark gap. The first
// tick anchors time; delayed ticks skip elapsed phases. Speech never clears a
// diagnostic. A one-shot returns to speech/idle brightness; count 0 clears it.
void diagnostic(unsigned count, bool repeat = false);
// Full pattern duration; include the inter-group gap for boot error retries.
uint32_t diagnostic_duration_ms(unsigned count, bool include_gap = true);
}
