#include "skull_config.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
using skull::storage::Config;
using skull::storage::parse_config;
bool parse(const std::string &text, Config &config) {
    FILE *file = std::tmpfile();
    assert(file);
    assert(std::fwrite(text.data(), 1, text.size(), file) == text.size());
    std::rewind(file);
    const bool result = parse_config(file, config);
    std::fclose(file);
    return result;
}
void defaults(const Config &config) {
    assert(!*config.speaker_name && !*config.secondary_mac);
    assert(!config.primary && config.volume_percent == 100);
}
int main() {
    Config config;
    defaults(config);
    assert(parse("\xef\xbb\xbf# comment\r\n role = primary \r\n speaker_name = JBL Flip 5 \r\nsecondary_mac_address=12:34:56:78:9A:BC\nspeaker_volume=80\nignored_future_key = yes\n", config));
    assert(config.primary && !std::strcmp(config.speaker_name, "JBL Flip 5"));
    assert(!std::strcmp(config.secondary_mac, "12:34:56:78:9A:BC") && config.volume_percent == 80);
    assert(parse("role=secondary\nspeaker_name=Jeff", config));
    assert(!config.primary && config.volume_percent == 100 && !*config.secondary_mac);
    for (const auto &bad : {
            std::string("speaker_name=JBL\n"), std::string("role=unknown\nspeaker_name=JBL\n"),
            std::string("role=Primary\nspeaker_name=JBL\n"), std::string("role=primary\n"),
            std::string("role=secondary\nspeaker_name= \n"), std::string("broken line\n"),
            std::string("=missing-key\n"), std::string(300, 'a') + "\n",
            "role=primary\nspeaker_name=" + std::string(80, 'a') + "\n",
            "role=primary\nspeaker_name=JBL\nsecondary_mac_address=" + std::string(18, 'a') + "\n"}) {
        assert(!parse(bad, config)); defaults(config);
    }
    for (const char *bad : {"-1", "+1", "101", "1.0", "1x", "", "4294967296000000", "1 0"}) {
        assert(!parse(std::string("role=primary\nspeaker_name=JBL\nspeaker_volume=") + bad + "\n", config));
        defaults(config);
    }
    for (const char *valid : {"0", "100", "0010"}) {
        assert(parse(std::string("role=primary\nspeaker_name=JBL\nspeaker_volume=") + valid + "\n", config));
    }
    assert(!parse_config(nullptr, config)); defaults(config);
    assert(parse("role=primary\nspeaker_name=Anthony\nspeaker_volume=20\n", config));
    assert(!parse("role=primary\n", config)); defaults(config);
    assert(parse("role=secondary\nspeaker_name=Jeff\n", config));
    assert(config.volume_percent == 100 && !config.primary && !*config.secondary_mac);
    std::puts("config tests passed");
}
