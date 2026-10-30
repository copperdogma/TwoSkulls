# Jeff — accepted jaw firmware update, 2026-10-30

Status: Application update, boot/config, speaker connection and startup PCM delivery PASS. Human physical/paired acceptance remains separate.

Cam requested updating the attached Jeff after accepting and pushing the softer
Anthony speech-jaw tuning. Use the identical shared native application from
`ccda781`, selected by the board's existing SD configuration.

## Identity and rollback preflight

- Private no-requested-reset serial capture reports secondary role, JBL Flip 6,
  volume 60%, mounted SD, 8 MB PSRAM (4 MB mapped), old ELF prefix `003f1adab`.
  Status: show idle, radio fault=0, actuator stuck=0 / pending_channels=0.
  `fault-log` is unknown on the old build. No pending SD report can be retrieved
  through that older console; preserved full private capture before flashing.
- ROM/stub identifies ESP32-D0WD-V3 rev 3.1, base MAC `f4:2d:c9:78:ad:78`.
- Existing validated rollback dump from Anthony contains the same original
  executable ELF SHA256. A first full padded-dump comparison did not match.
  General issue: verifying an executable image versus unrelated trailing flash
  bytes. [Espressif verify_flash documentation](https://docs.espressif.com/projects/esptool/en/release-v4/esp32/esptool/advanced-commands.html)
  compares the bytes in the supplied file; padding must not be confused with
  executable content.
- Parsed the saved ESP32 image's six segment lengths, aligned checksum and
  appended SHA256 boundary to extract the complete 2102016-byte executable
  (`0x201300`), excluding 126208 bytes beyond it. Esptool `image_info` validates
  checksum and digest and reports original ELF SHA256
  `003f1adabab1a027002db841756f4685d13b09c8dbb8d0253d8f71f4551d005a`.
- Esptool `verify_flash` at `0x30000` against that exact image PASS (digest
  matched). This locally demonstrates Jeff's existing app has a recoverable
  identical saved image; no claim is made about equality of trailing unused
  flash or device-specific NVS. No new full-device backup was necessary.
- Private rollback executable:
  `build/validation/pre-jaw-tuning-20261030/original-app-image.bin`.
  Raw preflight/verify captures: ignored `build/validation/2026-10-30-jeff-*`.

## New image and update scope

- Binary SHA256 checked before upload:
  `b2aaa5163be6811b437e270bdb70afc5e03a5b0d125a770ff9b538201e65d013`.
- New ELF SHA256:
  `6cd2543e7b65a3b6381e8fb8787005350f0cad6d3b9bcb51e11c33fe7889054a`.
- Identical image already flashed and accepted on Anthony. Final host sanitizer
  suites and native build passed before the push; no source rebuild/change
  required for Jeff. SD config selects secondary and retains volume 60%.
- Upload only the application at `0x30000`, 115200 baud. No bootloader,
  partition table, NVS erase, card content or calibration changes. Includes
  previously committed shared reconnect/fault logger support as on Anthony.

## Qualification remaining

Confirm flashed hash, new boot identity, secondary configuration, SD/PSRAM,
speaker/startup and normal role behavior. Physical jaw/eyes/audio and a paired
show require their own observed acceptance; upload success is not that proof.

## Upload and boot result

- Application-only write PASS: 2108192 bytes at 0x30000 in 127.4 seconds;
  esptool reports `Hash of data verified`. Old rollback image remains saved.
- Boot reports new ELF prefix `6cd2543e7`, mounted 961 MB SD card, 15 WAVs /
  11 scripts, secondary role, JBL Flip 6 and volume 60%. Startup PCM loaded at
  60% gain. PSRAM initialized (8 MB, 4 MB mapped), actuator pending_channels=0 /
  stuck=0, radio fault=0, pending fault report=0.
- Saved speaker attempts initially return ACL status 260; stream=0 and no PCM
  callbacks yet. Requested Flip 6 power/discoverability for startup verification.
  Firmware update is complete; audible/visual and paired qualification remain
  separate until observed. Private capture: `2026-10-30-jeff-jaw-after.log`.

## Speaker and startup check

Cam made the Flip 6 ready. Saved-address reconnect attempt 2 succeeded at uptime
61651 ms (ACL status 0); A2DP connected, SBC / 44.1 kHz contract and media start
acknowledged. Startup began at 61911 ms and PCM delivery completed at 65831 ms.
Status: speaker=1, stream=1, radio fault=0, SD mounted, secondary_ready=1.
This demonstrates firmware/connection/source delivery, not human-observed jaw,
audible quality, or a coordinated two-skull skit. No additional skit/loop was
started; normal secondary scheduling is retained.
