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
// Refuses any stale config.speaker/config.jaw tmp/previous rather than
// overwrite evidence from another configuration writer.
// Runtime selection is unchanged until reboot. ESP_OK means installed/readback
// verified (or already identical); failures may leave recovery files to inspect.
esp_err_t set_speaker_name(const char *name);
// Same exclusive idle/file ownership requirement as set_speaker_name(). Saves
// both endpoints together, preserving unrelated bytes and verifying readback.
// Missing keys are appended; existing keys retain formatting. Defaults/limits
// are defined by Config/valid_jaw_limits(). No actuator output is changed here.
// Preserves first config.jaw.original; refuses any speaker/jaw tmp/previous
// recovery file. FAT replacement is recoverable, not power-fail atomic.
esp_err_t set_jaw_limits(unsigned closed_us, unsigned open_us);
// Boot retry only, before any audio file/producer exists. Never formats.
esp_err_t unmount_for_retry();
}
