#pragma once
#include <cstdint>

namespace skull::startup {
// Boot/task context only. Retains immutable PCM in PSRAM for this boot.
bool load(bool primary, unsigned volume_percent);
// Called by the diagnostic task. Starts once after media readiness; cancels
// remaining speech after a link loss instead of replaying a stale tail.
void poll(bool stream_ready);
// Callback context: bounded copy only; caller has already zeroed output.
bool provide(uint8_t *data, int32_t length);
bool busy();
bool finished(); // Terminal, including a missing or interrupted announcement.
bool playing(); // Includes startup preroll, but not waiting for the speaker.
struct Animation { uint16_t rms; bool speaking; };
// Consumed PCM cursor + precomputed envelope; called from control task only.
Animation animation();
}
