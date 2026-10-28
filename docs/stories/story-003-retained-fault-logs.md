# Story 003 — Retained fault reports and rolling SD logs

Status: In Review — implementation, host tests, native build and independent review complete; device acceptance pending.

## Intent and acceptance

Cam requested the SD error logging that works in Death be added to TwoSkulls.
Port the existing implementation to the native shared Anthony/Jeff firmware,
with RAM recent-event capture, compact NVS fallback, eight bounded SD report
slots, idle-only persistence and 60-second retry cadence. Preserve current
playback, peer protocol, provisioning, card assets and unrelated work.

Human acceptance requires a report from each role and normal paired playback
with no logger writes while show/audio/startup/tone or the producer FILE is active.
Do not mark complete until human verification. Cam subsequently authorized check-in and push; no device upload requested.

## Work log

### 2026-10-27 — Port and host qualification

- Found the current native firmware in the existing `codex/onboard-sd-migration`
  worktree. The main checkout still contains the older Arduino implementation.
  Worked in the native worktree; preserved its unrelated perfboard/research edits.
- Reused Death Story 018 production capture/archive/NVS modules as `skull_fault_*`.
  Adapted retained tags to boot `skull`, `anthony`, `jeff`, `skull_show`, shared
  radio/storage/startup/actuator tags and selected SD driver tags. Show cancellation
  warnings trigger retention; idle trigger refusals do not. Config/provisioning
  and logger's own records are excluded as in the source logger.
- Dedicated NVS namespace `skulls_fault`; versioned roughly 1 KiB compact report,
  existing free-entry reserve, no partition changes or NVS erase. SD writes only
  `/sdcard/diagnostics/fault-0.log` through `fault-7.log` plus `pending.tmp`.
  Recent history is 32 x 192-byte lines, report limit 7168 bytes, one pending
  snapshot with later faults coalesced. Export is checked before acknowledgement.
- Initialized after NVS and before card mounting. Existing boot retry loop polls
  flash-only retention. Runtime owner polls only when show idle, producer
  quiescent (including final FILE close), startup idle and no tone. No logger
  I/O occurs in log hooks or A2DP callbacks; no new task or remount policy.
- Added `fault-log`, idle-only `fault-test` and logger status to shared console.
- Ported production-source core/archive and fake-SDK runtime tests; extended tag
  checks for both roles, cancellation warnings and logger recursion exclusion.
  `python3 scripts/run_host_tests.py` PASS: all 11 C++ sanitizer suites and
  8 Python cases, including failed SD export, cadence, compact recovery across
  reboot, acknowledgement retries, NVS reserve, concurrent capture and archive
  rotation/asset preservation. Existing 44 show-controller cases also PASS.
- Preserved the pre-change local `.bin`, `.elf` and flash arguments under ignored
  `build/validation/pre-fault-logger-20261027/` with SHA256 verification before
  rebuilding. Existing binary SHA256:
  `12fa4ea5b0c95dd5ac43a4b3a308e294fbd9312c24b614287d623246c5927569`.
- Next: native build, independent review, final diff checks. Hardware export,
  reboot recovery and paired playback with this build remain untested.

- Independent lower-cost review found two selected warnings that should also
  freeze a report: startup link-loss interruption and Jeff setup timeout. Added
  exact warning matches and regression cases; ordinary speaker-unavailable
  warnings and refused triggers remain non-faults.

### Final machine qualification

- ESP-IDF 5.5.4 ESP32 native build PASS. Shared Anthony/Jeff binary fits the
  existing app partition with 47% free; no partition/config changes. Existing
  upstream secure-cert Kconfig warnings remain.
- Re-ran all host suites after the review correction: PASS. Final whitespace
  check PASS. Private outputs in ignored `build/validation/`:
  `fault-logger-host-tests-20261027.log` and
  `fault-logger-native-build-20261027.log`.
- No device flash. Story remains In Review for actual two-role device
  export/reboot/paired-playback validation and human acceptance.

### Check-in authorization

- Cam requested committing and pushing this port to the TwoSkulls repository.
  Scope is Story 003 implementation, tests and documentation only; unrelated
  perfboard changes and earlier research notes remain outside this commit.

### 2026-10-27 — Discoverable debugging workflow

- Cam requested a general troubleshooting process and an obvious explanation of
  automatic SD error logs. Existing firmware details were present but buried.
- Added root README workflow, agent pointer and native README backlink. Covers
  board-vs-computer paths, eight-slot rotation, staging files, no-reset serial
  capture, pending/retry status, compact fallback limits, evidence preservation,
  bounded experiments and separate host/device/human verification. Commands and
  paths checked against the actual console, logger and serial helper sources.
- Documentation only; no device actions or firmware behavior changes.
