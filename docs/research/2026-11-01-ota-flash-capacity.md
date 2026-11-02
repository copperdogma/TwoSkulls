# OTA flash capacity check

Cam requested a feasibility check, not implementation or flashing. Inspected the
active native firmware on `codex/jaw-articulation` at `3192dcc`; the main checkout
still contains the historical Arduino application and Huge APP layout.

## General problem and source

The constraint is space for two bootable firmware images in nonvolatile flash.
[Espressif's ESP-IDF OTA guide](https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32/api-reference/system/ota.html)
requires two OTA application slots and an 8 KiB OTA data partition for normal
power-failure-resilient application updates. The inactive slot receives and
verifies the replacement before boot selection changes. SD storage or PSRAM
does not supply a second ordinary executable OTA slot.

## Local evidence and small test

- `firmware/README.md` records the verified WROVER target as 4 MB SPI flash.
  `firmware/sdkconfig.defaults` configures 4 MB; retained Anthony and Jeff boot
  captures also report 4 MB. No live flash-ID query or hardware reset was done.
- `firmware/partitions.csv` has 64 KiB NVS at `0x10000`, PHY at `0x20000`,
  and one factory app at `0x30000` with size `0x3D0000` (3,997,696 bytes).
  There are no OTA app slots or OTA data partition. Matter OTA requestor is off.
- Latest local native image, matching the retained November 1 startup-final
  binary: **2,114,048 bytes** (`0x204200`, 2.016 MiB), SHA256
  `1711c82db548b9708163ffef9f5612d27753d63e922b57b854b4e0da939aee39`.
  Build log: `build/validation/jbl-go2-20261101/startup-final-build.log`.
- The October 30 shared accepted jaw image is 2,108,192 bytes, also larger
  than half of 4 MiB even before reserving bootloader/settings space.
- The current factory partition has 1,883,648 bytes unused. That is less than
  one replacement image, and is inside the existing factory partition.
- Local ESP-IDF **5.5.4** `gen_esp32part.py` accepted a temporary hypothetical
  4 MB layout preserving NVS/PHY, adding OTA data at `0x21000`, and replacing
  factory with `ota_0` at `0x30000` size `0x1E0000` and `ota_1` at `0x210000`
  size `0x1F0000`. `check_sizes.py` rejected the current image for both slots:
  overflows **147,968** and **82,432** bytes respectively. Both slots must fit.
  Temporary files were removed; the actual partition table was not changed.

## Decision and uncertainty

Standard OTA does **not fit as-is** on the recorded 4 MB hardware. This is not
proof that OTA is permanently impossible: shrinking the image and repartitioning
could make it fit. Preserving current NVS/PHY placement needs at least 144.5 KiB
removed from the current image, plus space for the updater and future growth.
Repacking PHY into a lower gap could allow two 1,984 KiB slots and reduce the
initial deficit to 80.5 KiB, but requires a separately validated layout transition.
Neither optimization nor updater code was built in this check. Size optimization
is already enabled; OTA runtime memory/radio behavior is unqualified.

An otherwise compatible board with 8 MB or more flash would offer a much easier
capacity path, but no replacement hardware compatibility is established here.
Do not treat an SD-staged custom self-overwrite scheme as equivalent to standard
two-slot OTA. If implementation is requested, first measure a reduced build plus
updater in an isolated configuration, then plan a USB layout transition preserving
Matter/Wi-Fi NVS and qualify rollback and interrupted updates.
