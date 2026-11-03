// Exercises the actual production driver against deterministic hardware/time
// stubs. This proves commanded waveforms and state transitions, not real motion.
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <vector>
#include <sys/wait.h>
#include <unistd.h>
#include "driver/ledc.h"
#include "esp_private/periph_ctrl.h"
#include "hal/ledc_ll.h"
#include "skull_actuators.h"
#include "skull_speaker_indicator.h"

actuator_test_ledc_t actuator_test_ledc = {};

namespace {
int64_t now_us = 10000000;
std::vector<ledc_timer_config_t> timers;
std::vector<ledc_channel_config_t> channels;
std::array<uint32_t, 3> duty = {};
std::array<uint32_t, 3> staged_duty = {};
std::array<unsigned, 3> fail_set = {};
std::array<unsigned, 3> fail_update = {};
std::array<unsigned, 3> zero_attempts = {};
unsigned updates = 0;
unsigned resets = 0;
std::array<unsigned, 3> sets = {};
std::array<unsigned, 3> channel_updates = {};
void tick_after(int64_t us, uint16_t rms = 0, bool speaking = false, bool audio_busy = false)
{
    now_us += us;
    skull::actuators::tick(rms, speaking, audio_busy);
}
void tick_at(int64_t us, uint16_t rms = 0, bool speaking = false, bool audio_busy = false)
{
    assert(us >= now_us);
    tick_after(us - now_us, rms, speaking, audio_busy);
}
uint32_t jaw_duty(int degrees)
{
    return ((544U + degrees * 824U / 80U) * 65536U + 10000U) / 20000U;
}
void expect_idle_jaw(int degrees)
{
    assert(duty[0] == jaw_duty(degrees));
    assert(duty[1] == 100 && duty[2] == 100);
}
void expect_eyes(uint32_t value)
{
    assert(duty[1] == value && duty[2] == value);
}
}
int64_t esp_timer_get_time() { return now_us; }
void periph_module_reset(periph_module_t module)
{
    assert(module == PERIPH_LEDC_MODULE && timers.empty() && channels.empty());
    ++resets;
    actuator_test_ledc = {};
}
esp_err_t ledc_timer_config(const ledc_timer_config_t *config)
{
    assert(resets == 1);
    assert(channels.empty()); // Both timers must precede any GPIO output.
    timers.push_back(*config);
    return ESP_OK;
}
esp_err_t ledc_channel_config(const ledc_channel_config_t *config)
{
    assert(timers.size() == 2);
    channels.push_back(*config);
    duty[config->channel] = staged_duty[config->channel] = config->duty;
    return ESP_OK;
}
esp_err_t ledc_set_duty(ledc_mode_t mode, ledc_channel_t channel, uint32_t value)
{
    assert(mode == LEDC_LOW_SPEED_MODE);
    assert(!actuator_test_ledc.channel_group[mode].channel[channel].conf1.duty_start);
    ++sets[channel];
    if (!value) ++zero_attempts[channel];
    if (fail_set[channel]) { --fail_set[channel]; return ESP_FAIL; }
    staged_duty[channel] = value;
    return ESP_OK;
}
esp_err_t ledc_update_duty(ledc_mode_t mode, ledc_channel_t channel)
{
    assert(mode == LEDC_LOW_SPEED_MODE);
    assert(!actuator_test_ledc.channel_group[mode].channel[channel].conf1.duty_start);
    ++channel_updates[channel];
    ++updates;
    if (fail_update[channel]) { --fail_update[channel]; return ESP_FAIL; }
    duty[channel] = staged_duty[channel];
    return ESP_OK;
}

int main(int argc, char **argv)
{
    using namespace skull::actuators;
    // Policy boot age must remain independent of when control polling starts.
    {
        SpeakerIndicator missing;
        missing.update(29980000, false);
        assert(!missing.active);
        missing.update(30000000, false);
        assert(missing.active && missing.bright(30000000));
        assert(missing.bright(30999999) && !missing.bright(31000000));
        assert(!missing.bright(31999999) && missing.bright(32000000));
        SpeakerIndicator late;
        late.update(75000000, false);
        assert(late.active && late.started_us == 75000000 && late.bright(75000000));
        SpeakerIndicator early;
        early.update(1000000, true);
        assert(!early.active && early.connected_once);
        early.update(2000000, false);
        assert(early.active && early.bright(2000000));
        early.update(3500000, true);
        assert(!early.active);
        early.update(4000000, false);
        assert(early.active && early.started_us == 4000000 && early.bright(4000000));
    }
    // Optional trace mode consumes 20 ms RMS windows and exercises the actual
    // production driver. Used by jaw_preflight.py with repository WAV assets.
    if (argc == 2) {
        FILE *input = std::fopen(argv[1], "r");
        assert(input && init() == ESP_OK && configure_jaw(544, 1368));
        tick_after(20000, 0, false, true);
        unsigned rms;
        while (std::fscanf(input, "%u", &rms) == 1) {
            assert(rms <= 65535);
            tick_after(20000, static_cast<uint16_t>(rms), true, true);
            std::printf("%u\n", duty[0]);
        }
        std::fclose(input);
        return 0;
    }
    actuator_test_ledc.channel_group[LEDC_LOW_SPEED_MODE].channel[0].conf1.duty_start = 1;
    diagnostic(2); // Boot may select an indicator before hardware/task startup.
    assert(init() == ESP_OK);
    assert(!actuator_test_ledc.channel_group[LEDC_LOW_SPEED_MODE].channel[0].conf1.duty_start);
    assert(init() == ESP_OK && resets == 1); // Repeated init never resets live PWM.
    assert(timers.size() == 2 && channels.size() == 3);
    assert(timers[0].timer_num == LEDC_TIMER_0);
    assert(timers[0].freq_hz == 50 && timers[0].duty_resolution == LEDC_TIMER_16_BIT);
    assert(timers[1].timer_num == LEDC_TIMER_1);
    assert(timers[1].freq_hz == 5000 && timers[1].duty_resolution == LEDC_TIMER_8_BIT);
    for (const auto &timer : timers) {
        assert(timer.speed_mode == LEDC_LOW_SPEED_MODE && timer.clk_cfg == LEDC_USE_APB_CLK);
    }
    assert(channels[0].gpio_num == 23 && channels[0].channel == LEDC_CHANNEL_0);
    assert(channels[0].timer_sel == LEDC_TIMER_0);
    assert(channels[1].gpio_num == 32 && channels[1].channel == LEDC_CHANNEL_1);
    assert(channels[2].gpio_num == 33 && channels[2].channel == LEDC_CHANNEL_2);
    assert(channels[1].timer_sel == LEDC_TIMER_1 && channels[2].timer_sel == LEDC_TIMER_1);
    assert(duty[0] == 0); // No legacy closed pulse before SD settings load.
    assert(jaw_state().mode == JawMode::disabled && !jaw_state().enabled);
    assert(!configure_jaw(543, 1368) && !configure_jaw(544, 2401));
    assert(!configure_jaw(1000, 1000));
    assert(duty[0] == 0 && !jaw_manual_enter() && !jaw_resume());
    assert(configure_jaw(544, 1368));
    assert(duty[0] == 1783); // Calibrated closed command, rounded to 16-bit PWM.
    assert(duty[1] == 100 && duty[2] == 100);

    tick_after(40000000); // Late first diagnostic tick still begins at closed.
    const int64_t first_control_tick = now_us;
    assert(duty[0] == 1783);
    expect_eyes(255); // Pre-init selection survives and anchors at first tick.
    tick_after(600000);
    assert(duty[0] == 2795); // 30 degrees -> 853 us, not full servo range.
    expect_eyes(0); // At 600 ms, the second flash is in its off half.
    const unsigned peak_updates = updates;
    tick_after(19999);
    assert(updates == peak_updates && duty[0] == 2795);
    tick_after(1);
    assert(duty[0] < 2795 && duty[0] > 1783);
    tick_after(580000);
    assert(duty[0] == 1783); // Exactly 1.2 seconds from first tick.
    expect_eyes(100); // Delayed tick completes the one-shot without replaying.

    test();
    tick_after(20000);
    tick_after(600000);
    assert(duty[0] == 2795);
    tick_after(20000, 0, true); // Speech cancels check, even during silence.
    assert(duty[0] < 2795);
    assert(duty[1] == 255 && duty[2] == 255);
    tick_after(20000);
    assert(duty[0] == 1783 && duty[1] == 100 && duty[2] == 100);
    tick_after(600000);
    assert(duty[0] == 1783); // Interrupted boot check never resumes.

    const int64_t breath = first_control_tick + 7000000;
    tick_at(breath - 20000);
    expect_idle_jaw(0);
    tick_at(breath);
    expect_idle_jaw(0);
    tick_at(breath + 1000000);
    expect_idle_jaw(15);
    tick_at(breath + 2000000);
    expect_idle_jaw(30);
    const unsigned breathing_peak_updates = updates;
    tick_after(19999);
    assert(updates == breathing_peak_updates);
    tick_after(1);
    expect_idle_jaw(30);
    tick_at(breath + 2080000);
    expect_idle_jaw(30);
    tick_at(breath + 2100000);
    expect_idle_jaw(30);
    tick_at(breath + 3100000);
    expect_idle_jaw(15);
    tick_at(breath + 4100000);
    expect_idle_jaw(0);
    tick_at(breath + 6980000);
    expect_idle_jaw(0);
    tick_at(breath + 7000000); // Seven seconds from start, not completion.
    expect_idle_jaw(0);
    tick_after(1000000);
    expect_idle_jaw(15);

    tick_after(20000, 0, true, true); // Speech wins even while audio owns jaw.
    assert(duty[1] == 255 && duty[2] == 255);
    tick_after(20000);
    expect_idle_jaw(0);
    tick_after(1000000);
    expect_idle_jaw(0); // No stale mid-breath resume.

    // A long control delay finishes the old breath on the next tick, then
    // starts one new breath from closed instead of trying to catch up.
    tick_at(breath + 14000000);
    tick_after(1000000);
    expect_idle_jaw(15);
    tick_after(60000000);
    expect_idle_jaw(0);
    tick_after(20000);
    const int64_t delayed_breath = now_us;
    expect_idle_jaw(0);
    tick_after(1000000);
    expect_idle_jaw(15);
    tick_after(20000, 0, false, true); // Even silent non-speech audio owns jaw.
    expect_idle_jaw(0);
    tick_after(1000000);
    expect_idle_jaw(0);
    tick_at(delayed_breath + 7000000, 0, false, true);
    tick_after(4000000, 0, false, true);
    expect_idle_jaw(0);
    tick_after(20000); // Overdue cadence starts a fresh breath on audio end.
    const int64_t after_audio = now_us;
    expect_idle_jaw(0);
    tick_after(1000000);
    expect_idle_jaw(15);

    test(); // Manual check interrupts breathing and has priority while pending.
    tick_after(20000);
    expect_idle_jaw(0);
    tick_after(600000);
    expect_idle_jaw(30);
    tick_after(600000);
    expect_idle_jaw(0);
    tick_after(20000);
    expect_idle_jaw(0);
    test();
    tick_after(20000);
    tick_after(600000);
    expect_idle_jaw(30);
    tick_after(20000, 0, false, true); // Preroll also cancels manual/boot check.
    expect_idle_jaw(0);
    tick_after(20000);
    expect_idle_jaw(0);
    tick_after(600000);
    expect_idle_jaw(0);
    tick_at(after_audio + 7000000);
    tick_after(1000000);
    expect_idle_jaw(15);

    // Startup/manual check remains ahead of an already overdue idle cadence.
    test();
    tick_after(10000000);
    expect_idle_jaw(0);
    tick_after(600000);
    expect_idle_jaw(30);
    tick_after(600000);
    expect_idle_jaw(0);
    tick_after(20000);
    expect_idle_jaw(0); // Due breath begins only after the check finishes.
    tick_after(1000000);
    expect_idle_jaw(15);

    tick_after(20000); // Start speech from closed, independent of idle gesture.
    for (int syllable = 0; syllable < 3; ++syllable) {
        tick_after(20000, 2400, true);
        assert(duty[0] >= jaw_duty(50)); // Opens promptly with softer acceleration.
        tick_after(20000, 2400, true);
        assert(duty[0] >= jaw_duty(70));
        tick_after(20000, 2400, true);
        assert(duty[0] >= jaw_duty(77));
        const auto wide = duty[0];
        tick_after(20000, 0, true); // Quiet within a speaking line must close.
        assert(duty[0] < wide && duty[0] <= jaw_duty(36));
        tick_after(20000, 0, true);
        tick_after(20000, 0, true);
        tick_after(20000, 0, true);
        assert(duty[0] <= jaw_duty(3)); // Near closed within 80 ms, no long tail.
    }
    tick_after(20000);
    for (int i = 0; i < 20; ++i) tick_after(20000, 150, true);
    assert(duty[0] == jaw_duty(0)); // Recording noise cannot hold the jaw open.
    tick_after(20000, 1800, true);
    assert(duty[0] >= jaw_duty(38)); // Moderate speech gets useful travel.
    tick_after(20000); // Non-speaking role closes immediately and clears history.
    expect_idle_jaw(0);
    tick_after(20000, 0, true);
    assert(duty[0] == jaw_duty(0));

    for (int i = 0; i < 200; ++i) {
        tick_after(20000, 65535, true);
        assert(duty[0] >= 1783 && duty[0] <= 4483); // At most 80 deg /1368 us.
    }
    assert(duty[0] > 4000); // Loud input actually opens the jaw.
    tick_after(20000);
    assert(duty[0] == 1783 && duty[1] == 100 && duty[2] == 100);

    // Exact on/off phases and flash counts for both role successes and errors.
    // The deliberately late first tick must anchor, not consume, the pattern.
    for (unsigned count : {4U, 2U, 3U, 5U}) {
        diagnostic(count);
        tick_after(2000000);
        expect_eyes(255);
        const int64_t start = now_us;
        unsigned flashes = 1;
        bool was_on = true;
        for (int64_t elapsed = 20000; elapsed < count * 400000LL; elapsed += 20000) {
            tick_at(start + elapsed);
            const bool on = (elapsed / 200000) % 2 == 0;
            expect_eyes(on ? 255 : 0);
            if (on && !was_on) ++flashes;
            was_on = on;
        }
        assert(flashes == count);
        assert(diagnostic_duration_ms(count, false) == count * 400);
        tick_at(start + count * 400000LL);
        expect_eyes(100);
    }

    for (unsigned count : {3U, 5U}) {
        diagnostic(count, true);
        tick_after(20000);
        const int64_t start = now_us;
        const int64_t duration = count * 400000LL;
        const int64_t cycle = duration + 2000000;
        assert(diagnostic_duration_ms(count) * 1000LL == cycle);
        for (int64_t elapsed = 0; elapsed <= 2 * cycle; elapsed += 20000) {
            // Start was already ticked; all remaining samples are 20 ms apart.
            if (elapsed) tick_at(start + elapsed);
            const int64_t phase = elapsed % cycle;
            const bool on = phase < duration && (phase / 200000) % 2 == 0;
            expect_eyes(on ? 255 : 0);
        }
        diagnostic(0);
        tick_after(20000);
        expect_eyes(100);
    }

    diagnostic(3, true);
    tick_after(20000, 65535, true);
    const int64_t speaking_start = now_us;
    expect_eyes(255);
    tick_at(speaking_start + 200000, 65535, true);
    expect_eyes(0); // Speech brightness cannot erase a diagnostic off phase.
    assert(duty[0] > jaw_duty(0)); // Jaw speech control continues independently.
    tick_at(speaking_start + 1080000, 65535, true);
    expect_eyes(0); // Delayed tick lands in phase 5, with no catch-up pulses.
    tick_at(speaking_start + 3200000 + 480000, 65535, true);
    expect_eyes(255); // Repeated cycle, phase 2, still active after speech.
    diagnostic(0);
    tick_after(20000, 65535, true);
    expect_eyes(255); // Clear resumes current speaking brightness.
    tick_after(20000);
    expect_eyes(100);

    diagnostic(2);
    tick_after(20000, 65535, true);
    tick_after(800000, 65535, true);
    expect_eyes(255); // One-shot completion also resumes speaking brightness.
    tick_after(20000);
    expect_eyes(100);
    // Pending jaw updates must not touch either duty API or prevent eye work.
    auto &hw = actuator_test_ledc.channel_group[LEDC_LOW_SPEED_MODE];
    hw.channel[0].conf1.duty_start = 1;
    const auto jaw_sets = sets[0];
    const auto jaw_updates = channel_updates[0];
    const auto jaw_before = duty[0];
    test();
    tick_after(20000);
    tick_after(600000, 65535, true);
    assert(sets[0] == jaw_sets && duty[0] == jaw_before);
    assert(channel_updates[0] == jaw_updates);
    expect_eyes(255);
    assert(stats().deferred_updates > 0 && stats().pending_channels == 1);
    assert(stats().stuck && stats().max_pending_us >= 600000);
    hw.channel[0].conf1.duty_start = 0;
    tick_after(20000, 65535, true);
    assert(sets[0] > jaw_sets); // Deferred duty wasn't falsely cached as applied.
    assert(!stats().stuck && stats().pending_channels == 0);

    // Independent eye caches: a pending left eye doesn't suppress the right.
    hw.channel[1].conf1.duty_start = 1;
    const auto left_sets = sets[1];
    tick_after(20000);
    assert(sets[1] == left_sets && duty[1] == 255 && duty[2] == 100);
    hw.channel[1].conf1.duty_start = 0;
    tick_after(20000);
    expect_eyes(100);
    assert(sets[1] == left_sets + 1 && resets == 1);

    // Actual driver: connection loss owns only eye brightness, with exact
    // one-second bright/dim halves and higher-priority diagnostic darkness.
    speaker_connection(true);
    tick_after(20000);
    speaker_connection(false);
    const int64_t warning_start = now_us;
    tick_after(20000);
    expect_eyes(255);
    tick_at(warning_start + 980000);
    expect_eyes(255);
    tick_at(warning_start + 1000000, 2400, true, true);
    expect_eyes(100);
    assert(duty[0] >= jaw_duty(50)); // Warning cannot suppress speech motion.
    tick_at(warning_start + 1980000, 2400, true, true);
    expect_eyes(100); // Speech brightness cannot erase warning's dim phase.
    tick_at(warning_start + 2000000, 2400, true, true);
    expect_eyes(255);
    tick_at(warning_start + 5500000); // Skip elapsed phases without replay.
    expect_eyes(100);
    diagnostic(2, true);
    tick_after(20000);
    const int64_t diagnostic_start = now_us;
    expect_eyes(255);
    tick_at(diagnostic_start + 200000);
    expect_eyes(0);
    tick_at(diagnostic_start + 800000);
    expect_eyes(0); // Repeating diagnostic gap stays dark over the warning.
    diagnostic(0);
    tick_after(20000);
    expect_eyes(255); // Warning phase continues underneath the diagnostic.
    diagnostic(1);
    tick_after(20000);
    tick_after(400000, 2400, true, true);
    expect_eyes(255); // One-shot completion resumes current warning phase.
    tick_at(warning_start + 7000000, 2400, true, true);
    expect_eyes(100);
    speaker_connection(true);
    tick_after(20000, 2400, true, true);
    expect_eyes(255); // Reconnect clears immediately to speech brightness.
    tick_after(20000);
    expect_eyes(100);
    speaker_connection(false);
    tick_after(20000);
    expect_eyes(255); // Every new loss restarts with a full bright half.
    speaker_connection(true);
    tick_after(20000);
    expect_eyes(100);
    // Conservative manual tuning: exclusive hold, no entry pulse jump, bounded
    // individual commands, fixed session envelope and no hidden auto gestures.
    const auto pulse_duty = [](unsigned pulse) { return (pulse * 65536U + 10000U) / 20000U; };
    tick_after(20000, 0, false, true); // Establish known closed command.
    const unsigned held = jaw_state().applied_us;
    const auto before_enter = duty[0];
    assert(held == 544 && jaw_manual_enter());
    assert(jaw_state().mode == JawMode::manual && duty[0] == before_enter);
    assert(jaw_state().minimum_us == 544 && jaw_state().maximum_us == 1418);
    assert(!jaw_manual_enter());
    test(); // Legacy manual test cannot override calibration ownership.
    tick_after(20000, 65535, true, true);
    assert(duty[0] == before_enter); // Even unexpected audio cannot move it.
    assert(!jaw_manual_target(543) && !jaw_manual_target(570)); // >25 us jump.
    assert(!jaw_manual_step(0) && !jaw_manual_step(26) && !jaw_manual_step(-26));
    assert(jaw_manual_step(10));
    assert(jaw_state().requested_us == 554 && jaw_state().applied_us == 544);
    tick_after(20000);
    assert(jaw_state().applied_us == 554 && duty[0] == pulse_duty(554));
    assert(jaw_manual_step(1));
    tick_after(20000);
    assert(jaw_state().applied_us == 555);
    assert(jaw_manual_step(-1));
    tick_after(20000);
    assert(jaw_state().applied_us == 554);

    // Pending register updates do not masquerade as applied motion, and a
    // burst cannot accumulate farther than 25 us beyond the applied command.
    hw.channel[0].conf1.duty_start = 1;
    assert(jaw_manual_target(579));
    tick_after(20000);
    assert(jaw_state().applied_us == 554 && jaw_state().requested_us == 579);
    assert(!jaw_manual_step(1));
    jaw_disable();
    assert(jaw_state().mode == JawMode::disabled && jaw_state().enabled);
    assert(jaw_state().applied_us == 554); // Disable is pending, not falsely done.
    tick_after(20000, 65535, true, true);
    assert(duty[0] == pulse_duty(554));
    hw.channel[0].conf1.duty_start = 0;
    tick_after(20000);
    assert(!jaw_state().enabled && jaw_state().applied_us == 0 && duty[0] == 0);
    tick_after(10000000, 65535, true, true);
    assert(duty[0] == 0); // Disable is latched despite speech/breath cadence.
    assert(!jaw_manual_target(554));

    // Re-entry from disabled is explicit and restores only the last held pulse.
    assert(jaw_manual_enter());
    tick_after(20000);
    assert(duty[0] == pulse_duty(554));
    while (jaw_state().applied_us < 1418) {
        const unsigned target = std::min(1418U, jaw_state().applied_us + 25);
        assert(jaw_manual_target(target));
        tick_after(20000);
    }
    assert(!jaw_manual_step(1)); // No automatic sweep or envelope expansion.
    const auto manual_peak = duty[0];
    assert(!configure_jaw(0, 1200) && !configure_jaw(600, 2401));
    assert(jaw_state().closed_us == 544 && jaw_state().open_us == 1368);
    assert(configure_jaw(600, 1200)); // Save/apply changes no held command.
    assert(duty[0] == manual_peak && jaw_state().applied_us == 1418);
    assert(jaw_state().minimum_us == 544 && jaw_state().maximum_us == 1418);
    tick_after(20000, 65535, true, true);
    assert(duty[0] == manual_peak);
    assert(jaw_resume()); // Explicit return, never jump to new closed pulse.
    assert(!jaw_resume());
    unsigned previous = 1418;
    for (int i = 0; i < 100; ++i) {
        tick_after(20000, 0, false, true);
        const auto actual = jaw_state().applied_us;
        assert(previous >= actual && previous - actual <= 10);
        previous = actual;
    }
    assert(previous == 600);

    // Reversed per-skull endpoints retain direction and independent bounds.
    assert(jaw_manual_enter());
    assert(configure_jaw(1200, 600));
    assert(jaw_state().applied_us == 600);
    jaw_disable();
    assert(jaw_resume());
    for (int i = 0; i < 60; ++i) tick_after(20000, 0, false, true);
    assert(jaw_state().applied_us == 1200);
    for (int i = 0; i < 20; ++i) tick_after(20000, 65535, true, true);
    assert(jaw_state().applied_us == 600);
    assert(jaw_manual_enter());
    assert(jaw_state().minimum_us == 550 && jaw_state().maximum_us == 1250);
    assert(jaw_manual_target(610));
    tick_after(20000);
    const auto lease_start = now_us;
    tick_at(lease_start + 89960000, 65535, true, true);
    assert(jaw_state().mode == JawMode::manual && duty[0] == pulse_duty(610));
    tick_at(lease_start + 90000000, 65535, true, true);
    assert(jaw_state().mode == JawMode::disabled && duty[0] == 0);
    assert(!jaw_manual_target(610));
    tick_after(100000000, 65535, true, true);
    assert(duty[0] == 0);

    // An expired lease is also enforced before accepting a command, without
    // waiting for the next 20 ms tick, and no invalid input renews the lease.
    assert(jaw_manual_enter());
    tick_after(20000);
    now_us += 90000000;
    assert(!jaw_manual_step(1));
    assert(jaw_state().mode == JawMode::disabled && duty[0] == 0);
    assert(jaw_manual_enter());
    tick_after(20000);
    assert(!jaw_manual_target(2000));
    tick_after(90000000);
    assert(jaw_state().mode == JawMode::disabled && duty[0] == 0);
    // A deferred final resume step does not prematurely remove the slew guard
    // if the automatic target changes before that command was applied.
    assert(jaw_manual_enter());
    tick_after(20000);
    while (jaw_state().applied_us < 1190) {
        assert(jaw_manual_target(std::min(1190U, jaw_state().applied_us + 25)));
        tick_after(20000);
    }
    assert(jaw_resume());
    hw.channel[0].conf1.duty_start = 1;
    tick_after(20000, 0, false, true); // Wants1200 but remains1190.
    assert(jaw_state().applied_us == 1190 && jaw_state().requested_us == 1200);
    hw.channel[0].conf1.duty_start = 0;
    tick_after(20000, 65535, true, true); // New target opens toward600.
    assert(jaw_state().applied_us == 1180); // Still no more than10us/tick.
    jaw_disable();
    assert(duty[0] == 0);
    // Each child starts with the same healthy deterministic driver state.
    // Faults intentionally latch until reboot, so isolation also verifies there
    // is no in-process init/manual/resume path that clears the latch.
    for (unsigned scenario = 0; scenario < 4; ++scenario) {
        const pid_t child = fork();
        assert(child >= 0);
        if (child == 0) {
            assert(jaw_manual_enter());
            tick_after(20000);
            const auto held_us = jaw_state().applied_us;
            assert(held_us && duty[0] != 0);
            const auto zero_before = zero_attempts[0];
            if (scenario == 0) {
                // Eye set failure immediately removes a healthy jaw's pulses.
                fail_set[1] = 1;
                tick_after(20000, 65535, true, true);
                assert(zero_attempts[0] == zero_before + 1 && duty[0] == 0);
                assert(!jaw_state().enabled && jaw_state().applied_us == 0);
            } else if (scenario == 1) {
                // Eye update failure with two failed jaw-stop set attempts.
                fail_update[1] = 1;
                fail_set[0] = 2;
                tick_after(20000, 65535, true, true);
                assert(jaw_state().enabled && jaw_state().applied_us == held_us);
                assert(zero_attempts[0] == zero_before + 1 && duty[0] != 0);
                jaw_disable(); // STOP retries despite ready=false.
                assert(zero_attempts[0] == zero_before + 2 && jaw_state().enabled);
                tick_after(90000000); // Timer polling still retries the zero.
                assert(zero_attempts[0] == zero_before + 3 && duty[0] == 0);
            } else if (scenario == 2) {
                // Both a requested jaw update and its first zero update fail.
                assert(jaw_manual_step(1));
                fail_update[0] = 2;
                tick_after(20000);
                assert(jaw_state().enabled && jaw_state().applied_us == held_us);
                assert(zero_attempts[0] == zero_before + 1 && duty[0] != 0);
                hw.channel[0].conf1.duty_start = 1;
                jaw_disable();
                tick_after(90000000);
                assert(zero_attempts[0] == zero_before + 1 && jaw_state().enabled);
                hw.channel[0].conf1.duty_start = 0;
                jaw_disable();
                assert(zero_attempts[0] == zero_before + 2 && duty[0] == 0);
            } else {
                assert(jaw_manual_step(1));
                fail_set[0] = 2; // Requested set and first zero set both fail.
                tick_after(20000);
                assert(jaw_state().enabled && jaw_state().applied_us == held_us);
                assert(zero_attempts[0] == zero_before + 1);
                tick_after(20000);
                assert(zero_attempts[0] == zero_before + 2 && duty[0] == 0);
            }
            assert(jaw_state().faulted && jaw_state().mode == JawMode::disabled);
            assert(jaw_state().requested_us == 0 && jaw_state().applied_us == 0);
            assert(!jaw_state().enabled);
            assert(init() == ESP_FAIL && !configure_jaw(544, 1368));
            assert(!jaw_manual_enter() && !jaw_resume() && !jaw_manual_target(600));
            test();
            tick_after(100000000, 65535, true, true);
            assert(duty[0] == 0 && jaw_state().mode == JawMode::disabled);
            _exit(0);
        }
        int status = 0;
        assert(waitpid(child, &status, 0) == child);
        assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    }
    std::puts("Actuator command tests passed");
}
