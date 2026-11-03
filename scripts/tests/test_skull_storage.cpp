#include "skull_storage.h"
#include "esp_vfs_fat.h"
#include "driver/gpio.h"
#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <string>
#include <iterator>
#ifndef SKULL_STORAGE_ROOT
#error Compile this host test with SKULL_STORAGE_ROOT set to an isolated temporary directory
#endif
namespace fs = std::filesystem;
namespace {
int mounts = 0, unmounts = 0;
esp_err_t mount_result = ESP_FAIL, unmount_result = ESP_OK;
sdmmc_card_t fake_card{{4096, 512}};
void defaults(const skull::storage::Config &config) {
    assert(!*config.speaker_name && !*config.secondary_mac);
    assert(!config.primary && config.volume_percent == 100);
    assert(config.jaw_closed_us == 544 && config.jaw_open_us == 1368);
}
void write_config(const char *text) { std::ofstream(SKULL_STORAGE_ROOT "/config.txt", std::ios::binary) << text; }
std::string read(const char *name = "config.txt") {
    std::ifstream file(std::string(SKULL_STORAGE_ROOT) + "/" + name, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
void clear_update_files() {
    for (const auto *name : {"config.speaker.tmp", "config.speaker.previous", "config.speaker.original",
                           "config.jaw.tmp", "config.jaw.previous", "config.jaw.original"})
        fs::remove_all(std::string(SKULL_STORAGE_ROOT) + "/" + name);
}
}
esp_err_t esp_vfs_fat_sdmmc_mount(const char *root, const sdmmc_host_t *host,
        const sdmmc_slot_config_t *slot, const esp_vfs_fat_sdmmc_mount_config_t *options,
        sdmmc_card_t **card) {
    assert(!std::strcmp(root, SKULL_STORAGE_ROOT));
    assert(!options->format_if_mount_failed);
    assert(host->max_freq_khz == 20000 && slot->width == 1);
    ++mounts;
    if (mount_result == ESP_OK) *card = &fake_card;
    return mount_result;
}
esp_err_t esp_vfs_fat_sdcard_unmount(const char *root, sdmmc_card_t *card) {
    assert(!std::strcmp(root, SKULL_STORAGE_ROOT) && card == &fake_card);
    ++unmounts;
    return unmount_result;
}
int main() {
    using namespace skull::storage;
    fs::create_directories(SKULL_STORAGE_ROOT "/audio");
    Config config;
    assert(set_speaker_name("JBL GO 2") != ESP_OK); // Unmounted never creates files.
    assert(set_jaw_limits(600, 1400) != ESP_OK);
    assert(!fs::exists(SKULL_STORAGE_ROOT "/config.jaw.tmp"));
    assert(!fs::exists(SKULL_STORAGE_ROOT "/config.speaker.tmp"));
    std::strcpy(config.speaker_name, "stale"); config.volume_percent = 2;
    assert(mount_and_read_config(config) == ESP_FAIL && !mounted()); defaults(config);
    assert(gpio_pullup_mask == ((1u << 15) | (1u << 2) | (1u << 13)));
    assert(mounts == 1);
    assert(unmount_for_retry() == ESP_OK && unmounts == 0);
    mount_result = ESP_OK;
    assert(mount_and_read_config(config) == ESP_ERR_NOT_FOUND && mounted()); defaults(config);
    assert(mounts == 2);
    write_config("role=wrong\nspeaker_name=Anthony\nspeaker_volume=20\n");
    assert(mount_and_read_config(config) == ESP_ERR_INVALID_ARG && mounted()); defaults(config);
    assert(mounts == 2);
    write_config("role=primary\nspeaker_name=Anthony\nspeaker_volume=20\nsecondary_mac_address=12:34:56:78:9a:bc\n");
    assert(mount_and_read_config(config) == ESP_OK && mounted());
    assert(config.primary && config.volume_percent == 20 && *config.secondary_mac);
    assert(mounts == 2);
    write_config("role=primary\n");
    assert(mount_and_read_config(config) == ESP_ERR_INVALID_ARG); defaults(config);
    write_config("role=secondary\nspeaker_name=Jeff\n");
    assert(mount_and_read_config(config) == ESP_OK);
    assert(!config.primary && config.volume_percent == 100 && !*config.secondary_mac);
    assert(mounts == 2);
    const std::string original = "\xef\xbb\xbf# Keep comments\r\n role = secondary \r\n speaker_name = JBL Flip 6  \r\n"
                                 "speaker_volume=60\r\nfuture_option=keep=this\r\n"
                                 "secondary_mac_address=12:34:56:78:9a:bc\r\n";
    write_config(original.c_str());
    for (const char *invalid : {static_cast<const char *>(nullptr), "", " GO2", "GO2 ", "GO\n2", "GO\r2", "GO=2", "GO\t2"}) {
        assert(set_speaker_name(invalid) == ESP_ERR_INVALID_ARG);
        assert(read() == original);
    }
    const std::string too_long(80, 'a');
    assert(set_speaker_name(too_long.c_str()) == ESP_ERR_INVALID_ARG && read() == original);
    assert(set_speaker_name("JBL GO 2") == ESP_OK);
    std::string wanted = original;
    wanted.replace(wanted.find("JBL Flip 6"), 10, "JBL GO 2");
    assert(read() == wanted); // BOM, CRLF, whitespace, comment and unknown key preserved.
    assert(read("config.speaker.original") == original);
    assert(!fs::exists(SKULL_STORAGE_ROOT "/config.speaker.tmp"));
    assert(!fs::exists(SKULL_STORAGE_ROOT "/config.speaker.previous"));
    assert(mount_and_read_config(config) == ESP_OK);
    assert(!config.primary && config.volume_percent == 60 && !std::strcmp(config.speaker_name, "JBL GO 2"));
    assert(!std::strcmp(config.secondary_mac, "12:34:56:78:9a:bc"));
    assert(set_speaker_name("JBL GO 2") == ESP_OK && read() == wanted); // Idempotent.
    assert(set_speaker_name("JBL Flip 6") == ESP_OK && read() == original); // Reversal retains original backup.
    assert(read("config.speaker.original") == original);
    for (const char *stale : {"config.speaker.tmp", "config.speaker.previous"}) {
        const auto path = std::string(SKULL_STORAGE_ROOT) + "/" + stale;
        std::ofstream(path) << "retain recovery evidence";
        assert(set_speaker_name("JBL GO 2") != ESP_OK && read() == original);
        assert(read(stale) == "retain recovery evidence");
        fs::remove(path);
    }
    clear_update_files();
    write_config("speaker_name=first\nspeaker_name=\nspeaker_name=last\nrole=secondary");
    assert(set_speaker_name("JBL GO 2") == ESP_OK);
    assert(read() == "speaker_name=JBL GO 2\nspeaker_name=JBL GO 2\nspeaker_name=JBL GO 2\nrole=secondary");
    clear_update_files();
    write_config("role=wrong\nspeaker_name=Jeff\n");
    const auto invalid_original = read();
    assert(set_speaker_name("JBL GO 2") != ESP_OK && read() == invalid_original);
    assert(!fs::exists(SKULL_STORAGE_ROOT "/config.speaker.tmp"));
    // A corrupt/conflicting original backup is not overwritten and cannot replace config.txt.
    write_config(original.c_str());
    std::ofstream(SKULL_STORAGE_ROOT "/config.speaker.original") << "not a valid config";
    assert(set_speaker_name("JBL GO 2") != ESP_OK && read() == original);
    assert(read("config.speaker.original") == "not a valid config");
    clear_update_files();
    const std::string oversized = original + std::string(4096, '#');
    write_config(oversized.c_str());
    assert(set_speaker_name("JBL GO 2") != ESP_OK && read() == oversized);
    assert(!fs::exists(SKULL_STORAGE_ROOT "/config.speaker.tmp"));
    clear_update_files();
    write_config(original.c_str());
    for (const auto limits : {std::pair<unsigned, unsigned>{543, 1368}, {544, 2401}, {600, 600}, {0, 0}}) {
        assert(set_jaw_limits(limits.first, limits.second) == ESP_ERR_INVALID_ARG);
        assert(read() == original);
        assert(!fs::exists(SKULL_STORAGE_ROOT "/config.jaw.tmp"));
    }
    assert(set_jaw_limits(600, 1400) == ESP_OK);
    const std::string with_jaw = original + "jaw_closed_us=600\r\njaw_open_us=1400\r\n";
    assert(read() == with_jaw); // All unrelated bytes preserved, CRLF additions.
    assert(read("config.jaw.original") == original);
    assert(!fs::exists(SKULL_STORAGE_ROOT "/config.jaw.tmp"));
    assert(!fs::exists(SKULL_STORAGE_ROOT "/config.jaw.previous"));
    assert(mount_and_read_config(config) == ESP_OK);
    assert(config.jaw_closed_us == 600 && config.jaw_open_us == 1400);
    assert(!config.primary && config.volume_percent == 60 && !std::strcmp(config.speaker_name, "JBL Flip 6"));
    assert(set_jaw_limits(600, 1400) == ESP_OK && read() == with_jaw);
    assert(set_jaw_limits(2094, 1950) == ESP_OK); // Reversed servo direction.
    assert(mount_and_read_config(config) == ESP_OK);
    assert(config.jaw_closed_us == 2094 && config.jaw_open_us == 1950);
    assert(read("config.jaw.original") == original); // First original never replaced.
    assert(set_speaker_name("JBL GO 2") == ESP_OK);
    assert(mount_and_read_config(config) == ESP_OK);
    assert(config.jaw_closed_us == 2094 && config.jaw_open_us == 1950);
    assert(!std::strcmp(config.speaker_name, "JBL GO 2"));
    clear_update_files();
    const std::string formatted = "\xef\xbb\xbf jaw_closed_us = 600  \r\n# Keep me\r\n"
                                  "role=secondary\r\nspeaker_name=Jeff\r\n jaw_open_us = 1400\t\r\n"
                                  "wifi_password=private=fixture\r\n";
    write_config(formatted.c_str());
    assert(set_jaw_limits(610, 1410) == ESP_OK);
    std::string formatted_wanted = formatted;
    formatted_wanted.replace(formatted_wanted.find("600"), 3, "610");
    formatted_wanted.replace(formatted_wanted.find("1400"), 4, "1410");
    assert(read() == formatted_wanted); // Only numeric values changed.
    clear_update_files();
    const std::string no_final_lf = "role=secondary\nspeaker_name=Jeff";
    write_config(no_final_lf.c_str());
    assert(set_jaw_limits(600, 1400) == ESP_OK);
    assert(read() == no_final_lf + "\njaw_closed_us=600\njaw_open_us=1400\n");
    clear_update_files();
    const std::string one_key = "role=secondary\nspeaker_name=Jeff\njaw_open_us=1400\n";
    write_config(one_key.c_str());
    assert(set_jaw_limits(600, 1410) == ESP_OK);
    assert(read() == "role=secondary\nspeaker_name=Jeff\njaw_open_us=1410\njaw_closed_us=600\n");
    clear_update_files();
    write_config(original.c_str());
    for (const char *stale : {"config.jaw.tmp", "config.jaw.previous", "config.speaker.tmp", "config.speaker.previous"}) {
        const auto path = std::string(SKULL_STORAGE_ROOT) + "/" + stale;
        std::ofstream(path) << "retain recovery evidence";
        assert(set_jaw_limits(600, 1400) != ESP_OK && read() == original);
        assert(set_speaker_name("JBL GO 2") != ESP_OK && read() == original);
        assert(read(stale) == "retain recovery evidence");
        fs::remove(path);
    }
    // Model power loss between the two FAT renames: do not create a default
    // config or silently finish a transaction while the valid old file and new
    // candidate are evidence awaiting explicit recovery.
    std::ofstream(SKULL_STORAGE_ROOT "/config.jaw.tmp", std::ios::binary) << with_jaw;
    fs::rename(SKULL_STORAGE_ROOT "/config.txt", SKULL_STORAGE_ROOT "/config.jaw.previous");
    assert(mount_and_read_config(config) == ESP_ERR_NOT_FOUND); defaults(config);
    assert(set_jaw_limits(610, 1410) != ESP_OK);
    assert(set_speaker_name("JBL GO 2") != ESP_OK);
    assert(!fs::exists(SKULL_STORAGE_ROOT "/config.txt"));
    assert(read("config.jaw.previous") == original && read("config.jaw.tmp") == with_jaw);
    fs::rename(SKULL_STORAGE_ROOT "/config.jaw.previous", SKULL_STORAGE_ROOT "/config.txt");
    fs::remove(SKULL_STORAGE_ROOT "/config.jaw.tmp");
    // A failed save cannot claim either endpoint persisted or destroy evidence.
    std::ofstream(SKULL_STORAGE_ROOT "/config.jaw.original") << "corrupt backup";
    assert(set_jaw_limits(600, 1400) != ESP_OK && read() == original);
    assert(read("config.jaw.original") == "corrupt backup");
    assert(read("config.jaw.tmp") == with_jaw);
    assert(set_jaw_limits(610, 1410) != ESP_OK && read("config.jaw.tmp") == with_jaw);
    clear_update_files();
    for (const char *invalid : {"role=secondary\nspeaker_name=Jeff\njaw_open_us=1400\njaw_open_us=1500\n",
                                "role=secondary\nspeaker_name=Jeff\njaw_closed_us=0\n"}) {
        write_config(invalid);
        assert(set_jaw_limits(600, 1400) != ESP_OK && read() == invalid);
        assert(!fs::exists(SKULL_STORAGE_ROOT "/config.jaw.tmp"));
    }
    // Appending missing keys must refuse a bounded config with no remaining room.
    // Use multiple short comment lines so the parser remains valid.
    std::string bounded = "role=secondary\nspeaker_name=Jeff\n";
    while (bounded.size() + 201 <= 4070) bounded += std::string(200, '#') + "\n";
    bounded += "#" + std::string(4070 - bounded.size() - 2, 'x') + "\n";
    assert(bounded.size() == 4070);
    write_config(bounded.c_str());
    assert(set_jaw_limits(600, 1400) != ESP_OK && read() == bounded);
    assert(!fs::exists(SKULL_STORAGE_ROOT "/config.jaw.tmp"));
    clear_update_files();
    write_config("role=secondary\nspeaker_name=Jeff\n");
    unmount_result = ESP_FAIL;
    assert(unmount_for_retry() == ESP_FAIL && mounted());
    assert(mount_and_read_config(config) == ESP_OK && mounts == 2);
    unmount_result = ESP_OK;
    assert(unmount_for_retry() == ESP_OK && !mounted());
    assert(unmounts == 2);
    assert(mount_and_read_config(config) == ESP_OK && mounted() && mounts == 3);
    assert(unmount_for_retry() == ESP_OK && !mounted());
    std::puts("storage recovery tests passed");
}
