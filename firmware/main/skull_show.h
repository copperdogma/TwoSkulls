#pragma once
#include <cstdint>
namespace skull::catalog { struct Catalog; }
namespace skull::show {
void init(catalog::Catalog *assets, bool primary);
// After startup, a primary peer-readiness edge plays Marco to local PCM EOF,
// allows a 300 ms output-tail guard (speaker acoustic latency is unmeasured),
// requests raw /audio/Polo.wav, and waits for bounded legacy ACK/readback.
// Secondary connections alone are silent; Polo requires that explicit request.
// Safe from Matter's system task: bounded single pending trigger, no backlog.
// Primary random skit coordinates when peer-ready at execution, otherwise solo.
bool trigger();
// Diagnostic/control task only. Local playback deliberately bypasses the peer
// handshake, so it is available for single-board acceptance before Jeff returns.
bool local(const char *path);
// Diagnostic task only: choose one skit, retaining normal peer/readiness,
// cooldown and ACK-before-start gates. Never queues or bypasses the handshake.
bool paired(const char *path);
// Control owner only: exact catalog skit, enforcing startup/greeting/readiness/
// cooldown gates. Secondary plays locally; primary coordinates only if the peer
// is ready when this request begins. A later coordinated failure never replays
// solo automatically. False rejects immediately without a queued request.
bool play_skit(const char *path);
// Control-owner snapshots for remote command availability and current activity.
bool command_ready();
// Control-owner only. Calibration/disabled jaw owns the prop: drop queued
// requests/greetings and reject every local, Matter and incoming peer start.
void set_jaw_inhibited(bool inhibited);
// Active asset path including greeting/cleanup; empty when idle.
const char *current_path();
void tick(bool initialization_finished, bool startup_active);
// Active playback/handshake or queued work with a usable speaker. A greeting
// waiting idle for an unavailable speaker leaves manual connection retry free.
bool busy();
// Atomic snapshot of the primary trigger acceptance gate.
bool ready();
const char *state_name();
}
