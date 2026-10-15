# Anthony WROVER prototype implementation — 2026-10-15

Status: in progress. Source/build, serial observations, audible confirmation,
Apple Home commissioning and actual paired-skull acceptance are separate results.

## Current result

Identical native firmware now runs on both classic WROVER boards, selected only
by SD config. Anthony is the borrowed original v3.0 (`a0:a3:b3:80:16:38`); Jeff
is the new board (`f4:2d:c9:78:ad:78`) with his original card. Final app2098304
bytes, SHA256`cf4692a71f1f29405cf85f77b9c03606e826ea2730444cd1c633aec85cc88a25`,
hash-verified application-only on both. Original new-board4MiB backup was read
and verified before first native layout initialization. Existing native NVS and
Anthony's Home pairing were preserved.

Cam confirmed the added500ms startup preroll fixes Anthony's clipped initial
“A”. Jeff initialized and played Polo; later media silence was resolved by his
physical speaker-volume setting, without a transport workaround. Real BLE
filename/ACK/readback exchange now works. Final paired Starbucks completed
1647341frames on both boards with zero underruns; Cam confirmed clean speech
and correct turns. Apple Home then triggered imitations,1341581frames on both
with zero underruns; Cam confirmed the switch returned to off.

An8192-byte PSRAM stdio buffer and bounded two-block refill bursts resolved the
observed sustained refill deficit in these final tests. Host producer tests and
independent concurrency review pass. Preserve the failed first paired run and
intermediate39-underrun run as diagnostic evidence. Current static IRAM margin
is993bytes and primary local-window internal low31191bytes, below the proposed
32KiB floor: resource margin, measured acoustic offset, recovery/yard/long soak
remain open. Jeff's jaw/eyes are physically disconnected; Anthony's outputs and
breathing were already owner-confirmed. See [the full two-board record](2026-10-15-jeff-first-boot.md).

All implementation remains uncommitted in the requested isolated worktree.
Historical sections below retain earlier probe results and failures, rather
than describing the current build as diagnostic-only or primary-only.

## Physical identity and corrections

Cam authorized implementing the story and flashing the USB-connected board.
Initial board: MAC `08:b6:1f:6f:38:b0`, original Anthony v2.1. Cam unplugged it
during the initial read; it was not flashed. It has no onboard card slot.

Current board: MAC `a0:a3:b3:80:16:38`, original Jeff v3.0, borrowed to prototype
Anthony. ROM reports ESP32-D0WD-V3 rev 3.1, dual core, 40 MHz crystal; detected
SPI flash is 4 MB. Anthony's existing card is now in its onboard slot. Only
Anthony had the C3 Matter board; Jeff had a USB-C PD module underneath. The
physical secondary skull is therefore unavailable for BLE pair acceptance.
At the initial probe both speakers were powered and jaw/eyes disconnected.
Cam subsequently connected GPIO23 jaw and GPIO32/33 eyes and confirmed operation.

## Recovery evidence

A complete 4,194,304-byte pre-change flash image was read at 230400 baud and
compared against the device with esptool `verify_flash`: digest matched.
SHA-256: `2315245c343b66ab101b12659178719157731248e38638e606ae9a397778cfe9`.

Private backup directory (not in Git):
`/Users/cam/.codex/device-backups/TwoSkulls/Jeff-v3.0/2026-10-15-a0a3b3801638/`.
Contains original image, read/verification logs and a device manifest. Its parent
directory is restricted to the current user. Preserve it before further uploads.
The failed initial backup was caused by the user swapping physical boards and is
not a valid recovery image. MAC identities distinguish the two operations.

Original partition table at `0x8000`: NVS `0x9000/0x5000`, otadata
`0xe000/0x2000`, app0 `0x10000/0x140000`, app1 `0x150000/0x140000`, SPIFFS
`0x290000/0x160000`, coredump `0x3f0000/0x10000`.

Restoring the full original image also restores its original partition table and
Bluetooth state. It expects its original card/wiring, including external SD; a
restore is not evidence that it can use the onboard slot. Use the full backup
rather than flashing an arbitrary old application into a new partition layout.
Card contents have not been modified. A separate full card backup remains pending.

## Gate 0: observed SD-only baseline

Flashed the previously compiled Arduino onboard-SD build after clearing only the
backed-up old NVS region, so Jeff's remembered speaker is not inherited by this
Anthony test. Esptool verified each written image. Firmware size: 1,316,640 bytes;
bootloader/partition/application offsets came from the existing Arduino build.
Flash mode DIO, 40 MHz, detected 4 MB size.

Serial reset capture:
`build/validation/2026-10-15-arduino-sd-boot.log` (ignored).

Observed:

- SD_MMC mounted at 1-bit/20 MHz, 960 MB card.
- Both initialization WAVs found; 11 skits and their scripts parsed.
- Card says `role=primary`, `speaker_name=JBL Flip 5`, `speaker_volume=100`.
- A2DP initialization called and primary initialization WAV queued.
- Existing sketch's GPIO1 trigger setup interrupts serial output; it shares
  UART0 TX. The native prototype reserves GPIO1 for UART rather than copying it.

Cam reports no audible initialization despite resets/power cycles, and is unsure
which physical speaker is Anthony's. The card names Flip 5. This trial does not
prove A2DP playback; jaw/eye regression and pair tests are also pending.

## Gate 1: native shared-stack probe

New `firmware/` project: target `esp32`, ESP-IDF 5.5.4, registry ESP-Matter exactly
1.5.0 with generated target-specific `dependencies.lock`. The installed matching
Xtensa compiler is `esp-14.2.0_20260121`; the prior machine setup had only the
RISC-V compiler for this IDF release. Other projects/toolchains were retained.

Implemented probe scope:

- Read-only use of onboard card and existing config; no format-on-failure.
- Single application-owned BTDM controller and Bluedroid lifetime before Matter.
- Asynchronous wait for Matter's registered GAP callback, then a Matter-first
  dispatcher before GATTC discovery. Native A2DP source provides bounded quiet
  diagnostic PCM, only on explicit serial `tone` or Matter On.
- Original peer UUIDs, raw filename write, notification subscription, delayed
  matching readback and timeout; an ACK remains weaker than playback proof.
- Bounded copied-event queue; fatal loss of control events latches a diagnostic
  fault and silences application PCM, requiring deliberate reboot. Uncertain peer
  requests close the connection before reuse. Media startup retries are bounded.
- Status includes callback time, internal heap/largest block/minimum, PSRAM,
  source/peer/media readiness and queue failures. Jaw and eyes are not driven.

### Build findings and decisions

1. Registry Matter's Bluedroid backend has an upstream defect at its device-name
   generation: `ChipLogProgress` is used where formatting into the name buffer is
   required. Compiler reports `-Werror=format-nonliteral`. The same defect appears
   in [upstream issue #1441](https://github.com/espressif/esp-matter/issues/1441).
   Adopt a one-line `snprintf` correction in a generated build-local copy, guarded
   by the original source SHA-256. `firmware/cmake/matter_bluedroid_patch.cmake`
   substitutes only that source in the target. Managed files are unchanged;
   warnings remain enabled. A future package/hash change must review this patch.
2. Legacy client source actually writes CCCD `0x0001`: it calls
   `registerForNotify()` with default `notifications=true`, despite its method
   name `registerForIndications()` and the server advertising INDICATE. Jeff's
   `notify()` checks the notification bit. Native client preserves `0x0001`.
   Confirmed against the local Arduino 2.0.17 implementation, not just comments.
3. The initial combined image exceeded static DRAM by 6,600 bytes before boot.
   The map attributed about 35 KB BSS to Bluedroid and 48 KB to Matter. Adopt the
   documented dynamic Bluedroid environment plus PSRAM-first host allocation.
   This changes allocation location, not the total workload or radio capacity.
   Runtime internal/DMA headroom still needs measurement. Sources:
   [IDF RAM guidance](https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32/api-guides/performance/ram-usage.html),
   [external RAM limits](https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32/api-guides/external-ram.html).

### Observed native build and runtime

The combined image builds after the checked name correction and documented
allocation changes. Initial successful map: static DRAM 98,596 bytes, static IRAM
129,447 bytes, application about 2.06 MB within the 0x3D0000 factory slot. These
linker sizes are not runtime heap headroom. IRAM has only about 1.6 KB free and
needs deliberate attention before adding peripheral ISR drivers.

First native boot detected **8 MB physical PSRAM**, with **4 MB mapped** into
the address space. SD mount/config succeeded. Steady diagnostic internal free
heap was about 41 KB, minimum 36.8 KB and largest block 34 KB before commissioning
or A2DP streaming. These values do not qualify worst-case combined-load margins.

The first boot exposed a real asynchronous initialization race: beginning peer
scanning as soon as the Matter GAP handler appeared could race Matter's later
random-address configuration. Wait for Matter's advertising-started event on an
uncommissioned boot before discovery; commissioned boot does not configure
commissioning advertising until a later deliberate window. Recommissioning while
discovery is active remains a separate stress/recovery test. Scanner-only GAP
events are routed to the application rather than through Matter's peripheral-only
handler, which otherwise logs and reschedules its state machine for every advert.
All advertising/security events still reach Matter first. Enabling the Matter
commissioner now fails compilation until that routing contract is revised.

Enabled FAT long-filename support and ignored dot-prefixed metadata when counting
assets. The latest running image reports **15 WAVs and 11 scripts**, matching the
expected content (the earlier 29/22 count included macOS metadata).

One application upload was interrupted while Cam reported resetting the board;
the following 115200-baud retry fully wrote and verified the app. A subsequent
230400-baud application update also verified, preserving native NVS. The latest
image initiates only the configured speaker connection and rejects unselected
incoming peers: this borrowed PCB's previous speaker may otherwise reconnect to
its remembered MAC. An unowned incoming codec event was observed before this
change; the identity/reconnection explanation is an inference pending live logs.

Current live capture (ignored): `build/validation/2026-10-15-native-live.log`.
Build/flash logs, resolved config, size report and hashes are retained in the
ignored validation/build directories. The source README describes serial status,
tone, bounded discovery retry, pairing, deliberate reboot and explicit factory
reset commands. The UART capture records successful Matter commissioning at approximately 117
and 124 seconds, including pending-fabric commit. This establishes live Home
commissioning; Home On/control, persistence across reboot, and audible tone still
need separate evidence. No triple-workload or full-show success is claimed.

With Wi-Fi/Home active, steady internal heap was approximately 37.3 KB and the
commissioning minimum was **17,039 bytes**, below the story's proposed 32 KiB
minimum. No radio queue drops or latched fault were observed. This is a resource
gate still open, not a pass. The next controlled experiment selects the SDK's
external Matter heap allocator while retaining internal task stacks and DMA.

Cam confirmed Jeff is powered off; it cannot compete for either speaker. Both
speakers were powered, but the configured Flip 5 has not been discovered or
connected by the native probe. A source audit found that inquiry is one finite
10.24-second window, with explicit `retry` needed afterward, and successful
discovery transitions were silent. Added start/state/candidate diagnostics;
selection still requires the exact SD-configured speaker name. Cam is unavailable
for physical checks for roughly an hour, so audible/physical checks remain pending.

## Outstanding acceptance

Home control/reboot persistence, real A2DP playback, heap/stack stress,
original peer exchange, SD audio pipeline port, jaw/eyes and yard/soak tests remain
open. The tone probe is not the finished skull application. Follow
[Story 001](../stories/story-001-single-wrover-matter-prototype.md) for the complete
acceptance matrix and fallback policy.

## External Matter allocator and instrumented discovery trial

Build 9 succeeded: application 2,062,176 bytes, within the 0x3D0000 partition.
App-only flash 5 verified its digest on MAC a0:a3:b3:80:16:38; NVS was retained.
The bounded capture is `build/validation/2026-10-15-native-psram-boot.log`.

On reboot the board restored two operational fabrics, rejoined Wi-Fi, resumed a
secure CASE session and subscription activity. This is serial evidence of saved
credentials and protocol recovery, not owner verification of the Home UI/control.
Initial internal free was 40,439 bytes, then about 39,363 steady; minimum reached
27,151 bytes during this reboot/session recovery. This still falls below 32 KiB.
Fresh commissioning and streaming were not repeated, so do not compare its minimum
directly with the prior 17,039-byte fresh-commissioning minimum as an A/B result.

GATTC registration, initial Classic inquiry and the explicit retry all ran.
Both inquiries stopped without a configured-name match. Other named Bluetooth
devices appeared, proving the inquiry/result callback path is active. No Flip 5
connection, media start or PCM callback has been observed. Do not connect arbitrary
nearby devices or edit the card name based only on this discovery result. The
physical speaker's current pairing visibility/name remains an owner check.

The serial helper now forwards fd bytes directly to avoid stranding buffered
commands. Python compilation and whitespace validation passed. Main checkout is
still clean; implementation remains isolated in `codex/onboard-sd-migration`.

## Powered-speaker discovery and live A2DP — follow-up

Cam reported both speakers had powered off, then turned them on and power-cycled
the prototype. During the next capture, the exact SD-configured `JBL Flip 5` was
found at 3.275 seconds, connected at 4.065 seconds, and entered A2DP started state
at 4.195 seconds. SBC negotiated the 44.1 kHz frequency setting. This supersedes
the prior no-speaker observation: discovery and the outgoing audio transport now
have live evidence. The user also confirmed both speakers were in pairing mode;
the capture/answer timing does not establish precisely when each button was pressed.

`build/validation/2026-10-15-speakers-powered.log` records increasing PCM callback
and byte counts, stream=1, no radio drops/fault, internal steady around 36.3 KB
and minimum 26,959 bytes during this short combined Matter/A2DP boot. Serial tone
requests were accepted and consumed. Cam explicitly confirmed hearing the second
short tone and identifying which physical speaker is Anthony's. Audible diagnostic
A2DP playback is therefore demonstrated; SD WAV playback remains unimplemented.
Jeff remains powered off, so no real peer compatibility result is claimed.

## Home location and speaker reconnect correction

Cam found the existing accessory under Outside/Other in Apple Home. The UART
capture `2026-10-15-home-tone.log` records two Matter On requests, ignored because
the speaker was disconnected. The Home pairing was intact; no reset/re-add was
needed. A serial monitor reopen also caused a fresh boot in this setup, despite
no explicit reset request; keep a persistent capture during live testing.

The diagnostic source previously rediscovered by name on every boot, which
requires current speaker discoverability. Added a name-bound NVS speaker-address
cache, populated only after a selected successful connection, and one direct
reconnect before finite inquiry fallback. Cache uses its own namespace/key and
never clears Matter credentials. Independent review found and corrected an
unbounded failure/retry loop before upload. Build 11 passed; subsequent runtime
results are recorded below. Full-show playback remains outside this probe.

## Audible Home-trigger success and saved reconnect trial

Cam explicitly confirmed that the Outside/Other Home switch played the tone and
returned off. `reconnect-cache.log` records the accepted Matter trigger at
124.776 seconds. This is live end-to-end Home-triggered diagnostic audio proof.

Build 11/app flash 6 successfully remembered the selected Flip 5 in its own NVS
blob. A fresh boot loaded that cache and attempted a direct connection. First
attempt logged HCI status 0x0B (Connection Already Exists), then timed out near
63 seconds. One explicit retry connected without new discovery/pairing and Cam
heard both the connection sound and subsequent manual tone. That proves cached
identity/use, but startup reconnect is not yet reliable. Steady internal heap
with streaming was about 35.9 KB, minimum 28,503 bytes; no queue drops/fault.

Cam left for approximately an hour after confirming Home-trigger audio. Continue
serial/software work without physical prompts. Next isolated radio experiment
permits page scanning (CONNECTABLE/NON_DISCOVERABLE) only after known selected
identity is loaded; unknown incoming addresses remain rejected. See research
notes for the source-backed collision hypothesis and test limits.

## Controlled reconnect and queue-memory experiment

Build 12/app flash 7 enabled CONNECTABLE/NON_DISCOVERABLE after loading the known
speaker identity. With a single captured boot following esptool --after no_reset,
the configured speaker connected at 6.745 seconds and audio started at 6.925
seconds, without pairing or retry. No 0x0B appeared in this capture. This is one
successful boot; because the capture/reset procedure also changed, it is not an
isolated proof that page scanning alone fixes every reconnect collision.

That boot's internal minimum was 19,771 bytes during concurrent Matter session
recovery and A2DP start. Streaming stayed up for the short capture, no queue drop
or fault. Do not replace a worst-case measurement with a more favorable earlier
boot. The resource gate remains open.

Build 14/app flash 8 moves only the copied-event queue payload to PSRAM, retaining
static queue control, scheduler lists and task stacks internally. It also reports
radio/diagnostic minimum-unused stack bytes and DMA free memory. Independent
review checked scheduler timeout access and the static-control placement. No
stack sizes were reduced without measurement. Capture `2026-10-15-queue-psram.log`
records the next bounded runtime experiment; Home credentials and saved speaker
were retained across these app-only updates.

### Split-queue measurement

Build 14 reports 9,088 bytes of event payload in PSRAM. Speaker connected at
7.326 seconds and stream started at 7.426 seconds without pairing/retry. Internal
free settled near 45,167 bytes; minimum 32,675 bytes (93 bytes below 32 KiB),
largest free block 40,960 bytes. Minimum unused application stack: radio 3,656
bytes, diagnostic 3,888 bytes, with both stacks still 6,144 bytes. No queue drops
or latched fault. This is a short boot/recovery capture, not a fresh commissioning,
real-peer, SD streaming or servo/eye stress result.

Next resource experiment selects the supported MBEDTLS_EXTERNAL_MEM_ALLOC option.
It moves crypto dynamic allocations to PSRAM while keeping task stacks/internal
requirements unchanged. Do not shave stack sizes merely to pass the floor while
real GATTC request paths are untested. Build 15 is in progress; it must resume a
real Matter session and A2DP after flashing before any runtime benefit is claimed.

## Crypto allocation and revised recovery policy

Build 15/app flash 9 restored a secure Matter session with the external mbedTLS
allocator. Internal minimum was 39,639 bytes and steady free about 48.3 KB, but
A2DP did not start in that capture. This establishes crypto/session functionality,
not the final simultaneous-load floor. No stack sizes were reduced.

The CONNECTABLE experiment failed a later boot: ACL success, L2CAP bad-state,
second HCI 0x0B, and an A2DP open failure with the ACL persisting afterward. The
upstream source-specific reconnect implementation disables incoming connections;
the earlier Common-class comparison had missed that override. Reverted page
scanning to the source-role NON_CONNECTABLE/NON_DISCOVERABLE setting. No repeated
pairing or Bluetooth-host reset is used as a workaround.

Source review of the next recovery implementation checked bounded attempts,
backoff, profile-versus-ACL state, and missing terminal callbacks. Builds 16–18
were intermediate source-review iterations and were not flashed. Build 19/app
flash 10 retains the name-bound cache, caps direct attempts at three, uses
five-second backoff, observes actual selected-peer ACL events, bounds an ACL
teardown wait to 60 seconds, and holds uncertain CONNECTING state after a separate
75-second watchdog. It never starts another connection over unresolved state.
After three attempts, fallback is one finite name inquiry; a failed discovery
connection does not restart the cycle. Explicit retry cannot bypass active
backoff or unresolved ACL state. Runtime repeat-reboot results follow below.

### Latest image baseline before repeated reset trials

Build 19/app flash 10 verified. Actual binary 2,065,168 bytes; static IRAM
129,499/131,072 bytes (1,573 free), static DRAM 98,748 bytes. Static linker
remaining space and runtime capability-specific heap are different budgets.
Latest image uses external Matter/mbedTLS ordinary allocation, internal stacks
and queue control, PSRAM copied-event payload, and outgoing-only cached recovery.

On its first captured boot, cached attempt 1 connected at 4.496 seconds and
stream started at 4.706 seconds without pairing/retry. After Wi-Fi/operational
advertising/subscription resumption began, internal free was about 45.3 KB,
minimum 35,367 bytes, largest block 43,008 bytes. Radio/diagnostic unused stack
minima were 3,688/3,840 bytes. No dropped event or latched fault; callbacks
increased. Owner-confirmed Home control was on the earlier image; latest-image
control still needs confirmation when the controller/owner is available.

### Abrupt-reset trial 1 — automatic recovery

Using one persistent serial capture, issued `reboot` after the latest image had
connected and streamed. Second boot reproduced a cached connection-exists failure
(GAP mapped status 267 / HCI 0x0B); profile terminated at about 63 seconds.
Automatic attempt 2 began at 68.225 seconds and failed with mapped status 260 /
HCI page timeout. Attempt 3 began at 78.385 seconds, connected at 81.915 seconds
and streamed at 81.985 seconds. No pairing or manual retry was used. This exercises
the bounded automatic retry path; approximately 82-second recovery is a measured
latency limitation, not seamless reconnection. No watchdog/queue fault or panic.

During this boot, minimum internal free was 37,383 bytes, largest block 43,008,
steady streaming free around 45.2 KB, with radio/diagnostic stack unused minima
3,688/3,760 bytes. Cold first boot had lower minimum 35,367 bytes. More workload
and fresh-commissioning evidence are still required; no physical peer was present.

### Abrupt-reset trial 2 and retained running image

Third boot again used three attempts. The second obtained an ACL but the profile
remained pending until its terminal event at 115.405 seconds; no unsafe attempt
overlapped it. Third attempt started at 120.425 seconds, connected at 123.625 and
streamed at 123.705 seconds. No manual pairing/retry was used. A secure Matter
session became active at 8.595 seconds. Internal minimum was 36,871 bytes and
streaming free around 45 KB; stack minima radio 3,688, diagnostic 3,864 bytes.
The PCM-provider elapsed maximum reached 2,296 microseconds (includes scheduling
interruption); this is timing evidence, not proof of full-audio deadline margin.

The latest image remains running and capturing. Three boot endpoints were
streaming, no event drops/fault/panic observed, and source/build SHA manifest
matched. No further firmware changes were made after this verification. SDK
source review confirmed that esp_a2d_source_disconnect queues but does not cancel
an OPENING-state request in this pinned version; do not treat its ESP_OK as proof
of cancellation or launch another connect over uncertain state. The watchdog
therefore retains a visible hold policy instead of resetting the shared host.

### Offline work while replacement board is in transit

Added a read-only host asset preflight and seven passing adversarial/semantic
unit tests; no firmware or device changes. All 15 repository WAVs match the
intended PCM contract; one of 11 scripts has a final interval beyond EOF
(`Skit - names.txt:15`, about 2.597 seconds). Authored assets remain unchanged.
The actual WAV data offsets vary 44–110 bytes, reinforcing the native parser
requirement. See [preflight evidence](2026-10-15-asset-preflight.md). The live
Jeff peer gate still holds the full application port.

### Requested startup announcement restored in native prototype

Implemented and app-only flashed a bounded PSRAM-preloaded initialization WAV.
On the first captured boot Anthony connected at 8.237 s, began startup playback
at 8.527 s and delivered all PCM by 12.657 s, alongside resumed Matter CASE.
Cam confirmed audible completeness and clarity. Parser sanitizer tests and final
build pass; flash hash and source/artifact manifest match. Runtime internal
minimum 31,547 bytes remains below the proposed gate floor; broader memory/peer
acceptance is still open. This is startup-only, not the full skit/actuator port.
See [startup implementation and live evidence](2026-10-15-startup-announcement.md).

### Native jaw/eye extension after Cam connected jumpers

Added GPIO23 servo and GPIO32/33 eye PWM with separate timers, bounded gentle
startup check and precomputed RMS-based announcement animation. App-only upload
verified; first boot initialized outputs, ran the check and delivered startup
PCM alongside A2DP. Cam confirmed both eyes and jaw work. Driver host/sanitizer
tests and independent review pass. New image's short-run callback elapsed peak
21,963us and internal minimum30,703 bytes keep timing/resource gates open.
See [jaw/eye implementation and evidence](2026-10-15-jaw-eyes.md).

### Restored legacy idle breathing, owner-confirmed

Added the original30°/2s open/100ms hold/2s close movement on a7s start-to-start
cadence, using the existing nonblocking task. Speech, preroll and test tones
cancel idle motion. Host waveform/priority tests pass under sanitizers; final
build and app-only flash hash verified. Three logged starts were7.03s apart
while A2DP was unavailable. Cam confirmed breathing looks right. Actual battery
keep-alive behavior remains unmeasured. See [breathing evidence](2026-10-15-idle-breathing.md).
