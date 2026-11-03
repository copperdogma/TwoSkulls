# Home Assistant jaw calibration feasibility

Date: November 2, 2026. Status: investigated/proposed; not implemented or physically tested.

Update: implemented for software/build qualification in
[Story010](../stories/story-010-sd-jaw-calibration.md). Firmware/UI deployment
and physical endpoint acceptance remain pending; the findings below describe
the pre-change source unless explicitly marked as implementation guidance.

Owner clarification: Jeff's rod only pushes the jaw open. When the teeth meet,
the unattached linkage can freely retract roughly another half centimeter,
without reaching the servo limits. The earlier inference of closing strain was
incorrect. Closed calibration removes unused travel; teeth contact alone is not
a fault or an overdrive indication for this mechanism.

## Decision

Extend the existing native firmware/MQTT discovery and grouped HA dashboard.
No custom website, replacement firmware framework or new servo hardware is
needed for commanded-position calibration. HA MQTT Number supports numeric
input/slider, min/max/step, state feedback and discovery; MQTT Button supports
explicit step/capture/save actions:

- https://www.home-assistant.io/integrations/number.mqtt/
- https://www.home-assistant.io/integrations/button.mqtt/

Use a dedicated per-prop tuning panel: enter/exit manual mode, commanded pulse
in microseconds, exact numeric target, fine/coarse open/close steps (initially
1/10 us), saved closed/open endpoints, capture current as closed/open, explicit
Save to SD and discard/reset draft. Keep saved values separate from unsaved
drafts; only verified device acknowledgements update the displayed result.

## Local findings

- `firmware/main/skull_actuators.cpp`: `jaw_duty()` maps legacy 0..80 logical
  degrees through the old library's 544..2400 us / 0..180 mapping. Current
  commands are 544 us closed and 1368 us fully open (integer arithmetic).
  At 50 Hz / 16-bit duty, one PWM count is about 0.305 us. These are commands,
  not measured linkage angle or servo feedback.
- `skull_config.h/.cpp`: SD `config.txt` has role, speaker name, peer MAC and
  volume, but no jaw endpoints. Unknown well-formed keys are ignored. Adding
  keys alone will not change motion. Preserve legacy defaults for old cards.
- `skull_storage.cpp`: existing speaker-name transaction preserves other lines,
  uses staging/backup/readback/fsync and refuses stale recovery files. Adapt
  that pattern for saving both endpoints together; extend settings comparison.
  FAT rename is not a power-loss-atomic replace; test interrupted recovery.
- `app_main.cpp`: PWM initialization precedes SD config loading. To avoid the
  old closed command before calibration is applied, do not command the legacy closed
  position before loading calibrated endpoints. Review deferred jaw activation
  while retaining eye error indications and existing GPIO/timer constraints.
- `skull_actuators::tick()` owns PWM in the control task and normally performs
  speech, boot gestures and idle breathing. Route MQTT requests through that
  owner, not network callbacks. Disable automatic jaw motion during calibration.
- `skull_show.cpp` accepts local and paired work; exclusion must cover all show,
  startup, tone, Matter, serial and incoming peer paths, not merely hide HA Play.
- `skull_network.cpp` already rejects retained/stale commands and uses a bounded
  mailbox. Extend explicit command parsing/acks; measure step responsiveness
  and discovery buffer capacity rather than assuming current five-second state
  publishing is suitable for interactive tuning.
- Latest recorded native build has only 85 bytes IRAM free (Story007). Keep new
  logic in flash, reuse existing tasks and measure the resulting link/heap. A
  feasibility review does not establish that the new binary fits.

## Required behavior and remaining uncertainty

Enter only while quiescent, holding the last applied pulse without an entry
jump. Reject playback while tuning, including requests from the other skull.
Never automatically sweep for endpoints: there is no contact/current feedback.
Normal animation maps its existing normalized motion into the saved per-prop
closed/open range. Endpoint tuning must allow deliberate small excursions beyond
the old operational range, within separately validated electrical bounds; the
old 544..2400 us library span is not proof the linkage tolerates that span.

Provide an immediate pulse-disable action and explicit resume, with no
automatic jump to an unverified closed endpoint. Define a device-side session
lease/disconnect policy that ends manual drive without silently restarting
automatic gestures, and rejects late commands from the old session. Do not
claim that stopping PWM physically disconnects or instantly releases a servo.

Saving is explicit, idle-only, preserves unrelated SD settings and reports
failure without claiming persistence. Defaults, invalid values, reversed/equal
endpoints, interrupted writes and boot application need tests. Confirm the
actual attached servo's acceptable pulse envelope before widening hard bounds.
Jeff and Anthony need separate physical endpoint acceptance. No hardware was
moved, flashed or reconfigured for this investigation.

## Fault handling research and local verification

General problem: actuator output can outlive a failed control/update path.
The [pinned ESP-IDF 5.5.4 LEDC guide](https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32/api-reference/peripherals/ledc.html)
documents that channel configuration starts PWM and that duty changes require
set plus update. Thus suspending the task's updates is not a stop. Retain the
existing single-owner, pending-register guard rather than changing to callbacks
or hardware fades; fades introduce extra ownership/stop limitations and IRAM.

Independent review reproduced an eye-error path that left manual jaw PWM on
after timeout and STOP. Story010 now separates fault latching from the zero-duty
retry path: any output error inhibits further motion, but attempts to disable
the jaw continue despite normal update failure. Four injected eye/jaw set/update
failures pass in production-source ASan/UBSan tests. Permanently pending or
failed hardware and a blocked control task remain outside this software proof.
