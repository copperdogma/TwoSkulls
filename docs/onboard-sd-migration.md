# Jeff and Anthony: onboard SD migration

Status: Arduino firmware compiles; onboard mounting and Anthony's card/config were
observed on the borrowed v3.0 board on 2026-10-15. Audio, jaw/eyes and paired
hardware acceptance remain pending. The earlier perfboard conversion was deferred; the user subsequently
proposed unplugging Anthony's socketed board for a jumper-wired prototype. Current
research-only planning is in [Story 001](stories/story-001-single-wrover-matter-prototype.md).

## Scope and acceptance

Replace the external SPI SD reader with the onboard slot on each skull. Preserve
config.txt, audio/skit paths, roles, A2DP playback, BLE coordination, and animation.
Use the same firmware for both skulls; each card retains its own role/speaker config.
Completion requires both skulls to pass the bench checks below. Death's successful
migration is evidence for the approach, not validation of these two assemblies.

## Wiring and card preparation

Apply to one skull first, then repeat for the other after it passes.

1. With both USB power feeds disconnected, confirm the board has the onboard slot
   and matches the FREENOVE ESP32-WROVER board used in Death. The WROVER name alone
   does not guarantee a development board includes a card socket. If the board or
   wiring differs, check its schematic before proceeding.
2. Back up the skull's existing SD card, including its root config.txt and audio
   directory. Keep its role, speaker name, volume, and MAC address settings.
3. Disconnect the external reader's six connections: GPIO5 (CS), GPIO18 (CLK),
   GPIO23 (MOSI), GPIO19 (MISO), reader power, and reader ground. Remove the reader.
   Do not disconnect the shared ground used by the remaining components.
4. Move only the jaw servo signal (yellow) from GPIO15 to GPIO23. Preserve its
   separate 5 V supply and common ground. GPIO23 must no longer connect to MOSI.
5. Insert the existing backed-up card into the onboard slot. No files need to be
   renamed or moved. If preparing a replacement card, use FAT32 and copy the
   existing contents. config_primary.txt/config_secondary.txt in the repository
   are templates: the active filename on each card is config.txt at the root.
6. Compile/upload with the README's existing ESP32 core/library versions and board
   settings. Leave the servo signal off GPIO15 even during startup: setup moves
   the jaw before mounting the card.

| Function | GPIO after migration |
| --- | --- |
| Onboard SD clock | 14 |
| Onboard SD command | 15 |
| Onboard SD data D0 | 2 |
| Jaw servo signal | 23 |
| LED eyes | 32, 33 |
| Existing Matter trigger | 1 |
| Freed external SPI pins | 5, 18, 19 |

The slot also connects D1/D2/D3 to GPIO4/12/13 on the reference board. The driver
uses only one data bit; do not treat the other slot connections as automatically
safe to load. Existing GPIO1 trigger/serial behavior is outside this migration.

## Bench acceptance checklist

- [ ] First skull: mount log says onboard SD_MMC, 1-bit, 20 MHz and shows card size.
- [ ] Correct role/speaker config loads and skits are discovered without missing-file errors.
- [ ] Initialization WAV plays clearly; servo and LED behavior remain correct.
- [ ] Second skull passes the same checks with its own card/config.
- [ ] Pair performs Marco/Polo and connects normally.
- [ ] Matter trigger starts a coordinated skit; both jaws follow the correct parts.
- [ ] Repeat cold boots and several different skits with jaw motion; no mount/read failures or resets.
- [ ] Run the pair for an hour with repeated skits; record any glitches and user acceptance.

If mounting fails, power down before reseating the card. Check the servo is on
GPIO23, not GPIO15, and GPIO2/14/15 have no remaining external loads. Keep the
card backup intact. Firmware explicitly disables format-on-mount-failure.
To revert, restore the previous firmware and previous wiring together.

## Research and decision — 2026-10-14

Problem class: migrating a shared peripheral bus while preventing GPIO contention
and preserving an existing file/audio pipeline.

Local references inspected:
- ../death-fortune-teller/src/sd_card_manager.cpp: 1-bit SD_MMC at 20 MHz,
  CMD/D0 pull-ups, fixed GPIO14/15/2.
- ../death-fortune-teller/docs/stories/story-003b-built-in-sd-card-migration.md:
  records the GPIO15 servo conflict, move to GPIO23, and successful hardware
  mounting/audio/jaw regression. Its top-level status still says To Do, so use
  its dated work log as the specific evidence.

Primary sources:
- [Espressif SDMMC example](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/sdmmc.html):
  classic ESP32 pin assignments and fixed routing; setPins cannot remap them.
- [Arduino-ESP32 2.0.17 SD_MMC API](https://github.com/espressif/arduino-esp32/blob/2.0.17/libraries/SD_MMC/src/SD_MMC.h):
  begin(mountpoint, mode1bit, format_if_mount_failed, sdmmc_frequency, maxOpenFiles).

Decision: adapt Death's mount options, pull-ups and servo relocation; route every
filesystem access through SD_MMC, including config reads. Omit its setPins call
because routing is fixed on classic ESP32. The true argument selects 1-bit mode
directly; it is not an automatic fallback from 4-bit mode. Preserve /audio and
/config.txt paths because /sdcard is the VFS mountpoint, not an asset prefix.
Internal pull-ups reproduce Death's software setup; they do not establish that
a different board has the required physical pull-up network.

Remaining uncertainty: exact skull board revisions/wiring, card format, mounting
reliability under A2DP + jaw load, and two-skull coordination after conversion.
No power savings, speed improvements, or hardware reliability measurements have
been established for Jeff or Anthony by this change.

## Work log

- 2026-10-14: Compared both projects and audited all live SD calls. Replaced SPI
  SD access with SD_MMC, set explicit non-formatting 1-bit/20 MHz mount options,
  added card diagnostics, and reassigned the servo to GPIO23. Updated README and
  this wiring checklist. No hardware flashed or wiring changed.

- 2026-10-14: Full firmware build passed using a temporary PlatformIO configuration,
  Espressif32 6.12.0 / Arduino-ESP32 2.0.17, ESP-WROVER-KIT target, huge_app.csv,
  BOARD_HAS_PSRAM, and the existing Arduino libraries (A2DP 1.8.3, AnalogWrite
  5.0.2, arduinoFFT 1.6.1). All eight .cpp files and TwoSkulls.ino compiled and
  linked. Flash: 1,310,065 / 3,145,728 bytes; static RAM: 43,244 / 327,680 bytes.
  Warnings concerned existing ADC/FFT deprecations and A2DP's optional AudioTools
  include. git diff --check passed. No firmware was uploaded.

## Reproducing the compile check

This check uses a temporary PlatformIO project; the Arduino workflow and global
Arduino core installation were not changed. It proves source/API compatibility
with core 2.0.17 and the existing libraries, not hardware behavior or the exact
Arduino IDE flash settings. PlatformIO installed its matching compiler toolchain.
The local build log and configuration are retained in the ignored build directory:
`build/onboard-sd-check/build.log` and `build/onboard-sd-check/platformio.ini`.
Run `pio run -d build/onboard-sd-check` from this repository to repeat it on this
machine (the configuration references this checkout and local Arduino libraries).

Recommended next action: power down one skull, confirm its board/slot and wiring,
then perform steps 2–5 above before uploading through the established Arduino IDE
board settings. Validate mounting and initialization playback before converting
the second skull.

## Deferred — 2026-10-14

User deferred the migration because the servo wiring is soldered to perfboard
and moving GPIO15 to GPIO23 would require hardware rework. Keep this work on
branch codex/onboard-sd-migration in the isolated worktree. Main checkout was
restored to its original firmware. No upload, soldering, or commit was performed.

Both skulls use identical firmware and differ through SD-card contents/config.
When resuming, start with Anthony, which the user reports is working properly.
Jeff has a separate problem awaiting the user's description; do not conflate it
with this deferred SD migration.

## Prototype planning resumed — 2026-10-14

The user proposed testing Anthony outside the perfboard using the onboard slot,
eyes and servo leads, while keeping Jeff unchanged. The requested next deliverable
is a detailed implementation story, not new firmware. Story 001 uses this compiled
Arduino migration as the first physical baseline before trying Matter, A2DP and
BLE together under native IDF. No additional source changes, builds, uploads or
physical tests were performed during that research pass.

## First physical baseline — 2026-10-15

Cam discovered Anthony's original PCB is v2.1 without an onboard slot; Jeff's is
v3.0 with a slot. The v3.0 board now hosts Anthony's card for the prototype.
Its original full flash was backed up and device-verified, then the prepared
Arduino migration mounted the card at 1-bit/20 MHz and loaded the expected primary
settings and 11 skits. Cam did not hear the initialization announcement, so
A2DP playback is not accepted from that trial. Only Anthony had a C3; Jeff's
under-board module was USB-C PD power. See the implementation log for current
native-probe evidence, private recovery location and open hardware tests.
