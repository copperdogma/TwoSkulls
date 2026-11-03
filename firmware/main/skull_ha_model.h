#pragma once
#include "skull_catalog.h"
#include <cstddef>
#include <cstdint>

namespace skull::ha {
struct Snapshot {
    const char *show_state = "", *current_skit = "", *selected_skit = "", *command_result = "";
    const char *jaw_mode = "disabled", *jaw_result = "none";
    unsigned jaw_command_us = 0, jaw_applied_us = 0;
    unsigned jaw_closed_us = 544, jaw_open_us = 1368;
    unsigned jaw_draft_closed_us = 544, jaw_draft_open_us = 1368;
    bool jaw_dirty = false;
    bool speaker = false, peer = false, stream = false, sd = false, error_pending = false;
    uint64_t uptime_s = 0, report_id = 0;
    uint32_t free_heap = 0, min_free_heap = 0, report_count = 0;
    unsigned report_slots = 0; // Eight fixed SD archive slots; values above 255 are invalid.
    bool wifi_rssi_valid = false;
    int wifi_rssi_dbm = 0;
    int64_t control_ready_ms = -1, sd_mount_ms = -1, sd_config_ms = -1, speaker_connected_ms = -1;
    int64_t bluetooth_rssi_delta = -999;
    uint32_t sd_attempts = 0, sd_failures = 0, speaker_disconnects = 0;
};
// Exact basename of a catalog-validated scripted asset; never a display-name
// transform. nullptr rejects non-skits, malformed paths and ambiguous options.
const char *skit_option(const catalog::Asset &asset);
const char *skit_path(const catalog::Catalog &catalog, const char *option);

// Caller owns/serializes this object (PSRAM on device). No allocation or ESP-IDF
// dependencies. Failed encoding exposes an empty payload, never partial JSON.
class Model {
public:
    static constexpr size_t capacity = 24576;
    bool init(const char *device_id, bool primary, const char *firmware);
    bool discovery(const catalog::Catalog &catalog);
    bool state(const Snapshot &snapshot);
    const char *json() const { return buffer_; }
    size_t size() const { return used_; }
    const char *id() const { return id_; }
    const char *base_topic() const { return base_; }
private:
    char id_[49]{}, base_[80]{}, firmware_[65]{}, buffer_[capacity]{};
    size_t used_ = 0;
    bool primary_ = false, initialized_ = false, ok_ = false;
    void begin();
    bool finish();
    void append(const char *format, ...);
    void string(const char *value);
    void topic(const char *suffix);
    void sensor(const char *key, const char *name, const char *unit = nullptr,
                const char *device_class = nullptr);
    void binary(const char *key, const char *name, const char *device_class = nullptr);
    void jaw_button(const char *key, const char *name, const char *payload, bool manual_only = true);
    void component_start(const char *key, const char *platform, const char *name);
};
}
