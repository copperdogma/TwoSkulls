#pragma once
#include "esp_err.h"
#include "skull_config.h"

namespace skull::storage {
// Mounts the onboard slot read/write; never formats on a mount failure.
esp_err_t mount_and_read_config(Config &config);
bool mounted();
// Caller must hold show/audio/startup/tone idle and all content FILEs closed.
// Changes only speaker_name lines, preserving other bytes. Bounded to 4096
// bytes. Preserves first config.speaker.original; verified tmp -> previous ->
// installed transaction is recoverable but not power-fail atomic on FAT.
// Refuses stale config.speaker.tmp/previous rather than overwrite evidence.
// Runtime selection is unchanged until reboot. ESP_OK means installed/readback
// verified (or already identical); failures may leave recovery files to inspect.
esp_err_t set_speaker_name(const char *name);
// Boot retry only, before any audio file/producer exists. Never formats.
esp_err_t unmount_for_retry();
}
