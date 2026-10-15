# Anthony native startup announcement — 2026-10-15

Cam requested restoring the initialized WAV announcement when the skull wakes.
This is an explicitly authorized, bounded extension of the diagnostic probe,
independent of the full skit/Jeff port held behind Gate 1.

## Implementation

- After valid onboard SD/config, preload `/audio/Initialized - Primary.wav` into
  explicitly allocated PSRAM before radio initialization. The loader also knows
  the secondary filename, but the surrounding native app remains primary-only.
- Parse RIFF chunks and require PCM 44.1 kHz/stereo/16-bit; reject invalid bounds,
  duplicate chunks and incomplete frames. Cap this diagnostic clip at 1 MiB PCM.
  Missing/invalid audio logs a failure without disabling Home/tone diagnostics.
- Apply card volume once in task context. Retain the immutable PCM buffer for
  this boot. No file changes, callback allocation, SD I/O, analysis or blocking.
- When A2DP media is ready, deliver 300 ms silence followed by the announcement
  exactly once per boot. Normal completion is terminal, and speaker reconnection
  does not replay. A link loss observed by the approximately 100 ms diagnostic
  poll cancels remaining PCM. A shorter transition could evade that poll.
- Home/serial tone requests remain diagnostic tones and are ignored while
  startup is pending/playing. A disconnected speaker can still be retried.
- Existing speaker connection/cache/backoff, Matter endpoint/fabric and peer
  protocol are unchanged. No jaw/eye drivers or full skit pipeline added.

## Software verification

Portable C++ parser tests passed with address/undefined sanitizers and compiler
warnings as errors, including malformed/truncated/overflowing chunks, odd-byte
padding, unknown chunks, data-before-format, trailing bytes and real initialized
files. Primary PCM starts at byte 106, length 669,852 (3.797347 seconds).
Secondary starts at byte 104, length 549,428; secondary app support is not claimed.
The separate seven Python preflight tests also pass.

Final ESP-IDF build succeeded. Binary is 2,067,776 bytes; static DRAM 98,788 bytes,
IRAM 129,499 bytes with 1,573 remaining (unchanged IRAM budget). Source/artifact
manifest: ignored `build/validation/2026-10-15-startup-build-hashes.json`.

## Live evidence

Expected USB MAC `a0:a3:b3:80:16:38` reverified before upload. Application-only
write at `0x30000` preserves NVS/partition table and existing pairing/fabric data.
Upload/runtime/audible acceptance results are recorded below when observed.

App-only upload completed and esptool verified its data hash. Persistent capture:
`build/validation/2026-10-15-startup-runtime.log` (ignored, contains private Matter
traffic). On the first captured boot:

- Config primary / JBL Flip 5 / gain 100%; 669,852 PCM bytes loaded at 2.827 s.
- A2DP connected at 8.237 s; startup began at 8.527 s and reported all PCM
  delivered at 12.657 s. No manual tone or Home command initiated it.
- A secure Matter CASE session resumed at 6.427 s during the same boot.
- At 14.157 s, 1,944 PCM callbacks, zero radio event drops/fault, 45,163 bytes
  internal free, minimum 31,547, largest block 38,912; callback elapsed maximum
  3,061 microseconds, including scheduling time. PSRAM free about 3.44 MB.
  This boot's internal minimum is below the proposed 32 KiB gate floor, so the
  broader resource acceptance remains open. No claim of full-load acceptance.
- Source/artifact SHA manifest rechecked after flashing: all match.

Cam confirmed the initialization announcement was **complete and clear**,
including the beginning and end. This passes the bounded startup announcement
audible acceptance check on Anthony; it does not close full-show/peer/resource
acceptance.
The serial capture remains open so normal reconnect diagnostics do not incur an
extra USB-monitor reset. No NVS erasure or card edits were performed.


## Owner cold-start correction

Cam heard the initial “A” clipped after a later power-up. Increased initial PCM
silence from300ms to800ms (35280 frames), adding the requested half-second after
A2DP media readiness. This is stream preroll, not a delay before connecting; the
speaker must already be streaming to settle/unmute. Both roles use the same
constant and binary. Owner audible cold-start confirmation remains required.

Cam confirmed the updated announcement was complete, including the initial “A”,
on the first observed800ms-preroll boot. Repeated cold-start/yard soak is separate.
