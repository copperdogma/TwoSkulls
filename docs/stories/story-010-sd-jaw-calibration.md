# Story 010 — Per-skull SD jaw calibration and Home Assistant tuning

Status: Complete for Jeff — deployed, owner-accepted calibration and reboot
persistence. Anthony deployment remains a separate follow-up. Owner: TwoSkulls.
Started November 2, 2026.

## User outcome

Each skull retains its own closed/open servo pulse values on its SD card.
An operator can take exclusive manual control from Home Assistant, move in
small increments, read the commanded microseconds and capture/save endpoints.
Jeff's linkage pushes the jaw open but is not attached to pull it closed.
Cam clarified that after the teeth meet, the linkage freely retracts another
approximately half centimeter without reaching the servo's limits. Teeth contact
and this extra closing travel are not a reported strain or danger. Closed-end
calibration removes unused travel; wider opening still needs conservative tuning.

## Acceptance contract

- `config.txt` gains `jaw_closed_us` and `jaw_open_us`, defaults 544/1368 for
  existing cards. Parse strictly; support reversed servo direction. Preserve
  speaker/role/peer/volume, Wi-Fi/MQTT keys, comments and unknown settings.
- No jaw PWM before validated SD calibration is available. SD error eye
  patterns still work. All normal gestures/speech use calibrated endpoints.
- A dedicated native HA tuning view shows mode, requested/applied pulse,
  saved and draft endpoints, unsaved state and command/save outcome. Include
  clear enter, fine/coarse open/close, exact target, capture endpoints, save,
  discard, stop output and explicit return to automatic movement actions.
- Enter only while startup has finished and audio/show/SD-content use is
  quiescent. Keep one control owner. Block every show source (HA, serial,
  Matter, automatic greetings and incoming peer requests) during manual or
  disabled mode. Do not queue playback for later.
- No automatic endpoint sweeps. Manual requests change at most 25 us per
  command, with 1/10 us buttons. The session exploration envelope is saved
  endpoint span +/-50 us, clipped to firmware pulse bounds 544..2400. These
  bounds limit commands; they do not establish mechanical safety. Each save
  requires the operator's physical acceptance before extending further.
- Stop, MQTT connection loss or 90 seconds without accepted manual input
  disables pulses and latches disabled mode; never automatically resume.
  Explicit resume slews toward normal targets rather than jumping. Disabling
  PWM is not a physical disconnect or guaranteed torque release.
- Save both endpoints together only on explicit request, with validation,
  staging/readback and preserved recovery evidence. Report failure honestly;
  never label an unsaved draft as a persisted setting. FAT replacement is
  recoverable, not power-loss atomic.

## Implementation / sources

See [feasibility research](../research/2026-11-02-home-assistant-jaw-calibration.md)
and the maintained [HA integration guide](../home-assistant-prop-integration.md).
Reuse MQTT Number/Button and the current grouped native dashboard; no custom
website or additional always-on service.

## Verification plan and conservative hardware boundary

1. Host-test parser defaults/invalid/duplicate/reversed endpoints and SD
   preservation/readback/failure paths.
2. Test production actuator code against simulated time/PWM: no pre-config
   pulse, no manual entry jump, bounded increments, endpoint mapping,
   timeout/disable/resume, deferred writes and unchanged eye priority.
3. Test show arbitration for all local/paired/automatic sources; protocol
   parsing/discovery/state validation; run existing regression suites with
   AddressSanitizer/UndefinedBehaviorSanitizer.
4. Build/link the native image and measure IRAM/flash/heap-related buffers.
5. Do not flash an attached skull or move an actual servo as part of the initial
   qualification. Deployment/actual HA entity validation and hand-operated
   endpoint acceptance remain explicit follow-ups. Preserve existing live
   firmware and SD settings until software checks and review pass.

No servo was moved or endpoint changed during the preceding feasibility work.
This story does not claim measured physical jaw angle/current, strain detection,
safe mechanical travel, or both-skull acceptance from software tests.

## Work log

- November 2: reviewed active firmware and SD transaction. Jaw limits were
  hardcoded; PWM initialized before card config. Started implementation with
  independent SD, actuator and MQTT work areas, retaining the existing dirty
  Story007/008/009 work. Selected software-only initial qualification because
  the user explicitly requested very conservative servo testing.
- Implemented strict SD fields/defaults/reversed direction, preserving settings
  and transaction recovery evidence. Updated sample primary/secondary configs.
  Existing cards acquire explicit keys on the first Save, not an unsolicited
  boot-time write.
- Implemented exclusive actuator control, no pre-config PWM, fixed session
  bounds, no-entry-jump from an enabled held command, pending-write guards,
  inactivity/disable and explicit slewed resume. Re-entering from disabled mode
  explicitly re-enables the last held command; physical position is unknown.
- Added strict MQTT grammar/controller, draft/save acknowledgements, fast manual
  telemetry, Wi-Fi/MQTT loss detection and predictable discovery IDs. Prepared
  native tuning subview and operator/AI documentation. Startup must finish
  before Enter is permitted; speaker-off commissioning is not supported yet.
- Independent safety review reproduced an existing shared-output failure path
  that could strand jaw PWM after an eye error. Fixed it with a latched fault
  and independent best-effort zero retries; four injected eye/jaw set/update
  cases pass. Faults cannot be cleared by ordinary Resume/Enter.
- Review also found that preaccepted preparation/peer ACK/greeting-tail work
  could continue after STOP. Inhibition now cancels local playback and pending
  starts. Regression tests cover local prepare, pair ACK and tail-before-Polo;
  a peer command already transmitted cannot be recalled.
- Full host runner passed: 14 Python tests and all 15 portable C++ suites under
  ASan/UBSan, including new jaw policy and existing radio/audio/log regressions.
  After the final extra test additions, targeted jaw policy and all 57 show
  scenarios passed. Dashboard YAML parses and all 21 jaw entity references
  match actual generated discovery (47 components with a skit).
- Native ESP-IDF 5.5.4 `build size` passed. Image 2,324,752 bytes, SHA256
  `e450a715ccbd1fc56ad2df65b3857ea9fd463c38f53f9eda3971a64dfa04a67e`.
  IRAM 130,987/131,072 bytes (85 free, unchanged); static DRAM 102,548/124,580
  bytes. Discovery buffer grows 12→24 KiB in PSRAM; MQTT outbox limit 32 KiB.
  Runtime heap/latency still require real-device observation.
- Private reproducible build/test/JSON evidence and the built binary are under
  ignored `build/validation/story-010-jaw-calibration/`. No hardware flash,
  physical servo command, live SD edit or live HA dashboard edit was made.
  This records the initial software-only stage; the later supervised deployment
  is recorded below.

## Supervised Jeff deployment — November 2, 2026

- Captured status, retained fault log and all eight SD report slots before flashing.
  Preserved a full 4 MiB actual-flash rollback image, the known previous application
  and matching partition evidence privately under the validation directory above.
  Existing report slots 0/1 retained their known hashes (413/5,689 bytes).
- Flashed only the qualified 2,324,752-byte application at `0x30000`, SHA256
  `e450a715ccbd1fc56ad2df65b3857ea9fd463c38f53f9eda3971a64dfa04a67e`,
  with flash hash verification. No partition, endpoint or other setting changed.
- Opening serial reset Jeff even without an explicit reset request. The post-flash
  warm reset interrupted SD startup and reproduced the existing ACMD41/FAT timeout;
  recovery required Cam's full power cycle. Afterwards SD and speaker were online,
  show idle and error pending off. Report count was 3, including the new genuine
  SD fault report. This is a deployment recovery observation, not a resolved SD issue.
- Live Mini HA discovered 47 entities, including all 21 jaw entities with expected
  names. Installed the native compact tuning UI through authenticated WebSocket
  configuration, preserving existing Props cards and a full JSON backup. Jeff's
  player has a Tune jaw row linking to the separate numeric/fine/coarse tuning view.
- Verified STOP: disabled mode, applied 0, `pulses_disabled`; Enter from disabled:
  manual mode, applied 544, `manual_entered`, tuning controls available; then STOP
  again left output disabled at 0. First Enter during automatic breathing returned
  `entry_rejected` because PWM was pending. Keep this conservative guard: STOP,
  then Enter if that occurs. Enter from disabled re-enables the held command.
- No Open/Close, exact target, endpoint capture or Save commands were issued.
  Saved endpoints remain the old-card fallback 544/1368 us until explicit Save.
  Observed internal heap free/minimum was 46,075/28,439 bytes. Independent Mini
  read-only checks found all nine health checks passing, no errors/warnings/OOM or
  restarts, with HA/MQTT memory about 311/2.7 MiB. This proves deployment and the
  bounded control transitions, not physical endpoint or persistence acceptance.

## Owner calibration acceptance

Cam reported "Seems perfect!" after hands-on testing. A subsequent read-only HA
snapshot shows active closed/open endpoints **654/1404 us**, matching drafts,
unsaved changes off, automatic mode and `automatic_resumed`. The production
controller activates new endpoints only after successful SD save/readback, so
this establishes the live Save path as well as Cam's control acceptance. No
pending error; report count remains 3. Evidence:
`build/validation/story-010-jaw-calibration/owner-test-followup.json`.
No additional movement, save or reboot was commanded for this check.

Cam subsequently confirmed: "Yup it survived reboot. Excellent." This closes
Jeff's owner-observed persistence check for the saved 654/1404 us calibration.
The final read-only closeout probe found Jeff offline (HA entities unavailable),
so no second live post-reboot measurement is claimed. No reset, motion or SD
write was issued during closeout. All 15 portable C++ suites with ASan/UBSan and
14 Python tests passed again; the native build/size check passed with the same
qualified image hash and 85-byte IRAM margin. Independent networking and
motion/storage source reviews found no blocking issues. Private validation
artifacts remain under `build/validation/story-010-jaw-calibration/`.

## Remaining acceptance

Anthony's deployment and individual mechanical calibration remain separate.
Jeff's paired playback with the new endpoints has not been separately measured.
The steps below retain the supervised commissioning procedure for future skulls.

## Physical acceptance procedure (operator present)

For a newly deployed skull, finish startup with the
speaker connected and show idle, then follow the [operator steps](../../home_assistant/README.md#jaw-tuning-story010):

1. Enter manual control. If `entry_rejected` occurs during automatic breathing,
   choose STOP and Enter again; do not bypass the pending-PWM guard.
2. Choose Open 1 us once and verify both the displayed microseconds and actual
   movement. Watch the linkage throughout each deliberate small step.
3. Find the desired closed position and capture Set closed. Teeth contact and
   free rod retraction are normal for Jeff's linkage, not evidence of strain.
4. Open in small steps, particularly when widening the opening; capture Set open
   only after physical inspection. There is no automated sweep.
5. Explicitly Save, confirm `saved_to_sd` and both saved values, then verify those
   values after a later controlled reboot. Keep power stable through Save.
6. Explicitly Resume normal movement or STOP before leaving.

Jeff's owner-accepted controls/calibration, live SD Save and owner-observed reboot
persistence are recorded above. Paired playback with these endpoints and
Anthony deployment/calibration remain unqualified.
Preserve fresh status/fault/report evidence before a further reset or flash.
