#pragma once
#include <cstdio>

namespace skull::network {
struct Config {
    char ssid[33]{};
    char password[65]{};
    char mqtt_uri[192]{};
    char mqtt_username[64]{};
    char mqtt_password[128]{};
};
// Read the existing config.txt format without changing the file. Network fields
// are optional; legacy role/speaker keys and other well-formed keys are ignored.
// Keys are ASCII case-insensitive, ignoring underscores, hyphens and spaces.
// Supports ssid/wifi_ssid/wifi_network_name; password/wifi_password/wifi_pass/
// wifi_network_password; mqtt_uri/broker_uri; mqtt_username; mqtt_password.
// Values are trimmed like the legacy config parser (no quote/escape syntax).
// Limits: 4096 input bytes, 255 bytes per line. Failure resets every output field.
// Never logs values. A true result with no SSID leaves network setup disabled.
// MQTT URI, when supplied, must be mqtt[s]://host[:port], without userinfo/path.
bool read_config(FILE *file, Config &config);
}
