# Restore native idle breathing — 2026-10-15

Cam requested restoring the original idle animation, both for appearance and
because its extra load had helped some battery packs avoid low-draw shutdown.
The firmware change restores the motion pattern; battery retention is not yet
measured on the present supply.

Legacy source: TwoSkulls.ino uses a 7,000 ms start-to-start gate, moves from closed
to30° in2,000ms, holds100ms, and closes in2,000ms. The old implementation blocks
for the entire movement. Retain its angles/durations in the existing native
20ms control tick, without blocking radio/audio/Home work or adding a task.

Speech and active audio take priority over idle breathing. The integration uses
an explicit startup-playing flag (including preroll) and the diagnostic tone's
remaining frames, qualified by media readiness. Merely waiting for the speaker
must not prevent breathing. Boot/manual jaw checks also take priority. An
interrupted breath is canceled, and a future due cycle starts fresh from closed
rather than resuming a stale phase. Long delays produce one new cycle, without
a burst of catch-up motions. Eyes retain their existing dim idle state.

The first breathing deadline is seven seconds after the first control tick,
rather than the old sketch's zero-initialized millis reference; subsequent
cycles preserve the seven-second start-to-start cadence when uninterrupted.

Software/build/upload/live results will be recorded below after verification.

Host tests of the actual production driver passed with ASan/UBSan and warnings
as errors. They exercise pulse limits, exact waveform/cadence, dim idle eyes,
interruption by speech/silent audio/manual checks, delayed ticks without catch-up
and the20ms update gate. ESP-IDF build succeeded; source/binary/ELF hashes are
recorded in ignored `build/validation/2026-10-15-breathing-build-hashes.json`.

Expected USB prototype MAC `a0:a3:b3:80:16:38` reverified before app-only upload
at0x30000. NVS, partition table and card files are preserved. Physical movement
and battery behavior are distinct from command-level test acceptance.

## Live result and owner acceptance

App-only flash completed with esptool data hash verification. Binary2,078,656
bytes; static DRAM99,028; IRAM130,079 with993 remaining. SHA manifest rechecked
after upload: matches all source/artifacts. Persistent capture is ignored
`build/validation/2026-10-15-breathing-runtime.log`.

First boot ran the gentle check and logged idle breath starts at11.201,18.231
and25.261 seconds, approximately7.03 seconds apart at this task cadence. These
cycles occurred while the speaker connection was unavailable, demonstrating
that waiting for A2DP does not inhibit idle movement. No actuator error, panic
or brownout was observed in the bounded capture. Startup/Wi-Fi showed a transient
esp_wifi_connect error and A2DP attempts failed in this window; no new combined
streaming/Home acceptance is claimed from the idle-only observation.

Cam confirmed **breathing looks right**: slow open, brief pause, close, repeating
about every seven seconds. This closes the requested visual breathing check.
Battery-pack retention is still unmeasured. Full skit/peer/resource and acoustic
alignment gates remain separate.
