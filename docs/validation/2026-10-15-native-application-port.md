# Native primary and secondary application port — 2026-10-15

Cam explicitly requested a goal to port the rest of the application and make as
much progress as possible before the replacement ESP32 arrives. This authorizes
software integration ahead of the earlier real-peer gate; it does not convert
missing Jeff, radio/resource or full-show evidence into acceptance. The source now covers both roles: primary Anthony starts Matter and its BLE
client; secondary Jeff starts a standalone GATT server and the Polo response
without starting Matter. The existing raw-filename/ACK/readback contract remains
the compatibility target. Native secondary physical pairing and synchronized
playback are pending; the dual-role image is built and app-only flashed.

## Implemented behavior

- Boot eye patterns:4 primary,2 secondary pattern available,3 SD mount failure,
  5 config failure. Failure patterns repeat with500ms gap;100ms on/off pulses.
  GPIO outputs initialize before storage, and the first sequence runs before
  lengthy radio setup. Both explicit primary and secondary configs are accepted.
  SD/config failures retry automatically before any audio producer or radio starts;
  boot-only unmount/remount permits a repaired or reinserted card to recover.
  A mount failure selects3 flashes; a mounted card with invalid config selects5.
- Catalog of supported PCM WAVs and immutable scripts in caller-owned PSRAM.
  Host proof accepts all15 checked-in WAVs/11 scripts. Exact A/B interval masks
  use ceil(ms*44100/1000), start-inclusive/end-exclusive. Invalid assets are
  diagnosed/skipped; asset/path/line capacities are bounded.
- Names' final authored interval exceeds PCM EOF. Runtime clips that endpoint
  toEOF with a warning, preserving effective old EOF behavior; authored files
  remain unchanged. The separate strict preflight still reports this asset issue.
- Weighted top-three selection excluding the previous skit. Empty/single cases
  are safe; equal weights use a deterministic filename tie-break. Count once on
  selection, deliberately correcting the old selection-plus-completion double
  count. No new persisted selection history is introduced.
- Eight882-frame blocks (~28KiB PSRAM) with a4096-byte internal producer stack.
  SD reads, gain, exact role muting and RMS preparation run in the producer.
  Static queues have internal scheduler controls and bounded index payloads.
  Provider performs bounded copies/queue operations only, zero-fills starvation,
  preserves the source cursor on silence insertion, and drains EOF before complete.
- Bounded show controller: idle→prepare→peer ACK/readback→play→10s cooldown;
  failures cancel without retransmission or delayed surprise playback. Validate
  and prebuffer locally before sending Jeff the unchanged raw filename. An
  observed peer disconnect afterACK vetoes local start. ACK still does not prove
  that Jeff's speaker played, an inherited protocol limitation.
- Matter On now requests a real paired skit, with a single pending request and
  no backlog; it requires startup completion, speaker, Jeff and cooldown. Serial
  `tone` remains a diagnostic. Serial `play /audio/<filename>.wav` is an explicit
  local diagnostic bypass for testing Anthony without Jeff.
- Primary peer discovery starts after initialization audio finishes; scans pause for
  coordinated/local skits. Marco plays every5s while actually scanning and idle,
  after init, with no queued catch-up calls. Scanning continues during Marco.
- Native secondary source advertises after startup, answers the legacy GATT
  filename request, prebuffers its B-role audio before queuing acceptance, and
  plays Polo on a peer connection when idle and its speaker is ready. Invalid,
  busy, disconnected and expired requests are rejected without delayed replay.
  Matter remains primary-only. Secondary startup/Polo, BLE exchange and paired audio now have bounded bench
  proof; see the later [two-board validation](2026-10-15-jeff-first-boot.md).
- Existing jaw/eye/startup/breathing behavior remains; full clips use the audio
  provider's consumed metadata and keep the jaw closed during the other role.
  Eyes follow role intervals at block-envelope resolution (~20ms), not task
  read-ahead. Acoustic alignment remains a physical test.

## Review fixes and remaining boundaries

Independent review caught signed PCM multiplied by unsigned gain, which corrupts
negative samples below100% volume. Both startup and stream gain now multiply
signed values; regression checks exact +500/-500 at50% from +1000/-1000 input.
This does not change the current card's100% volume startup audio.

Cancellation waits for the provider to release a partial block before the worker
resets queues. If A2DP stops callbacks, cancellation remains visibly pending;
manual `retry` is allowed during that hold while disconnected. Reconnection
resumes callbacks, acknowledges cancellation and returns idle without replay.
A latched radio fault still explicitly requires reboot. Never force-reset queues
while the callback may retain ownership. Live cancellation/reconnection evidence
is still required.

Starvation inserts silence without advancing source frames. This preserves local
script alignment but can delay Anthony relative to Jeff: synchronized acceptance
requires zero unexplained underruns and measured acoustic timing. Static IRAM and
runtime internal RAM remain tight; the previous probe already missed the proposed
32KiB runtime floor and had callback wall-time peaks above5ms. New full-load
measurements are required rather than carrying over earlier bounded success.

Failure codes and automatic boot SD/config recovery are implemented/tested, but
live SD removal/config corruption has not been injected. A boot failure blocks
radio/Matter initialization while the eye pattern repeats and storage retries.
Unmount is confined to this boot phase, before any audio file or producer exists.
Failed unmount retains mounted state; successful unmount clears state and permits
a fresh mount. Neither mount nor retry formats or writes the card.

## Software evidence

Production-source host suites cover: RIFF parsing, catalog/script boundaries and
malformed input, weighted selection, actual producer/provider gain/role masks,
EOF/starvation/callback-held cancellation, actuator timing and blink patterns,
and 28 controller scenarios (startup/readiness/cooldown, prepare-before-peer,
ACK-before-play, failure holds, no backlog/replay, local bypass, Marco cadence,
and secondary requests/Polo). Portable config tests cover explicit roles, missing
fields, bounded lines/names, strict volume parsing and reset-to-default retries.
Tests of the actual storage implementation with SDK mount stubs cover mount
failure, invalid/missing config after mount, repaired config without redundant
mounting, failed/successful unmount and fresh mounting. Every mocked mount asserts
that formatting is disabled.
Tests use mock transports/PWM where appropriate; they do not prove RF/audio timing.

Device build/upload/run results follow when observed. Main checkout remains
untouched; all implementation is in `codex/onboard-sd-migration`.

## First integrated device run and scheduling correction

First app-only upload verified; boot showed the4-flash indicator and loaded
startup/catalog, then the audio worker starved IDLE0/main. Repeated watchdog
reports were preserved in ignored `build/validation/2026-10-15-full-port-runtime-1.log`.
Backtrace resolves to worker stack-water scan in a loop using a2ms delay that
rounds down to0 at100Hz. This is a failed integration run, not a successful
playback/boot result. Replaced with a minimum-one-tick blocked delay, and updated
host RTOS stubs to catch the conversion. Producer tests pass under modeled100Hz.
The corrected primary binary was subsequently programmed and recovered; its
observed playback result is recorded below. The subsequent dual-role source
revision builds successfully and has not inherited secondary hardware acceptance.

The catalog now ignores dot-prefixed AppleDouble metadata files rather than
warning about them as unsupported WAVs. Source assets/card files remain untouched.

## Corrected primary playback — observed hardware result

After correcting the scheduler delay, Anthony completed local diagnostic playback
of `Skit - milkshakes.wav`: 741424 source frames, zero reported underruns. Cam
heard Anthony's A-role dialogue through “we don't have hands,” and Marco resumed
after playback while Jeff was unavailable. This verifies that specific primary
local playback run and recovery to peer-search behavior. It does not demonstrate
Jeff's B-role sound, real GATT compatibility, acoustic synchronization, Matter
triggered paired playback, or the newest dual-role binary on hardware.


## Dual-role build and upload

Final source compiled with ESP-IDF5.5.4. Image2096256 bytes; static IRAM130079
of131072 bytes (993 free), DRAM100604 bytes. Eight Python tests passed, including
an actual-secondary-adapter stale-connection regression. Actual config/storage
source tests and28 controller scenarios passed under host sanitizers. Independent
integration review found no additional blocker; RF/protocol timing remains open.

App-only upload at0x30000 verified the image hash on expected MAC
`a0:a3:b3:80:16:38`. No NVS/partition erase was performed. Manifest:
`build/validation/2026-10-15-dual-role-build-hashes-1.json`; upload:
`2026-10-15-dual-role-flash-1.log`; capture:
`2026-10-15-dual-role-runtime-1.log` (all ignored/private). Boot mounted Anthony's
card, emitted the4-flash code, completed the gentle jaw check, loaded all15assets,
connected the saved speaker and started initialization at7.796s. Further runtime
results follow after observation. The latest image still awaits physical Jeff.


## Latest primary regression and local heap windows

The dual-role image delivered all startup PCM by11.966s and resumed Marco. Serial
`trigger` was ignored while peer=0, exercising the same admission gate used by
Matter. Local Milkshakes then ran39.656–56.476s:741424 source frames, zero reported
underruns, followed by Marco. This image has serial delivery proof; earlier owner
audible confirmation applies to the preceding corrected primary image.

The SDK local-minimum monitor recorded32163 internal bytes for a12.8s ordinary
search/Marco window, and29747 bytes for a22.3s window containing the full skit
and subsequent Marco. The latter is below the proposed32KiB floor. These are
conservative per-region minima, not attribution of one contiguous allocation.
Largest free block sampled was36864 bytes; producer minimum unused stack2356
bytes, provider wall-time maximum3252us at window end, zero dropped radio events.
No watchdog/panic appeared in this bounded run. Static IRAM remains only993bytes
free. Preserve resource/timing acceptance as open under real paired load; do not
infer yard/soak readiness or change stacks based on these short windows alone.

The pre-board software slice is complete: both roles compile, the available host
checks pass and Anthony's local pipeline runs. Next physical gates are secondary
startup/Polo, actual filename/ACK/readback exchange, both speakers' role audio,
paired acoustic timing, Home-triggered paired skits, cancellation/reconnection,
fault patterns and recovery, full-load resources and yard soak. Work remains in
the requested isolated worktree, uncommitted; no card contents were changed.


## Replacement board follow-up

The same final binary now runs on both boards. The new Jeff mounted his original
card, selected secondary/JBLFlip6/60%, announced his role and played Polo. Physical
volume explained his initially silent media. Longer playback exposed a separate
producer throughput deficit; an8192-byte PSRAM stdio buffer and bounded two-block
refill bursts corrected it in the subsequent paired Starbucks run. Both drained
1647341frames with zero underruns; Cam confirmed clean alternating dialogue.
See [the full two-board evidence](2026-10-15-jeff-first-boot.md), including failed
runs, owner confirmation and remaining resource/actuator/soak boundaries.
