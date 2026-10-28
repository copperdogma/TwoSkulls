# Story 002 — Continuous Bluetooth Speaker Reconnect

Status: In Review; TwoSkulls hardware acceptance pending.

## Requirement

Cam requested the same indefinite Bluetooth-speaker recovery installed and
verified on Death. Anthony and Jeff must keep trying their configured speaker
when it is absent, rejects a connection, or becomes available late.

## Work log — 2026-10-27

- Located active native implementation in onboard-sd-migration worktree; main
  checkout still holds older Arduino firmware. Preserved unrelated perfboard
  edit and untracked battery/connector/prototyping research.
- Verified native skull_radio.cpp is byte-identical to Death's pre-fix source.
  It stops after3 saved-address attempts/one inquiry, permanently disables retry
  after slow connection/ACL teardown, and stops media readiness after3 tries.
- Reused Death's tested radio patch exactly. Episodes repeat indefinitely with
  five-second pacing. Pending A2DP opens and existing ACL teardown are still
  completion-gated; late completion automatically resumes recovery. No shared
  Matter/BT host reset or new pairing. Media readiness repeats indefinitely,
  initially2second cadence and5seconds after the third attempt.
- Copied production-source fake-SDK/virtual-clock regression harness and wired
  it into scripts/run_host_tests.py. Extended every recovery scenario to both
  primary Anthony and secondary Jeff, and added established-session loss.
  Covers20 absent-speaker episodes then recovery, uncached repeated discovery,
  immediate API failures/pacing,450second pending open and300second ACL wait,
  callback ordering, >20 media retries and successful connection/media readiness.
- Updated current firmware guidance; dated historical qualification evidence
  remains intact. Existing Story001 links this follow-up to supersede its old
  explicit-retry limitation.
- Running all host suites with ASan/UBSan and native ESP32 build. No board flash
  requested/performed; connected USB device in this chat is Death. Paired-device
  delayed-speaker, HomeKit and acoustic confirmation remain hardware gates.
- No commit/push.

## Acceptance

- [x] Automated recovery cases pass for both roles; native build passes.
- [ ] Both physical skulls automatically reconnect after the speaker stays off
  beyond a complete failed episode; no console retry or pairing reset.
- [ ] Human confirms actual speech and normal paired/Home behavior.

Research: [continuous reconnect](../research/2026-10-27-continuous-speaker-reconnect.md).

- Final validation: all host suites PASS with ASan/UBSan, including both-role
  production radio scenarios; native ESP-IDF5.5.4 build PASS. Binary SHA256
  `12fa4ea5b0c95dd5ac43a4b3a308e294fbd9312c24b614287d623246c5927569`.
  Updated radio module is byte-identical to tested Death implementation.
  Private ignored logs: firmware/build/story002-{host-tests,build}.log.
  Source ready; no physical TwoSkulls flash or acceptance claimed.
