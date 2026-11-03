#include "skull_network_config.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
using skull::network::Config;
using skull::network::read_config;
namespace {
bool parse(const std::string &text, Config &config) {
    FILE *file = std::tmpfile(); assert(file);
    assert(std::fwrite(text.data(), 1, text.size(), file) == text.size());
    std::rewind(file);
    const bool ok = read_config(file, config);
    // Parser is read-only: bytes on disk remain identical.
    std::rewind(file);
    std::string after(text.size(), '\0');
    assert(std::fread(after.data(), 1, after.size(), file) == after.size());
    assert(after == text);
    assert(std::fclose(file) == 0);
    return ok;
}
void empty(const Config &c) {
    assert(!*c.ssid && !*c.password && !*c.mqtt_uri && !*c.mqtt_username && !*c.mqtt_password);
}
void rejected(const std::string &text, Config &c) { assert(!parse(text, c)); empty(c); }
}
int main() {
    Config c;
    assert(parse("role=secondary\nspeaker_name=JBL Flip 6\nspeaker_volume=60\n", c)); empty(c);
    assert(parse("", c)); empty(c);
    assert(parse("\xef\xbb\xbf# private credentials\r\n Wi-Fi SSID = Example network \r\n WiFi Password = sample=pass#word \r\n MQTT_URI = mqtt://192.168.1.99:1883 \r\n MQTT-Username = test \r\n mqtt_password = local-secret\r\n", c));
    assert(!std::strcmp(c.ssid, "Example network"));
    assert(!std::strcmp(c.password, "sample=pass#word"));
    assert(!std::strcmp(c.mqtt_uri, "mqtt://192.168.1.99:1883"));
    assert(!std::strcmp(c.mqtt_username, "test") && !std::strcmp(c.mqtt_password, "local-secret"));
    for (const char *ssid : {"ssid", "wifi_ssid", "wifi_network_name"}) {
        assert(parse(std::string(ssid) + "=Example\n", c)); assert(!std::strcmp(c.ssid, "Example"));
    }
    for (const char *password : {"password", "wifi_password", "wifi_pass", "wifi_network_password"}) {
        assert(parse(std::string(password) + "=sample\n", c)); assert(!std::strcmp(c.password, "sample"));
    }
    assert(parse("ssid=" + std::string(32, 's') + "\npassword=" + std::string(64, 'a') + "\nmqtt_username=" + std::string(63, 'u') + "\nmqtt_password=" + std::string(127, 'p'), c));
    assert(std::strlen(c.ssid) == 32 && std::strlen(c.password) == 64);
    assert(std::strlen(c.mqtt_username) == 63 && std::strlen(c.mqtt_password) == 127);
    rejected("ssid=" + std::string(33, 's') + "\n", c);
    rejected("password=" + std::string(65, 'p') + "\n", c);
    rejected("mqtt_username=" + std::string(64, 'u') + "\n", c);
    rejected("mqtt_password=" + std::string(128, 'p') + "\n", c);
    rejected("mqtt_uri=mqtt://" + std::string(192, 'a') + "\n", c);
    for (const char *uri : {"mqtt://broker", "mqtt://broker.local:1883", "mqtts://broker.example:8883", "mqtt://[::1]:1883", "mqtt://[2001:db8::1]"}) {
        assert(parse(std::string("broker_uri=") + uri + "\n", c)); assert(!std::strcmp(c.mqtt_uri, uri));
    }
    for (const char *uri : {"http://broker", "mqtt://", "mqtt://user:pass@broker", "mqtt://broker/path", "mqtt://broker?x", "mqtt://broker#x", "mqtt://broker:", "mqtt://broker:0", "mqtt://broker:65536", "mqtt://broker:99999999999999999999", "mqtt://broker:x", "mqtt://two hosts", "mqtt://[::1", "mqtt://[]", "mqtt://-broker"})
        rejected(std::string("mqtt_uri=") + uri + "\n", c);
    rejected(std::string("ssid=valid\npassword=abc\0def\n", 28), c);
    rejected("ssid=valid\npassword=abc\nnot a config line\n", c);
    rejected("ssid=valid\n=empty-key\n", c);
    rejected("ssid=valid\npassword=abc\rbroken\n", c);
    rejected(std::string(256, '#') + "\n", c);
    std::string maxfile;
    while (maxfile.size() + 2 <= 4096) maxfile += "#\n";
    assert(parse(maxfile, c)); empty(c);
    rejected(maxfile + "#", c);
    assert(parse("ssid=old\nssid=new\nmqtt_uri=\npassword=\n", c));
    assert(!std::strcmp(c.ssid, "new") && !*c.mqtt_uri && !*c.password);
    assert(!read_config(nullptr, c)); empty(c);
    std::puts("Network config read-only/bounds/aliases/URI tests passed.");
}
