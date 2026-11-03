# Story 007 — Home Assistant diagnostics and skit control

Status: In Review — Jeff HA/MQTT device checks passed; positive audio and Anthony/paired hardware qualification remain pending.
Date: November 1, 2026.

## Requested behavior

Use Home Assistant as the user interface rather than building a custom fleet
website. Each skull advertises its diagnostics and controls using MQTT discovery.
List all validated scripted skits available on its own SD card. A skit selector
only changes selection; a separate Play button starts the selected skit.

Jeff plays his B part when directly triggered. Anthony plays A and requests the
same skit from Jeff when his peer connection is ready; Anthony can play his part
alone when Jeff is absent. This also applies to the existing random/Matter trigger.
Do not silently replay solo after an already-started paired transaction fails.
Preserve startup/greeting/cooldown gates and reject unknown/non-skit paths.

Cam authorized writing/building/flashing/testing Jeff, a temporary HA/MQTT test
system on this Mac, and using Wi-Fi credentials already present on Jeff's SD card.
Never print credentials or commit raw captures/network settings. Anthony is not
attached; physical paired acceptance requires a later check. The speaker was
previously absent; device tests must report that limitation if still absent.

## Design

- Preserve the native ESP-IDF/Matter/Bluetooth/audio runtime and existing protocol.
- Secondary Wi-Fi station uses optional existing SD credentials; primary Matter
  remains its Wi-Fi owner. No AP and no Wi-Fi/fabric erasure or copying credentials
  between devices. Missing/invalid networking leaves local operation available.
- ESP-MQTT client with bounded buffers/outbox, stable per-chip identity, retained
  discovery, Last Will availability and periodic non-retained typed state.
- Device-level discovery supplies grouped sensor/binary-sensor/select/button
  entities. Play availability follows application readiness. Selection is an
  exact catalog basename mapping; no arbitrary file or console commands remotely.
- Control callbacks copy one bounded pending command; the existing diagnostic
  owner validates and applies it. Ignore retained, duplicate, fragmented or stale
  controls; no delayed playback queue or control replay after reconnect.
- Use SD MQTT settings if supplied, otherwise separate NVS broker configuration
  set through an idle-only USB mqtt-config command; passwords never echoed.
- Reuse lifecycle/status/fault evidence. Saved-report count is cached from fixed
  SD file metadata only at boot/idle after a report changes, not on every publish.
  Full SD file download remains separate from HA scalar entities.

## Primary sources / bounded research

- [HA MQTT discovery](https://www.home-assistant.io/integrations/mqtt/)
- [MQTT Select](https://www.home-assistant.io/integrations/select.mqtt/)
- [MQTT Button](https://www.home-assistant.io/integrations/button.mqtt/)
- [ESP-MQTT](https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32/api-reference/protocols/mqtt.html)
- [ESP-IDF Wi-Fi station lifecycle](https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32/api-guides/wifi.html)
- [Research comparison](../research/2026-11-01-prop-monitoring-interfaces.md)

## Acceptance and evidence

Machine checks: production-source show routing/validation tests, config parsing
without credential leakage, bounded/escaped JSON discovery/state, native build
and linker/resource checks. Test negative commands and reconnect/no-replay.

Jeff checks: preserve status/fault-log/saved SD evidence first; verify current
application/layout against retained rollback before app-only upload; preserve
Wi-Fi and speaker settings. Verify IP/MQTT connectivity, complete discovered skit
list, state typing, selection acknowledgements, safe play refusal without speaker,
retained/invalid command rejection, reconnect and persistence after reboot.
Positive audible playback and paired behavior need their own device acceptance.

Private execution evidence: ignored `build/validation/story-007-ha/`.

## Temporary test infrastructure constraint

Mac initially had only 654 MiB free. Registry manifest showed official HA 2026.10.4
ARM64 image about 615 MiB compressed before unpacking/runtime, so no safe HA pull.
User was asked whether to free several GB or leave the HA app test pending.
A 13 MB compressed Mosquitto image fit; an authenticated, isolated temporary broker
is running on the Mac LAN. No unrelated files/images/caches were deleted.
This permits real MQTT/device testing but is not an HA frontend qualification.

## Implementation and machine results

- Production portable model, normalized read-only SD network parser, native
  Wi-Fi/MQTT adapter and control-owner command mailbox implemented. No custom
  website, new partition or replacement radio/animation framework.
- All portable host suites passed with ASan/UBSan, including 52 show scenarios,
  HA JSON/discovery bounds/types/options, config/credential input handling,
  lifecycle measurement validity, fault retention and storage/audio tests.
- Independent review corrected callback subscription ownership via documented
  MQTT_USER_EVENT, pending-to-saved report-count refresh, idle SD metadata before
  command preparation, and invalidation of a failed Bluetooth signal sample.
- ESP-IDF 5.5.4 native build passed. Initial compiler-only indentation errors were
  corrected; SDK deprecation/Kconfig warnings remain. Final application is
  2,312,320 bytes (0x234880), existing factory partition 42% free. Linker size
  evidence records **85 bytes IRAM free**: this is a material tight margin.
- User freed disk space; official Home Assistant2026.10.4 and authenticated
  Mosquitto run as isolated temporary Docker containers on this Mac. Neither has
  an automatic restart policy. Fixture using production model/catalog validated
  actual HA discovery and select/button services, then was fully removed.

## Jeff upload and end-to-end results

- Preserved status/fault-log/saved SD reports before reset. Verified existing
  application against the retained Story 006 installed binary and the actual
  partition table against the unchanged native layout (esptool digest checks).
- Uploaded only the application at 0x30000; hash verification passed. Source hashes
  match the retained installed candidate. Bootloader/partitions/fabrics and card
  config/assets were not rewritten. USB stored generated test-broker credentials
  in its separate NVS namespace without printing them. Wi-Fi came from the
  existing SD configuration, as Cam requested.
- Real HA automatically discovered **25 Jeff entities and all 11 validated skits**.
  Diagnostics included SD mounted, both existing reports, speaker/peer off,
  unknown absent Bluetooth strength, real Wi-Fi dBm, startup/SD measurements and
  application state. The speaker was absent throughout this test.
- Actual HA select.select_option changed Jeff's selection and received the
  device's selected acknowledgement. Invalid ../../config.txt was rejected with
  invalid_skit and preserved selection. Raw PLAY was rejected with not_ready;
  HA's Play button was unavailable. No PCM playback or skit began.
- Retained valid selection and PLAY were seeded while Jeff was held in reset.
  After boot he kept his default selection and command_result=none: stored
  commands were not replayed. Test retained control topics were cleared.
- Restarted the temporary MQTT broker while Jeff kept running. He reconnected,
  republished discovery/state and accepted a new HA selection, without command
  replay. Independent offline test held the MCU in reset: HA selector became
  unavailable after **22.87 seconds**, then recovered on release/reconnect.
- Both original saved SD reports were byte-identical after all tests/reboots.
  Speaker config remained JBL Flip 6. Final status: SD mounted, idle, speaker=0,
  peer=0, zero PCM callbacks/underruns, radio fault=0, no pending report.
- Jeff idle internal-heap minimum observed about 37.3 kB (37,343 bytes),
  largest free block 45,056 bytes, current free roughly 48 kB. These are
  no-speaker/no-audio observations, not active-audio margin qualification.

Temporary HA: http://127.0.0.1:8123 (LAN http://192.168.1.99:8123). Private admin
login and API tokens are saved under ignored evidence directory; never paste
those values into tracked notes. Runtime control/API helpers and schema fixture,
actual HA states/services, installed/source hashes, flash logs, live captures,
readback files and reconnect/availability assertions are in
`build/validation/story-007-ha/`.

## Remaining acceptance / limits

Anthony was not flashed or accessed. Automated production-source tests cover
solo/paired routing, exact skit validation, cooldown, startup/greeting, and
no-replay failure behavior. Positive audible Jeff playback and physical paired
behavior still need speakers/Anthony and human observation. New networking must
also be qualified under active audio and primary Matter load before claiming full
performance acceptance. TLS, arbitrary Wi-Fi failures, exhausted broker outbox,
and all possible callback timing faults were not exhaustively device-tested.

Later UI acceptance is recorded in [the native dashboard guide](../../home_assistant/README.md):
Cam approved grouping selection, Play and Now playing into one card. The Halloween
dashboard is now the default, and an actual UI Play request for Pumpkin flowers
was accepted and observed through preparing/playing/idle without a pending fault.
Selection alone remained idle. This narrows the earlier no-speaker limitation for
the command path; audible quality and Anthony/paired hardware remain separate.

## Chrome UI verification

Inspected Cam's logged-in Chrome Home Assistant tab directly, including Overview,
Skit more-info/dropdown, Jeff's full device page and the repair notice. No device
commands, settings changes, firmware writes or credential changes were made.

All eleven skit options were present. Play was visibly disabled, speaker/peer
disconnected, stream off, show idle, SD mounted, saved reports 2 and pending error
OK. This boot reported one SD attempt/zero failures, 111 ms mount, 145 ms mount+config
and 3629 ms control startup. Wi-Fi signal was -56 dBm. Uptime advanced during the
inspection, confirming live updates. Current heap around 49,559 bytes and minimum
36,339 bytes were visible; this remains idle/no-speaker evidence.

The single HA repair is an unset country in the temporary HA instance, not a
reported Jeff fault. Left it unchanged and returned Chrome to Jeff's device page.
Overview contains the two control favorites; the 23 diagnostic entities are on
the device page rather than all appearing as Overview cards. Current skit is
blank while idle, and firmware metadata is still generic 0.1.0-probe. HA currently
shows saved-log count/ID, not an SD-file viewer/downloader. These are presentation
or scope limitations, not missing telemetry or evidence of failed playback.

Private screenshots and current AX state:
`build/validation/story-007-ha/browser-review/`.

## Follow-up: country, display, Overview and saved-report access

Cam requested setting Canada and fixing the issues found in Chrome. The temporary
HA instance is now configured with country **CA**; an authenticated API check
confirmed the value after restart. This removes the unset-country setup issue.
The Overview uses existing native HA favorites for the key prop diagnostics,
rather than introducing a custom dashboard website. Physical/browser acceptance
of the final saved-report display is recorded separately from configuration.

The updated firmware reports **Idle** when no skit is active and advertises its
application version plus a short ELF SHA256 fingerprint. The build seen during
follow-up was 0.2.0-ha+165e5282ca7a; later fixes have their own identity. This avoids
confusing an empty field with missing telemetry or a generic probe label with a
unique build. Selecting a future skit still does not imply it is playing.

### Native HA report access design

- A 26th device entity, Open saved reports, requests a read-only collection through
  the existing authenticated MQTT connection. No prop HTTP server, public file
  URL, custom frontend or SD write/delete API is introduced.
- Native transfer is scoped to slots 0–7 and a fresh 16-hex request ID. It snapshots
  one file, capped at 14,335 bytes, in PSRAM only when the control owner is idle.
  Ordered binary chunks are at most 256 bytes; begin/end metadata include exact
  size and the end carries standard zlib-compatible CRC32.
- The small `twoskulls_diagnostics` HA integration consumes a fresh report-slot
  bitmask, fetches slots sequentially with bounded timeouts, validates scope,
  completeness, order, duplicates and checksum, then saves verified raw copies
  privately. Late/old request IDs cannot overwrite a new collection.
- Home Assistant's built-in Notifications contains a report index and full
  escaped plaintext report cards. No file body is put in sensor attributes or
  recorder history. Cache names are SHA256-based; identical copies deduplicate
  and older versions are preserved. No automatic cache deletion was added.
- Busy, unavailable, malformed, corrupt or incomplete reads fail explicitly and
  do not replace a verified cached report. The bridge never retries automatically.

Install/use details and primary HA API sources are in
[the integration README](../../home_assistant/custom_components/twoskulls_diagnostics/README.md).
The existing runtime evidence/private login files remain in their original
ignored paths; they were not rewritten into tracked documentation.

### Receiver and real-HA verification

Six production Python protocol/async tests passed without an HA dependency:
maximum 14,335-byte binary roundtrip; identical duplicate begin/chunk/end frames;
wrong slot/request scope; oversized/out-of-order/conflicting/corrupt data;
explicit device errors and deadline cleanup; and exact private caching/plaintext
escaping. HA check_config passed and the restarted 2026.10.4 instance registered
`twoskulls_diagnostics.refresh_logs` and confirmed country CA.

An isolated disposable MQTT fixture then passed through the actual HA service,
full-size transfer, exact cache and native notification APIs. WebSocket inspection
confirmed every escaped character of the 14,335-byte report. Duplicate frames and
an unrelated request ID did not corrupt it. Only the fixture's two notifications
and verified cache file were removed afterward; evidence was preserved privately.
No real Jeff request was sent by that fixture.

### Initial actual Jeff transfer finding and correction

Open saved reports was requested once through the logged-in Chrome UI.
fault-0.log, 413 bytes, was verified and shown in a native notification. The second
5,689-byte report received an explicit native **timeout** error and was not cached
or presented as complete. This was not a receiver checksum success or a fully
qualified two-slot transfer. First-failure evidence is preserved at
`build/validation/story-007-ha/fixes/ha-saved-reports-first-failure.json`.

No automatic retry or special-case receiver workaround was added. Bounded SDK
research and metadata-only traces showed steady roughly one-second frame
progression caused by the MQTT worker's default quiet TCP poll cadence, not a
verified lost-ACK condition. The supported MQTT poll interval was changed from
1000 ms to 50 ms while retaining the original protocol, one in-flight frame,
20-second native deadline and 30-second collector deadline. See the
[throughput evidence and decision](../research/2026-11-02-mqtt-log-export-throughput.md).

After the corrected native build was flashed, Open saved reports was invoked
once through the logged-in HA Overview control. An independent read-only observer
sent no request of its own and verified both cached files byte-for-byte against
the preserved pre-flash SD evidence: slot 0, 413 bytes, SHA256
`384ceb1748afa6731ecda421a16b081bff442327628b582731cf3cb549b48f3c`;
slot 1, 5,689 bytes, SHA256
`65179f9c17092aaa1b3b75b8fb0973aed495d60ca5d506ed50024980ecff8095`.
Authenticated HA WebSocket inspection confirmed each complete escaped report body
in its native notification and an index listing both successful reads. The
metadata trace recorded 413 bytes in 4 frames / 0.354 s and 5,689 bytes in 25 frames / 2.049 s.
This is an observed healthy no-speaker run, not a guaranteed wireless bound.

Evidence: `build/validation/story-007-ha/fixes/ha-saved-reports-validation.json`
and `log-throughput-after-ui.txt`. The main Chrome verification opened Notifications
and visibly confirmed both verified report entries and their full plaintext
contents; screenshot `fixes/screenshots/03-saved-reports.png` records the result.
Country Canada and eight native Overview favorites were also visibly verified
(`01-canada.png`, `02-overview.png`). This is an HA-native interface, not a separate
prop website. The actual corrected firmware application is 2,315,248 bytes; its
native linker check still records 85 bytes IRAM free. Keep that material limit
visible when planning further code. Final installed-image/resource evidence is
under the same ignored fixes directory.

Existing speaker/Anthony/active-audio qualification limits above remain in force.
The final post-fix broker-reconnect pass is recorded separately after completion;
do not infer it from the pre-fix network test or the report-transfer fixture.

### Final installed and visible verification (November 2)

The pacing candidate is 2,315,248 bytes, SHA256
`85d2945e8093119245a6055b1a3129df3add8d9162115098c790744e9bc6e17a`.
Before this second application-only upload, captured status/fault-log/saved reports
and verified the installed prior application and unchanged partition table by
digest. Application write at 0x30000 passed hash verification. Static DRAM remains
102,476 bytes and IRAM remains 130,987 / 131,072 bytes (85 bytes free).

Chrome visibly shows firmware `0.2.0-ha+0b6b2d38dc15`, 26 entities, Current skit
Idle, SD mounted, zero SD initialization failures, two saved reports, no pending
error, and disconnected speaker/peer. Both saved-report success entries and full
report text were inspected in Notifications. Overview has eight saved favorites:
current skit, speaker/peer connection, saved count, pending error, skit selection,
Play, and Open saved reports. Canada is configured and the country repair is gone.
Final screenshots are under `fixes/screenshots/`; Chrome was left on Overview.

A further broker restart with this installed image recovered live increasing
uptime, preserved selection and command result, and left the show idle without
command replay (`fixes/pacing-reconnect-validation.json`). Idle internal heap
minimum after transfers/reconnect was about 36 kB. This does not qualify active
audio or Anthony hardware. No commit or push was requested for these changes.
