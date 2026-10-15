# Finite discovery and internal heap pressure — 2026-10-15

## Problem and established approach

Two separate diagnostic problems are present: asynchronous device discovery with
silent successful transitions, and scarce capability-specific internal memory
when several stacks share a microcontroller. Instrument state transitions before
inferring hardware or pairing failures, and move eligible ordinary data to PSRAM
while retaining memory required by stacks, interrupts and DMA internally.

The [IDF 5.5.4 Classic GAP API](https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32/api-reference/bluetooth/esp_gap_bt.html)
defines finite inquiry duration in units of 1.28 seconds and asynchronous result
and discovery-state callbacks. Our length 8 therefore gives a 10.24-second window.
The probe had no autonomous rediscovery; only explicit retry invokes another
window. Jeff being off does not prevent a speaker inquiry. Added logs for radio
start, GATTC registration, inquiry request/state and Classic candidates. Exact
configured-name selection is preserved; no arbitrary device is selected.

The [external RAM guide](https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32/api-guides/external-ram.html)
distinguishes ordinary malloc preferences from explicit capability allocation,
internal reservations, and cache/stack limitations. The pinned Matter component
provides CONFIG_CHIP_MEM_ALLOC_MODE_EXTERNAL. Its CMake selects
CHIPMem-PlatformExternal.cpp, whose MemoryAlloc/Calloc/Realloc use
MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, rather than changing application task stacks.
Adopt this supported allocator as a bounded experiment, since the default mode
prefers internal memory for the many allocations below the 4096-byte threshold.

## Local evidence and limits

Before the allocator experiment, Home commissioning completed in the UART log,
but internal heap minimum reached 17,039 bytes; steady free was about 37.3 KB.
This does not meet the proposed 32 KiB minimum. Audio remained disconnected and
Jeff was off. No combined-load acceptance follows from this capture.

Build/boot and post-change measurements will be recorded in the implementation
log. Preserving commissioned NVS allows a reboot/persistence test; it does not
repeat fresh onboarding or establish its new worst-case minimum. Do not erase
Home credentials to manufacture a fresh commissioning trial while the owner is
unavailable. Physical speaker discoverability and audible output remain unknown.

The serial capture helper also mixed select(fd) with buffered readline(), which
can strand a second command already buffered in Python. It now forwards raw fd
chunks, allowing the device to parse multiple newline-delimited commands.

## Additional GATTC source check

Independent review of pinned IDF found ordinary disconnect/service-change paths
supply a concrete registered client interface. The current filter is therefore
not a demonstrated cause of the absent speaker inquiry. IDF's gattc_demo also
supports ESP_GATT_IF_NONE broadcast events; accepting that contract with existing
connection/address filtering is a future peer-robustness improvement, to validate
with a real secondary. Do not substitute this unobserved possibility for the
measured finite inquiry behavior or claim a peer test while Jeff is off.

## Bounded local result

The external-allocator build and verified app-only flash succeeded. Commissioned
reboot restored Wi-Fi/fabrics and resumed a secure Matter session. Internal free
settled near 39.4 KB, minimum 27,151 bytes. The 32 KiB minimum remains unmet; fresh
commissioning is not compared against this narrower reboot load as an A/B test.
The instrumented initial inquiry and explicit retry found other named devices,
but no configured Flip 5. Adopt the logs and supported allocator provisionally;
retain speaker/combined-load acceptance as open. Next discriminating experiment:
when the owner is available, overlap a logged inquiry with the intended speaker's
pairing mode and confirm its actual advertised name, then test selected A2DP.

## Reconnect semantics after audible success

The tone was audible after exact-name discovery, but a subsequent reboot found no
Flip 5 once its pairing visibility ended. The probe had no saved target address,
so it required discovery again. This is a reconnect-state omission, not evidence
that the powered speaker or Matter trigger failed. The existing
[ESP32-A2DP upstream implementation](https://github.com/pschatzmann/ESP32-A2DP/blob/main/src/BluetoothA2DPCommon.cpp)
uses a saved last-connection address and reconnects directly. Adopt that standard
pattern with stricter identity binding: one atomic NVS blob containing the
SD-configured speaker name and successfully connected address, loaded only when
the name still matches. Save only after the selected A2DP CONNECTED event, not
from an arbitrary discovery candidate. Do not import the library's automatic
whole-NVS erase behavior.

At startup or explicit retry, try the remembered address once, then use finite
name discovery if that direct connection fails. Review caught an unbounded
failure loop in the first draft; a distinct direct-attempt fallback flag now
prevents a discovery-selected connection failure from starting another inquiry.
Real saved-address reboot proof remains necessary after the new cache is seeded
by an actual successful connection. NVS errors are logged without erasing data.

## Reconnect collision experiment

The first cached boot logged HCI 0x0B, timed out after about 60 seconds, and an
explicit retry then connected without pairing. Bluetooth's
[controller error table](https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-60/out/en/architecture,-change-history,-and-conventions/controller-error-codes.html)
defines this as Connection Already Exists. Pinned
[IDF L2CAP source](https://github.com/espressif/esp-idf/blob/v5.5.4/components/bt/host/bluedroid/stack/l2cap/l2c_link.c)
waits for an incoming request in this case and handles concurrent connection
attempts. Local btm_inq.c enables page scanning only for CONNECTABLE mode.
This supports testing CONNECTABLE/NON_DISCOVERABLE after cache identity is loaded;
it does not prove simultaneous paging caused the observed failure. Unknown
incoming addresses still fail selection. The official A2DP source example disables
incoming connections for its simpler state machine, so this is a measured policy
adaptation rather than a universal correction to the SDK example.

Opening this Mac/CH340 serial capture causes reboot even without --reset; repeated
monitor reopen and post-flash startup can therefore create rapid back-to-back
reboots while the speaker retains a previous link. Use esptool --after no_reset,
then one deliberate captured reset, and keep the monitor open through subsequent
controlled reboot tests. Record transport recovery separately from audible proof.

## Internal queue allocation candidate

The [IDF supplemental FreeRTOS API](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32/api-reference/system/freertos_additions.html)
provides xQueueCreateWithCaps. Pinned idf_additions.c allocates both queue storage
and control via the requested heap capabilities before xQueueCreateStatic. Our
32 copied radio events are task-context messages, not ISR or DMA buffers. Moving
this approximately 9 KB queue to PSRAM is an eligible next memory experiment;
task stacks remain internal and should be measured before reducing them.
A queue created with that API must be deleted with vQueueDeleteWithCaps if teardown
is added. Current radio ownership is boot-lifetime. No ISR may use this queue
without revisiting cache-safety constraints.

### Queue placement decision

Use xQueueCreateStatic with a static internal StaticQueue_t and a separately
allocated PSRAM payload, rather than WithCaps(SPIRAM) for the entire object.
Pinned FreeRTOS tasks.c removes a timed-out receiver's xEventListItem, updating
its containing list in queue control; keeping that control internal avoids a
hidden scheduler dependency on external memory. Only copied payload accesses are
external. CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY is off. The payload has
boot lifetime, all producers/consumer are task context, and overflow remains a
latched failure. A future teardown uses vQueueDelete plus explicit payload free;
WithCaps deletion is not appropriate for the static control object. Independent
review validated this source reasoning; runtime stress still needs measurement.

## Crypto allocation candidate

The split queue recovered roughly 9 KB, but the short combined recovery minimum
was 32,675 bytes, still below the 32 KiB proposal. Both application stacks have
more than 3 KB unused in this capture; keep their sizes until untested peer paths
are exercised. Pinned
[mbedTLS Kconfig](https://github.com/espressif/esp-idf/blob/v5.5.4/components/mbedtls/Kconfig)
provides MBEDTLS_EXTERNAL_MEM_ALLOC when SPIRAM is enabled and explicitly permits
an alternative allocation mode when internal memory is insufficient. The port's
allocator uses MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT for that option. Test this
supported setting rather than reducing stacks or weakening the proposed guard.

Kconfig prefers internal allocation for protection of crypto material; classic
ESP32 external PSRAM lacks the later chips' encrypted-PSRAM protection. This is a
prototype tradeoff consistent with the already external Matter heap, not a
production physical-security qualification. It does not change NVS ownership,
erase fabrics, move stacks, or imply that any buffer is now directly DMA-capable.
Fresh commissioning and combined-load stress remain distinct tests.

Independent pinned-source review found the classic target selects block AES and
parallel-engine SHA, while MPI copies limbs through CPU register accesses. There
was no demonstrated arbitrary-PSRAM DMA incompatibility. The external allocator
is external-only, not prefer/fallback: PSRAM exhaustion returns allocation failure
even if internal heap remains. Preserve cache workarounds and keep crypto in
normal task context. This source review permits the bounded hardware experiment;
it does not replace runtime secure-session/latency measurements.

## Revised reconnect decision after repeat failure

The CONNECTABLE experiment was not consistently successful. A later boot logged
an ACL success followed by L2CAP bad-state handling and another 0x0B; audio did
not start. Do not claim page scanning fixed reconnect. The earlier upstream
comparison read BluetoothA2DPCommon but missed BluetoothA2DPSource's override,
which selects nonconnectable mode. Correct that interpretation and restore the
source-role NON_CONNECTABLE/NON_DISCOVERABLE policy.

The [upstream auto-reconnect notes](https://github.com/pschatzmann/ESP32-A2DP/wiki/Auto-Reconnect)
explicitly describe a peer retaining an old link after abrupt ESP reset and the
source trying a limited number of remembered-address connections before name
scanning. Adopt that established pattern with three attempts and five-second
backoff as local experimental bounds, not guarantees of peer link expiry.

Observe selected-address GAP ACL connection/disconnection events as well as
A2DP state. Profile DISCONNECTED alone does not prove the underlying ACL is gone.
Retain an already-successful ACL when a second connect reports failure, wait for
its actual teardown before another attempt/fallback, and bound that wait to 60
seconds. A separate 75-second connection-attempt watchdog stops automatic work
when terminal callbacks are absent, without launching overlapping connections or
resetting the shared Bluetooth host. Explicit retry also respects active backoff
and unresolved ACL state. A late terminal event can establish safe state again;
otherwise report the held source connection instead of guessing.

This policy is source-reviewed before flashing; runtime reconnect repetitions
must still establish whether it solves the observed startup cases. The successful
Home/audible tone on earlier probe images remains separate evidence.

## Full-port worker starvation: RTOS tick granularity

General problem class: a short millisecond delay converted to zero OS ticks can
leave a higher-priority polling task perpetually runnable, starving idle/main
rather than actually blocking. First integrated run triggered IDLE0 watchdog;
addr2line identified skull_audio worker at its stack-water sampling loop.
The project has CONFIG_FREERTOS_HZ=100; pdMS_TO_TICKS(2) is zero.

[FreeRTOS vTaskDelay reference](https://www.freertos.org/Documentation/02-Kernel/04-API-references/02-Task-control/01-vTaskDelay)
and pinned local IDF FreeRTOS tasks.c:1583 explicitly state that zero delay only
forces a reschedule. Adopt a minimum-one-tick blocked delay, preserving100Hz
and the watchdog. The host shim now models100Hz and asserts that worker delays
are nonzero; producer tests pass with this condition. Do not disable watchdogs
or blindly increase task priorities to hide scheduler starvation. Device rerun
is needed to establish application progress after this fix.

One subsequent bootloader entry reported an already-running stub/flash-command
format error. Closed monitor ownership was checked with lsof, then explicit
ESP32 ROM/no-stub identity read succeeded. [Espressif troubleshooting](https://docs.espressif.com/projects/esptool/en/latest/esp32/troubleshooting.html)
recommends exclusive serial access, explicit chip selection and lower baud for
unreliable transfer. Chose the verified ROM state followed by no-reset stub
upload at230400 baud; this is a bounded programming transport experiment,
not evidence of an electrical fault. Do not erase NVS or change wiring based
on that isolated error. Write/verification outcome remains separately recorded.

## Attributing internal-RAM lows before optimizing

General problem class: a historical low-water statistic does not identify the
operation that caused it. ESP-IDF's `heap_caps_get_minimum_free_size` sums each
matching heap region's independent minimum; those minima can occur at different
times. The reported23095-byte low therefore is a conservative aggregate, not
proof that a particular WAV read allocated a15KiB buffer.

The pinned5.5.4 `components/heap/include/esp_heap_caps.h` documents this caveat
and the supported local-window monitor start/stop API. The [Espressif heap API
reference](https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32/api-reference/system/mem_alloc.html)
provides the same measurement contract. Adopt bounded idle/search and skit
windows with current free/largest-block samples before changing allocation
policy. New diagnostic commands `heap-start`/`heap-stop` expose that API; they
preserve the earlier boot history after the window ends.

Read-only source audit: FatFS already prefers external RAM; its FIL caches are
allocated during mount. This toolchain's stdio buffer is128 bytes, and classic
ESP32 SDMMC may need a512-byte internal DMA bounce sector for PSRAM reads.
Moving a hypothetical large stdio buffer is therefore not a supported remedy.
Matter and mbedTLS already use external allocation policies. Do not shave task
stacks or enable costly heap tracing without a discriminating measurement.
Real paired-show internal RAM, radio timing and acoustic synchronization remain
unmeasured until Jeff is available.


## Long paired playback: sustained producer throughput and role silence

Problem class: a bounded FIFO absorbs jitter but cannot compensate when average
production is below the44.1kHz consumer rate. After the first actual paired test,
Anthony advanced roughly35.6kframes/s during one10s window; short local files had
not exposed the sustained deficit. A20ms block plus unconditional10ms sleep leaves
only~10ms for I/O, role/gain/RMS and scheduling. Jeff was near the required rate,
but owner heard no role dialogue despite its startup/Polo sound.

[Espressif5.5.4 speed guidance](https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32/api-guides/performance/speed.html)
recommends direct POSIX I/O or larger stdio buffers for FatFS throughput; small
Newlib buffers and PSRAM DMA bounce are plausible I/O costs. Adaptive refill
blocking is another standard bounded-producer approach, but deleting all sleeps
would repeat the previously demonstrated idle starvation. Choose no performance
change yet: first measure fread vs processing vs refill gap, and count raw and
post-role-mask nonzero samples on the actual cards. Bigger queues alone would
only delay a sustained deficit. Script intervals must be inspected from Jeff's
actual card; the checked-in assets do not establish its data.


### First throughput measurements and controlled changes

Actual default-buffer favourite-movie test:2446blocks, Anthony read15.828s and
process14.892s; Jeff read18.730s and process13.044s. Both generated role-selected
nonzero PCM. Jeff's actual card intervals match the checked-in B schedule, so
missing audible output is not established as a role-parser failure. A subsequent
Jeff-only synthetic tone was accepted/drained but inaudible to Cam.

An8192-byte PSRAM stdio buffer reduced Anthony read time to13.157s; processing
13.505s. Cam heard clean dialogue, but39 underruns remained by EOF. This is an
improvement, not zero-underrun acceptance. Next controlled change permits at most
two successful refill publications before a mandatory>=1tick block. Non-producing
loops always block. Host tests assert the bound and that a two-block burst occurs;
this preserves the lesson from the earlier unbounded zero-tick polling failure.

### Distinguishing sink silence from PCM production

Pinned [IDF A2DP source](https://github.com/espressif/esp-idf/blob/v5.5.4/components/bt/host/bluedroid/btc/profile/std/a2dp/btc_a2dp_source.c)
can encode callback PCM and subsequently discard queued/flush-marked output.
Callbacks plus frame counts therefore do not prove sink delivery. The late
`is_cong_cback_context` messages are an explicit recursion guard in pinned
[L2CAP source](https://github.com/espressif/esp-idf/blob/v5.5.4/components/bt/host/bluedroid/stack/l2cap/l2c_link.c),
not a demonstrated cause of the earlier silence. The historical
[Espressif outage issue7923](https://github.com/espressif/esp-idf/issues/7923)
was fixed for an older controller role-switch error; do not apply that fix as
though absent in5.5.4. Missing AVRCP produces unknownPSM0x17 rejections, but audible
startup already shows this alone does not prevent rendering. Leave AVRCP and
modem-sleep policy unchanged until an experiment implicates them.

Selected discriminator: hold Anthony in verified ROM bootloader, boot Jeff fresh,
then compare a fixed tone with no BLE link against the same tone after restoring
Anthony. This changes the peer link while retaining the same Jeff image/card and
speaker. If silence persists alone, next distinguish transport state using bounded
media suspend/start or complete A2DP reconnect; no host reset or NVS erase.


Resolution of the silent-sink branch: Cam heard the tone and Polo after raising
speaker volume and reported previously bottoming it out. Adopt the physical
volume explanation supported by that intervention; the congestion messages did
not establish a firmware fault. Remove the unflashed speculative media restart
API. Retain the isolated tests and source caveats for future diagnosis, without
claiming BLE interference, missing AVRCP or a controller regression here.
