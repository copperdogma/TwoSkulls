# Single-WROVER prototype: sibling audit and technical research

Date: 2026-10-14 (America/Edmonton). Scope: research and planning only.
Companion: [implementation story](../stories/story-001-single-wrover-matter-prototype.md).

## Finding and recommendation

Prototype Anthony on one classic ESP32-WROVER with onboard SDMMC, A2DP **source**,
existing BLE GATT peer control, and a Matter-over-Wi-Fi accessory. Keep Jeff's
firmware and direct BLE protocol unchanged. Weak yard Wi-Fi makes moving peer
coordination onto the house network undesirable. A working single-board result
has not been demonstrated by any inspected sibling project.

The best local starting stack is **ESP-IDF 5.5.4 + managed ESP-Matter 1.5.0**,
using native IDF for the combined experiment and one Bluedroid BTDM host/controller.
This choice follows current local examples and source availability, not a claim
that these are the newest upstream releases. Resolve and freeze an **esp32** target
lock independently; the sibling locks target **esp32c3**. Preserve the already
compiled Arduino SD-only migration as the baseline and rollback.

Two earlier conversational simplifications need correction:

- Matter is not inherently NimBLE-only: the inspected ESP-Matter 1.5.0 component
  selects a Bluedroid backend when NimBLE is disabled. Integration still has
  controller-lifetime and callback-ownership traps.
- Putting every radio task on one “communications core” is not the documented
  coexistence starting point. Espressif recommends separating Wi-Fi from Bluetooth
  controller/host tasks across cores. Profile application work around those tasks.

## Evidence classes and provenance

This is a local filesystem/source audit plus primary-source web research. Local
tracking refs were inspected but not fetched; ahead/behind counts are relative to
those local refs. No sibling checkout, firmware, credentials, or hardware changed.
Documentation of prior hardware success is labeled as such, not a fresh rerun.
Compiled firmware, source capability, commissioning success, audible quality, and
owner acceptance are separate evidence levels.

### Project inventory

| Project | Local provenance | What is current/useful | Limit or caution |
| --- | --- | --- | --- |
| TwoSkulls | `main e4b48ff`, clean; isolated migration branch at same base with uncommitted changes | Arduino-ESP32 2.0.17; A2DP 1.8.3; AnalogWrite 5.0.2; FFT 1.6.1; SD migration compiled earlier in this chat | No board flash or physical migration yet |
| lightbox-sky | `e2dd3c7`, local main one ahead; firmware tracked/clean, unrelated `outputs/` untracked | IDF 5.5.4 / managed Matter 1.5.0; committed source, dated Apple Home/bench evidence; state ownership, persistence, mDNS | C3/NimBLE, no A2DP or custom peer BLE proof |
| box-of-death | `f8d4225`, local tracking equal; dirty docs and **untracked firmware/** | IDF 5.5.4 / Matter 1.5.0 prototype; physical button event handling, nonblocking light controller, Oct 10 bench notes | Active local prototype, not a committed firmware release; generic-switch input semantics differ from a remotely commanded skull |
| matter-occupancy-sensor | `ed1d28c`, extensive uncommitted modernization | Working tree v2.1: IDF 5.5.4 / Matter 1.5.0 lock; commissioning/reset/flash-preservation methodology | HEAD date does not describe working source; preserve all owner edits |
| death-fortune-teller | `e9756f4`, one behind local origin; unrelated dirty plan and untracked schematics | Arduino/PlatformIO audio refactor, SDMMC, UART adapters, native **host tests** | No native-IDF firmware migration; platform/A2DP URL not fully pinned; long A2DP reliability story still open |
| death-matter-controller | `519413ee`, clean | C3 Matter-to-UART protocol; lock records IDF 5.4.1; prior Apple Home/UART documentation | External `ESP_MATTER_PATH`; lock does **not** pin esp_matter. Its `1.5.2` entry belongs to `rmaker_common`, not Matter |
| death-poc | `5033c84`, clean | Prior two-chip Matter/UART proof and protocol observations | Reported POC uses S3 + C3; not classic-WROVER triple-radio evidence |
| matter-skull-switch | `0119c36`, four ahead; modified todo | Existing incoming OnOff → 500 ms GPIO pulse; documents Home behavior | Older 5.4.1-era stack; trigger GPIO differs between TwoSkulls docs and C3 source |
| matter-generic-switch | `1120d96`, clean | Older generic-switch/button example; Matter manifest `^1.4.0` | Input accessory semantics are not a remote playback command |
| matter-two-zone-occupancy-sensor | `8082a7c`, clean | Older C3/H2 target variants, factory partition and OTA examples | External SDK checkout; device-specific flash layout and old provisioning workarounds |
| known-good-matter-two-zone-occupancy-sensor | non-Git 2025-10-17 snapshot | Legacy comparison only | No commit provenance; deterministic credential fallback/logging unsuitable for reuse |
| esp32-matter-guide | `bd33060`, clean | Historical setup background | 5.4.1-era mutable SDK and patch/erase advice should not be carried forward automatically |
| xavier-dino-skull | `1a7e035`, modified source/README | Peripheral exploration, S3 touch POC | Current C3 touch readings are synthetic; not reusable physical validation |

Death's one-commit difference is `9665c0f`, which adds only
`docs/project-context.md`. It changes the planned visitor interaction toward an
illuminated button box, not the firmware source audited here. It does not change
TwoSkulls' accepted phone/motion-trigger use case.

### Best local source patterns to adapt

1. **Small callbacks and one owner for Matter state.** Lightbox
   [app_main.cpp](/Users/cam/Documents/Projects/lightbox-sky/firmware/main/app_main.cpp:383)
   schedules its small controls on the Matter system layer. Its
   [web boundary](/Users/cam/Documents/Projects/lightbox-sky/firmware/main/sky_web.cpp:60)
   uses stack locking/scheduled work rather than arbitrary-thread attribute mutation.
   For TwoSkulls, keep the same boundary but move audio/actuator work to separate
   bounded tasks; do not run the show from a Matter callback.
2. **Correct attribute handling and persistence.** Lightbox
   [attribute dispatch](/Users/cam/Documents/Projects/lightbox-sky/firmware/main/app_main.cpp:500)
   filters endpoint/cluster/type, guards internal updates, handles nullable values,
   and reads hydrated retained state. For a momentary skull command, restored OnOff
   must never replay a skit. Treat expected unchanged-value results as nonfatal.
3. **Current commissioning/upgrade discipline.** The local occupancy
   [handoff](/Users/cam/Documents/Projects/matter-occupancy-sensor/docs/device-methodology-handoff.md:18)
   and [setup](/Users/cam/Documents/Projects/matter-occupancy-sensor/SETUP.md:34)
   require a per-device partition/identity contract, app-only update verification,
   and separate controller/hardware evidence. Reuse the process, not its C3 offsets.
4. **Deliberate reset and no automatic erase.** Lightbox/Box fail an NVS error
   without silently erasing fabrics. The occupancy
   [reset implementation](/Users/cam/Documents/Projects/matter-occupancy-sensor/firmware/main/drivers/factory_reset_button.cpp:11)
   uses a long-hold/release gesture. TwoSkulls must choose its own validated GPIO or
   explicit maintenance command; do not copy C3 BOOT pin assignments.
5. **Simple trigger semantics.** The actual skull C3
   [pulse handler](/Users/cam/Documents/Projects/matter-skull-switch/firmware/main/app_main.cpp:138)
   consumes incoming Matter OnOff. Box's generic-switch `InitialPress` and
   `ShortRelease` are useful event examples, but they describe an input button;
   they are the wrong direct replacement for an Apple Home “turn on skull” command.
6. **Audio producer separation.** Death's
   [audio player](/Users/cam/Documents/Projects/death-fortune-teller/src/audio_player.cpp:85)
   moves SD filling into `update()` and pads shortages. Adapt the principle. It
   still performs String/state/animation work from the provider path and is not
   a finished real-time template. Its native test environment tests portable C++;
   it is not proof of a native-IDF firmware build.
7. **Onboard card setup.** Death's
   [SD manager](/Users/cam/Documents/Projects/death-fortune-teller/src/sd_card_manager.cpp:8)
   provides the local 1-bit/20 MHz configuration and servo GPIO23 precedent. Its
   migration log reports basic physical success; it does not close the separate
   [A2DP reliability story](/Users/cam/Documents/Projects/death-fortune-teller/docs/stories/story-006-bluetooth-a2dp-reliability.md:1).
8. **Only add discovery if needed.** Lightbox shares IDF mDNS with Matter using
   `CONFIG_USE_MINIMAL_MDNS=n` and a delegated alias. Reuse only if a local web
   console is later needed; no web UI/server is required for this prototype.

### Advice that should not be copied

Do not import old full-chip erase commands, fixed factory offsets, commissioning
payloads, MACs, certificates/private keys, deterministic setup-code fallbacks, or
mutable dependency patches. Do not import C3-only pins, partitions, NimBLE setup,
BLE memory-release behavior, or “production ready” labels. Death's old IDF
migration story contains unmeasured percentage gains and proposed core layouts;
those are not benchmark results. Toolchain changes need an explicit new evidence
baseline rather than a general “newer is better” assumption.

## TwoSkulls source findings that affect the design

Source links refer to the original main checkout, not the planned implementation.

- [A2DP provider](/Users/cam/Documents/Projects/TwoSkulls/audio_player.cpp:57)
  holds a mutex while refilling from SD and invoking callbacks. The frame callback
  calls the animator, which may copy skits/Strings, compute RMS, log and drive
  servo/eyes. Core assignment alone cannot make this path bounded.
- The provider can copy fewer frames than requested but report the full request;
  the unused tail is not always initialized. End-of-file and starvation behavior
  need explicit tests before using audio quality as a radio metric.
- Current ring is 8,192 bytes = **46.44 ms** at 44,100 × 2 × 2 bytes/second.
  This is PCM buffering capacity, not available radio airtime or total latency.
- [startNextFile](/Users/cam/Documents/Projects/TwoSkulls/audio_player.cpp:219)
  skips a hardcoded 128 bytes instead of reading RIFF chunks. Read-only parsing of
  all 15 checked-in WAVs found 44.1 kHz stereo 16-bit PCM, durations 0.6–63.7 s,
  and data offsets from **44 to 110 bytes**. Some skips are not stereo-frame
  aligned. A deterministic native pipeline needs RIFF parsing plus a comparison
  against current output; the source card on Anthony remains uninspected.
- [Servo breathing](/Users/cam/Documents/Projects/TwoSkulls/servo_controller.cpp:115)
  uses a delay loop and cross-context interrupt flag. [Eye control](/Users/cam/Documents/Projects/TwoSkulls/light_controller.cpp:33)
  writes the previous brightness in its PWM branch. These are existing behavior
  risks to isolate, not evidence that missing LEDs cause SD errors.
- [BLE client](/Users/cam/Documents/Projects/TwoSkulls/bluetooth_controller.cpp:578)
  blocks up to five seconds awaiting response and then readback. The scan duration
  is 10000 with a “10 seconds” comment; verify the library's **seconds** API before
  carrying this value forward. No unbounded scan/retry belongs in a show task.
- The server advertises INDICATE, while its write callback invokes `notify()`.
  Freeze the observed CCCD/notification/indication behavior of real Jeff before
  implementing an IDF client. Do not “correct” the server during Anthony's port.
- Jeff's validator checks file existence and not-currently-playing, then the
  server notifies **before** dispatching playback. Its later playback callback
  can reject when the speaker is disconnected. Existing ACK/readback is therefore
  not proof of readiness or audible playback. The client reads only after an
  indication; readback alone is not a timeout fallback.
- Script A/B speaking state also controls **PCM muting**, not just animation.
  A new pipeline must carry sample-indexed role masks independently of any
  jaw/speaker-latency compensation, including transitions within a PCM block.
- `isBleConnected()` tests the server flag twice; the primary path instead checks
  `clientIsConnectedToServer()`. Record this as existing code evidence, not a
  proven explanation of historical failures.
- The [current setup](/Users/cam/Documents/Projects/TwoSkulls/TwoSkulls.ino:276)
  prefers Wi-Fi even though this sketch has no Matter/Wi-Fi setup. Re-evaluate
  policy from the SDK coexistence defaults and measured results.

## Primary-source research and decisions

All sources accessed 2026-10-14. Versioned 5.5.4 sources are preferred for the
candidate stack; upstream examples are ingredients, not proof of this workload.

| ID | Source | Finding and applicability |
| --- | --- | --- |
| R1 | [IDF 5.5.4 ESP32 RF coexistence](https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32/api-guides/coexist.html) | Wi-Fi, Classic and BLE share time on one RF path. STA coexistence is supported; scanning/reconnecting changes arbitration. Enable software coexistence and compare documented cross-core placement. No guarantee for this whole application. |
| R2 | [IDF 5.5.4 speed/task priorities](https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32/api-guides/performance/speed.html) | Measure first; preserve radio-task priority/CPU access. Flash operations can interrupt normal execution. Application work must yield and remain bounded. |
| R3 | [IDF FreeRTOS SMP](https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32/api-reference/system/freertos_idf.html) | Shared memory requires synchronization. Affinity does not prevent races. Task stacks use bytes in IDF; blocking/critical sections need careful ownership. |
| R4 | [ESP32 external RAM](https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32/api-guides/external-ram.html) | PSRAM cannot replace every internal/DMA allocation and is unavailable during cache-disabled intervals. Keep critical working storage internal; measure both heaps. |
| R5 | [A2DP API](https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32/api-reference/bluetooth/esp_a2dp.html) and [legacy header](https://github.com/espressif/esp-idf/blob/v5.5.4/components/bt/host/bluedroid/api/include/api/esp_a2dp_legacy_api.h) | Legacy PCM source callback still exists in 5.5.4 but is deprecated; it runs in the source task. The newer encoded-data API is a distinct path. Choose one explicitly; do not accidentally add an unbudgeted encoder migration. |
| R6 | [ESP coexistence FAQ](https://docs.espressif.com/projects/esp-faq/en/latest/software-framework/coexistence.html) | Records audio loss/lag for Wi-Fi + BLE scan + A2DP **sink**. Useful warning about the problem class, not measurement of our source. |
| R7 | [Matter 1.4 FAQ](https://docs.espressif.com/projects/esp-matter/en/release-v1.4/esp32/faq.html) | BLE is normally reclaimed after commissioning; ongoing BLE needs explicit retention. FAQ examples are NimBLE-specific and do not settle Bluedroid integration. |
| R8 | [IDF heap diagnostics](https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32/api-reference/system/heap_debug.html) | Monitor capability-specific free/minimum/largest block and allocation failures, not just total free heap or static linker size. |
| R9 | [ESP32 Bluetooth host API](https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32/api-reference/bluetooth/index.html) | Bluedroid supports Classic and BLE. NimBLE alone cannot provide this A2DP source. |
| R10 | [IDF A2DP source example](https://github.com/espressif/esp-idf/tree/v5.5.4/examples/bluetooth/bluedroid/classic_bt/a2dp_source) | Native source integration and callback structure to adapt; synthetic example audio is not local SD/skit/servo acceptance. |
| R11 | [Pinned Matter Bluedroid backend](https://github.com/espressif/connectedhomeip/blob/cf84d0360c48dbc194c48b47b09169f302a9745b/src/platform/ESP32/bluedroid/BLEManagerImpl.cpp) and [backend selection](https://github.com/espressif/esp-matter/blob/a51f624f0735aefd0a9cfe1e0039d68de8ce24e2/CMakeLists.txt) | Existing alternative to NimBLE; controller startup and GAP/GATTS ownership require explicit integration. |
| R12 | [IDF 5.5.4 GAP implementation](https://github.com/espressif/esp-idf/blob/v5.5.4/components/bt/host/bluedroid/api/esp_gap_ble_api.c) and [public declarations](https://github.com/espressif/esp-idf/blob/v5.5.4/components/bt/host/bluedroid/api/include/api/esp_gap_ble_api.h) | Registration replaces the BLE GAP callback; a getter permits investigating deliberate forwarding instead of assuming an SDK patch is mandatory. |
| R13 | [Standalone source initialization](https://github.com/espressif/esp-idf/blob/v5.5.4/examples/bluetooth/bluedroid/classic_bt/a2dp_source/main/main.c) | Releases BLE memory and selects Classic-only; cannot be copied unchanged into a BTDM owner. |
| R14 | [Pinned 1.5 README](https://github.com/espressif/esp-matter/blob/a51f624f0735aefd0a9cfe1e0039d68de8ce24e2/README.md), [moving release README](https://github.com/espressif/esp-matter/blob/release/v1.5/README.md), [release manifest](https://github.com/espressif/esp-matter/blob/release/v1.5/idf_component.yml) | Distinguish the local 1.5.0/5.5.4 candidate from later 1.5.1/5.5.5 recommendations; freeze a tested pair. |
| R15 | [CHIP commissioning guide](https://project-chip.github.io/connectedhomeip-doc/development_controllers/chip-tool/chip_tool_guide.html), [factory payload generator](https://github.com/project-chip/connectedhomeip/blob/master/scripts/tools/generate_esp32_chip_factory_bin.py) | On-network commissioning requires already connected IP networking and appropriate discovery/setup payload. It does not prove Apple Home usability. |
| R16 | [Apple Matter overview](https://developer.apple.com/apple-home/matter/) and [commissioning delegate](https://developer.apple.com/documentation/matter/mtrcommissioningdelegate) | Apple supports Matter and its framework recognizes on-network devices; test the stock Home app rather than infer its exact onboarding behavior. |
| R17 | [Bluedroid compile regression #1441](https://github.com/espressif/esp-matter/issues/1441) | Historical 1.4.1-path build failures justify an early exact-version build gate; not proof that the selected pair fails. |
| R18 | [A2DP/GATTS coexistence example](https://github.com/espressif/esp-idf/blob/v5.5.4/examples/bluetooth/bluedroid/coex/a2dp_gatts_coex/README.md) | Example uses A2DP sink and BLE server, the opposite application roles from Anthony, and does not supply the Matter/Wi-Fi workload. |

The local `/Users/cam/esp/esp-idf-v5.5.4` checkout reports `v5.5.4` and was clean
when inspected. Its A2DP main header includes `esp_a2dp_legacy_api.h`; the example
still calls the legacy PCM provider. This resolves a superficial API mismatch
without inventing a new encoder workaround. No SDK build was run in this pass.

### Local modern stack versus upstream latest

The local managed component's metadata identifies ESP-Matter 1.5.0, repository
commit `8e51777a50f5a82f945ce055f3ad1a9792b974b4`. Its installed Bluedroid backend
matches the R11 pinned CHIP file byte for byte, SHA-256
`e580620ef301c7e9fdae628ed07ec9ba8fedf72eca5d6807eeaf4df394ffc506`.
This is a source applicability check, not an ESP32 compile or runtime test.

The public 1.5 reference at `a51f624` recommends IDF 5.4.1 for the general case;
the current `release/v1.5` README/manifest advertises 1.5.1 and recommends IDF
5.5.5. Thus the locally successful C3 combination 1.5.0/5.5.4 is not identical
to either published recommendation. The decision is to try that familiar pinned
combination in the small classic-ESP32 gate first, and move to a deliberately
locked recommended pair if required. Do not claim that sibling hardware results
certify the new target, or upgrade to moving main merely because it is newer.

## Exact Matter/Bluetooth integration gate

The managed component installed under Lightbox is ESP-Matter **1.5.0**; its
`CMakeLists.txt:421` selects `platform/ESP32/bluedroid` when NimBLE is off.
The local backend is therefore the primary applicability reference:

[BLEManagerImpl.cpp](/Users/cam/Documents/Projects/lightbox-sky/firmware/managed_components/espressif__esp_matter/connectedhomeip/connectedhomeip/src/platform/ESP32/bluedroid/BLEManagerImpl.cpp).

Audit and test all of the following before importing the skull application:

- Controller ownership: Matter's idle-controller path releases Classic memory
  and enables BLE-only. A shared owner must initialize and enable **BTDM** first,
  or use an explicit supported integration hook; do not allow Classic memory release.
  Conversely, the standalone A2DP example releases BLE memory: use neither
  example's single-purpose initialization policy unchanged.
- Host ownership: exactly one Bluedroid initialization, lifetime, and teardown.
  No simultaneous independent NimBLE host, second Arduino Bluetooth initializer,
  or post-commissioning controller shutdown while A2DP/peer BLE are active.
- GAP ownership: Matter registers a BLE GAP callback. A custom peer scanner also
  needs events. IDF 5.5.4 exposes a callback getter; assess an intentional dispatcher
  preserving Matter's handler and routing application events. Registration order,
  asynchronous initialization and re-registration must be tested. A source patch
  is **not assumed mandatory**; if required, pin and test a narrow component patch.
- GATT ownership: distinct Matter server and peer client app IDs, correct filtering
  by interface/connection, sufficient handles/connections, and no event theft.
- Identity/security: test discovery and pairing using the actual Apple Home/iPhone
  path, preserving real Jeff's UUIDs and behavior. Generic chip-tool success is
  insufficient for the user's workflow.

An alternative experiment can disable CHIPoBLE and commission on-network after
explicit Wi-Fi provisioning, leaving Bluedroid entirely application-owned. This
is a fallback experiment only: Apple Home usability, reopen/recommissioning, and
recoverability must pass before selecting it. It is not permission to replace the
normal pairing experience silently or to leave a permanent provisioning AP.

## What was locally verified this pass

- Read current code/config/lockfiles, source state and recorded bench evidence in
  the listed sibling projects; corrected a misleading `1.5.2` component attribution.
- Compared Death's behind-tracking delta: documentation-only, no source difference.
- Verified selected 5.5.4 API/Kconfig symbols and local Matter backend source.
- Parsed the 15 repository WAVs without modifying them; confirmed format/offset
  variation and calculated the current ring duration.
- No new firmware was written, built, flashed, paired, or tested on hardware.
  Every radio, timing, heap and Apple Home result in the story remains a future gate.

## Decision record

Adopt current managed-IDF workflow, explicit lifecycle ownership, queues,
capability-aware resource measurements, preserved credentials, and staged physical
acceptance. Adapt Death's SD/audio lessons and Lightbox's Matter boundaries.
Preserve peer BLE and the existing skit/role contract. Defer Wi-Fi peer control,
ESP-NOW, new clock synchronization, OTA/web UI, and perfboard fabrication.

Next experiment when implementation is authorized: confirm Anthony's hardware and
recoverable firmware baseline, then build the minimal exact-stack Bluetooth/Matter
integration probe. Keep the C3 available as the fallback if radio, memory, or
commissioning gates fail; failure of the experiment is a valid recorded result.
