#include "skull_ha_model.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace skull::ha {
namespace {
// Reject malformed UTF-8 rather than publishing non-JSON byte sequences from an
// unexpected card filename. Controls remain valid through JSON escaping.
bool utf8(const char *text) {
    if (!text) return false;
    const auto *p = reinterpret_cast<const unsigned char *>(text);
    while (*p) {
        const unsigned char lead = *p++;
        if (lead < 0x80) continue;
        unsigned count;
        uint32_t value;
        if (lead >= 0xc2 && lead <= 0xdf) { count = 1; value = lead & 0x1f; }
        else if (lead >= 0xe0 && lead <= 0xef) { count = 2; value = lead & 0x0f; }
        else if (lead >= 0xf0 && lead <= 0xf4) { count = 3; value = lead & 7; }
        else return false;
        for (unsigned i = 0; i < count; ++i) {
            if (*p < 0x80 || *p > 0xbf) return false;
            value = (value << 6) | (*p++ & 0x3f);
        }
        if ((count == 2 && value < 0x800) || (count == 3 && value < 0x10000) ||
            (value >= 0xd800 && value <= 0xdfff) || value > 0x10ffff) return false;
    }
    return true;
}
}
const char *skit_option(const catalog::Asset &asset) {
    if (!asset.skit || !std::memchr(asset.path, 0, sizeof(asset.path)) ||
        std::strncmp(asset.path, "/audio/", 7)) return nullptr;
    const char *option = asset.path + 7;
    const size_t n = std::strlen(option);
    if (n <= 4 || option[n-4] != '.' ||
        (option[n-3] != 'w' && option[n-3] != 'W') ||
        (option[n-2] != 'a' && option[n-2] != 'A') ||
        (option[n-1] != 'v' && option[n-1] != 'V') || std::strchr(option, '/') ||
        std::strchr(option, '\\') || !utf8(option)) return nullptr;
    return option;
}
const char *skit_path(const catalog::Catalog &content, const char *option) {
    if (!option || content.count > catalog::max_assets) return nullptr;
    const char *result = nullptr;
    for (size_t i = 0; i < content.count; ++i) {
        const char *candidate = skit_option(content.assets[i]);
        if (candidate && !std::strcmp(candidate, option)) {
            if (result) return nullptr;
            result = content.assets[i].path;
        }
    }
    return result;
}
bool Model::init(const char *device_id, bool primary, const char *firmware) {
    initialized_ = false; used_ = 0; buffer_[0] = 0; id_[0] = base_[0] = firmware_[0] = 0;
    if (!device_id || !firmware || !*device_id || std::strlen(device_id) >= sizeof(id_) ||
        std::strlen(firmware) >= sizeof(firmware_) || !utf8(firmware)) return false;
    for (const char *p = device_id; *p; ++p)
        if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '_' || *p == '-')) return false;
    std::strcpy(id_, device_id); std::strcpy(firmware_, firmware);
    std::snprintf(base_, sizeof(base_), "halloween/%s", id_);
    primary_ = primary; initialized_ = true; return true;
}
void Model::begin() { used_ = 0; buffer_[0] = 0; ok_ = initialized_; }
bool Model::finish() {
    if (!ok_) { used_ = 0; buffer_[0] = 0; }
    return ok_;
}
void Model::append(const char *format, ...) {
    if (!ok_) return;
    va_list args; va_start(args, format);
    const int n = std::vsnprintf(buffer_ + used_, capacity - used_, format, args);
    va_end(args);
    if (n < 0 || static_cast<size_t>(n) >= capacity - used_) { ok_ = false; return; }
    used_ += static_cast<size_t>(n);
}
void Model::string(const char *value) {
    if (!ok_) return;
    if (!utf8(value)) { ok_ = false; return; }
    append("\"");
    for (const auto *p = reinterpret_cast<const unsigned char *>(value); *p && ok_; ++p) {
        if (*p == '"' || *p == '\\') append("\\%c", *p);
        else if (*p < 0x20) append("\\u%04x", unsigned(*p));
        else append("%c", *p);
    }
    append("\"");
}
void Model::topic(const char *suffix) {
    append("\"%s/%s\"", base_, suffix); // base is validated ASCII; suffix is internal.
}
void Model::component_start(const char *key, const char *platform, const char *name) {
    append("\"%s\":{\"platform\":\"%s\",\"unique_id\":\"%s_%s\",\"name\":", key, platform, id_, key);
    string(name);
    if (!std::strncmp(key, "jaw_", 4))
        append(",\"default_entity_id\":\"%s.%s_%s\"", platform, primary_ ? "anthony" : "jeff", key);
}
void Model::sensor(const char *key, const char *name, const char *unit, const char *device_class) {
    component_start(key, "sensor", name);
    append(",\"value_template\":\"{{ value_json.%s }}\",\"entity_category\":\"diagnostic\",\"expire_after\":20", key);
    if (unit) { append(",\"unit_of_measurement\":"); string(unit); }
    if (device_class) { append(",\"device_class\":"); string(device_class); }
    append("},");
}
void Model::binary(const char *key, const char *name, const char *device_class) {
    component_start(key, "binary_sensor", name);
    append(",\"value_template\":\"{{ 'ON' if value_json.%s else 'OFF' }}\",\"payload_on\":\"ON\",\"payload_off\":\"OFF\",\"entity_category\":\"diagnostic\",\"expire_after\":20", key);
    if (device_class) { append(",\"device_class\":"); string(device_class); }
    append("},");
}
void Model::jaw_button(const char *key, const char *name, const char *payload, bool manual_only) {
    component_start(key, "button", name);
    append(",\"command_topic\":"); topic("jaw/set");
    append(",\"payload_press\":"); string(payload);
    append(",\"qos\":0,\"retain\":false,\"entity_category\":\"config\"");
    if (manual_only) {
        append(",\"availability\":[{\"topic\":"); topic("availability");
        append("},{\"topic\":"); topic("jaw_available");
        append("}],\"availability_mode\":\"all\"");
    }
    append("},");
}
bool Model::discovery(const catalog::Catalog &content) {
    begin();
    if (content.count > catalog::max_assets) { ok_ = false; return finish(); }
    size_t skits = 0;
    for (size_t i = 0; i < content.count; ++i) {
        if (!content.assets[i].skit) continue;
        const char *option = skit_option(content.assets[i]);
        if (!option || !skit_path(content, option)) { ok_ = false; return finish(); }
        ++skits;
    }
    append("{\"device\":{\"identifiers\":[\"%s\"],\"name\":\"%s\",\"manufacturer\":\"TwoSkulls\",\"model\":\"%s WROVER\",\"sw_version\":", id_, primary_ ? "Anthony" : "Jeff", primary_ ? "Primary" : "Secondary");
    string(firmware_);
    append("},\"origin\":{\"name\":\"TwoSkulls native firmware\"},\"state_topic\":"); topic("state");
    append(",\"availability\":[{\"topic\":"); topic("availability");
    append("}],\"qos\":0,\"cmps\":{");
    binary("speaker", "Speaker connected", "connectivity");
    binary("peer", "Peer connected", "connectivity");
    binary("stream", "Speaker stream ready");
    binary("sd", "SD mounted");
    binary("error_pending", "Error report pending", "problem");
    sensor("show_state", "Show state");
    sensor("current_skit", "Current skit");
    sensor("command_result", "Last command result");
    sensor("uptime_s", "Uptime", "s", "duration");
    sensor("free_heap", "Free internal heap", "B");
    sensor("min_free_heap", "Minimum internal heap", "B");
    sensor("wifi_rssi_dbm", "Wi-Fi signal", "dBm", "signal_strength");
    sensor("report_id", "Latest report ID");
    sensor("report_count", "Saved report count");
    sensor("role", "Role");
    sensor("control_ready_ms", "Control startup time", "ms", "duration");
    sensor("sd_mount_ms", "SD mount time", "ms", "duration");
    sensor("sd_config_ms", "SD mount and config time", "ms", "duration");
    sensor("speaker_connected_ms", "First speaker connection time", "ms", "duration");
    sensor("bluetooth_rssi_delta", "Bluetooth RSSI delta (not dBm)");
    sensor("sd_attempts", "SD initialization attempts");
    sensor("sd_failures", "SD initialization failures");
    sensor("speaker_disconnects", "Speaker disconnect count");
    sensor("jaw_mode", "Jaw mode");
    sensor("jaw_result", "Jaw tuning result");
    sensor("jaw_command_us", "Jaw commanded pulse", "µs");
    sensor("jaw_applied_us", "Jaw applied pulse", "µs");
    sensor("jaw_closed_us", "Jaw saved closed", "µs");
    sensor("jaw_open_us", "Jaw saved open", "µs");
    sensor("jaw_draft_closed_us", "Jaw draft closed", "µs");
    sensor("jaw_draft_open_us", "Jaw draft open", "µs");
    binary("jaw_dirty", "Jaw unsaved changes");
    component_start("jaw_target", "number", "Jaw target");
    append(",\"command_topic\":"); topic("jaw/set");
    append(",\"command_template\":\"TARGET:{{ value | int }}\",\"value_template\":\"{{ value_json.jaw_command_us }}\",\"unit_of_measurement\":\"µs\",\"min\":544,\"max\":2400,\"step\":1,\"mode\":\"box\",\"optimistic\":false,\"qos\":0,\"retain\":false,\"entity_category\":\"config\",\"availability\":[{\"topic\":"); topic("availability");
    append("},{\"topic\":"); topic("jaw_available");
    append("}],\"availability_mode\":\"all\"},");
    jaw_button("jaw_enter", "Jaw enter tuning", "ENTER", false);
    jaw_button("jaw_stop", "Jaw disable pulses", "STOP", false);
    jaw_button("jaw_resume", "Jaw resume automatic", "RESUME", false);
    jaw_button("jaw_close_fine", "Jaw close fine", "STEP:CLOSE1");
    jaw_button("jaw_open_fine", "Jaw open fine", "STEP:OPEN1");
    jaw_button("jaw_close_coarse", "Jaw close coarse", "STEP:CLOSE10");
    jaw_button("jaw_open_coarse", "Jaw open coarse", "STEP:OPEN10");
    jaw_button("jaw_capture_closed", "Jaw capture closed", "CLOSED");
    jaw_button("jaw_capture_open", "Jaw capture open", "OPEN");
    jaw_button("jaw_save", "Jaw save to SD", "SAVE");
    jaw_button("jaw_discard", "Jaw discard draft", "DISCARD");
    // Firmware is part of the device metadata and state; avoid a redundant
    // firmware entity. Current activity is diagnostic, never an action.
    if (skits) {
        component_start("skit", "select", "Skit");
        append(",\"command_topic\":"); topic("select/set");
        append(",\"value_template\":\"{{ value_json.selected_skit }}\",\"retain\":false,\"options\":[");
        bool first = true;
        for (size_t i = 0; i < content.count; ++i) {
            const char *option = skit_option(content.assets[i]); if (!option) continue;
            if (!first) append(",");
            string(option); first = false;
        }
        append("]},");
        component_start("play", "button", "Play selected skit");
        append(",\"command_topic\":"); topic("play");
        append(",\"payload_press\":\"PLAY\",\"qos\":0,\"retain\":false,\"availability\":[{\"topic\":"); topic("availability");
        append("},{\"topic\":"); topic("play_available");
        append("}],\"availability_mode\":\"all\"},");
    }
    component_start("saved_reports", "button", "Open saved reports");
    append(",\"command_topic\":"); topic("logs/request");
    append(",\"payload_press\":\"REFRESH\",\"qos\":0,\"retain\":false,\"availability\":[{\"topic\":"); topic("availability");
    append("},{\"topic\":"); topic("logs_available");
    append("}],\"availability_mode\":\"all\"},");
    // All helpers end components with comma. Remove exactly that final comma.
    if (ok_ && used_ && buffer_[used_ - 1] == ',') buffer_[--used_] = 0;
    append("}}");
    return finish();
}
bool Model::state(const Snapshot &s) {
    begin();
    if (!s.jaw_mode || (std::strcmp(s.jaw_mode,"automatic") && std::strcmp(s.jaw_mode,"manual") && std::strcmp(s.jaw_mode,"disabled")) ||
        s.jaw_command_us > 2400 || (s.jaw_command_us && s.jaw_command_us < 544) ||
        s.jaw_applied_us > 2400 || (s.jaw_applied_us && s.jaw_applied_us < 544) ||
        s.jaw_closed_us < 544 || s.jaw_closed_us > 2400 || s.jaw_open_us < 544 || s.jaw_open_us > 2400 || s.jaw_closed_us == s.jaw_open_us ||
        s.jaw_draft_closed_us < 544 || s.jaw_draft_closed_us > 2400 || s.jaw_draft_open_us < 544 || s.jaw_draft_open_us > 2400) {
        ok_ = false; return finish();
    }
    if (s.report_slots > 255) { ok_ = false; return finish(); }
    append("{\"role\":\"%s\",\"firmware\":", primary_ ? "primary" : "secondary"); string(firmware_);
    append(",\"show_state\":"); string(s.show_state);
    append(",\"current_skit\":");
    const char *current = s.current_skit;
    if (current && !*current)
        current = s.show_state && !std::strcmp(s.show_state, "idle") ? "Idle" : "Unknown";
    string(current);
    append(",\"selected_skit\":"); string(s.selected_skit);
    append(",\"command_result\":"); string(s.command_result);
    append(",\"speaker\":%s,\"peer\":%s,\"stream\":%s,\"sd\":%s,\"error_pending\":%s",
           s.speaker ? "true" : "false", s.peer ? "true" : "false", s.stream ? "true" : "false", s.sd ? "true" : "false", s.error_pending ? "true" : "false");
    append(",\"uptime_s\":%llu,\"free_heap\":%lu,\"min_free_heap\":%lu,\"wifi_rssi_dbm\":",
           static_cast<unsigned long long>(s.uptime_s), static_cast<unsigned long>(s.free_heap), static_cast<unsigned long>(s.min_free_heap));
    if (s.wifi_rssi_valid) append("%d", s.wifi_rssi_dbm); else append("null");
    const struct { const char *key; int64_t value; } durations[] = {
        {"control_ready_ms", s.control_ready_ms}, {"sd_mount_ms", s.sd_mount_ms},
        {"sd_config_ms", s.sd_config_ms}, {"speaker_connected_ms", s.speaker_connected_ms}
    };
    for (const auto &entry : durations) {
        append(",\"%s\":", entry.key);
        if (entry.value < 0) append("null");
        else append("%lld", static_cast<long long>(entry.value));
    }
    append(",\"bluetooth_rssi_delta\":");
    if (s.bluetooth_rssi_delta == -999) append("null");
    else append("%lld", static_cast<long long>(s.bluetooth_rssi_delta));
    append(",\"sd_attempts\":%lu,\"sd_failures\":%lu,\"speaker_disconnects\":%lu",
           static_cast<unsigned long>(s.sd_attempts), static_cast<unsigned long>(s.sd_failures),
           static_cast<unsigned long>(s.speaker_disconnects));
    append(",\"jaw_mode\":"); string(s.jaw_mode);
    append(",\"jaw_result\":"); string(s.jaw_result);
    append(",\"jaw_command_us\":%u,\"jaw_applied_us\":%u,\"jaw_closed_us\":%u,\"jaw_open_us\":%u,\"jaw_draft_closed_us\":%u,\"jaw_draft_open_us\":%u,\"jaw_dirty\":%s",
           s.jaw_command_us, s.jaw_applied_us, s.jaw_closed_us, s.jaw_open_us,
           s.jaw_draft_closed_us, s.jaw_draft_open_us, s.jaw_dirty ? "true" : "false");
    // String avoids loss of uint64 report identity in JavaScript consumers.
    append(",\"report_id\":\"%016llx\",\"report_count\":%lu,\"report_slots\":%u}",
           static_cast<unsigned long long>(s.report_id), static_cast<unsigned long>(s.report_count), s.report_slots);
    return finish();
}
}
