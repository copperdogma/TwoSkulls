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
}
void write_config(const char *text) { std::ofstream(SKULL_STORAGE_ROOT "/config.txt", std::ios::binary) << text; }
std::string read(const char *name = "config.txt") {
    std::ifstream file(std::string(SKULL_STORAGE_ROOT) + "/" + name, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
void clear_update_files() {
    for (const auto *name : {"config.speaker.tmp", "config.speaker.previous", "config.speaker.original"})
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
