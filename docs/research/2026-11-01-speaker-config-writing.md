# Updating the speaker setting on an installed SD card

Cam requested a reusable USB writing utility, selection of JBL GO 2 for a
demonstration, and later restoration of JBL Flip 6 while retaining the utility.
Do not revert the speaker before the demonstration is finished.

## Filesystem decision

The problem is recoverable replacement of a small configuration on FAT while
preserving unrelated settings. FAT rename behavior differs from desktop POSIX
replacement, so an unchecked overwrite or assumed atomic replacement is unsuitable.

Primary sources:

- [FatFs rename](https://elm-chan.org/fsw/ff/doc/rename.html): the destination
  must not already exist; files must be closed before renaming.
- [FatFs sync](https://elm-chan.org/fsw/ff/doc/sync.html): flushing limits loss of
  cached writes; it does not establish a power-fail atomic transaction.

Decision: idle-only speaker-name command, bounded config input, preserve other
lines and parsed settings, validate and flush a candidate, read it back, retain
an original backup and use separate rollback/staging names for replacement.
Stop on unresolved transaction files rather than overwriting evidence. Close
files before rename. The existing control task owns both the command and show
preparation, preventing a new local audio file from opening during the update.
The running radio selection remains unchanged until deliberate reboot.

The tested native layout and jaw tuning remain in place; remove the experimental
GO 2 compile-time override. Normal boot must read the speaker name from SD.
Host and physical verification results will be recorded after implementation.

## Implementation and host verification

`speaker-name` reads persisted and current-boot selection. `speaker-name <name>`
updates only the speaker-name assignments in valid configs up to 4096 bytes.
Names are 1–79 bytes; control characters, equals and outer whitespace are
rejected. BOM, line endings, comments, unknown keys, role, volume and peer MAC
are preserved. The first original is retained as `config.speaker.original`.
`config.speaker.tmp` is exclusively created, flushed and readback-checked;
closed files are renamed using `config.speaker.previous` for rollback. The
previous file is removed only after the installed config verifies. Unresolved
staging files stop subsequent writes. No automatic boot recovery is added;
if power fails between renames, preserve/recover those files manually.

All portable host suites passed with ASan/UBSan. Storage tests exercise normal
write/revert, immutable original backup, invalid names/configs, oversize inputs,
duplicate/empty assignments, BOM/CRLF preservation, unrelated settings and
stale staging or invalid backup refusal. These verify filesystem behavior on
the host; the on-card write and reboot still require hardware verification.
Private log: `build/validation/jbl-go2-20261101/storage-utility-host-tests.log`.

## Device update and initialization obstacle

Native build passed, application-only upload of 2,111,632 bytes at 0x30000
completed with esptool hash verification. BIN SHA256:
`d8e5855b1c3e5c3dacf8eed1b4f5c6716da13cfc514ca14821c7404f544044d2`;
boot ELF prefix `c54463e89`. No experimental override remains in source.

The first captured boot after uploading hit SDMMC read/initialization timeout
0x107 before config load or the new utility command. No speaker-name write had
run. Before upload the card was mounted and status/fault-log showed no pending
report; the new boot retained an initialization-failure report in internal NVS.
Capture: `build/validation/jbl-go2-20261101/utility-live.log`.

This project already records an SD card needing a full power cycle after a
reset (native README, reboot guard discussion). A bounded research check found
[Espressif’s SDMMC example](https://github.com/espressif/esp-idf/blob/master/examples/storage/sd_card/sdmmc/main/sd_card_example_main.c)
with an explicit card-power reset, and
[its SPI example notes](https://github.com/espressif/esp-idf/blob/master/examples/storage/sd_card/sdspi/README.md)
describing a reset-without-power-cycle condition. The SPI condition is not proof
of this SDMMC cause. Chosen local test: request full Jeff power removal for
about 10 seconds, leave the card installed, then retry normal initialization.
No reformat, card edits or speculative driver changes. Recovery result pending.

## Final device verification

The original power-cycle request was superseded by the user's request to debug
and harden startup first. See [the recovery investigation](2026-11-01-sd-startup-recovery.md)
for the failed CMD12 software recovery, improved boot console, bounded retry
behavior and DAT3 pull-up. Final app ELF prefix 474104357 has the utility and
startup mitigations, with no compile-time speaker override.

After Cam removed all Jeff power for about 10 seconds, SD mounted on the first
attempt, read secondary/Flip 6/60%, and loaded the audio catalog normally.
`speaker-name JBL GO 2` saved and readback-verified the installed card at uptime
19.372 s. Query confirmed SD GO 2 while current boot still selected Flip 6.
Idle `reboot` then mounted the card on its first attempt again and loaded GO 2
from config. The first cached speaker open failed; its normal second retry
connected, streamed and delivered startup PCM. No new radio workaround added.

After reboot, query reported SD GO 2/current boot GO 2, secondary role and 60%
volume retained. Explicit Polo completed 24,289 frames, zero underruns. Cam
confirmed both startup announcement and Polo were clear. Status: sd=1,
speaker=1, stream=1, dropped=0, fault=0, actuator stuck=0. The retained original
fault was successfully archived on SD and pending became 0.

Live runtime checks discarded an overlong line ending in a valid `status`
suffix and an embedded-NUL `status` prefix; subsequent query/status worked.
Speaker stayed GO 2. Private capture: `post-power-cycle.log` in the same evidence
directory. This verifies the installed-card setting and one orderly reboot;
it does not qualify arbitrary interrupted writes or every card startup fault.

Leave GO 2 selected for Cam's demonstration. The later authorized speaker
reversal is `speaker-name JBL Flip 6`, then `reboot`, once Cam says the demo is
finished. Keep the utility/startup code. No Anthony or Death changes.

## Demonstration closeout and speaker restoration

Cam confirmed the demonstration worked and requested restoration plus check-in
and push. Captured status/fault-log first: no pending report, mounted SD,
same ELF prefix 474104357. `speaker-name JBL Flip 6` saved/readback-verified at
uptime 29.032 s. Before reboot, query correctly distinguished persisted Flip 6
from current-boot GO 2. After idle `reboot`, SD mounted on attempt 1 and read
secondary/Flip 6/60%. Query at 31.242 s confirmed SD Flip 6/current boot Flip 6.
No firmware rollback, factory reset or SD removal was needed. Utility and startup
mitigations retained. Capture: `revert-flip6.log` in the private evidence folder.

The Flip 6 did not appear in completed inquiry at closeout, so its new audible
connection was not qualified. The requested speaker setting is restored and
automatic connection attempts continue. No Anthony or Death changes.
