#include "skull_storage.h"
#include <cstdio>
#include <cstring>
#include <strings.h>
#include <dirent.h>
#include "driver/gpio.h"
#include "driver/sdmmc_host.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

#ifndef SKULL_STORAGE_ROOT
#define SKULL_STORAGE_ROOT "/sdcard"
#endif

namespace skull::storage {
namespace {
constexpr char tag[] = "skull_sd";
bool is_mounted = false;
sdmmc_card_t *mounted_card = nullptr;

}

bool mounted() { return is_mounted; }
esp_err_t unmount_for_retry() {
    if (!is_mounted) return ESP_OK;
    const esp_err_t result = esp_vfs_fat_sdcard_unmount(SKULL_STORAGE_ROOT, mounted_card);
    if (result == ESP_OK) { is_mounted = false; mounted_card = nullptr; }
    return result;
}

esp_err_t mount_and_read_config(Config &config) {
    config = Config{};
    if (!is_mounted) {
        sdmmc_host_t host = SDMMC_HOST_DEFAULT();
        host.max_freq_khz = 20000;
        sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
        slot.width = 1;
        slot.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;
        gpio_set_pull_mode(GPIO_NUM_15, GPIO_PULLUP_ONLY);
        gpio_set_pull_mode(GPIO_NUM_2, GPIO_PULLUP_ONLY);
        esp_vfs_fat_sdmmc_mount_config_t mount{};
        mount.format_if_mount_failed = false;
        mount.max_files = 5;
        mount.allocation_unit_size = 16 * 1024;
        const esp_err_t result = esp_vfs_fat_sdmmc_mount(SKULL_STORAGE_ROOT, &host, &slot, &mount, &mounted_card);
        if (result != ESP_OK) {
            ESP_LOGE(tag, "Onboard SDMMC 1-bit 20 MHz mount failed: %s (no format)", esp_err_to_name(result));
            return result;
        }
        is_mounted = true;
        ESP_LOGI(tag, "Onboard SDMMC mounted, capacity=%llu MB",
                 static_cast<unsigned long long>(mounted_card->csd.capacity) * mounted_card->csd.sector_size / (1024 * 1024));
    }
    FILE *file = std::fopen(SKULL_STORAGE_ROOT "/config.txt", "r");
    if (!file) return ESP_ERR_NOT_FOUND;
    const bool valid = parse_config(file, config);
    std::fclose(file);
    if (!valid) {
        ESP_LOGE(tag, "Valid primary/secondary role and speaker_name required in config.txt");
        return ESP_ERR_INVALID_ARG;
    }
    ESP_LOGI(tag, "Card config role=%s speaker='%s' volume=%u%%",
             config.primary ? "primary" : "secondary", config.speaker_name, config.volume_percent);
    static bool inventory_logged = false;
    if (inventory_logged) return ESP_OK;
    DIR *directory = opendir(SKULL_STORAGE_ROOT "/audio");
    unsigned wav_count = 0, script_count = 0;
    if (directory) {
        while (const dirent *entry = readdir(directory)) {
            if (entry->d_name[0] == '.') continue; // macOS AppleDouble metadata is not an audio asset.
            const char *extension = std::strrchr(entry->d_name, '.');
            if (extension && !strcasecmp(extension, ".wav")) ++wav_count;
            if (extension && !strcasecmp(extension, ".txt")) ++script_count;
        }
        closedir(directory);
    }
    ESP_LOGI(tag, "Read-only content inventory: %u WAVs, %u scripts", wav_count, script_count);
    inventory_logged = true;
    return ESP_OK;
}
}
