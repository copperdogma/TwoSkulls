# Story 008 — Missing-speaker eye warning

Date: November 2, 2026.
Status: Implemented; host checks and Jeff no-speaker boot passed. Physical speaker-loss/reconnect and Anthony checks remain pending.

## Behavior

Both eyes alternate one second bright (255/255), one second normal dim (100/255)
while the configured Bluetooth speaker is disconnected. A never-connected boot
gets 30 seconds from monotonic boot time to connect. Once an A2DP connection has
been observed, disconnection enables the warning on the next control tick,
normally within 20 ms. Reconnection clears it; a subsequent loss restarts bright.
This uses actual connection state, not A2DP media/stream readiness.

Counted diagnostic patterns take priority over this warning, including their
dark phases and gaps. Existing SD mount failure (three repeating flashes), config
failure (five), and boot role (two Jeff/four Anthony) retain their meaning.
Warning overrides ordinary speech/idle eye brightness only. Jaw movement and
speaker reconnect attempts continue. No new task, callback writes, blocking delay
or allocation is introduced; the existing control owner supplies radio state.
Elapsed time skips delayed phases. Only warning transitions are logged at INFO;
a missing speaker does not manufacture a fault report.

## Verification

Deterministic tests exercise the production LED driver and a portable timing
policy: deadline boundaries, late first poll, early loss, reconnection, one-second
phases, diagnostic priority and independent speech/jaw behavior. Build/linker
resources and an application-only Jeff upload are required. Preserve current
status/fault-log/saved SD evidence and verify the installed rollback/layout first.

Speaker and Anthony remain absent. A no-speaker boot can validate the 30-second
warning activation on Jeff; actual disconnection/reconnection and visual hardware
brightness still need a powered speaker/human observation. Host duty assertions
prove commanded values, not physical LED light output.

Private evidence: `build/validation/story-008-speaker-eyes/`.

## Cam's physical observation

After the update, Cam reported starting with the speaker off and seeing Jeff's
eyes flash. Turning the speaker on connected it, stopped the flashing and played
"Jeff initialized." This confirms visible warning output and clearing on a real
initial connection. A later drop from an already-connected speaker still needs
separate physical observation; the deterministic loss/reconnect tests cover policy.

## Results

- All portable host suites passed with ASan/UBSan and warnings as errors. Tests
  cover exact 30-second deadline, late first poll, connection loss before the
  deadline, reconnection, repeated loss, one-second halves, skipped phases,
  counted-pattern dark/gap priority, and continuing jaw speech motion.
- Native ESP-IDF build passed. Image 2,315,472 bytes, SHA256
  `67b22b359ab28532ae8cbfdc3a1301ea4301640669766969f205fc59442a9e76`.
  Static DRAM 102,500 bytes (+24); IRAM unchanged at 130,987 / 131,072 (85 free).
- Before upload, a serial capture retained boot/current fault status (no pending
  report); both SD reports had already been preserved by the preceding HA/SD
  validation. Verified the actual prior image and unchanged partition layout by
  digest. Application-only upload at 0x30000 passed flash hash verification.
  Card, NVS, partitions and bootloader were not rewritten.
- Jeff booted without a speaker and logged `Speaker eye warning active` at logger
  timestamp 31,923 ms. Relative to the captured initial logger/monotonic offset,
  this is approximately 30,025 ms SDK boot age. Exact boundary/phase timing is
  demonstrated by virtual-time production tests; this log proves device activation,
  not physical brightness measurement.
- After activation: SD mounted, show idle, zero PCM callbacks/underruns, radio
  fault=0, no pending fault. Both SD reports read back byte-identical to the
  preserved 413-byte/5,689-byte originals. No synthetic error was injected.
- No speaker was available: real connection-loss/reconnection, visible LED light
  levels and Anthony/shared-audio behavior remain unobserved. The state source is
  polled about every 20 ms; an entire connect/disconnect episode between polls
  could be missed. No claim of callback-time response is made.

Evidence: `host-tests.log`, `native-build.log`, `native-size.log`,
`rollback-verify.log`, `jeff-upload.log`, `jeff-before.log`, `jeff-after.log`,
`candidate-manifest.json` and `validation.json` in the private evidence directory.
