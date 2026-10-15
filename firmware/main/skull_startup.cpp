#include "skull_startup.h"
#include "skull_wave.h"
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <cmath>
#include "esp_heap_caps.h"
#include "esp_log.h"

namespace skull::startup {
namespace {
constexpr char tag[] = "skull_startup";
constexpr uint32_t max_bytes = 1024 * 1024;
// Let the speaker unmute before speech: 800 ms (500 ms beyond the first probe).
constexpr uint32_t preroll_frames = 35280;
enum class State { unavailable, ready, playing, complete, interrupted };
std::atomic<State> state{State::unavailable};
uint8_t *pcm = nullptr;
uint32_t bytes = 0;
constexpr uint32_t envelope_frames = 882; // 20 ms at 44.1 kHz
uint16_t *envelope = nullptr;
std::atomic<uint32_t> consumed_frames{0};
uint32_t cursor = 0, preroll = preroll_frames; // audio provider owns these
State reported = State::unavailable; // diagnostic task owns this
}

bool load(bool primary, unsigned volume_percent) {
    if (pcm || state.load(std::memory_order_acquire) != State::unavailable) return false;
    const char *path = primary ? "/sdcard/audio/Initialized - Primary.wav"
                               : "/sdcard/audio/Initialized - Secondary.wav";
    FILE *file = std::fopen(path, "rb");
    wave::Info info{};
    if (!file) { ESP_LOGE(tag, "Startup announcement missing: %s", path); return false; }
    const bool valid = wave::inspect(file, info);
    if (!valid || info.data_bytes > max_bytes) {
        std::fclose(file);
        ESP_LOGE(tag, "Startup WAV invalid/unsupported or exceeds 1 MiB: %s", path);
        return false;
    }
    auto *loaded = static_cast<uint8_t *>(heap_caps_malloc(info.data_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!loaded) { std::fclose(file); ESP_LOGE(tag, "Startup PCM PSRAM allocation failed"); return false; }
    const bool read = std::fread(loaded, 1, info.data_bytes, file) == info.data_bytes && !std::ferror(file);
    std::fclose(file);
    if (!read) { heap_caps_free(loaded); ESP_LOGE(tag, "Startup PCM read failed"); return false; }
    // Apply configured gain once outside the callback. No remote-volume or
    // speaker-wide volume change, and no callback-time analysis/SD access.
    const unsigned gain = std::min(volume_percent, 100u);
    if (gain != 100) {
        for (uint32_t i = 0; i < info.data_bytes; i += 2) {
            int16_t sample;
            std::memcpy(&sample, loaded + i, 2);
            sample = static_cast<int16_t>(static_cast<int32_t>(sample) * static_cast<int32_t>(gain) / 100);
            std::memcpy(loaded + i, &sample, 2);
        }
    }
    const uint32_t frames = info.data_bytes / 4;
    const uint32_t windows = (frames + envelope_frames - 1) / envelope_frames;
    envelope = static_cast<uint16_t *>(heap_caps_malloc(windows * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (envelope) {
        for (uint32_t window = 0; window < windows; ++window) {
            const uint32_t first = window * envelope_frames;
            const uint32_t count = std::min(envelope_frames, frames - first);
            uint64_t sum = 0;
            for (uint32_t i = 0; i < count * 2; ++i) {
                int16_t sample;
                std::memcpy(&sample, loaded + first * 4 + i * 2, 2);
                const int32_t wide = sample;
                sum += static_cast<uint64_t>(wide * wide);
            }
            envelope[window] = static_cast<uint16_t>(std::sqrt(static_cast<double>(sum) / (count * 2)));
        }
    } else ESP_LOGE(tag, "Startup envelope allocation failed; audio available without jaw animation");
    pcm = loaded; bytes = info.data_bytes;
    state.store(State::ready, std::memory_order_release);
    ESP_LOGI(tag, "Startup announcement loaded: role=%s PCM=%lu offset=%lu gain=%u%%",
             primary ? "primary" : "secondary", static_cast<unsigned long>(bytes),
             static_cast<unsigned long>(info.data_offset), gain);
    return true;
}

void poll(bool stream_ready) {
    State current = state.load(std::memory_order_acquire);
    if (current == State::ready && stream_ready) {
        if (state.compare_exchange_strong(current, State::playing, std::memory_order_acq_rel))
            ESP_LOGI(tag, "Startup announcement starting (once per boot)");
    } else if (current == State::playing && !stream_ready) {
        state.compare_exchange_strong(current, State::interrupted, std::memory_order_acq_rel);
    }
    current = state.load(std::memory_order_acquire);
    if (current != reported) {
        if (current == State::complete) ESP_LOGI(tag, "Startup announcement PCM delivered; awaiting audible owner confirmation");
        if (current == State::interrupted) ESP_LOGW(tag, "Startup announcement interrupted by link loss; no automatic replay");
        reported = current;
    }
}

bool provide(uint8_t *data, int32_t length) {
    if (!data || length <= 0) return false;
    if (state.load(std::memory_order_acquire) != State::playing) return false;
    uint32_t capacity = static_cast<uint32_t>(length / 4);
    const uint32_t silent = std::min(capacity, preroll);
    preroll -= silent; capacity -= silent;
    const uint32_t count = std::min(capacity, (bytes - cursor) / 4) * 4;
    if (count) std::memcpy(data + silent * 4, pcm + cursor, count);
    cursor += count;
    consumed_frames.store(cursor / 4, std::memory_order_relaxed);
    if (cursor == bytes) {
        State expected = State::playing;
        state.compare_exchange_strong(expected, State::complete, std::memory_order_release);
    }
    return true;
}

Animation animation() {
    if (state.load(std::memory_order_acquire) != State::playing) return {0, false};
    const uint32_t frames = consumed_frames.load(std::memory_order_relaxed);
    if (!frames || !envelope) return {0, false};
    return {envelope[(frames - 1) / envelope_frames], true};
}

bool finished() {
    const auto current = state.load(std::memory_order_acquire);
    return current == State::complete || current == State::interrupted || current == State::unavailable;
}

bool playing() {
    return state.load(std::memory_order_acquire) == State::playing;
}

bool busy() {
    const State current = state.load(std::memory_order_acquire);
    return current == State::ready || current == State::playing;
}
}
