#include "skull_config.h"
#include <cctype>
#include <cstring>

namespace skull::storage {
namespace {
char *trim(char *text) {
    while (*text && std::isspace(static_cast<unsigned char>(*text))) ++text;
    char *end = text + std::strlen(text);
    while (end > text && std::isspace(static_cast<unsigned char>(end[-1]))) --end;
    *end = 0;
    return text;
}
bool copy(char *dest, size_t size, const char *source) {
    if (std::strlen(source) >= size) return false;
    std::strcpy(dest, source);
    return true;
}
bool volume(const char *text, unsigned &result) {
    if (!*text) return false;
    unsigned value = 0;
    for (; *text; ++text) {
        if (*text < '0' || *text > '9') return false;
        value = value * 10 + (*text - '0');
        if (value > 100) return false;
    }
    result = value;
    return true;
}
}
bool parse_config(FILE *file, Config &config) {
    config = Config{};
    if (!file) return false;
    Config candidate;
    bool have_role = false, first_line = true;
    char buffer[256];
    while (std::fgets(buffer, sizeof buffer, file)) {
        if (!std::strchr(buffer, '\n') && !std::feof(file)) return false;
        char *line = buffer;
        if (first_line && std::strlen(line) >= 3 &&
            static_cast<unsigned char>(line[0]) == 0xef &&
            static_cast<unsigned char>(line[1]) == 0xbb &&
            static_cast<unsigned char>(line[2]) == 0xbf) line += 3;
        first_line = false;
        char *key = trim(line);
        if (!*key || *key == '#') continue;
        char *equals = std::strchr(key, '=');
        if (!equals) return false;
        *equals = 0;
        char *value = trim(equals + 1);
        key = trim(key);
        if (!*key) return false;
        if (!std::strcmp(key, "speaker_name")) {
            if (!copy(candidate.speaker_name, sizeof candidate.speaker_name, value)) return false;
        } else if (!std::strcmp(key, "role")) {
            if (std::strcmp(value, "primary") && std::strcmp(value, "secondary")) return false;
            candidate.primary = !std::strcmp(value, "primary");
            have_role = true;
        } else if (!std::strcmp(key, "secondary_mac_address")) {
            if (!copy(candidate.secondary_mac, sizeof candidate.secondary_mac, value)) return false;
        } else if (!std::strcmp(key, "speaker_volume")) {
            if (!volume(value, candidate.volume_percent)) return false;
        }
        // Preserve legacy extensibility: well-formed unknown keys are ignored.
    }
    if (std::ferror(file) || !have_role || !*candidate.speaker_name) return false;
    config = candidate;
    return true;
}
}
