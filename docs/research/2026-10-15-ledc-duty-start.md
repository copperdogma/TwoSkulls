# Classic ESP32 LEDC pending-duty crash and reboot recovery

## Observed failure

`build/validation/2026-10-15-catalogue-qualification-1/primary.log`
records a cache-error panic during playback after the 127-second status report
(ELF prefix `ce081a695`). The backtrace reaches `ledc_hal_set_duty_start`
through `ledc_update_duty` and the jaw actuator's `write_duty`. Register A3 is
channel 0; the computed register address is low-speed channel 0. Its `conf1`
value `0xc0100400` still has `duty_start` set. Subsequent software restarts
repeatedly hit an interrupt watchdog in the same wait while configuring the
actuator channels, before Bluetooth starts. CPU1's decoded trace is idle.

The actual SDK at `/Users/cam/esp/esp-idf-v5.5.4` has an unbounded
`while (...conf1.duty_start)` in `components/hal/esp32/include/hal/ledc_ll.h:487`.
`ledc_update_duty` enters this helper with a critical section held. Power
management is disabled (`CONFIG_PM_ENABLE` unset); application tasks have
internal stacks. These observations do not establish the initial cache-error
trigger, but directly identify the subsequent blocked driver path.

## Established approach and decision

The upstream [ESPHome PR14720](https://github.com/esphome/esphome/pull/14720)
(merged March12,2026) describes the same runtime crash and retained-peripheral
setup loop. Its three mitigations are caching applied duty, resetting LEDC once
before setup, and deferring writes while the previous update is pending.
Current [ESPHome source](https://github.com/esphome/esphome/blob/657890695f8215aeaa4166d6c41b950273538b64/esphome/components/ledc/ledc_output.cpp)
and [ESP-IDF5.5.4 LL implementation](https://github.com/espressif/esp-idf/blob/v5.5.4/components/hal/esp32/include/hal/ledc_ll.h)
provide the primary implementation evidence.

Adapt that approach in `skull_actuators.cpp`:

- Reset `PERIPH_LEDC_MODULE` once, before configuring any timer or channel.
  Repeated successful `init()` calls do not reset running outputs.
- Check the classic ESP32 `duty_start` register before **both** duty APIs.
  Defer pending work to the next 20ms control tick without spinning.
- Update each channel's duty cache only after its write was applied. Deferred
  jaw/eye channels do not prevent independent channels updating.
- Observe pending channels each tick; expose deferred count, maximum sampled
  pending duration, a pending-channel bit mask, and a current >=100ms stuck
  flag. Emit one warning per continuous pending episode, not per retry.
- Do not reset running peripherals automatically, increase watchdog timeouts,
  or expand IRAM configuration.

The guard depends on classic ESP32 register layout and the existing single-task
ownership of all three channels. Another software writer could invalidate the
check-before-write assumption. Duration telemetry is based on task samples,
not an exact hardware assertion timestamp. This is bounded containment and
recovery engineering, not proof of the original cache-error root cause.

## Local verification and remaining hardware check

Production `skull_actuators.cpp` compiles with this project's actual Xtensa
compiler and SDK headers. Its host test compiles with C++17 and
`-Wall -Wextra -Werror`; all existing animation/diagnostic assertions pass.
Additional simulated register tests verify reset ordering/once-only behavior,
pending-write suppression before either duty API, no false cache advancement,
retry after hardware clears, stuck telemetry, and independent eye progress.

Hardware replay remains required: rerun the same catalogue workload, retain
pending telemetry and audio/heap statistics, and verify controlled software
restart recovery. No flash or serial-port action was performed for this change.
