# Offline asset preflight — 2026-10-15

Prepared in the onboard-SD worktree while waiting for the replacement board.
No firmware changed or flashed, and no attached device/card was accessed. The
input is the checked-in `sd_card_files/audio` directory, not a fresh SD snapshot.

## Result

- 15 WAVs are valid PCM 44.1 kHz, stereo, 16-bit; frame alignment and byte rates
  agree. All 11 skit WAVs have matching scripts. Independent read-only review
  counted 120 script lines and 66,799,450 total WAV file bytes.
- WAV PCM offsets vary from 44 to 110 bytes (44, 78, 98, 100, 102, 104, 106,
  108, 110). The legacy `audio_player.cpp` uses `seek(128)`, which skips actual
  samples and can start halfway through a stereo frame. Keep the approved native
  RIFF chunk-walking requirement; do not infer a single replacement offset.
- **Validation failure:** `Skit - names.txt`, physical line 15, contains
  `A,62250,4000`. Its endpoint is 66,250 ms; the WAV is 63,653.015873 ms long.
  The final interval exceeds audio by approximately 2,597 ms. Authored files
  remain unchanged; listening/reconciliation is required before choosing an edit.
- Milkshakes' 1,480 ms internal silence is valid and remains part of the schedule.
  Touching interval boundaries and consecutive same-role intervals are allowed.

## Implementation and evidence

`scripts/validate_assets.py` reads only RIFF headers/chunk metadata, skipping PCM
payloads. It checks declared file/chunk bounds and odd-byte padding, unique fmt
and data chunks, PCM format, complete frames, paired scripts, field types,
positive durations, 32-bit timing bounds, ordering/overlaps and EOF limits.
Unknown chunks and fmt/data order are supported. Optional finite fourth-column
jaw values produce a warning because legacy playback ignores that metadata.
Intervals convert to `[ceil(start_ms*44100/1000), ceil(end_ms*44100/1000))`.
Duplicate fmt/data chunks and bytes outside the declared RIFF size are deliberately
rejected by this narrow playback contract, even if another player tolerates them.

Seven host tests pass, including adversarial RIFF lengths/truncation, unknown
odd-sized chunks, chunk order, unsupported formats, duplicate/missing chunks,
incomplete frames, invalid times, overlap, exact end boundaries and file pairing.
The real-asset command returns **1**, as expected for the names script defect:

```sh
python3 -m unittest discover -s scripts/tests -v
python3 scripts/validate_assets.py sd_card_files/audio
```

Detailed ignored JSON: `build/validation/2026-10-15-asset-preflight.json`.
This is a host-side preflight/reference, not the production native parser, actual
card acceptance, or a passed Gate 1/Gate 2. Full streaming/actuator integration
remains behind the real-secondary/resource gate. Next: verify the new board,
restore Jeff's original role safely, and test the live legacy BLE handshake.

## Source and decision

[Microsoft's RIFF specification](https://learn.microsoft.com/en-us/windows/win32/xaudio2/resource-interchange-file-format--riff-)
defines chunk sizes, arbitrary chunk order and WORD padding. Adopt bounded chunk
walking rather than assuming a fixed WAV header. Synthetic cases and the actual
repository WAVs verify applicability locally. The later firmware parser still
needs its own portable tests and device streaming evidence.
