#pragma once
#include <cstdio>

namespace skull::storage {
struct Config {
    char speaker_name[80]{};
    char secondary_mac[18]{};
    bool primary = false;
    unsigned volume_percent = 100;
    // Commanded PWM pulse widths, not measured jaw angles. Reversed servo
    // orientation is supported; legacy cards keep the previous mapping.
    unsigned jaw_closed_us = 544;
    unsigned jaw_open_us = 1368;
};
// Electrical command bounds only; these do not establish safe linkage travel.
bool valid_jaw_limits(unsigned closed_us, unsigned open_us);
// Parses read-only input, requiring a speaker and explicit primary/secondary role.
// Resets output to defaults on every attempt, including failure/null input.
bool parse_config(FILE *file, Config &config);
}
