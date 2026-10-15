#pragma once
#include <cstdio>

namespace skull::storage {
struct Config {
    char speaker_name[80]{};
    char secondary_mac[18]{};
    bool primary = false;
    unsigned volume_percent = 100;
};
// Parses read-only input, requiring a speaker and explicit primary/secondary role.
// Resets output to defaults on every attempt, including failure/null input.
bool parse_config(FILE *file, Config &config);
}
