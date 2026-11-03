#include "skull_actuators.h"
#include "skull_speaker_indicator.h"

#include <algorithm>
#include <cmath>
#include "driver/ledc.h"
#include "esp_private/periph_ctrl.h"
#include "hal/ledc_ll.h"
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
constexpr unsigned jaw_absolute_min = 544;
constexpr unsigned jaw_absolute_max = 2400;
constexpr unsigned manual_margin_us = 50;
constexpr unsigned manual_max_change_us = 25;
constexpr unsigned resume_step_us = 10;
constexpr int64_t manual_lease_us = 90000000;
// RMS is already measured over 20 ms of PCM. Use one fast follower so
// separate syllables survive instead of cascading two slow low-pass filters.
constexpr double speech_noise_floor = 200.0;
constexpr double speech_full_open_rms = 2400.0;
// Cover 20% less distance per tick than the first articulated-motion trial
// (0.85/0.70), softening rapid changes without restoring the old long tail.
constexpr double speech_attack = 0.68;
constexpr double speech_release = 0.56;
constexpr int64_t check_half_us = 600000;
constexpr int64_t breathing_interval_us = 7000000;
constexpr int64_t breathing_movement_us = 2000000;
constexpr int64_t breathing_hold_us = 100000;

bool ready = false;
bool fault_latched = false;
bool jaw_channel_configured = false;
bool fault_stop_confirmed = false;
bool peripheral_reset_done = false;
bool check_pending = false;
int64_t check_started = -1;
int64_t last_tick = -1;
int64_t last_breath_started = -1;
int64_t breath_started = -1;
double speech_jaw = 0;
uint32_t last_jaw_duty = 0;
bool jaw_configured = false;
JawMode jaw_mode = JawMode::disabled;
unsigned jaw_closed_us = 544;
unsigned jaw_open_us = 1368;
unsigned jaw_requested_us = 0;
unsigned jaw_applied_us = 0;
unsigned jaw_held_us = 0;
unsigned manual_min_us = 544;
unsigned manual_max_us = 1368;
int64_t manual_command_us = -1;
bool jaw_resuming = false;
uint32_t last_eye_duty[2] = {dim, dim};
Stats telemetry = {};
int64_t pending_since[3] = {-1, -1, -1};
bool pending_warned[3] = {};
unsigned diagnostic_count = 0;
bool diagnostic_repeat = false;
int64_t diagnostic_started = -1;
constexpr int64_t blink_half_us = 200000;
constexpr int64_t diagnostic_gap_us = 2000000;
SpeakerIndicator speaker_indicator;

// Existing logical animation range remains 0..80; calibration determines the
// pulse range independently for each skull and supports reversed mechanics.
unsigned jaw_pulse(int degrees)
{
    const int bounded = std::clamp(degrees, 0, jaw_max);
    return static_cast<unsigned>((static_cast<int>(jaw_closed_us) * (jaw_max - bounded) +
                                  static_cast<int>(jaw_open_us) * bounded) / jaw_max);
}

uint32_t pulse_duty(unsigned pulse_us)
{
    return (pulse_us * 65536U + 10000U) / 20000U;
}

void cancel_jaw_gestures()
{
    check_pending = false;
    check_started = -1;
    breath_started = -1;
    last_breath_started = esp_timer_get_time();
    speech_jaw = 0;
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

void stop_jaw_after_fault();

esp_err_t checked(esp_err_t error, const char *operation)
{
    if (error != ESP_OK) {
        ESP_LOGE(tag, "%s failed: %s; actuator updates disabled", operation,
                 esp_err_to_name(error));
        ready = false;
        fault_latched = true;
        fault_stop_confirmed = false;
        cancel_jaw_gestures();
        jaw_mode = JawMode::disabled;
        jaw_requested_us = 0;
        jaw_resuming = false;
        // A healthy jaw must still be stopped if an eye channel failed. This
        // path deliberately avoids checked() recursion when the jaw also fails.
        stop_jaw_after_fault();
    }
    return error;
}

bool update_pending(ledc_channel_t channel, int64_t now)
{
    // Classic ESP32 only: IDF's LL function busy-waits on this register with
    // interrupts disabled. Single task owns these channels, so a cleared bit
    // cannot become newly pending between this check and our own update.
    auto *hw = LEDC_LL_GET_HW();
    const bool pending = hw->channel_group[mode].channel[channel].conf1.duty_start != 0;
    if (pending) {
        telemetry.pending_channels |= 1U << channel;
        if (pending_since[channel] < 0) pending_since[channel] = now;
        const uint64_t duration = now - pending_since[channel];
        telemetry.max_pending_us = std::max(telemetry.max_pending_us, duration);
        if (duration >= 100000 && !pending_warned[channel]) {
            pending_warned[channel] = true;
            ESP_LOGW(tag, "LEDC channel %d duty_start pending >=100 ms; updates deferred", channel);
        }
    } else {
        telemetry.pending_channels &= ~(1U << channel);
        if (pending_since[channel] >= 0) {
            telemetry.max_pending_us = std::max(telemetry.max_pending_us,
                static_cast<uint64_t>(now - pending_since[channel]));
        }
        pending_since[channel] = -1;
        pending_warned[channel] = false;
    }
    telemetry.stuck = pending_warned[0] || pending_warned[1] || pending_warned[2];
    return pending;
}

void stop_jaw_after_fault()
{
    if (!jaw_channel_configured || fault_stop_confirmed) return;
    if (update_pending(jaw_channel, esp_timer_get_time())) {
        ++telemetry.deferred_updates;
        return;
    }
    // Preserve the last successful applied state if either operation fails;
    // neither a requested zero nor a failed update proves pulses have stopped.
    if (ledc_set_duty(mode, jaw_channel, 0) != ESP_OK ||
        ledc_update_duty(mode, jaw_channel) != ESP_OK) return;
    last_jaw_duty = 0;
    jaw_applied_us = 0;
    fault_stop_confirmed = true;
}

enum class WriteResult { applied, deferred, failed };
WriteResult write_duty(ledc_channel_t channel, uint32_t duty)
{
    // Guard BEFORE set_duty as well: reprogramming an active change can upset
    // the peripheral's internal logic even before update_duty enters its wait.
    if (update_pending(channel, esp_timer_get_time())) {
        ++telemetry.deferred_updates;
        return WriteResult::deferred;
    }
    if (checked(ledc_set_duty(mode, channel, duty), "set duty") != ESP_OK) return WriteResult::failed;
    if (checked(ledc_update_duty(mode, channel), "update duty") != ESP_OK) return WriteResult::failed;
    return WriteResult::applied;
}
}

esp_err_t init()
{
    if (fault_latched) return ESP_FAIL; // Fault recovery requires a reboot.
    if (ready) return ESP_OK;
    // Software reset can preserve a pending duty_start bit. Reset once before
    // configuring any timer/channel, never while audio/actuators are running.
    if (!peripheral_reset_done) {
        periph_module_reset(PERIPH_LEDC_MODULE);
        peripheral_reset_done = true;
    }
    esp_err_t error;
    // Configure timers first so the first pulse already has the right period.
    error = configure_timer(LEDC_TIMER_0, LEDC_TIMER_16_BIT, 50);
    if (checked(error, "jaw timer init") != ESP_OK) return error;
    error = configure_timer(LEDC_TIMER_1, LEDC_TIMER_8_BIT, 5000);
    if (checked(error, "eye timer init") != ESP_OK) return error;
    last_jaw_duty = 0;
    error = configure_channel(23, jaw_channel, LEDC_TIMER_0, last_jaw_duty);
    if (checked(error, "jaw channel init") != ESP_OK) return error;
    jaw_channel_configured = true;
    error = configure_channel(32, eye_left_channel, LEDC_TIMER_1, dim);
    if (checked(error, "left eye channel init") != ESP_OK) return error;
    error = configure_channel(33, eye_right_channel, LEDC_TIMER_1, dim);
    if (checked(error, "right eye channel init") != ESP_OK) return error;
    last_eye_duty[0] = last_eye_duty[1] = dim;
    speech_jaw = 0;
    last_tick = -1;
    last_breath_started = -1;
    breath_started = -1;
    ready = true;
    ESP_LOGI(tag, "Jaw GPIO23 disabled until SD calibration loads; eyes GPIO32/33 dim (%lu/255)",
             static_cast<unsigned long>(dim));
    return ESP_OK;
}

bool configure_jaw(unsigned closed_us, unsigned open_us)
{
    if (!ready || closed_us < jaw_absolute_min || closed_us > jaw_absolute_max ||
        open_us < jaw_absolute_min || open_us > jaw_absolute_max || closed_us == open_us)
        return false;
    jaw_closed_us = closed_us;
    jaw_open_us = open_us;
    if (!jaw_configured) {
        jaw_configured = true;
        jaw_mode = JawMode::automatic;
        jaw_requested_us = closed_us;
        const auto result = write_duty(jaw_channel, pulse_duty(closed_us));
        if (result == WriteResult::failed) return false;
        if (result == WriteResult::applied) {
            last_jaw_duty = pulse_duty(closed_us);
            jaw_applied_us = jaw_held_us = closed_us;
        }
        test();
    }
    return true;
}

JawState jaw_state()
{
    return {jaw_mode, jaw_requested_us, jaw_applied_us, jaw_closed_us, jaw_open_us,
            manual_min_us, manual_max_us, last_jaw_duty != 0, fault_latched};
}

bool jaw_manual_enter()
{
    if (!ready || !jaw_configured || jaw_mode == JawMode::manual || !jaw_held_us)
        return false;
    // Do not enter while an automatic command is still awaiting hardware: a
    // later pending update could otherwise defeat the no-entry-jump contract.
    if (update_pending(jaw_channel, esp_timer_get_time())) return false;
    cancel_jaw_gestures();
    jaw_mode = JawMode::manual;
    jaw_requested_us = jaw_held_us;
    const unsigned low = std::min(jaw_closed_us, jaw_open_us);
    const unsigned high = std::max(jaw_closed_us, jaw_open_us);
    manual_min_us = low - std::min(low - jaw_absolute_min, manual_margin_us);
    manual_max_us = std::min(jaw_absolute_max, high + manual_margin_us);
    manual_command_us = esp_timer_get_time();
    jaw_resuming = false;
    return true;
}

bool jaw_manual_target(unsigned pulse_us)
{
    if (!ready || jaw_mode != JawMode::manual ||
        esp_timer_get_time() - manual_command_us >= manual_lease_us) {
        if (jaw_mode == JawMode::manual) jaw_disable();
        return false;
    }
    const auto distance = [](unsigned a, unsigned b) { return a > b ? a - b : b - a; };
    if (pulse_us < manual_min_us || pulse_us > manual_max_us ||
        distance(pulse_us, jaw_held_us) > manual_max_change_us ||
        distance(pulse_us, jaw_requested_us) > manual_max_change_us)
        return false;
    jaw_requested_us = pulse_us;
    manual_command_us = esp_timer_get_time();
    return true;
}

bool jaw_manual_step(int delta_us)
{
    if (!delta_us || delta_us < -static_cast<int>(manual_max_change_us) ||
        delta_us > static_cast<int>(manual_max_change_us)) return false;
    const int target = static_cast<int>(jaw_requested_us) + delta_us;
    return target >= 0 && jaw_manual_target(static_cast<unsigned>(target));
}

void jaw_disable()
{
    cancel_jaw_gestures();
    jaw_mode = JawMode::disabled;
    jaw_requested_us = 0;
    jaw_resuming = false;
    if (fault_latched) {
        stop_jaw_after_fault();
        return;
    }
    if (!ready || last_jaw_duty == 0) return;
    const auto result = write_duty(jaw_channel, 0);
    if (result == WriteResult::applied) {
        last_jaw_duty = 0;
        jaw_applied_us = 0;
    }
}

bool jaw_resume()
{
    if (!ready || !jaw_configured || !jaw_held_us || jaw_mode == JawMode::automatic)
        return false;
    cancel_jaw_gestures();
    jaw_mode = JawMode::automatic;
    jaw_requested_us = jaw_held_us;
    jaw_resuming = true;
    return true;
}

void test()
{
    if (!ready || !jaw_configured || jaw_mode != JawMode::automatic) return;
    check_pending = true;
    check_started = -1;
    breath_started = -1;
}

uint32_t diagnostic_duration_ms(unsigned count, bool include_gap)
{
    return static_cast<uint32_t>((static_cast<int64_t>(count) * 2 * blink_half_us + (include_gap ? diagnostic_gap_us : 0)) / 1000);
}

void diagnostic(unsigned count, bool repeat)
{
    diagnostic_count = count;
    diagnostic_repeat = repeat;
    diagnostic_started = -1;
}

Stats stats() { return telemetry; }

void speaker_connection(bool connected)
{
    const bool previous = speaker_indicator.active;
    speaker_indicator.update(esp_timer_get_time(), connected);
    if (previous != speaker_indicator.active)
        ESP_LOGI(tag, "Speaker eye warning %s", speaker_indicator.active ? "active" : "cleared");
}

void tick(uint16_t rms, bool speaking, bool audio_busy)
{
    if (!ready) {
        if (fault_latched) stop_jaw_after_fault();
        return;
    }
    const int64_t now = esp_timer_get_time();
    if (last_tick >= 0 && now - last_tick < 20000) return;
    last_tick = now;
    for (int channel = 0; channel < 3; ++channel) {
        update_pending(static_cast<ledc_channel_t>(channel), now);
    }
    // Start the idle cadence at the first control tick, after potentially long
    // Matter initialization, so boot never jumps into an overdue breath.
    if (last_breath_started < 0) last_breath_started = now;
    int jaw = 0;
    if (jaw_mode != JawMode::automatic) {
        if (jaw_mode == JawMode::manual && now - manual_command_us >= manual_lease_us)
            jaw_disable();
    } else if (speaking) {
        check_pending = false;
        breath_started = -1;
        const double level = std::clamp((rms - speech_noise_floor) /
                                       (speech_full_open_rms - speech_noise_floor), 0.0, 1.0);
        const double target = level * jaw_max;
        const double response = target > speech_jaw ? speech_attack : speech_release;
        speech_jaw += response * (target - speech_jaw);
        jaw = static_cast<int>(std::lround(speech_jaw));
    } else {
        speech_jaw = 0;
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
    if (jaw_mode == JawMode::automatic && jaw_configured) {
        unsigned target = jaw_pulse(jaw);
        if (jaw_resuming) {
            if (target > jaw_held_us) target = std::min(target, jaw_held_us + resume_step_us);
            else target = std::max(target, jaw_held_us > resume_step_us ? jaw_held_us - resume_step_us : 0U);
            if (target == jaw_pulse(jaw) && pulse_duty(target) == last_jaw_duty)
                jaw_resuming = false;
        }
        jaw_requested_us = target;
    }
    const uint32_t duty = pulse_duty(jaw_requested_us);
    if (duty != last_jaw_duty) {
        const auto result = write_duty(jaw_channel, duty);
        if (result == WriteResult::failed) return;
        if (result == WriteResult::applied) {
            last_jaw_duty = duty;
            jaw_applied_us = jaw_requested_us;
            if (jaw_applied_us) jaw_held_us = jaw_applied_us;
            if (jaw_mode == JawMode::automatic && jaw_resuming &&
                jaw_applied_us == jaw_pulse(jaw)) jaw_resuming = false;
        }
    }
    uint32_t eye_duty = speaking ? bright : dim;
    if (speaker_indicator.active)
        eye_duty = speaker_indicator.bright(now) ? bright : dim;
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
    for (int eye = 0; eye < 2; ++eye) {
        if (eye_duty != last_eye_duty[eye]) {
            const auto channel = eye == 0 ? eye_left_channel : eye_right_channel;
            const auto result = write_duty(channel, eye_duty);
            if (result == WriteResult::failed) return;
            if (result == WriteResult::applied) last_eye_duty[eye] = eye_duty;
        }
    }
}
}
