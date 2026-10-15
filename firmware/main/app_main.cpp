#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <new>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_psram.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include <esp_matter.h>
#include <app/server/Server.h>
#include <platform/CHIPDeviceLayer.h>
#include <setup_payload/OnboardingCodesUtil.h>
#include "skull_radio.h"
#include "skull_storage.h"
#include "skull_startup.h"
#include "skull_actuators.h"
#include "skull_audio.h"
#include "skull_catalog.h"
#include "skull_show.h"
#include "skull_secondary.h"

using namespace esp_matter;
using namespace chip::app::Clusters;
namespace {
const char *tag = "skull"; // Set once from validated SD config before starting tasks.
std::atomic<uint32_t> tone_frames{0};
std::atomic<uint32_t> callback_max_us{0};
std::atomic<uint32_t> triggers{0};
std::atomic<bool> matter_ble_ready{false};
uint16_t trigger_endpoint = 0;
bool internal_update = false; // Accessed only on Matter's system layer.
skull::storage::Config card_config;
skull::catalog::Catalog *assets = nullptr;
bool heap_window = false; // Diagnostic task only.

// Startup PCM has priority over the explicit diagnostic tone.
// No card I/O, logging, allocation or blocking in this provider.
int32_t provide_pcm(uint8_t *data, int32_t length) {
    if (!data || length <= 0) return 0;
    const int64_t started = esp_timer_get_time();
    std::memset(data, 0, static_cast<size_t>(length));
    const bool startup = skull::startup::provide(data, length);
    const bool show_audio = !startup && skull::audio::provide(data, length);
    const uint32_t remaining = tone_frames.load(std::memory_order_relaxed);
    const uint32_t count = startup || show_audio ? 0 : std::min(static_cast<uint32_t>(length / 4), remaining);
    static uint32_t phase = 0;
    for (uint32_t index = 0; index < count; ++index) {
        const int16_t sample = static_cast<int16_t>((phase < 50 ? static_cast<int>(phase) - 25 : 75 - static_cast<int>(phase)) * 20);
        std::memcpy(data + index * 4, &sample, 2);
        std::memcpy(data + index * 4 + 2, &sample, 2);
        phase = (phase + 1) % 100;
    }
    // Only the provider decrements. Trigger requests are accepted only at zero.
    if (count) tone_frames.fetch_sub(count, std::memory_order_relaxed);
    const uint32_t elapsed = static_cast<uint32_t>(esp_timer_get_time() - started);
    uint32_t maximum = callback_max_us.load(std::memory_order_relaxed);
    while (elapsed > maximum && !callback_max_us.compare_exchange_weak(maximum, elapsed, std::memory_order_relaxed)) {}
    return length;
}

bool request_tone() {
    if (!skull::radio::stream_ready() || skull::startup::busy() || skull::audio::busy() || skull::show::busy()) return false;
    uint32_t expected = 0;
    if (!tone_frames.compare_exchange_strong(expected, 13230)) return false;
    triggers.fetch_add(1, std::memory_order_relaxed);
    return true;
}

void reset_momentary(chip::System::Layer *, void *) {
    internal_update = true;
    auto off = esp_matter_bool(false);
    const esp_err_t result = attribute::update(trigger_endpoint, OnOff::Id, OnOff::Attributes::OnOff::Id, &off);
    internal_update = false;
    if (result != ESP_OK && result != ESP_ERR_NOT_FINISHED)
        ESP_LOGW(tag, "Momentary Off reset: %s", esp_err_to_name(result));
}

esp_err_t attribute_update(attribute::callback_type_t type, uint16_t endpoint, uint32_t cluster,
                           uint32_t attribute_id, esp_matter_attr_val_t *value, void *) {
    if (type != attribute::PRE_UPDATE || internal_update || endpoint != trigger_endpoint ||
        cluster != OnOff::Id || attribute_id != OnOff::Attributes::OnOff::Id ||
        value->type != ESP_MATTER_VAL_TYPE_BOOLEAN || !value->val.b) return ESP_OK;
    const bool accepted = skull::show::trigger();
    if (accepted) triggers.fetch_add(1, std::memory_order_relaxed);
    ESP_LOGI(tag, "Matter skit trigger %s", accepted ? "accepted" : "ignored (startup/busy/cooldown/speaker/Jeff unavailable)");
    chip::DeviceLayer::SystemLayer().StartTimer(chip::System::Clock::Milliseconds32(300), reset_momentary, nullptr);
    return ESP_OK;
}

esp_err_t identify(identification::callback_type_t, uint16_t, uint8_t, uint8_t, void *) { return ESP_OK; }

void matter_event(const chip::DeviceLayer::ChipDeviceEvent *event, intptr_t) {
    // GAP callback registration is earlier than advertising/address setup.
    // Don't start our scanner until uncommissioned Matter has finished it.
    if (event->Type == chip::DeviceLayer::DeviceEventType::kCHIPoBLEAdvertisingChange &&
        event->CHIPoBLEAdvertisingChange.Result == chip::DeviceLayer::kActivity_Started)
        matter_ble_ready.store(true);
    if (event->Type == chip::DeviceLayer::DeviceEventType::kCommissioningComplete)
        ESP_LOGI(tag, "Matter commissioning complete");
    else if (event->Type == chip::DeviceLayer::DeviceEventType::kBLEDeinitialized)
        ESP_LOGE(tag, "Unexpected Matter BLE deinitialization: retained Bluetooth gate FAILED");
    else if (event->Type == chip::DeviceLayer::DeviceEventType::kFabricRemoved)
        ESP_LOGI(tag, "Matter fabric removed; use explicit commissioning-window command to re-add");
}

void status() {
    const auto radio = skull::radio::stats();
    const auto audio = skull::audio::stats();
    const auto actuators = skull::actuators::stats();
    ESP_LOGI(tag, "ACTUATORS deferred=%lu max_pending_us=%llu pending_channels=%u stuck=%d",
             static_cast<unsigned long>(actuators.deferred_updates),
             static_cast<unsigned long long>(actuators.max_pending_us),
             static_cast<unsigned>(actuators.pending_channels), actuators.stuck);
    ESP_LOGI(tag, "PRODUCER blocks=%lu read_total_us=%lu read_max_us=%lu process_total_us=%lu process_max_us=%lu refill_gap_max_us=%lu role_frames=%lu raw_nonzero=%lu output_nonzero=%lu peak=%lu",
             static_cast<unsigned long>(audio.blocks), static_cast<unsigned long>(audio.read_us_total),
             static_cast<unsigned long>(audio.read_us_max), static_cast<unsigned long>(audio.process_us_total),
             static_cast<unsigned long>(audio.process_us_max), static_cast<unsigned long>(audio.refill_gap_us_max),
             static_cast<unsigned long>(audio.role_frames), static_cast<unsigned long>(audio.input_nonzero_samples),
             static_cast<unsigned long>(audio.output_nonzero_samples), static_cast<unsigned long>(audio.output_peak));
    ESP_LOGI(tag, "SHOW state=%s PCM frames=%lu underruns=%lu producer_stack=%lu cancel_waiting=%d assets=%u heap_window=%d secondary_ready=%d",
             skull::show::state_name(), static_cast<unsigned long>(audio.frames),
             static_cast<unsigned long>(audio.underruns), static_cast<unsigned long>(audio.stack_high_water),
             audio.cancel_waiting, assets ? static_cast<unsigned>(assets->count) : 0,
             heap_window, skull::secondary::ready());
    ESP_LOGI(tag, "STATUS sd=%d speaker=%d peer=%d tone_frames=%lu triggers=%lu callbacks=%lu bytes=%lu cb_max_us=%lu dropped=%lu internal=%u min_internal=%u largest=%u psram=%u stream=%d fault=%d",
             skull::storage::mounted(), skull::radio::speaker_connected(), skull::radio::peer_ready(),
             static_cast<unsigned long>(tone_frames.load()), static_cast<unsigned long>(triggers.load()),
             static_cast<unsigned long>(radio.audio_callbacks), static_cast<unsigned long>(radio.audio_bytes),
             static_cast<unsigned long>(callback_max_us.load()), static_cast<unsigned long>(radio.dropped_events),
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
             static_cast<unsigned>(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
             static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)), skull::radio::stream_ready(), radio.faulted);
    ESP_LOGI(tag, "STACK minimum unused bytes: radio=%lu diagnostic=%u; DMA free=%u",
             static_cast<unsigned long>(radio.stack_free_min),
             static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)),
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL)));
}

void inspect_asset(const char *path) {
    if (!assets) return;
    for (size_t i = 0; i < assets->count; ++i) {
        const auto &asset = assets->assets[i];
        if (std::strcmp(path, asset.path)) continue;
        ESP_LOGI(tag, "ASSET role=%c path=%s frames=%lu lines=%u skit=%d",
                 card_config.primary ? 'A' : 'B', asset.path,
                 static_cast<unsigned long>(asset.wave.data_bytes / 4),
                 static_cast<unsigned>(asset.line_count), asset.skit);
        for (unsigned line = 0; line < asset.line_count; ++line) {
            const auto &interval = asset.lines[line];
            ESP_LOGI(tag, "ASSET interval=%u role=%c frames=%lu..%lu", line, interval.role,
                     static_cast<unsigned long>(interval.first_frame),
                     static_cast<unsigned long>(interval.end_frame));
        }
        return;
    }
    ESP_LOGW(tag, "ASSET not found: %s", path);
}

void pairing() {
    // Called only on Matter's system layer. Do not open an additional fabric
    // window on a commissioned device through this diagnostic command.
    if (chip::Server::GetInstance().GetFabricTable().FabricCount() != 0) {
        ESP_LOGI(tag, "Already commissioned; use Home's existing accessory");
        return;
    }
    auto &manager = chip::Server::GetInstance().GetCommissioningWindowManager();
    if (!manager.IsCommissioningWindowOpen()) {
        const CHIP_ERROR error = manager.OpenBasicCommissioningWindow(chip::System::Clock::Seconds32(300));
        if (error != CHIP_NO_ERROR) {
            ESP_LOGE(tag, "Commissioning window failed: %" CHIP_ERROR_FORMAT, error.Format());
            return;
        }
    }
    PrintOnboardingCodes(chip::RendezvousInformationFlags(chip::RendezvousInformationFlag::kBLE));
}

void diagnostic_task(void *) {
    const int64_t deadline = esp_timer_get_time() + 10000000;
    esp_err_t attached = card_config.primary ? ESP_ERR_NOT_FOUND : skull::radio::attach_standalone_gap();
    while (attached == ESP_ERR_NOT_FOUND && esp_timer_get_time() < deadline) {
        attached = skull::radio::attach_matter_gap();
        if (attached == ESP_ERR_NOT_FOUND) vTaskDelay(pdMS_TO_TICKS(50));
    }
    if (attached != ESP_OK) {
        ESP_LOGE(tag, "Matter GAP dispatcher readiness failed: %s; no peer scans started", esp_err_to_name(attached));
    } else if (card_config.primary) {
        while (!matter_ble_ready.load() && esp_timer_get_time() < deadline)
            vTaskDelay(pdMS_TO_TICKS(50));
        if (!matter_ble_ready.load())
            ESP_LOGE(tag, "Matter advertising/address readiness timed out; discovery gate FAILED");
    }
    if (attached == ESP_OK && (!card_config.primary || matter_ble_ready.load()) && *card_config.speaker_name) {
        skull::radio::Config config{card_config.speaker_name, card_config.secondary_mac, card_config.primary};
        ESP_ERROR_CHECK(skull::radio::start(config));
    }
    ESP_LOGI(tag, "Skull ready. Commands: status, asset /audio/<file>.wav, heap-start, heap-stop, tone, trigger, pair /audio/<file>.wav, play /audio/<file>.wav, jaw-test, retry, pairing, reboot, factory-reset CONFIRM. Matter On requests a paired skit.");
    char line[256]{};
    size_t used = 0;
    int64_t next_status = 0;
    while (true) {
        skull::startup::poll(skull::radio::stream_ready());
        const bool tone_active = tone_frames.load(std::memory_order_relaxed) != 0;
        skull::show::tick(skull::startup::finished(), skull::startup::playing() || tone_active);
        const auto startup_animation = skull::startup::animation();
        const auto show_animation = skull::audio::animation();
        const bool audio_active = skull::radio::stream_ready() &&
            (skull::startup::playing() || tone_active || skull::show::busy() || skull::audio::busy());
        skull::actuators::tick(startup_animation.rms ? startup_animation.rms : show_animation.rms,
                              startup_animation.speaking || show_animation.speaking, audio_active);
        char character;
        if (uart_read_bytes(UART_NUM_0, &character, 1, pdMS_TO_TICKS(20)) == 1) {
            if (character == '\r' || character == '\n') {
                line[used] = '\0';
                if (!std::strcmp(line, "status")) status();
                else if (!std::strncmp(line, "asset ", 6) && !skull::show::busy()) inspect_asset(line + 6);
                else if (!std::strcmp(line, "heap-start") && !heap_window) {
                    heap_window = heap_caps_monitor_local_minimum_free_size_start() == ESP_OK;
                    ESP_LOGI(tag, "Local heap window %s", heap_window ? "started" : "failed");
                    status();
                }
                else if (!std::strcmp(line, "heap-stop") && heap_window) {
                    status(); // Record window minimum before restoring boot history.
                    ESP_LOGI(tag, "Local heap window stop: %s", esp_err_to_name(heap_caps_monitor_local_minimum_free_size_stop()));
                    heap_window = false;
                }
                else if (!std::strcmp(line, "tone")) ESP_LOGI(tag, "Serial tone %s", request_tone() ? "accepted" : "ignored");
                else if (!std::strcmp(line, "trigger")) ESP_LOGI(tag, "Serial paired trigger %s", skull::show::trigger() ? "accepted" : "ignored");
                else if (!std::strncmp(line, "pair ", 5)) ESP_LOGI(tag, "Named paired diagnostic %s", (tone_frames.load() == 0 && skull::show::paired(line + 5)) ? "accepted" : "ignored");
                else if (!std::strncmp(line, "play ", 5)) ESP_LOGI(tag, "Local diagnostic play %s", (tone_frames.load() == 0 && skull::show::local(line + 5)) ? "accepted" : "ignored");
                else if (!std::strcmp(line, "jaw-test") && !skull::startup::busy() && !skull::show::busy() && !skull::audio::busy()) skull::actuators::test();
                else if (!std::strcmp(line, "retry") && tone_frames.load() == 0 && !(skull::startup::busy() && skull::radio::stream_ready()) && ((!skull::show::busy() && !skull::audio::busy()) ||
                         (skull::audio::stats().cancel_waiting && !skull::radio::stream_ready()))) skull::radio::retry_connections();
                else if (!std::strcmp(line, "pairing") && card_config.primary) chip::DeviceLayer::SystemLayer().ScheduleLambda([]() { pairing(); });
                else if (!std::strcmp(line, "reboot")) {
                    // This task also schedules shows/prepare, so an accepted
                    // reboot cannot race a newly launched SD read. Waiting for
                    // initial speaker discovery is safe: startup PCM was loaded
                    // and its FILE closed before this task was created.
                    if (skull::show::busy() || !skull::audio::quiescent() ||
                        skull::startup::playing() || tone_frames.load() != 0)
                        ESP_LOGW(tag, "Reboot refused while show/audio/startup/tone or SD file active; retry when idle");
                    else esp_restart();
                }
                else if (!std::strcmp(line, "factory-reset CONFIRM") && card_config.primary) {
                    chip::DeviceLayer::SystemLayer().ScheduleLambda([]() { esp_matter::factory_reset(); });
                }
                else if (used) ESP_LOGW(tag, "Unknown command");
                used = 0;
            } else if (used < sizeof(line) - 1) line[used++] = character;
            else used = 0;
        }
        if (esp_timer_get_time() >= next_status) {
            status();
            next_status = esp_timer_get_time() + 10000000;
        }
    }
}
}

extern "C" void app_main() {
    ESP_LOGI(tag, "TwoSkulls shared WROVER firmware (onboard SD, A2DP, BLE, Matter)");
    ESP_ERROR_CHECK(skull::actuators::init());
    ESP_ERROR_CHECK(nvs_flash_init()); // Never erase fabric/NVS automatically.
    ESP_LOGI(tag, "PSRAM initialized=%d size=%u", esp_psram_is_initialized(), static_cast<unsigned>(esp_psram_get_size()));
    esp_err_t sd = skull::storage::mount_and_read_config(card_config);
    while (sd != ESP_OK) {
        const unsigned failure_blinks = skull::storage::mounted() ? 5 : 3;
        ESP_LOGW(tag, "Boot storage/config retry: %s; %u eye flashes", esp_err_to_name(sd), failure_blinks);
        skull::actuators::diagnostic(failure_blinks, true);
        const unsigned diagnostic_ticks = (skull::actuators::diagnostic_duration_ms(failure_blinks) + 19) / 20;
        for (unsigned i = 0; i < diagnostic_ticks; ++i) {
            skull::actuators::tick(0, false);
            vTaskDelay(pdMS_TO_TICKS(20));
        }
        // No audio file or producer exists yet; remount permits a repaired or
        // reinserted card to recover without stale host/card state.
        const esp_err_t unmount = skull::storage::unmount_for_retry();
        if (unmount != ESP_OK) ESP_LOGW(tag, "Retry unmount: %s", esp_err_to_name(unmount));
        sd = skull::storage::mount_and_read_config(card_config);
    }
    tag = card_config.primary ? "anthony" : "jeff";
    ESP_LOGI(tag, "Configured role=%s", card_config.primary ? "primary" : "secondary");
    const unsigned blinks = card_config.primary ? 4 : 2;
    skull::actuators::diagnostic(blinks);
    ESP_LOGI(tag, "Boot eye diagnostic: %u flashes", blinks);
    // Finish both the role flashes and gentle check before lengthy radio setup.
    const unsigned role_ticks = (skull::actuators::diagnostic_duration_ms(blinks, false) + 19) / 20;
    for (unsigned i = 0; i <= std::max(60u, role_ticks); ++i) {
        skull::actuators::tick(0, false);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    if (sd == ESP_OK) {
        skull::startup::load(card_config.primary, card_config.volume_percent);
        assets = static_cast<skull::catalog::Catalog *>(heap_caps_malloc(sizeof(skull::catalog::Catalog), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        if (assets) new (assets) skull::catalog::Catalog;
        if (!assets || !skull::catalog::scan(*assets, "/sdcard/audio")) {
            ESP_LOGE(tag, "Audio catalog unavailable");
            if (assets) heap_caps_free(assets);
            assets = nullptr;
        }
        if (assets && !skull::audio::init(card_config.volume_percent, card_config.primary)) {
            ESP_LOGE(tag, "PCM producer initialization failed; skit control disabled");
            heap_caps_free(assets); assets = nullptr;
        }
    }
    skull::show::init(assets, card_config.primary);
    ESP_ERROR_CHECK(skull::radio::init(provide_pcm));
    if (card_config.primary) {
        node::config_t node_config{};
        std::strcpy(node_config.root_node.basic_information.node_label, "Anthony prototype");
        node_t *root = node::create(&node_config, attribute_update, identify);
        ESP_ERROR_CHECK(root ? ESP_OK : ESP_FAIL);
        endpoint::on_off_plug_in_unit::config_t trigger_config{};
        trigger_config.on_off.on_off = false;
        endpoint_t *trigger = endpoint::on_off_plug_in_unit::create(root, &trigger_config, ENDPOINT_FLAG_NONE, nullptr);
        ESP_ERROR_CHECK(trigger ? ESP_OK : ESP_FAIL);
        trigger_endpoint = endpoint::get_id(trigger);
        ESP_ERROR_CHECK(esp_matter::start(matter_event));
        chip::DeviceLayer::SystemLayer().ScheduleLambda([]() {
            reset_momentary(nullptr, nullptr); // Hydrated On never starts a show.
            // Commissioned boot does not advertise or set a random address until
            // a future explicit commissioning window; the peer uses a public address.
            if (chip::Server::GetInstance().GetFabricTable().FabricCount() != 0)
                matter_ble_ready.store(true);
            else pairing();
        });
    } else ESP_ERROR_CHECK(skull::secondary::init());
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, 1024, 0, 0, nullptr, 0));
    ESP_ERROR_CHECK(xTaskCreate(diagnostic_task, "skull_diag", 6144, nullptr, 4, nullptr) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}
