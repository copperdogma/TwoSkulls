# Names final interval correction

The checked-in `Skit - names.wav` contains 2,807,098 stereo PCM frames at
44,100 Hz: 63.653015873 seconds. Its last script interval previously read
`A,62250,4000`, ending at 66.250 seconds, beyond the audio EOF. The runtime
catalog clipped that interval, while the strict asset preflight correctly
reported an error.

Changed only the final duration to 1,403 ms (`A,62250,1403`). Both validators
convert millisecond boundaries with ceiling division. The resulting end is
`ceil(63653 * 44100 / 1000) = 2807098`, exactly the audio's frame count.
The interval still starts at frame 2,745,225 and covers every available final
frame. All earlier timings and the WAV bytes remain unchanged. This preserves
the runtime's previous clipped playback behavior without relying on clipping.

Validation: `python3 scripts/validate_assets.py sd_card_files/audio` accepts
all 15 WAV files and 11 scripts without errors or script warnings. The actual
catalog host test passes with AddressSanitizer and UndefinedBehaviorSanitizer;
the real-fixture assertion now requires zero clipped assets and verifies this
exact EOF. Synthetic overrun coverage remains in the test.

This change is in the repository assets only. Neither physical SD card has
been updated. Their old interval remains safe under the existing runtime EOF
clipping, but their authored asset preflight warning remains until the script
is copied onto them. No speaker listening result is inferred from this edit.
