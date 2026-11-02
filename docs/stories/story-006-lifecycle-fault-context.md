# Story 006 — Startup and wireless context in fault reports

Status: Implemented; Jeff-only no-speaker SD export/readback/reboot check passed. Anthony and audio qualification remain pending.
Date: November 1, 2026.

## Scope

Keep bounded startup milestones, SD initialization/config timings and results,
speaker connect/stream timings, losses and reasons, peer transitions and wireless
signal samples in RAM. Include this context in existing selected-error SD reports.
No remote access, core dumps, new tasks, partition changes or ordinary telemetry
writes. Preserve current show behavior, speaker settings and Matter fabrics.

See [research and design](../research/2026-11-01-lifecycle-context.md) and the
[native logging guide](../../firmware/README.md#retained-fault-diagnostics).

## Validation

- All portable host suites passed with ASan/UBSan, including 44 show scenarios,
  radio cadence/async-result cases for both roles, startup/recovery retention
  through churn, maximum context bounds and fault-tail preservation, report
  immutability, concurrent capture, archive rotation and NVS recovery.
- Native ESP32 build passed using ESP-IDF 5.5.4. An initial target compiler
  `size_t`/`uint32_t` template mismatch in report append was corrected with an
  explicit `std::min<size_t>`; host compilation alone had not exposed it.
- Initial machine-validated binary: 2,120,864 bytes, growth 6,816 bytes from the retained startup-final
  image; 1,876,832 bytes remain in the existing factory app partition. No new
  partition, dependency or task. Build/size logs retain existing SDK warnings.
- Native linker IRAM: 130,391 / 131,072 bytes (681 bytes free); static DRAM:
  100,948 bytes. IRAM margin remains tight. Logger Core is 31,792 bytes in the
  host layout, roughly 18 KiB larger than before, allocated in existing PSRAM
  storage rather than a new internal static buffer. Runtime heap/stack/timing
  acceptance is not established by linker size.
- Independent review found no blocking defect. Unusually late Bluetooth RSSI
  replies can be misattributed across reconnects/new requests because the SDK
  supplies no request token; this changes diagnostic attribution only.
- Private logs and source/image hashes:
  `build/validation/lifecycle-context-20261101/`.

The initial build was not uploaded. The subsequent authorized Jeff device test
below updates that boundary. Prior November 1 startup-final binary remains
retained under ignored `build/validation/jbl-go2-20261101/`.

## Jeff-only device check — November 1, 2026

Cam attached Jeff and authorized a synthetic error after about 60 seconds, with
no speaker available and Anthony off. Collected preflight status/fault-log/speaker
setting over USB; secondary role, SD mounted, idle, speaker/peer absent, pending=0,
JBL Flip 6 retained. Verified actual partition table against the native build and
the installed application against the retained startup-final rollback image via
esptool digest checks. No full-device erase was needed.

### Findings and bounded corrections

- Waiting startup PCM was classified busy indefinitely when the speaker was
  absent, blocking logger export and the existing synthetic fault command.
  Permit SD I/O while waiting, while retaining show/audio quiescence, actual
  startup playback and tone gates. Only the synchronous control owner can start
  startup playback, after I/O returns; PCM is already preloaded in PSRAM.
- Added idle-only `fault-saved` to inspect fixed SD slots without removing the
  card. rb-only, bounded 64-byte chunks, slot+offset+hex and checked END framing;
  portable sanitizer tests cover roundtrip/bounds/failures/preservation.
- Corrected secondary lifecycle peer observation to `connected()` rather than
  local service `ready()`. The observed state correctly reports no Anthony link,
  while the local GATT service is available. No protocol/control behavior changed.
- Independent review found no blocker; full host suites and updated native build
  passed. Uploaded only the 2,121,792-byte application at 0x30000; esptool hash
  verification passed. Bootloader, partition table, NVS layout and SD config/assets
  were not rewritten. The fault logger subsequently used its normal NVS namespace.

### Observed result

- First mount succeeded in **111 ms**; mount+config wrapper **141 ms**. Control/radio
  startup reached control_ready at SDK uptime 3409 ms, 3377 ms after app entry.
  This is not speaker/show readiness or a power-rail boot measurement.
- Before injection, read all eight report slots: slot 0 contained an existing
  413-byte report; other slots were absent. Preserved its exact bytes/hash.
- Host sent the existing `fault-test` at 60.03 seconds after reset. Serial error
  timestamp 60025 ms; report's SDK uptime 58198 ms. This logs a labelled synthetic
  error, not a crash, SD failure or playback failure. No timed injection was
  compiled into firmware, so there is nothing to disable/remove after the test.
- Logger saved and acknowledged slot 1 by serial timestamp 60245 ms, with
  SD_errno=0, NVS=ESP_OK, pending=0, one persistence attempt each.
- Read the actual SD file back: **5,689 bytes**, fault report v2, boot/SD/config/radio
  milestones, no-peer observation, Wi-Fi disabled_role, speaker=0, peer=0 and
  FAULT_LOG_TEST marker all verified. No false speaker/operational-ready milestone.
- Rebooted and read both files again: byte-identical. New report SHA256
  `65179f9c17092aaa1b3b75b8fb0973aed495d60ca5d506ed50024980ecff8095`;
  old report SHA256
  `384ceb1748afa6731ecda421a16b081bff442327628b582731cf3cb549b48f3c`
  remained unchanged. Acknowledged report was not spuriously re-exported.
- Final status after reboot: mounted SD, idle show, absent speaker/peer, zero PCM
  callbacks/underruns, radio fault=0 and pending=0. JBL Flip 6 setting preserved.
  Jeff retains this normal firmware. Anthony was not accessed or updated.

Private evidence, installed/rollback hashes, exact decoded files and host schedule
are under `build/validation/lifecycle-context-20261101/`; installed image retained
as `jeff-installed.bin`, hardware manifest `jeff-device-manifest.json`.

### Qualification limits

This demonstrates normal SD export/readback and persistence across reset with
startup waiting silently. It does not test a real SD failure, speaker connection,
Bluetooth signal values, Wi-Fi RSSI on Anthony, full startup speech or paired
playback. No destructive fault was injected. Serial saved-file retrieval blocks
the control owner briefly (observed roughly 2 s actuator deferral while idle, no
stuck outputs); it is explicitly idle-only. Active audio/timing/resource checks
remain future device qualification.
