#include "skull_storage.h"
#include <cstdio>
#include <cctype>
#include <cerrno>
#include <fcntl.h>
#include <memory>
#include <new>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>
#include <strings.h>
#include <dirent.h>
#include "driver/gpio.h"
#include "driver/sdmmc_host.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#ifdef ESP_PLATFORM
#include "esp_timer.h"
#include "sd_protocol_defs.h"
#endif

#ifndef SKULL_STORAGE_ROOT
#define SKULL_STORAGE_ROOT "/sdcard"
#endif

namespace skull::storage {
namespace {
constexpr char tag[] = "skull_sd";
bool is_mounted = false;
sdmmc_card_t *mounted_card = nullptr;
constexpr size_t config_limit = 4096;
constexpr char config_path[] = SKULL_STORAGE_ROOT "/config.txt";
constexpr char temp_path[] = SKULL_STORAGE_ROOT "/config.speaker.tmp";
constexpr char previous_path[] = SKULL_STORAGE_ROOT "/config.speaker.previous";
constexpr char original_path[] = SKULL_STORAGE_ROOT "/config.speaker.original";
constexpr char jaw_temp_path[] = SKULL_STORAGE_ROOT "/config.jaw.tmp";
constexpr char jaw_previous_path[] = SKULL_STORAGE_ROOT "/config.jaw.previous";
constexpr char jaw_original_path[] = SKULL_STORAGE_ROOT "/config.jaw.original";
#ifdef ESP_PLATFORM
unsigned mount_attempts = 0, trace_commands = 0;
bool abort_before_init = false;
uint32_t last_ocr = 0, ocr_polls = 0;
esp_err_t last_ocr_error = ESP_OK;
esp_err_t traced_transaction(int slot, sdmmc_command_t *command) {
    if (abort_before_init) {
        abort_before_init = false;
        sdmmc_command_t stop{};
        stop.opcode = MMC_STOP_TRANSMISSION;
        stop.flags = SCF_CMD_AC | SCF_RSP_R1B;
        stop.timeout_ms = 250;
        const esp_err_t abort = sdmmc_host_do_transaction(slot, &stop);
        ESP_LOGW(tag, "SD recovery CMD12 result=%s cmd_error=%s response=%08lx D0=%d D3=%d",
                 esp_err_to_name(abort), esp_err_to_name(stop.error),
                 static_cast<unsigned long>(stop.response[0]), gpio_get_level(GPIO_NUM_2), gpio_get_level(GPIO_NUM_13));
    }
    const int64_t started = esp_timer_get_time();
    const esp_err_t result = sdmmc_host_do_transaction(slot, command);
    if (command->opcode == SD_APP_OP_COND) {
        last_ocr = command->response[0];
        ++ocr_polls;
        last_ocr_error = result == ESP_OK ? command->error : result;
    }
    if (trace_commands) {
        --trace_commands;
        ESP_LOGI(tag, "SD trace CMD%lu arg=%08lx result=%s cmd_error=%s response=%08lx bytes=%u us=%lld CMD=%d D0=%d D3=%d",
                 static_cast<unsigned long>(command->opcode), static_cast<unsigned long>(command->arg),
                 esp_err_to_name(result), esp_err_to_name(command->error),
                 static_cast<unsigned long>(command->response[0]), static_cast<unsigned>(command->datalen),
                 static_cast<long long>(esp_timer_get_time() - started), gpio_get_level(GPIO_NUM_15), gpio_get_level(GPIO_NUM_2), gpio_get_level(GPIO_NUM_13));
    }
    return result;
}
#endif

bool exists(const char *path) {
    struct stat info{};
    return stat(path, &info) == 0 || errno != ENOENT;
}
bool same_settings(const Config &a, const Config &b) {
    return a.primary == b.primary && a.volume_percent == b.volume_percent &&
           a.jaw_closed_us == b.jaw_closed_us && a.jaw_open_us == b.jaw_open_us &&
           !std::strcmp(a.secondary_mac, b.secondary_mac);
}
bool same_config(const Config &a, const Config &b) {
    return same_settings(a, b) && !std::strcmp(a.speaker_name, b.speaker_name);
}
bool unresolved_update() {
    return exists(temp_path) || exists(previous_path) ||
           exists(jaw_temp_path) || exists(jaw_previous_path);
}
bool read_config_bytes(const char *path, char *bytes, size_t &length, Config &config) {
    FILE *file = std::fopen(path, "rb");
    if (!file) return false;
    length = std::fread(bytes, 1, config_limit + 1, file);
    bool ok = !std::ferror(file) && length <= config_limit &&
              !std::memchr(bytes, 0, length);
    if (ok) {
        bytes[length] = 0;
        std::rewind(file);
        ok = parse_config(file, config);
    }
    if (std::fclose(file) != 0) ok = false;
    return ok;
}
bool verify_bytes(const char *path, const char *bytes, size_t length, Config &config) {
    FILE *file = std::fopen(path, "rb");
    if (!file) return false;
    char chunk[128];
    size_t offset = 0;
    bool ok = true;
    while (ok && offset < length) {
        const size_t count = length - offset < sizeof(chunk) ? length - offset : sizeof(chunk);
        ok = std::fread(chunk, 1, count, file) == count &&
             !std::memcmp(chunk, bytes + offset, count);
        offset += count;
    }
    if (ok) ok = std::fgetc(file) == EOF && !std::ferror(file);
    if (ok) { std::rewind(file); ok = parse_config(file, config); }
    if (std::fclose(file) != 0) ok = false;
    return ok;
}
// Exclusive creation never overwrites an old recovery/staging file. On failure
// retain the partial file as evidence; a later command refuses to overwrite it.
bool write_new(const char *path, const char *bytes, size_t length) {
    const int fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0666);
    if (fd < 0) return false;
    FILE *file = fdopen(fd, "wb");
    if (!file) { close(fd); return false; }
    bool ok = std::fwrite(bytes, 1, length, file) == length;
    if (std::fflush(file) != 0) ok = false;
    if (ok && fsync(fileno(file)) != 0) ok = false;
    if (std::fclose(file) != 0) ok = false;
    return ok;
}
bool append(char *dest, size_t &used, const char *text, size_t length) {
    if (length > config_limit - used) return false;
    std::memcpy(dest + used, text, length);
    used += length;
    dest[used] = 0;
    return true;
}

// Every writer shares a validated, byte-exact readback/rollback path. FAT has
// no overwrite-rename: originals and staging files are retained on failure.
esp_err_t install_config(char *original, size_t original_length,
        const char *candidate, size_t candidate_length, const Config &expected,
        const char *staging, const char *previous, const char *backup) {
    if (candidate_length == original_length &&
        !std::memcmp(original, candidate, original_length)) return ESP_OK;
    Config after{};
    if (!write_new(staging, candidate, candidate_length) ||
        !verify_bytes(staging, candidate, candidate_length, after) ||
        !same_config(expected, after)) return ESP_FAIL;
    if (!exists(backup)) {
        if (!write_new(backup, original, original_length) ||
            !verify_bytes(backup, original, original_length, after)) return ESP_FAIL;
    } else {
        // A first valid original remains untouched across later calibrations.
        size_t backup_length = 0;
        if (!read_config_bytes(backup, original, backup_length, after)) return ESP_FAIL;
    }
    if (std::rename(config_path, previous) != 0) return ESP_FAIL;
    if (std::rename(staging, config_path) != 0) {
        if (std::rename(previous, config_path) != 0)
            ESP_LOGE(tag, "Config update rollback failed: preserve recovery files");
        return ESP_FAIL;
    }
    if (!verify_bytes(config_path, candidate, candidate_length, after) ||
        !same_config(expected, after)) {
        if (std::rename(config_path, staging) != 0 || std::rename(previous, config_path) != 0)
            ESP_LOGE(tag, "Config verification/rollback failed: preserve recovery files");
        return ESP_FAIL;
    }
    if (unlink(previous) != 0)
        ESP_LOGE(tag, "Config saved, but previous file retained; recover before another update");
    return ESP_OK;
}

}

bool mounted() { return is_mounted; }
esp_err_t unmount_for_retry() {
    if (!is_mounted) return ESP_OK;
    const esp_err_t result = esp_vfs_fat_sdcard_unmount(SKULL_STORAGE_ROOT, mounted_card);
    if (result == ESP_OK) { is_mounted = false; mounted_card = nullptr; }
    return result;
}

esp_err_t set_speaker_name(const char *name) {
    if (!is_mounted) return ESP_FAIL;
    if (!name) return ESP_ERR_INVALID_ARG;
    const size_t name_length = std::strlen(name);
    if (!name_length || name_length >= sizeof(Config{}.speaker_name) ||
        std::isspace(static_cast<unsigned char>(name[0])) ||
        std::isspace(static_cast<unsigned char>(name[name_length - 1]))) return ESP_ERR_INVALID_ARG;
    for (size_t i = 0; i < name_length; ++i) {
        const auto value = static_cast<unsigned char>(name[i]);
        if (value < 32 || value == 127 || value == '=') return ESP_ERR_INVALID_ARG;
    }
    if (unresolved_update()) {
        ESP_LOGE(tag, "Speaker update refused: retain/recover config staging files first");
        return ESP_FAIL;
    }
    // Heap buffers keep this maintenance operation off the small control stack.
    std::unique_ptr<char[]> original(new(std::nothrow) char[config_limit + 1]);
    std::unique_ptr<char[]> candidate(new(std::nothrow) char[config_limit + 1]);
    if (!original || !candidate) return ESP_FAIL;
    size_t length = 0, used = 0;
    Config before{};
    if (!read_config_bytes(config_path, original.get(), length, before)) return ESP_FAIL;
    bool replaced = false;
    for (size_t offset = 0; offset < length;) {
        size_t end = offset;
        while (end < length && original[end] != '\n') ++end;
        if (end < length) ++end;
        size_t key = offset;
        if (!offset && length >= 3 &&
            static_cast<unsigned char>(original[0]) == 0xef &&
            static_cast<unsigned char>(original[1]) == 0xbb &&
            static_cast<unsigned char>(original[2]) == 0xbf) key += 3;
        while (key < end && std::isspace(static_cast<unsigned char>(original[key]))) ++key;
        size_t equals = key;
        while (equals < end && original[equals] != '=') ++equals;
        size_t key_end = equals;
        while (key_end > key && std::isspace(static_cast<unsigned char>(original[key_end - 1]))) --key_end;
        if (equals < end && key_end - key == 12 && !std::memcmp(original.get() + key, "speaker_name", 12)) {
            size_t value = equals + 1, value_end = end;
            while (value_end > equals + 1 && std::isspace(static_cast<unsigned char>(original[value_end - 1]))) --value_end;
            while (value < value_end && std::isspace(static_cast<unsigned char>(original[value]))) ++value;
            if (!append(candidate.get(), used, original.get() + offset, value - offset) ||
                !append(candidate.get(), used, name, name_length) ||
                !append(candidate.get(), used, original.get() + value_end, end - value_end)) return ESP_FAIL;
            replaced = true;
        } else if (!append(candidate.get(), used, original.get() + offset, end - offset)) return ESP_FAIL;
        offset = end;
    }
    if (!replaced) return ESP_FAIL;
    Config expected = before;
    std::strcpy(expected.speaker_name, name);
    const auto result = install_config(original.get(), length, candidate.get(), used,
                                       expected, temp_path, previous_path, original_path);
    if (result == ESP_OK) ESP_LOGI(tag, "SD speaker name saved and verified; takes effect after reboot");
    return result;
}

esp_err_t set_jaw_limits(unsigned closed_us, unsigned open_us) {
    if (!is_mounted) return ESP_FAIL;
    if (!valid_jaw_limits(closed_us, open_us)) return ESP_ERR_INVALID_ARG;
    if (unresolved_update()) {
        ESP_LOGE(tag, "Jaw update refused: retain/recover config staging files first");
        return ESP_FAIL;
    }
    std::unique_ptr<char[]> original(new(std::nothrow) char[config_limit + 1]);
    std::unique_ptr<char[]> candidate(new(std::nothrow) char[config_limit + 1]);
    if (!original || !candidate) return ESP_FAIL;
    size_t length = 0, used = 0;
    Config before{};
    if (!read_config_bytes(config_path, original.get(), length, before)) return ESP_FAIL;
    const char *keys[] = {"jaw_closed_us", "jaw_open_us"};
    char values[2][12];
    std::snprintf(values[0], sizeof values[0], "%u", closed_us);
    std::snprintf(values[1], sizeof values[1], "%u", open_us);
    bool replaced[2] = {};
    for (size_t offset = 0; offset < length;) {
        size_t end = offset;
        while (end < length && original[end] != '\n') ++end;
        if (end < length) ++end;
        size_t key = offset;
        if (!offset && length >= 3 &&
            static_cast<unsigned char>(original[0]) == 0xef &&
            static_cast<unsigned char>(original[1]) == 0xbb &&
            static_cast<unsigned char>(original[2]) == 0xbf) key += 3;
        while (key < end && std::isspace(static_cast<unsigned char>(original[key]))) ++key;
        size_t equals = key;
        while (equals < end && original[equals] != '=') ++equals;
        size_t key_end = equals;
        while (key_end > key && std::isspace(static_cast<unsigned char>(original[key_end - 1]))) --key_end;
        int found = -1;
        for (int i = 0; i < 2; ++i) {
            if (equals < end && key_end - key == std::strlen(keys[i]) &&
                !std::memcmp(original.get() + key, keys[i], key_end - key)) found = i;
        }
        if (found >= 0) {
            size_t value = equals + 1, value_end = end;
            while (value_end > equals + 1 && std::isspace(static_cast<unsigned char>(original[value_end - 1]))) --value_end;
            while (value < value_end && std::isspace(static_cast<unsigned char>(original[value]))) ++value;
            if (!append(candidate.get(), used, original.get() + offset, value - offset) ||
                !append(candidate.get(), used, values[found], std::strlen(values[found])) ||
                !append(candidate.get(), used, original.get() + value_end, end - value_end)) return ESP_FAIL;
            replaced[found] = true;
        } else if (!append(candidate.get(), used, original.get() + offset, end - offset)) return ESP_FAIL;
        offset = end;
    }
    // Match the existing first newline style while leaving original bytes intact.
    const char *newline = "\n";
    const char *first_lf = std::strchr(original.get(), '\n');
    if (first_lf && first_lf > original.get() && first_lf[-1] == '\r') newline = "\r\n";
    for (int i = 0; i < 2; ++i) {
        if (replaced[i]) continue;
        if (used && candidate[used - 1] != '\n' && !append(candidate.get(), used, newline, std::strlen(newline))) return ESP_FAIL;
        if (!append(candidate.get(), used, keys[i], std::strlen(keys[i])) ||
            !append(candidate.get(), used, "=", 1) ||
            !append(candidate.get(), used, values[i], std::strlen(values[i])) ||
            !append(candidate.get(), used, newline, std::strlen(newline))) return ESP_FAIL;
    }
    Config expected = before;
    expected.jaw_closed_us = closed_us;
    expected.jaw_open_us = open_us;
    const auto result = install_config(original.get(), length, candidate.get(), used,
                                       expected, jaw_temp_path, jaw_previous_path, jaw_original_path);
    if (result == ESP_OK) ESP_LOGI(tag, "SD jaw limits saved and verified: closed=%u us open=%u us", closed_us, open_us);
    return result;
}

esp_err_t mount_and_read_config(Config &config) {
    config = Config{};
    if (!is_mounted) {
        sdmmc_host_t host = SDMMC_HOST_DEFAULT();
        host.max_freq_khz = 20000;
#ifdef ESP_PLATFORM
        trace_commands = ++mount_attempts <= 2 ? 8 : 0;
        last_ocr = ocr_polls = 0;
        last_ocr_error = ESP_OK;
        // One bounded read-abort before the second attempt; no repeated aborts.
        abort_before_init = mount_attempts == 2;
        host.do_transaction = traced_transaction;
        // DAT3 must remain high in 1-bit mode to prevent SPI mode selection.
        // Do not change GPIO12's boot-strapping pull state.
        gpio_input_enable(GPIO_NUM_13);
#endif
        sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
        slot.width = 1;
        slot.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;
        gpio_set_pull_mode(GPIO_NUM_15, GPIO_PULLUP_ONLY);
        gpio_set_pull_mode(GPIO_NUM_2, GPIO_PULLUP_ONLY);
        gpio_set_pull_mode(GPIO_NUM_13, GPIO_PULLUP_ONLY);
        esp_vfs_fat_sdmmc_mount_config_t mount{};
        mount.format_if_mount_failed = false;
        mount.max_files = 5;
        mount.allocation_unit_size = 16 * 1024;
#ifdef ESP_PLATFORM
        const int64_t mount_started = esp_timer_get_time();
#endif
        const esp_err_t result = esp_vfs_fat_sdmmc_mount(SKULL_STORAGE_ROOT, &host, &slot, &mount, &mounted_card);
#ifdef ESP_PLATFORM
        ESP_LOGI("lifecycle", "event=sd_mount_result attempt=%u duration_ms=%lld uptime_ms=%lld result=%s",
                 mount_attempts, static_cast<long long>((esp_timer_get_time()-mount_started)/1000),
                 static_cast<long long>(esp_timer_get_time()/1000), esp_err_to_name(result));
#endif
        if (result != ESP_OK) {
#ifdef ESP_PLATFORM
            ESP_LOGW(tag, "SD startup attempt=%u ACMD41 polls=%lu last_OCR=%08lx ready=%d error=%s DAT3=%d",
                     mount_attempts, static_cast<unsigned long>(ocr_polls), static_cast<unsigned long>(last_ocr),
                     !!(last_ocr & (1ul << 31)), esp_err_to_name(last_ocr_error), gpio_get_level(GPIO_NUM_13));
#endif
            ESP_LOGE(tag, "Onboard SDMMC 1-bit 20 MHz mount failed: %s (no format)", esp_err_to_name(result));
            return result;
        }
        is_mounted = true;
#ifdef ESP_PLATFORM
        // Diagnostics are boot-only; leave streaming reads on the SDK path.
        mounted_card->host.do_transaction = sdmmc_host_do_transaction;
        trace_commands = 0;
#endif
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
