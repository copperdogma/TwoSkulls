#include "skull_actuators.h"

#include <algorithm>
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_timer.h"

namespace skull::actuators {
namespace {
constexpr char tag[] = "skull_actuators";
constexpr ledc_mode_t mode = LEDC_LOW_SPEED_MODE;
constexpr ledc_channel_t jaw_channel = LEDC_CHANNEL_0;
constexpr ledc_channel_t eye_left_channel = LEDC_CHANNEL_1;
constexpr ledc_channel_t eye_right_channel = LEDC_CHANNEL_2;
constexpr uint32_t dim = 100;
constexpr uint32_t bright = 255;
constexpr int jaw_max = 80;
constexpr int64_t check_half_us = 600000;
constexpr int64_t breathing_interval_us = 7000000;
constexpr int64_t breathing_movement_us = 2000000;
constexpr int64_t breathing_hold_us = 100000;

bool ready = false;
bool check_pending = false;
int64_t check_started = -1;
int64_t last_tick = -1;
int64_t last_breath_started = -1;
int64_t breath_started = -1;
double smoothed_amplitude = 0;
int previous_jaw = 0;
uint32_t last_jaw_duty = 0;
uint32_t last_eye_duty = dim;
unsigned diagnostic_count = 0;
bool diagnostic_repeat = false;
int64_t diagnostic_started = -1;
constexpr int64_t blink_half_us = 100000;
constexpr int64_t diagnostic_gap_us = 500000;

// Preserve ESP32_ESP32S2_AnalogWrite's Servo mapping: 0..180 degrees maps
// to 544..2400 us. Restrict the application to its legacy 0..80 degree range;
// 80 degrees is NOT mapped to the library's 2400 us endpoint.
uint32_t jaw_duty(int degrees)
{
    const int bounded = std::clamp(degrees, 0, jaw_max);
    const uint32_t pulse_us = 544 + bounded * (2400 - 544) / 180;
    return (pulse_us * 65536U + 10000U) / 20000U;
}

esp_err_t configure_timer(ledc_timer_t timer, ledc_timer_bit_t bits, uint32_t hz)
{
    ledc_timer_config_t config = {};
    config.speed_mode = mode;
    config.duty_resolution = bits;
    config.timer_num = timer;
    config.freq_hz = hz;
    config.clk_cfg = LEDC_USE_APB_CLK;
    return ledc_timer_config(&config);
}

esp_err_t configure_channel(int gpio, ledc_channel_t channel,
                            ledc_timer_t timer, uint32_t duty)
{
    ledc_channel_config_t config = {};
    config.gpio_num = gpio;
    config.speed_mode = mode;
    config.channel = channel;
    config.intr_type = LEDC_INTR_DISABLE;
    config.timer_sel = timer;
    config.duty = duty;
    config.hpoint = 0;
    return ledc_channel_config(&config);
}

esp_err_t checked(esp_err_t error, const char *operation)
{
    if (error != ESP_OK) {
        ESP_LOGE(tag, "%s failed: %s; actuator updates disabled", operation,
                 esp_err_to_name(error));
        ready = false;
    }
    return error;
}

bool write_duty(ledc_channel_t channel, uint32_t duty)
{
    if (checked(ledc_set_duty(mode, channel, duty), "set duty") != ESP_OK) return false;
    return checked(ledc_update_duty(mode, channel), "update duty") == ESP_OK;
}
}

esp_err_t init()
{
    if (ready) return ESP_OK;
    esp_err_t error;
    // Configure timers first so the first pulse already has the right period.
    error = configure_timer(LEDC_TIMER_0, LEDC_TIMER_16_BIT, 50);
    if (checked(error, "jaw timer init") != ESP_OK) return error;
    error = configure_timer(LEDC_TIMER_1, LEDC_TIMER_8_BIT, 5000);
    if (checked(error, "eye timer init") != ESP_OK) return error;
    last_jaw_duty = jaw_duty(0);
    error = configure_channel(23, jaw_channel, LEDC_TIMER_0, last_jaw_duty);
    if (checked(error, "jaw channel init") != ESP_OK) return error;
    error = configure_channel(32, eye_left_channel, LEDC_TIMER_1, dim);
    if (checked(error, "left eye channel init") != ESP_OK) return error;
    error = configure_channel(33, eye_right_channel, LEDC_TIMER_1, dim);
    if (checked(error, "right eye channel init") != ESP_OK) return error;
    last_eye_duty = dim;
    previous_jaw = 0;
    smoothed_amplitude = 0;
    last_tick = -1;
    last_breath_started = -1;
    breath_started = -1;
    ready = true;
    test();
    ESP_LOGI(tag, "Jaw GPIO23 closed (544 us), eyes GPIO32/33 dim (%lu/255)",
             static_cast<unsigned long>(dim));
    return ESP_OK;
}

void test()
{
    if (!ready) return;
    check_pending = true;
    check_started = -1;
    breath_started = -1;
}

void diagnostic(unsigned count, bool repeat)
{
    diagnostic_count = count;
    diagnostic_repeat = repeat;
    diagnostic_started = -1;
}

void tick(uint16_t rms, bool speaking, bool audio_busy)
{
    if (!ready) return;
    const int64_t now = esp_timer_get_time();
    if (last_tick >= 0 && now - last_tick < 20000) return;
    last_tick = now;
    // Start the idle cadence at the first control tick, after potentially long
    // Matter initialization, so boot never jumps into an overdue breath.
    if (last_breath_started < 0) last_breath_started = now;
    int jaw = 0;
    if (speaking) {
        check_pending = false;
        breath_started = -1;
        // Match skull_audio_animator.cpp's legacy amplitude and jaw smoothing.
        smoothed_amplitude = 0.1 * rms + 0.9 * smoothed_amplitude;
        double amplitude = std::min(smoothed_amplitude * 5.0, 15000.0);
        if (amplitude < 1000.0) amplitude = 0;
        const int target = static_cast<int>(amplitude * jaw_max / 15000.0);
        jaw = static_cast<int>(0.2 * target + 0.8 * previous_jaw);
    } else {
        smoothed_amplitude = 0;
        if (audio_busy) {
            // Audio preroll and quiet diagnostic tones also own the jaw. Never
            // resume an interrupted gesture when audio releases it.
            check_pending = false;
            breath_started = -1;
        } else if (check_pending) {
            if (check_started < 0) {
                check_started = now;
                ESP_LOGI(tag, "Gentle jaw check 0 -> 30 -> 0 degrees");
            }
            const int64_t elapsed = now - check_started;
            if (elapsed < check_half_us) {
                jaw = static_cast<int>(30 * elapsed / check_half_us);
            } else if (elapsed < 2 * check_half_us) {
                jaw = static_cast<int>(30 * (2 * check_half_us - elapsed) / check_half_us);
            } else {
                check_pending = false;
                ESP_LOGI(tag, "Gentle jaw check complete; jaw closed");
            }
        } else {
            if (breath_started < 0 && now - last_breath_started >= breathing_interval_us) {
                // Start-to-start cadence, without replaying missed breaths
                // after audio or a delayed tick.
                breath_started = now;
                last_breath_started = now;
                ESP_LOGI(tag, "Idle breath: 2 s open, 100 ms hold, 2 s close");
            }
            if (breath_started >= 0) {
                const int64_t elapsed = now - breath_started;
                if (elapsed < breathing_movement_us) {
                    jaw = static_cast<int>(30 * elapsed / breathing_movement_us);
                } else if (elapsed < breathing_movement_us + breathing_hold_us) {
                    jaw = 30;
                } else if (elapsed < 2 * breathing_movement_us + breathing_hold_us) {
                    jaw = static_cast<int>(30 * (2 * breathing_movement_us + breathing_hold_us - elapsed)
                                           / breathing_movement_us);
                } else {
                    breath_started = -1;
                }
            }
        }
    }
    previous_jaw = jaw;
    const uint32_t duty = jaw_duty(jaw);
    if (duty != last_jaw_duty) {
        if (!write_duty(jaw_channel, duty)) return;
        last_jaw_duty = duty;
    }
    uint32_t eye_duty = speaking ? bright : dim;
    if (diagnostic_count) {
        if (diagnostic_started < 0) diagnostic_started = now;
        const int64_t duration = static_cast<int64_t>(diagnostic_count) * 2 * blink_half_us;
        int64_t phase = now - diagnostic_started;
        if (diagnostic_repeat) phase %= duration + diagnostic_gap_us;
        if (phase < duration) {
            eye_duty = (phase / blink_half_us) % 2 == 0 ? bright : 0;
        } else if (diagnostic_repeat) {
            eye_duty = 0;
        } else {
            diagnostic_count = 0;
        }
    }
    if (eye_duty != last_eye_duty) {
        if (!write_duty(eye_left_channel, eye_duty)) return;
        if (!write_duty(eye_right_channel, eye_duty)) return;
        last_eye_duty = eye_duty;
    }
}
}
