# Matching firmware on the new Jeff board — 2026-10-15

Cam plugged the new classic WROVER and Jeff's existing SD card into a second Mac
USB port and authorized initialization. Jeff's speaker is on; jaw and eyes are
not connected. Keep a single executable for both skulls, selected by card config.

## Identified boards and change

- Anthony: `/dev/cu.usbserial-10`, MAC `a0:a3:b3:80:16:38` (the original v3.0
  board borrowed for Anthony), existing native partition/NVS preserved.
- New Jeff: `/dev/cu.usbserial-110`, MAC `f4:2d:c9:78:ad:78`, ROM identifies
  ESP32-D0WD-V3 revision3.1. Original Arduino partition table at0x8000; new native table at0xc000.
- The initial300ms startup PCM silence clipped the first “A” on Cam's later
  power-up. Added500ms, for800ms/35280 frames of preroll after media readiness.
  The connection starts normally; speech waits for the already-streaming speaker.
- Both boards will receive the same2096256-byte application, SHA256
  `70b8b0d4689b3f49f44e5c71a69b0ca554bba070dafe1bd8a1e80f7b4b546933`.
  Build/hash manifest: ignored `build/validation/2026-10-15-jeff-shared-build-hashes.json`.

## Programming transport

Initial new-board4MiB backup read at460800 baud failed with a truncated packet.
This is an incomplete attempt, not a recovery image. Following the existing
[Espressif serial troubleshooting guidance](https://docs.espressif.com/projects/esptool/en/latest/esp32/troubleshooting.html),
retry the read at115200 baud before any erase/write to that board. Record its
result and digest before calling the original backup complete. Do not infer a
board electrical fault from one high-speed serial-transfer error.

## Acceptance pending

Verify new-board PSRAM, onboard card/config role=secondary, correct speaker,
startup announcement, standalone GATT/Polo and actual paired filename/ACK
exchange. Owner confirmation is needed for unclipped announcements and both
speakers' dialogue. Jeff's jaw/eyes are physically disconnected, so no actuator
acceptance can be claimed. Combined memory/timing and yard soak remain open.


## Recovery image and first native storage initialization

Read4194304 bytes at115200 baud; complete original backup SHA256
`4a1ee4e5ad899eca63f7a98d9a0e53c8632f62d38e50399ebe7928c6b325a378`.
Esptool `verify_flash` against the whole board returned digest matched. Private
backup directory: `/Users/cam/.codex/device-backups/TwoSkulls/Jeff-new-v3.0/2026-10-15-f42dc978ad78/`.
The initial failed high-speed read is not the retained verified image.

Only on new Jeff, erased0x10000–0x20fff to initialize the new64KiB NVS and4KiB PHY
regions before first native boot. This deliberately replaces bytes from its
original Arduino application layout; recovery is the complete original image.
No Anthony NVS erase occurred. Upload uses bootloader0x1000, table0xc000 and
shared application0x30000. Future app updates must preserve native NVS.

Cam confirmed Anthony's updated initialization was complete, including “A”.


## First native boot and failed paired playback

Image upload verified. New Jeff boot found8MiB PSRAM (4MiB mapped), mounted the
961MB card, read `role=secondary`, `speaker_name=JBL Flip 6`, volume60%, and indexed
15WAVs/11scripts. Saved first speaker connection. Startup PCM delivered by15.676s;
Polo drained24289 frames with zero reported underruns. Cam heard both Jeff
initialized and Polo. Anthony discovered the legacy service and completed its
initial read atMTU247. These are real two-board protocol/startup observations.

A normal controller trigger selected favourite movie. Jeff queued acceptance,
Anthony received notification and exact readback, and both consumed2157034 source
frames. However, Anthony reported3389 underruns and took56.680s; Jeff reported321
and took49.630s for48.912s authored audio. Cam heard continuous slight gravelly
stutter/slowness on Anthony and no Jeff skit dialogue. This is a failed paired
playback run, despite successful BLE exchange and complete source consumption.
Do not treat software PCM delivery as proof of audible Jeff role data.

Next image adds producer read/process/refill timing and pre/post-mask sample
counts, plus read-only `asset /audio/<filename>.wav` to inspect actual card script
intervals. It leaves the playback algorithm unchanged to discriminate SD I/O,
CPU/scheduler budget and role masking before choosing a fix. Both boards receive
the same instrumented application; native NVS is preserved on both.


## Silence resolved by physical media volume

Jeff alone (Anthony held in ROM, no BLE link) still initially had no audible
startup/tone. This ruled out the peer link as the immediate cause. Cam then raised
the JBL's physical volume, heard both a repeated tone and Polo, and explained
that he had previously turned the speaker to minimum and attempted several
up-clicks. The observed fix is speaker volume, not a transport code change.
Connection chimes were audible even when media was inaudible. A speculative
media-restart API was prepared but removed before flashing; AVRCP, modem sleep
and BLE connection policy remain unchanged. Keep the underrun correction separate.


## Final shared image and first clean paired skit

Final application2098304 bytes, SHA256
`cf4692a71f1f29405cf85f77b9c03606e826ea2730444cd1c633aec85cc88a25`.
Same binary hash-verified on both MAC-identified boards, application-only updates
at0x30000; all native settings preserved. Includes800ms startup silence,
8192-byte PSRAM stdio buffer and at most two refill blocks per mandatory blocked
yield. No new AVRCP, media-restart, modem-sleep or BLE-parameter workaround.
Actual producer sanitizer tests assert exact gain/role metrics and burst bound;
independent ownership/scheduler review found no blocker.

Both boards reconnected saved speakers and restored the BLE link. A normal
serial paired trigger selected Starbucks. Jeff queued its ACK at28.347s local
boot time; Anthony accepted notify/readback and started at28.577s on his local
clock. These independent clocks are not synchronized and do not establish a
precise acoustic offset. Both completed1647341frames with zero reported
underruns (Anthony28.577–65.967s; Jeff28.347–65.707s;37.355s source duration).
Cam confirmed both speakers sounded clean and took turns correctly.

At the end of this bounded test, primary provider wall-time max2945us, secondary
1716us; producer minimum unused stack2332/2320bytes; no dropped radio events,
watchdogs or panics observed. Bounded internal-memory aggregate low31191bytes
on Anthony,103343 on Jeff. Primary remains below the proposed32KiB floor, with
only993staticIRAMbytes free in this configuration. One clean skit is functional
bench proof, not full resource or yard/soak acceptance. Jeff's jaw/eyes remain
physically disconnected. Apple Home paired trigger check follows separately.


## Apple Home paired trigger — passed

Cam activated Anthony's existing Home switch. Serial records Matter trigger
accepted at141.857s and the selected imitations skit. Jeff queued acceptance,
Anthony verified readback, and both completed1341581frames with zero reported
underruns. Cam confirmed a paired skit started and the switch returned to off.
No recommissioning was required. Together with owner-confirmed clean Starbucks,
this establishes bounded functional two-board and Home-trigger bench proof.
It does not close Jeff's disconnected actuator or yard/recovery/long-soak gates.

Final capture end-of-second-skit provider maxima: Anthony3428us, Jeff1716us;
no radio queue drops or watchdog/panic observed. Retain both active captures for
further bench work; ignored logs may include private Matter exchanges.
