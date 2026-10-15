# Catalogue, recovery and resource qualification — 2026-10-15

The current shared image passed all 11 paired skits on both boards: exact source
frame counts, zero underruns, zero task-watchdog events, and no unexpected reset.
Cam confirmed clean dialogue and normal timing, including a Home interaction
while Names was playing. This is bounded bench acceptance; resource margin,
Jeff's disconnected actuators and outdoor soak remain separate open gates.

## Current image and checks

- Identical app-only image on Anthony `a0:a3:b3:80:16:38` and Jeff
  `f4:2d:c9:78:ad:78`; both flash digests verified. NVS/fabrics preserved.
- Image: 2102016 bytes, SHA256
  `0cf0961ed9fdaa414b867ca3258a358eff1464f29c5ef5f0a2a5f4a1879644d9`.
  ELF SHA256
  `003f1adabab1a027002db841756f4685d13b09c8dbb8d0253d8f71f4551d005a`.
- Native ESP-IDF 5.5.4 build and all host suites pass, including sanitizer-enabled
  production-source suites and 44 deterministic show-control scenarios.
- Strict repository asset preflight: 15 WAVs, 11 scripts, no errors/warnings.
- Ignored evidence: `build/validation/2026-10-15-catalogue-release/summary.json`,
  both raw serial logs and matching ELF. Raw captures contain private Matter
  traffic and remain outside Git. Summary reports `complete=true`, `passed=true`,
  `busy_reboot_refused=true`, one boot and zero watchdogs/panics per board
  during the automated catalogue. A deliberate idle reboot was tested afterward.
- All skits: Starbucks, favourite movie, fear exam, imitations, ju jubes,
  milkshakes, names, plastic monsters, pumpkin flowers, skin cream and spears.
  Earlier targeted encoder-image testing also passed movie/ju jubes/pumpkin twice
  each, with Cam confirming both stayed clean through the last replay.

## Behaviors demonstrated

Anthony keeps the original approximately five-second Marco search call. When
Jeff becomes ready, Anthony finishes an in-flight Marco or plays one, waits a
nonblocking 300 ms output-tail guard, then explicitly requests Polo through the
existing raw filename/ACK/readback protocol. Connecting alone is silent on Jeff.
Cam heard Marco then Polo and confirmed Anthony's eyes/jaw still operated.
The tail guard is practical allowance, not measured JBL acoustic latency.

Cam found the old error flashes too fast. Diagnostics now use 200 ms on/200 ms
off, with an additional two-second dark gap between repeated groups. Both boot
loops use the driver's duration helper, preventing retry or radio setup from
truncating a pattern. Codes remain 4 primary, 2 secondary, 3 SD, 5 config.
Cam confirmed the revised three-flash pattern is clear.

Anthony's card was removed while idle, then rebooted. It stayed in the repeating
three-flash mount-failure stage without formatting or starting radios. On the
same boot, reinsertion restored the primary config at about 34.7 s, then the role
flashes, startup announcement and Marco/Polo. No further reset or card write.

During Names, Cam turned Home On. It returned Off, both skulls stayed clean and
in time, and firmware logged the busy request as ignored. No queued skit replay.
During the first Starbucks clip, a console reboot request to Jeff was explicitly
refused and the clip finished cleanly. New producer quiescence also covers the
final-close window after PCM completion; its host test holds fclose to prove it.

Jeff speaker shutdown was detected and the existing ACL teardown/backoff gates
were respected. A quick off/about-five-seconds/on recovered automatically on
attempt 2. A longer off interval exhausted the bounded three-attempt/discovery
episode before the speaker returned; explicit idle retry restored media without
stale playback. This does not establish indefinite late-return reconnect.

## Failures that informed the changes

| Capture / image prefix | Result | Decision |
| --- | --- | --- |
| catalogue-qualification-1 / `7f3e4e61` | Starbucks passed; primary cache-error panic during movie, then interrupt-watchdog reset loop in LEDC duty-start wait | Once-only peripheral reset and pending-duty guard |
| catalogue-qualification-2 / `4ef8954e` | All source frames; primary 0 underruns, secondary 1370 underruns and 70 task-watchdog events | Investigate encoding/scheduling; no broad pass |
| catalogue-qualification-3-240mhz / `05ef595d` | Primary clean; secondary 488 pumpkin underruns and 11 watchdog events | Clock improvement insufficient; target sampled SBC hotspot |
| sbc-o2-targeted / `e58d670e` | Three troublesome skits twice each, both boards clean | Proceed to full catalogue and recovery checks |
| catalogue-release / `0cf0961e` | All 11 paired skits and busy-reboot guard pass; Home and SD recovery observed | Bounded bench acceptance |

First panic decoded against retained ELF `ce081a695`: LEDC duty-start → update
→ actuator write/tick → diagnostic task. Later setup watchdogs hit the same
unbounded wait with stale peripheral state. The initial cache-error trigger is
not fully established. The adapted [upstream LEDC fix](../research/2026-10-15-ledc-duty-start.md)
resets once before setup and defers pending updates without blocking or falsely
advancing applied-duty caches. No watchdog suppression or blanket IRAM expansion.
Final run has no continuously observed >=100 ms stuck-channel warning.

Repeated secondary task-watchdog stacks sampled SBC analysis, IDCT, packing and
bit allocation in BTC_TASK on CPU0 while CPU1 was idle. CPU 240 MHz helped but
was insufficient. Exactly eight SDK encoder C files now compile with `-O2` after
`-Os`; all 308 other Bluetooth files retain `-Os`. No SDK source edits, queue
resizing, affinity/priority changes, flash/PSRAM clock changes or removed yields.
See [throughput research](../research/2026-10-15-a2dp-throughput.md). Negotiated SBC
masks match between speakers; maximum bitpool is 40 for Anthony and 38 for Jeff.
These values do not establish the full cause of their different prior behavior.

During an intentionally forced mid-Names software reboot, Anthony finished all
2807098 local frames with zero underruns after losing Jeff. Jeff then failed SD
reinitialization before radio startup. Power removal restored the card; an idle
software reboot subsequently mounted/initialized correctly. Card status on one
failed retry showed state 5, supporting retained transaction state rather than
an entirely nonresponsive card. [SD reset research](../research/2026-10-15-sd-soft-reset-recovery.md)
documents why another ordinary CMD0/remount is not a proven fix. Console reboot
now refuses active show/startup/tone/file work rather than interrupting it.
Uncontrolled panic/brownout/external-reset recovery during SD access is not solved.

The repository Names final interval is corrected from `A,62250,4000` to
`A,62250,1403`, ending at PCM EOF. Physical cards were not edited; existing runtime
EOF clipping yields the same effective interval. [Timing research](../research/2026-10-15-names-timing.md).
Established-speaker loss now rearms the existing bounded reconnect episode;
failed attempts do not reset its budget. [Reconnect research](../research/2026-10-15-speaker-recovery.md).
Setup-only later captures were stopped to correct blink timing and are not skit
qualification evidence.

## Resource limits

| Final catalogue measurement | Anthony | Jeff |
| --- | ---: | ---: |
| Current free internal bytes | 36563 | 103895 |
| Local-window minimum internal bytes | 29379 | 102831 |
| Largest free internal block | 34816 | 98304 |
| Maximum measured PCM callback wall time, us | 31996 | 2154 |
| Radio queue drops / fault | 0 / 0 | 0 / 0 |

The proposed 32 KiB primary low-water diagnostic margin is still unmet. Regional
minimum totals may reflect different instants; they are not proof of a
simultaneous allocation failure. No allocation failure or cumulative leak was
demonstrated in these finite runs. Callback wall times include preemption and do
not establish a hard real-time deadline. Do not equate zero PCM underruns with
measured acoustic alignment or unlimited radio/heap capacity. No speculative
stack trimming or Wi-Fi/controller-capacity reduction was applied.

Remaining: investigate primary margin if load grows, measured acoustic offset,
Jeff servo/eyes after wiring, long outdoor coexistence/soak, and recovery after
uncontrolled reset during SD access. A speaker returning after the bounded retry
episode needs explicit retry. The branch is suitable for continued hardware
prototyping with these limits recorded.
