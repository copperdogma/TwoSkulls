#pragma once

#include <cstdint>
#include "esp_err.h"

namespace skull::actuators {
// Initializes jaw GPIO23 (50 Hz), eyes GPIO32/33 (5 kHz), and closed/dim
// outputs. Call before starting the diagnostic task.
esp_err_t init();
// Diagnostic task only, approximately every 20 ms. RMS uses signed 16-bit PCM
// units. Speech/audio_busy cancel the boot check and idle breathing. Idle jaw
// breathing opens to 30 degrees over 2 s, holds 100 ms, closes over 2 s, and
// starts every 7 s (first interval begins at the first tick). Eyes remain dim.
void tick(uint16_t rms, bool speaking, bool audio_busy = false);
// Repeat the gentle check; invoke from the same task as tick(), while idle.
void test();
// Same control-task owner as tick(); may also be set during boot before that
// task starts. Overrides only eye brightness: count flashes, 100 ms on/100 ms
// off each, optionally repeating after an additional 500 ms dark gap. The first
// tick anchors time; delayed ticks skip elapsed phases. Speech never clears a
// diagnostic. A one-shot returns to speech/idle brightness; count 0 clears it.
void diagnostic(unsigned count, bool repeat = false);
}
