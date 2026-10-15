# WROVER revision swap and missing startup — 2026-10-15

Cam returned with original Anthony v2.1 WROVER fitted to Jeff's perfboard, which
previously held the v3.0 WROVER now used as Anthony's native prototype. Reported:
no startup servo movement, no speaker connection, and no eye error pattern.
Original v2.1 was unplugged during the initial backup and was not flashed this
session. Do not confuse it with the modified v3.0 prototype.

## Source-grounded observations

Legacy `TwoSkulls.ino` initializes eyes, then the servo sweep, then attempts SD
mount and config parsing. SD failure blinks three times; config failure five.
Role selection occurs after these initializations. Therefore a wrong card role
or absent SD alone does not explain a missing initial servo sweep if that exact
firmware and working actuator wiring are present. Actual flashed v2.1 firmware
has not yet been identified from serial output.

Freenove publishes separate [older 1.6 pinout](https://github.com/Freenove/Freenove_ESP32_WROVER_Board/blob/main/ESP32_Pinout_1.6.png)
and [v3.0 pinout](https://github.com/Freenove/Freenove_ESP32_WROVER_Board/blob/main/ESP32_Pinout_V3.0.png).
Visual comparison shows common actuator/SPI GPIO positions unchanged, but these
positions differ (viewed component-side, antenna up, USB down):

| Header positions | Published older layout | Published v3.0 layout |
| --- | --- | --- |
| Left, three positions immediately above two bottom 5V pins | SD2/GPIO9, SD3/GPIO10, CMD/GPIO11 | Three 3V supply pins |
| Right, three positions immediately above bottom GND | SD1/GPIO8, SD0/GPIO7, CLK/GPIO6 | Three GND pins |

A perfboard using those v3.0 supply/ground positions could load or ground an
older board's flash bus, preventing boot before software error signals exist.
This is a concrete compatibility hypothesis, not yet the diagnosis: the available
older drawing is labelled 1.6, and the actual v2.1 labels must be checked. Which
power/ground positions Jeff's perfboard uses is also not yet established.

## Next discriminating checks

1. Power Jeff off and read actual v2.1 header labels before assuming socket
   compatibility. Cam has been asked to check the two groups above.
2. If labels differ, trace the relevant unpowered perfboard connections before
   inserting that board again. Software cannot reassign flash/power connections.
3. If needed, boot v2.1 outside the perfboard over USB and capture ROM/application
   output without flashing it. Missing external SD is expected in that isolated
   check; the goal is to identify boot success and existing firmware/pins.

SD config controls logical role and speaker name in the legacy source. It cannot
make differing physical header power/flash assignments interchangeable. Preserve
the original image and the separate v3.0 prototype throughout diagnosis.

## Physical label confirmation

Cam read the actual Anthony v2.1 labels: left SD2/SD3/CMD and opposite
SD1/SD0/CLK. These are the differing groups shown in the older manufacturer
drawing. This confirms the two physical boards are not fully header-compatible;
it does not yet establish which of those positions Jeff's perfboard connects.
Power/ground tied to those flash signals would explain failure before startup
software. Do not attempt to solve that physical mismatch by flashing firmware.

Requested the next safe test: remove v2.1 from the perfboard and connect USB alone
for a read-only boot capture. Its missing external SD is expected while isolated.
A matching v3.0 is the direct replacement for a perfboard using the v3.0 supply
and ground groups. The original v3.0 image remains backed up separately from the
current native Anthony prototype. No firmware or hardware changes were made to
the v2.1 board during this diagnostic step.

## Alternative board photo: Freenove ESP32-S3 WROOM

Cam supplied `/Users/cam/Downloads/IMG_1180.jpg`. PCB silkscreen identifies
ESP32-S3 WROOM/FREENOVE; shield reads ESP32-S3-WROOM-1, with OV2640 camera.
This is the S3 family, not the classic ESP32 used by the working A2DP probe.

[Espressif's Bluetooth capability matrix](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/bt-architecture/overview.html)
marks ESP32-S3 as BLE-supported but Classic-unsupported. A2DP to the existing JBL
speakers requires Classic; this excludes it as Jeff's sole processor under the
current audio design. No flashing experiment can supply the missing chip feature.

[Freenove's FNK0085 hardware guide](https://docs.freenove.com/projects/fnk0085/en/latest/fnk0085/codes/C/Preface.html)
confirms rear microSD, 1-bit SDMMC on GPIO38–40, and the black N8R8 board variant
(8 MB flash/8 MB PSRAM). Photo-based capacity identification remains provisional
without the complete module suffix or device query. The chip has dual LX7 cores
up to 240 MHz, Wi-Fi and BLE; [Matter SDK supports S3](https://docs.espressif.com/projects/esp-matter/en/latest/esp32s3/developing.html).
SD, BLE, Matter and PWM capability do not substitute for A2DP. Use a matching
classic ESP32-WROVER v3.0/onboard-slot board to preserve the present JBL audio
architecture. The S3 assessment required no firmware or hardware changes.

## Replacement shopping decision (2026-10-15)

Recommended matching classic Freenove ESP32-WROVER camera board with onboard
microSD. [Amazon Canada listing B0CJJHXD1W](https://www.amazon.ca/Freenove-ESP32-Camera-Development-Board/dp/B0CJJHXD1W)
was visibly in stock at CA$26.95, sold by Freenove and shipped by Amazon, with
variant **With Memory Card Slot and Card** selected. No order was placed.

[Manufacturer FNK0060B product specifications](https://store.freenove.com/products/fnk0060)
list dual-core 240 MHz classic ESP32, 4 MB flash and 8 MB PSRAM, with microSD
slot and included 1 GB card. Amazon's detailed description agrees; its generic
RAM attribute inconsistently says 16 MB and should not override these specs.
Camera is unnecessary for the skull application. Classic Bluetooth preserves
the JBL A2DP path; matching the working prototype board avoids a new chip port.
The listing does not explicitly guarantee the delivered PCB revision: verify
v3.0 and supply/header pin labels before installing in Jeff's perfboard.
Price/stock are observations at lookup time, not guarantees.
