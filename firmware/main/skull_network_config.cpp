#include "skull_network_config.h"
#include <cctype>
#include <cstring>

namespace skull::network {
namespace {
constexpr size_t file_limit = 4096;
char *trim(char *text) {
    while (*text && std::isspace(static_cast<unsigned char>(*text))) ++text;
    char *end = text + std::strlen(text);
    while (end > text && std::isspace(static_cast<unsigned char>(end[-1]))) --end;
    *end = 0;
    return text;
}
bool copy(char *dest, size_t capacity, const char *source) {
    const size_t size = std::strlen(source);
    if (size >= capacity) return false;
    std::memcpy(dest, source, size + 1);
    return true;
}
bool normalize(const char *source, char *dest, size_t capacity) {
    size_t length = 0;
    for (; *source; ++source) {
        const unsigned char c = static_cast<unsigned char>(*source);
        if (c == '_' || c == '-' || c == ' ' || c == '\t') continue;
        if (c < 33 || c > 126 || length + 1 >= capacity) return false;
        dest[length++] = static_cast<char>(std::tolower(c));
    }
    dest[length] = 0;
    return length != 0;
}
bool valid_port(const char *port) {
    if (!*port) return false;
    unsigned number = 0;
    for (; *port; ++port) {
        if (*port < '0' || *port > '9') return false;
        number = number * 10 + static_cast<unsigned>(*port - '0');
        if (number > 65535) return false;
    }
    return number != 0;
}
bool valid_uri(const char *uri) {
    if (!*uri) return true;
    const char *host = nullptr;
    if (!std::strncmp(uri, "mqtt://", 7)) host = uri + 7;
    else if (!std::strncmp(uri, "mqtts://", 8)) host = uri + 8;
    else return false;
    if (!*host) return false;
    const char *end = host;
    if (*host == '[') {
        ++end;
        const char *begin = end;
        bool colon = false;
        for (; *end && *end != ']'; ++end) {
            const unsigned char c = static_cast<unsigned char>(*end);
            if (c == ':') colon = true;
            else if (!std::isxdigit(c) && c != '.') return false;
        }
        if (end == begin || !colon || *end != ']') return false;
        ++end;
    } else {
        for (; *end && *end != ':'; ++end) {
            const unsigned char c = static_cast<unsigned char>(*end);
            if (!std::isalnum(c) && c != '-' && c != '.') return false;
        }
        if (end == host || *host == '-' || *host == '.') return false;
    }
    return !*end || (*end == ':' && valid_port(end + 1));
}
bool line(char *buffer, bool first, Config &candidate) {
    const char *carriage = std::strchr(buffer, '\r');
    if (carriage && carriage[1]) return false;
    char *text = buffer;
    if (first && std::strlen(text) >= 3 &&
        static_cast<unsigned char>(text[0]) == 0xef &&
        static_cast<unsigned char>(text[1]) == 0xbb &&
        static_cast<unsigned char>(text[2]) == 0xbf) text += 3;
    char *key = trim(text);
    if (!*key || *key == '#') return true;
    char *equals = std::strchr(key, '=');
    if (!equals) return false;
    *equals = 0;
    char normalized[80];
    if (!normalize(trim(key), normalized, sizeof(normalized))) return false;
    const char *value = trim(equals + 1);
    if (!std::strcmp(normalized, "ssid") || !std::strcmp(normalized, "wifissid") ||
        !std::strcmp(normalized, "wifinetworkname"))
        return copy(candidate.ssid, sizeof(candidate.ssid), value);
    if (!std::strcmp(normalized, "password") || !std::strcmp(normalized, "wifipassword") ||
        !std::strcmp(normalized, "wifipass") || !std::strcmp(normalized, "wifinetworkpassword"))
        return copy(candidate.password, sizeof(candidate.password), value);
    if (!std::strcmp(normalized, "mqtturi") || !std::strcmp(normalized, "brokeruri"))
        return copy(candidate.mqtt_uri, sizeof(candidate.mqtt_uri), value) && valid_uri(value);
    if (!std::strcmp(normalized, "mqttusername"))
        return copy(candidate.mqtt_username, sizeof(candidate.mqtt_username), value);
    if (!std::strcmp(normalized, "mqttpassword"))
        return copy(candidate.mqtt_password, sizeof(candidate.mqtt_password), value);
    return true;
}
}
bool read_config(FILE *file, Config &config) {
    config = Config{};
    if (!file) return false;
    Config candidate;
    char buffer[256];
    size_t used = 0, total = 0;
    bool first = true;
    int c;
    while ((c = std::fgetc(file)) != EOF) {
        if (++total > file_limit || c == 0 || (c < 32 && c != '\r' && c != '\n' && c != '\t') || c == 127)
            return false;
        if (c == '\n') {
            buffer[used] = 0;
            if (!line(buffer, first, candidate)) return false;
            first = false; used = 0;
        } else {
            if (used + 1 >= sizeof(buffer)) return false;
            buffer[used++] = static_cast<char>(c);
        }
    }
    if (std::ferror(file)) return false;
    if (used) {
        buffer[used] = 0;
        if (!line(buffer, first, candidate)) return false;
    }
    config = candidate;
    return true;
}
}
