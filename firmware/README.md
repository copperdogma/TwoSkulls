# Native WROVER skull application

This is Story 001's native application port; complete two-skull acceptance remains
pending. It targets classic `esp32` with ESP-IDF **5.5.4** and managed ESP-Matter
**1.5.0**, with one Bluedroid BTDM owner. SD config selects the role: primary
Anthony starts a momentary Matter endpoint and the legacy secondary BLE client;
secondary Jeff starts a standalone legacy GATT server and requested Polo response without
starting Matter. Both connect their configured A2DP speaker and drive jaw GPIO23
and eyes GPIO32/33. The role-specific `Initialized - Primary.wav` or
`Initialized - Secondary.wav` is preloaded into PSRAM and played once after A2DP
media readiness, with 800 ms of silence first (including the added500ms cold-start margin). The same application is flashed on Anthony and the new Jeff board.
Both completed a paired Starbucks skit with zero reported underruns; Cam confirmed
clean dialogue and correct turns. An Apple Home trigger also completed imitations
with zero underruns on both, and the switch returned to off. See the [two-board evidence](../docs/validation/2026-10-15-jeff-first-boot.md).
Matter On requests a paired skit after startup and cooldown, requiring the
configured speaker and Jeff. Serial `tone` retains the quiet diagnostic tone.
Serial `play /audio/<filename>.wav` explicitly tests one local asset without Jeff.

## Eye status priority

Counted diagnostics have highest eye priority: repeating three flashes for SD
mount failure, five for config failure, and the two/four role flashes at boot.
Next is a missing Bluetooth speaker: both eyes alternate 255/255 brightness for
one second and the normal dim 100/255 for one second. The warning starts at 30 s
boot age if no connection was made, or on the next control tick (normally 20 ms)
after an observed A2DP connection drops. A late first control tick does not grant
an extra grace period. Reconnection clears it immediately on the control owner.
Stream readiness alone does not trigger this warning. Speech/idle brightness is
lowest priority; jaw speech, boot check and breathing remain independent.
The phase uses elapsed time, so delayed ticks skip phases rather than queueing
flashes. See [Story 008](../docs/stories/story-008-speaker-eye-warning.md).

## Build

Activate the matching ESP-IDF installation and ESP32 toolchain, then:

```sh
idf.py -C firmware -DIDF_TARGET=esp32 build
idf.py -C firmware size
```

The manifest and generated target-specific `dependencies.lock` are tracked.
`sdkconfig.defaults` is the reproducible starting configuration; generated
`sdkconfig`, managed components and build outputs are ignored. Use an independent
build/config path to compare configurations; do not replace another project's
SDK files. For a fresh machine, Espressif's matching `install.sh esp32` installs
the required classic-ESP32 compiler in addition to any existing C3 tools.

One checked upstream correction is applied to a build-local copy of the Matter
Bluedroid backend. The original source hash and exact replacement are enforced
by `cmake/matter_bluedroid_patch.cmake`; managed component files are unchanged.
See the [implementation log](../docs/validation/2026-10-15-implementation-log.md)
for the error, research and decision.

## Board and first upload

Verified development target: the user's v3.0 WROVER PCB, ESP32-D0WD-V3 rev 3.1,
4 MB SPI flash, onboard SD slot. The v2.1 board has no onboard slot and is not an
equivalent target. Verify the actual PSRAM boot result before acceptance.

Before a first native upload, preserve and verify the complete original flash.
The new partition table at `0xC000` has a 64 KiB NVS partition at `0x10000` and a
factory app at `0x30000`. This is a layout transition from Arduino, not an ordinary
app update. Initialize only the new NVS partition deliberately on that first
transition; app startup fails an NVS error instead of erasing automatically.

Generate/use this build's `flash_args` or `idf.py flash` to flash the proper
offsets. Do not paste C3 offsets or an arbitrary old app image. After the initial
native layout is established, use `idf.py app-flash` for application updates;
this preserves this board's Matter fabric and Wi-Fi data. Do not erase NVS on
ordinary updates, and do not copy the previous C3's device/fabric credentials.

SD pins are fixed: CLK14, CMD15, D0=2, 1-bit at 20 MHz. Card/config reads and
audio assets are unchanged, and mount failure never formats the card. Idle-only
fault diagnostics now write the bounded reports described below. GPIO1 remains
UART0 TX; the old trigger input is not installed. Servo signal is GPIO23
for the native jaw output. Eye outputs use GPIO32/33. Preserve the existing
current-limiting resistor and separate 5 V servo supply with common ground.

## Diagnostics and commissioning

Start with the [debugging workflow](../README.md#debugging-the-native-firmware)
for collecting serial/SD evidence before rebooting, finding the card’s
`diagnostics` folder on a computer, and isolating a failure.

115200 baud serial commands:

- `status`: current SD, source, media/peer readiness, callback timings, heaps and fault logger state.
- `sd-retry`: boot-failure console only; request the next SD mount attempt now.
  The failure console also supports `status` and `fault-log` before radio/audio
  workers start. It allows no card writes, playback or resets. Startup performs
  one quick retry, including one bounded CMD12 read-abort before reinitializing,
  then waits 30 seconds between failures while diagnostics remain available.
  DAT3/GPIO13 is pulled up even in 1-bit mode; GPIO12's strap is unchanged.
  Persistent internal card-not-ready states may still need a true power cycle;
  no GPIO-controlled card-power switch has been verified on this board.
- `fault-log`: dump the pending RAM/recovered compact fault report over serial.
- `fault-saved`: read the eight fixed SD report slots over serial while idle.
  `faultarchive` BEGIN/DATA/END records encode exact bytes as hex with offsets,
  length and result. Missing slots report ENOENT; no files are changed. This can
  take several seconds and holds the control owner until complete.
- `fault-test`: idle-only controlled error to exercise normal capture and export.
- `speaker-name`: read the speaker name on the installed SD card and the name
  selected for the current boot.
- `speaker-name JBL GO 2`: save an exact speaker name to `config.txt` over USB;
  use `speaker-name JBL Flip 6` to switch Jeff back after the demonstration.
  This is idle-only and requires a deliberate `reboot` to apply. It preserves
  other config bytes/settings, validates/readback-checks the replacement and
  retains the first original in `config.speaker.original`. Configs are bounded
  to 4096 bytes; names must be 1–79 bytes without control characters, `=` or
  leading/trailing whitespace. Unresolved `config.speaker.tmp` or
  `config.speaker.previous` stops another write; preserve/recover these files
  rather than deleting them blindly. The FAT transaction is recoverable, not
  power-fail atomic. Keep power connected until the command returns.
- `asset /audio/<filename>.wav`: inspect the actual card's parsed A/B intervals while idle.
- `heap-start` / `heap-stop`: measure a bounded local heap-minimum window, then restore boot history.
- `tone`: request a quiet test tone; ignored while unavailable/busy.
- `trigger`: request a normal paired skit with the same gates as Matter On.
- `pair /audio/<filename>.wav`: play a specific skit through the normal paired readiness/cooldown gates; requires Jeff.
- `play /audio/<filename>.wav`: explicit local playback diagnostic; uses the configured A/B role mask for scripts and bypasses the peer requirement.
- `jaw-test`: repeat the gentle servo check while idle.
- `retry`: request an idle connection/media check and peer discovery attempt.
  Both roles also reconnect their speaker automatically forever: three saved-address
  attempts, a finite inquiry, then a fresh episode with five-second backoff.
  Media-start retries continue until the stream is ready. Classic inquiry lasts
  10.24 seconds plus any name discovery; first pairing requires discoverability.
  Start/state/candidate logs distinguish an absent configured speaker from a
  discovery path that never started.
- `pairing`: print onboarding codes and, if uncommissioned, open a commissioning
  window when needed. It does not open an additional fabric on a commissioned device.
- `reboot`: deliberately restart the prototype.
- `factory-reset CONFIRM`: explicitly discard this prototype's Matter/network
  provisioning and restart. Do not use it for ordinary app updates.

Capture a bounded run with the ESP-IDF Python environment:

```sh
python scripts/serial_probe.py --port /dev/cu.usbserial-10 \
  --output build/validation/probe-boot.log --seconds 30 --reset
```

The capture filename must be new. Logs may contain Matter setup payloads; keep
them in ignored/private directories. Use the actual boot-reported QR/manual code
in iPhone Home's Add Accessory flow. This uses SDK prototype/test attestation and
is a new accessory; label it Anthony prototype. Commissioning success must be
verified in actual Home and after reboot before treating it as a C3 replacement.

The BLE dispatcher waits for Matter's asynchronous GAP registration, forwards
Matter events first, and then feeds copied events to the application worker.
Matter and mbedTLS ordinary heap allocations use the SDK external-memory allocators; task
stacks, DMA and controller requirements retain their internal-memory constraints.
Runtime capability-specific minima still need measurement under all workloads.
The radio queue keeps scheduler control in internal memory and copies its
task-only event payloads through PSRAM. Stack high-water marks are reported.

After an exact configured-name speaker connects, its name/address are saved in
a separate NVS blob. Reboots reconnect by address without needing discoverability.
Classic remains nonconnectable/nondiscoverable, following the source-role policy.
Unselected addresses are rejected. Up to three direct attempts use five-second
backoff; a confirmed live ACL must tear down before another open.
One finite name inquiry follows three failed saved-address attempts, then a new
connection episode starts automatically after five seconds. This repeats indefinitely
on both Anthony and Jeff, including when the speaker powers on late. Pending opens
and ACL teardown remain event-gated; warnings after 75/60 seconds do not disable
recovery, which resumes automatically when the stack completes the operation.
Abrupt board resets can leave
the speaker's old link alive temporarily, so reconnect can take longer than a
clean first connection.

Bluetooth host/controller are initialized once in BTDM, and neither Classic nor
BLE memory is released after commissioning. Queue overflow or failed peer
teardown latches a diagnostic fault; capture status/logs and deliberately reboot
instead of continuing with uncertain radio state.

Jeff's client contract preserves the legacy notification CCCD `0x0001`, raw
filename and notify-then-readback check. Error/timeout closes the connection to
quarantine late replies; no automatic skit retransmit occurs. A matching ACK
does not prove Jeff played audio. On the new secondary board, subsequent paired
Starbucks and Home-triggered imitations runs completed with zero underruns on
both boards; Cam separately confirmed clean audible turn-taking.

## Acceptance boundary

Follow [Story 001](../docs/stories/story-001-single-wrover-matter-prototype.md).
Successful compilation, SD mounting, synthetic PCM callbacks, audible sound,
Apple Home operation, peer compatibility and a complete two-skull show are
distinct milestones. Preserve the recorded Arduino/onboard-SD baseline and the
private original image until the complete replacement passes bench/yard tests.

## Offline audio preflight

Run all portable production-source suites, including sanitizer checks, with:

```sh
python3 scripts/run_host_tests.py
```

This requires Python 3 and a C++17 compiler with AddressSanitizer and
UndefinedBehaviorSanitizer. It creates fixtures and binaries in a temporary
directory and does not access either connected board.

Before the full show port, validate a copy of the card's audio directory:

```sh
python3 scripts/validate_assets.py sd_card_files/audio
python3 -m unittest discover -s scripts/tests -v
```

The preflight is standard-library-only and read-only. It emits JSON and returns
nonzero for invalid RIFF/PCM, script timing, or missing asset pairs. It expects
PCM stereo 44.1 kHz/16-bit and preserves start-inclusive/end-exclusive role
boundaries using integer sample-frame rounding. It is an offline reference
contract, not the native firmware parser or proof of playback. See the
[asset results](../docs/validation/2026-10-15-asset-preflight.md).

## Startup announcement

The configured primary/secondary role selects its corresponding startup file.
The short startup WAV must be PCM 44.1 kHz, stereo, 16-bit and at most 1 MiB of PCM. A portable
RIFF parser validates the whole file and locates its actual PCM offset.
Loading and configured volume scaling happen before radio initialization,
using explicitly allocated PSRAM. The immutable buffer remains for this boot;
the audio callback only copies PCM and pads silence. No SD writes are made.

Playback is once per boot after the first ready stream, independent of Jeff.
An observed link loss during playback cancels its tail; speaker reconnection
does not restart it. Missing/invalid audio is logged and leaves the existing
tone diagnostics available (and Matter on primary). The diagnostic task polls stream readiness
every approximately 100 ms, so very brief link transitions can be missed.
The startup clip remains preloaded; subsequent clips use the bounded SD producer
and PCM block pipeline described in the application port record.

Portable parser tests:

```sh
c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -Ifirmware/main firmware/main/skull_wave.cpp \
  scripts/tests/test_skull_wave.cpp -o /tmp/twoskulls-test-wave
/tmp/twoskulls-test-wave 'sd_card_files/audio/Initialized - Primary.wav' \
  'sd_card_files/audio/Initialized - Secondary.wav'
```

## Jaw and eye bench outputs

LEDC uses separate low-speed APB timers: jaw timer0/channel0 at 50 Hz/16-bit;
eyes timer1/channels1/2 at 5 kHz/8-bit. Story010 adds per-card `config.txt`
keys `jaw_closed_us` and `jaw_open_us`. Old cards default to the legacy
544/1368 us endpoints. Values must be distinct integers in 544..2400 us;
reversed endpoints are supported. These electrical command bounds do not prove
safe mechanical travel. Normal 0..80 logical motion interpolates between the
saved endpoints; those logical values are not measured jaw angles.

Initialization keeps jaw pulses off and eyes dim (100/255). After valid SD
configuration loads, jaw output starts at that card's closed endpoint. The control loop
runs a gentle 0→30→0 degree check over 1.2 seconds on its first tick. Serial
`jaw-test` repeats it while startup is idle. Speech overrides that check.
The existing diagnostic task controls outputs with a 20 ms rate gate; no new
task is allocated. Eye brightness rises to 255 during startup speech.

### Home Assistant jaw tuning (Story010, Jeff deployed)

The dashboard source adds **Jeff jaw tuning** as a dedicated subview. See the
[operator steps](../home_assistant/README.md#jaw-tuning-story010) and
[Story010](../docs/stories/story-010-sd-jaw-calibration.md). The firmware and
dashboard are now deployed on Jeff: live discovery has 47 entities and
STOP/Enter/STOP passed, ending disabled at applied 0. Cam then accepted the
controls, saved closed/open 654/1404 us and resumed automatic movement. Those
active values match drafts with no unsaved changes; Cam confirmed they survived
reboot. Anthony deployment remains pending. The initial
software qualification was followed by an application-only verified flash;
see Story010 for rollback evidence and the SD warm-reset recovery observation.

Tuning requires completed startup and quiescent show/audio/tone/content files.
If Enter is rejected during automatic breathing because PWM is pending, STOP
then Enter; retain the conservative pending-write guard.
The existing control owner holds the current applied PWM command; all playback
sources and automatic jaw gestures are blocked. Fine/coarse open/close moves are
1/10 us; exact targets may differ by at most 25 us per accepted command. Each
session is bounded to its starting saved span plus 50 us, clipped to 544..2400.
There is no automatic sweep, torque/current sensing or physical angle feedback.

Capture closed/open into drafts, then explicitly Save both to SD. The writer
preserves other config bytes and verifies readback; first save adds keys to old
cards. No unsolicited default-migration write occurs during boot. Retain
`config.jaw.original`, and preserve/recover unresolved `config.jaw.tmp` or
`config.jaw.previous` before retrying. Both speaker and jaw writers refuse
unresolved transactions from either writer. FAT replacement is recoverable,
not power-fail atomic; keep power stable until the result returns.

STOP, Wi-Fi/MQTT loss or 90 seconds without accepted tuning input requests zero
PWM and latches disabled mode. Closed-browser detection relies on inactivity;
the broker can remain connected after closing a browser. Leave with STOP or
explicit Resume. Resume discards drafts and slews at most 10 us/20 ms toward
normal targets. Output errors latch a fault that prevents motion until reboot;
best-effort zero updates continue despite an eye-channel failure. A permanently
pending/failing driver can prevent zero output: `applied_us`/enabled remain
truthful and results indicate a pending disable. This is not a physical emergency
stop, guaranteed torque release or cancellation of a command another skull has
already accepted. STOP cancels local active/prepared shows and pending starts.

The startup loader computes 20 ms RMS windows in PSRAM after applying volume.
The audio callback only publishes its consumed source-frame cursor; the control
task reads the matching envelope and applies one fast speech-jaw follower.
RMS 200 is the closed target and RMS 2400 reaches the existing 80-degree limit;
rising/falling targets cover 68%/56% of the remaining distance each 20 ms tick.
This preserves short speech peaks and closes between them without the old
cascaded smoothing. See [Story 004](../docs/stories/story-004-expressive-speech-jaw.md)
for Anthony’s accepted tuning trial and the remaining Jeff/paired qualification boundary.
This tracks PCM delivery, so Bluetooth speaker buffering may make physical jaw
motion lead the sound. Audible/visual timing still needs owner assessment.
Tone diagnostics and full skit playback are separate: the quiet test tone does
not drive jaw animation. Full skit and Marco audio use consumed PCM animation
metadata; live synchronized Jeff acceptance remains pending.

## Testing a chosen skit over serial

At 115200 baud, send a complete line with the exact card filename, including
spaces. For example:

```text
asset /audio/Skit - ju jubes.wav
play /audio/Skit - ju jubes.wav
```

`asset` inspects the card's role intervals while idle. `play` requests one local
playback, bypassing the peer requirement while retaining the configured A/B role
mask. On Anthony, Jeff's lines are silent gaps. Wait until startup is finished,
the speaker stream is ready and the current audio has stopped; the console
reports `Local diagnostic play accepted` or `ignored` and completion/underruns.
After the one-shot test, the usual show/Marco scheduling resumes automatically.
The command accepts any catalogued WAV, not just the example above.

For both skulls, send `pair /audio/Skit - ju jubes.wav` instead. This requires
the configured speaker and Jeff to be ready, plus the normal show/cooldown gates.
It plays that named skit once using the existing peer protocol.

These commands are always present in the shared native firmware. No temporary
startup selection or firmware rewrite is needed for another skit. Keep one
interactive serial session open across commands: depending on the USB driver,
opening a new connection can toggle reset lines even without `--reset`.

## Idle breathing

While no audio is active, jaw breathing repeats every seven seconds, measured
from the start of each cycle: open to30° over2s, hold100ms, close over2s. This
restores the original movement using the existing nonblocking control tick.
The first deadline is seven seconds after the first control tick. Eyes remain
dim. Speech, startup preroll, diagnostic tones and manual jaw checks take
priority; canceled breaths do not resume an old phase or replay missed cycles.
The servo's extra load may help the owner's battery packs stay awake, as with
the original build; that supply-specific result has not been remeasured here.

## Native application and startup diagnostics

Both roles catalog WAV/scripts, prebuffer PCM and mask A/B lines at exact
source-frame boundaries. The primary selects from the weighted top-three pool and
uses the existing BLE filename/ACK/readback protocol before starting a normal
paired skit. All supported audio completions impose a ten-second trigger cooldown.
Startup audio precedes peer discovery. Anthony says Marco about every five
seconds while scanning after init. On peer readiness he completes any current
Marco or plays one, waits a nonblocking300ms output-tail guard, then explicitly
requests Polo using the legacy filename protocol. A connection alone is silent
on Jeff. This preserves a clear audible Marco-then-Polo greeting.
The selector counts once per selection, correcting the legacy double increment.

Startup eye codes retain the original meanings:4 primary,2 secondary,3 SD mount
failure,5 config failure. Each diagnostic blink is200ms on/200ms off; failures repeat after an additional
2000ms dark gap. Boot SD/config failure
blocks radio/Matter startup and automatically retries after a boot-only unmount;
a repaired/reinserted card can recover without rebooting. No audio file or
producer exists during these retries. Config parsing resets fields on every
attempt, requires an explicit primary/secondary role and speaker name, and
accepts unsigned volume 0–100 with default 100. Mount/retry never formats or writes
card contents.

The secondary source advertises after initialization and accepts valid legacy
GATT filename requests (including Polo) only when it
can prebuffer and play its B-role audio. Busy/invalid/expired/disconnected
requests fail without queued replay. Real Jeff startup/Polo, BLE pairing/acknowledgments and one clean paired skit
are now demonstrated; measured acoustic offset, full-load resource acceptance
and yard soak remain open.

The PCM producer uses8x882-frame PSRAM blocks, an8192-byte PSRAM stdio read buffer,
and its own4096-byte internal stack. It publishes at most two refill blocks
before blocking for at least one tick; every non-producing iteration also blocks.
Cancellation waits for provider ownership acknowledgment; a disconnected
speaker may leave a visible hold. Console `retry` is allowed during that hold
while disconnected; resumed callbacks complete cancellation without replaying.
A latched radio fault requires deliberate reboot. See the
[application evidence](../docs/validation/2026-10-15-native-application-port.md)
for exact proof boundaries, host tests and remaining resource/hardware gates.

Production-source host coverage includes 28 show-controller scenarios, portable
config parsing and actual storage mount/config-repair/unmount recovery using
SDK stubs, as well as catalog, PCM and actuator tests. The corrected primary
hardware run completed `Skit - milkshakes.wav` with 741424 frames and zero
reported underruns; Cam heard A-role dialogue through “we don't have hands,”
then Marco resumed. That local result does not establish paired playback or
secondary hardware acceptance. See the application evidence for boundaries.


The current firmware uses240MHz CPU speed, retaining40MHz flash and
PSRAM clocks and existing scheduler yields. A160MHz catalogue run exposed SBC
encoder starvation on Jeff's Flip6 path. See the [scheduling research](../docs/research/2026-10-15-a2dp-throughput.md)
and [qualification record](../docs/validation/2026-10-15-code-qualification.md) for
failed evidence and the successful bounded comparison; source-frame completion alone does
not qualify audio/scheduler health. The LEDC driver resets its peripheral once
before setup and defers updates while hardware duty_start remains set, avoiding
the SDK's unbounded critical-section wait. Status exposes deferred/stuck updates.

For repeatable bench qualification (read-only card access), use the ESP-IDF Python
environment and `scripts/paired_bench.py` from the repository root. Supply both
`--primary-port` and `--secondary-port`, a fresh ignored `--output-directory`,
and optionally `--check-reboot-guard`. It resets both devices, waits for startup
and Marco/Polo, then exercises each repository skit through normal paired gates.
`--skit` selects exact WAV basenames and `--repeat` repeats that subset. Captures
include private Matter traffic and belong under ignored `build/validation`.
The summary checks source frames, underruns, all-run watchdogs and unexpected
resets. Resource and audible/acoustic acceptance remain separate checks.

Console `reboot` is refused during playback, a pending show, startup speech,
tones, or SD-file cleanup; retry it when idle. This prevents a deliberate reset
from interrupting an SD transaction. It does not establish SD recovery after a
panic/brownout/external reset; the observed card required a full power cycle
after a forced reboot during playback. Speaker recovery repeats cached-address
attempts and discovery indefinitely, so a speaker powered on late is retried
automatically without a console command.

## Retained fault diagnostics

Both Anthony and Jeff keep a bounded recent-event RAM ring. Selected application
and SD-driver errors, show cancellation, startup link-loss interruption and
Jeff setup timeout warnings freeze one report with
firmware hash and uptime. Later faults coalesce while it is pending. Filtered
configuration and provisioning logs are excluded.

After show/startup playback/tone stop and the audio worker closes its file, the existing
control task saves a compact fallback in the separate `skulls_fault` NVS namespace
and exports the report to `/sdcard/diagnostics/fault-0.log` through `fault-7.log`.
Waiting silently for a missing speaker does not block export: startup PCM is
already preloaded in PSRAM and only the control owner can start playback after
synchronous I/O returns. A single `pending.tmp` is written, flushed, synced, closed and readback-checked
before replacement. Each report is at most 14335 bytes; retention is eight slots
plus that one staging file. Failed exports keep the pending report and retry at
most once per 60 seconds while idle. The logger does not remount the card.

`fault-log` retrieves a pending report without needing a working SD card. After
successful export and acknowledgement, inspect the SD files. An unacknowledged
compact report can be recovered after reboot; it holds roughly the last 1 KiB,
not all RAM history. Abrupt power loss before persistence can lose the report.
Reports now also include pinned startup milestones and a separate bounded
lifecycle history: boot/reset reason, SD mount and config attempt durations/results,
first successful SD recovery, radio setup, speaker connection/stream readiness,
and first operational readiness. Primary readiness means the actual paired-trigger
acceptance gate; secondary readiness is labelled local-audio readiness. Startup
`finished` is a terminal state and does not prove an audible announcement.
Boot timings use the SDK's monotonic uptime (not a measured power-rail timestamp);
`app_ms` measures from application entry. The logger begins after actuator/NVS
initialization, before SD; earlier failures are outside its capture window.

First milestones survive status-ring churn for the current boot. Twelve recent
lifecycle events wrap with an eviction count; latest Wi-Fi/speaker signal and
speaker-disconnect observations remain separately available with sample timestamps.
Speaker losses have a cumulative per-boot count. Both roles request an asynchronous
Classic Bluetooth RSSI sample every 30 seconds while connected: `rssi_delta` is
relative to the controller's Golden Receive Power Range, **not dBm**. Anthony
samples Wi-Fi RSSI in dBm every 30 seconds; Jeff records Wi-Fi disabled for its
role. Wi-Fi sampling can miss outages shorter than the interval. No new network
service or Wi-Fi provisioning is added.

Lifecycle capture is RAM-only in bounded PSRAM, without per-event SD writes or
new tasks. A fault report seals lifecycle context when the control task next
materializes it; the original application/driver ring still freezes at the first
fault. This permits a driver's error and the enclosing SD attempt result/duration
to appear together. Later events cannot change an already materialized report.
The compact roughly 1 KiB reboot fallback retains only the report tail, so it
cannot preserve the full lifecycle context. Full startup context requires the
normal SD export before RAM is lost.

This is ordinary error retention, not a panic/core-dump facility. No formatting,
NVS erasure or flash partition changes are introduced.

See [Story 003](../docs/stories/story-003-retained-fault-logs.md) for validation
and its original acceptance boundary. See the [SD startup recovery record](../docs/stories/story-005-sd-startup-recovery.md)
for Jeff's utility/startup update and device checks.

## Home Assistant MQTT

Story 007 retains the native ESP-IDF radio/audio runtime and adds the SDK MQTT
client. Home Assistant device discovery creates 26 entities with the current
11-skit card: connectivity, SD/fault-report state, current activity, timings,
heap/signal/counters, Skit selector, Play selected skit and Open saved reports
buttons. Other MQTT
consumers can read the same typed JSON state without Home Assistant.

The Skit list comes from validated scripted assets on that skull's card, excluding
startup/Marco/Polo and malformed or ambiguous paths. Selection only updates RAM;
Play requests that exact selected skit. Selection resets to the first valid skit
on reboot. Controls are not queued for later playback. Busy, startup/greeting,
cooldown and unavailable-speaker gates return not_ready. Primary named/random
triggers play solo if Jeff is absent at acceptance, or use the existing paired
handshake if Jeff is ready. A paired transaction that subsequently fails still
aborts without automatically replaying solo. Direct secondary control plays B.

Optional networking keys in the existing `/sdcard/config.txt`:

```ini
wifi_ssid=YOUR_NETWORK
wifi_password=YOUR_PASSWORD
mqtt_uri=mqtt://BROKER_HOST:1883
mqtt_username=YOUR_MQTT_USER
mqtt_password=YOUR_MQTT_PASSWORD
```

These are placeholders, not the installed credentials. Existing role/speaker
settings stay in the same file. Common ssid/password and wifi-prefixed aliases
are accepted. Config is read-only at boot. Missing/invalid optional networking
leaves normal local operation available. Jeff uses a Wi-Fi station with RAM-only
SDK settings; Anthony's Matter stack remains his Wi-Fi owner. Networking does
not initialize a Matter node on Jeff or overwrite primary fabrics/settings.

If the SD card has no mqtt_uri, idle USB maintenance can save broker settings in
the separate `skulls_net` NVS namespace:

```text
mqtt-config mqtt://BROKER_HOST:1883 USERNAME PASSWORD
reboot
```

Use `-` for an empty auth field. This USB command accepts single tokens without
spaces; SD values support spaces. Credentials are not echoed, published or added
to source. Reboot applies changes. SD broker settings take precedence over the
USB fallback. `mqtts://` uses SDK certificate-bundle verification; TLS with a
production broker has not been device-qualified in this story. No AP is started.

Topic prefix is `halloween/twoskulls_<stable-chip-id>`:

| Suffix | Meaning |
| --- | --- |
| state | Non-retained typed JSON snapshot, about every 5 seconds |
| availability | Retained online plus broker Last Will offline |
| play_available | Retained playback-readiness gate |
| select/set | Exact advertised skit option; selection only |
| play | Exact PLAY command; no retained command replay |
| logs_available | Retained readiness for an idle read-only SD snapshot |
| logs/request | REFRESH request from the native HA button to the HA report bridge |
| logs/get | Scoped GET request with a fresh request ID and fixed SD slot |
| logs/&lt;request-id&gt;/… | Non-retained bounded report transfer, separate from state |

Discovery is retained under `homeassistant/device/<id>/config`, republished on
MQTT connection and HA birth. Command subscriptions must receive successful
SUBACKs before controls become available. SDK custom events serialize subscription
retries on the MQTT event owner. Only one bounded pending command is copied;
control owner rejects stale (>2 s), fragmented, duplicate or retained-at-subscribe
messages, and validates catalog membership. MQTT v3 can clear RETAIN on live
forwarded messages: this guard prevents stored replay, not every live publish
originally sent with retain enabled. Commands from HA explicitly use retain=false.

Sensors expire after 20 seconds if application telemetry stops. Last Will handles
connection loss; observed selector availability went offline in about 23 seconds.
Wi-Fi RSSI is dBm; Bluetooth strength is a controller delta, explicitly not dBm.
Missing measurements are null/unknown. Startup/SD timings are monotonic SDK values,
not external power-rail measurements. Latest report ID is a hex string. Full SD report text stays on the card until
requested through the read-only report path below; it is not stored in a sensor
attribute or recorder history. Report count is cached from fixed file metadata
at boot or idle after persistence changes, not polled from SD every heartbeat.

### Open saved SD reports

Install the [TwoSkulls saved-report integration](../home_assistant/custom_components/twoskulls_diagnostics/README.md)
in Home Assistant with its existing MQTT integration. Press **Open saved reports**
on Jeff's device page, then open Home Assistant's **Notifications** sidebar. An
index lists the available slots and one notification per report contains the
complete literal text. The initial bridge is a native HA integration and
notification view, not a separate website or public download endpoint.

The collector uses the fresh `report_slots` state bitmask (slots 0–7). Each request
has a fresh 16-hex ID and targets one fixed slot. The firmware snapshots at most
14,335 bytes into PSRAM only while its diagnostic owner is idle; SD/card contents
are never changed. It sends a begin frame, ordered binary chunks of at most 256
bytes and an end frame with byte count/standard CRC32. QoS1 duplicates are handled
idempotently; stale IDs, invalid ordering, corruption or incomplete transfers
cannot become a verified report. No report body enters regular state telemetry.

HA verifies complete contents before saving a private content-addressed copy
under `.storage/twoskulls_reports/<device>/` and displaying escaped plaintext.
Repeated reads deduplicate; older cached versions remain available on disk when
an SD slot later rotates. Report notifications are snapshots, not live streams.
There is no automatic cache deletion. Dismissing a notification does not delete
the SD report. Busy/unavailable/error/timeout results are explicit; they do not
replace verified cached data. The integration does not automatically retry.

The device exposes Idle for an empty current skit and identifies its firmware
with the application version plus a short ELF SHA256 build fingerprint. A
selected skit remains separate from current playback. Generic 0.1.0-probe alone
is no longer used as the HA build identifier on the updated firmware.

The live HA/MQTT hub is now on the Mac Mini (`192.168.1.80:8123` / broker1883),
with the preserved grouped Halloween dashboard. Jeff's separate NVS broker URI
was updated; SD Wi-Fi settings and firmware were not rewritten during migration.
The old laptop containers/images were removed during cleanup. Their private
configuration and cold rollback backups remain under ignored
`build/validation/story-007-ha/`; do not
commit captures or credentials or casually start a second HA copy. See the
[hub migration record](../docs/stories/story-009-mac-mini-home-assistant-hub.md)
and [current integration guide](../docs/home-assistant-prop-integration.md).
No public DNS, custom website or off-LAN access was configured.

Device qualification: actual HA log reading verified both existing Jeff SD
reports (413 and 5,689 bytes) byte-identical to their pre-flash copies, with complete
native notifications. An initial larger-report timeout was corrected through the
supported MQTT worker poll setting after bounded source/trace research; no
unbounded retries or receiver bypass were introduced. See Story 007 and its
throughput research for the failure, correction and observed timings.

Other checks: actual Jeff discovery/selection/negative commands,
retained-at-boot rejection, broker reconnection, offline/recovery, and preservation
of both prior SD reports passed. No speaker was present, so the HA Play button
correctly remains unavailable. Positive audible playback, active-audio resource
behavior, and Anthony/paired hardware acceptance remain pending. The corrected
application including the speaker eye warning is 2,315,472 bytes and still has only 85 bytes of measured
IRAM margin. Further features require resource work rather than assuming flash
free space is enough. Actual Chrome inspection confirmed the two full report
notifications, Canada configuration and the native Overview status favorites.
