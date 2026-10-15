# SD card recovery after an interrupted software reboot — 2026-10-15

## Problem and local evidence

This is peripheral/card reinitialization after a CPU reset while external card
power remains applied. It occurs before radio startup, so the observed failure
is not a Matter/A2DP/BLE coexistence failure.

In `build/validation/2026-10-15-sbc-o2-targeted/secondary.log`, Jeff was playing
`Skit - names.wav` when an intentional console reboot interrupted it. The next
boot loaded flash/PSRAM and entered the SD retry loop. OCR initialization repeatedly
returned `ESP_ERR_TIMEOUT` (`0x107`). One retry passed card initialization but
failed a filesystem sector read, with a successful CMD13 status of `0xb00`.
The current-state field is5, rather than the driver's normal transfer state4;
the card was not simply absent or incapable of answering commands.

Subsequent parent-run hardware observations: a full Jeff/card power cycle
restored mounting; an idle console `secondary:reboot` then mounted the card,
loaded secondary config and played its startup announcement successfully. The
failure is therefore isolated so far to the interrupted-read reboot, rather
than demonstrated on all warm reboots. No card corruption was established;
this application does not write or format card contents.

## Primary sources and applicability

- [Espressif ESP-IDF5.5.4 reset documentation](https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32/api-reference/system/misc_system_api.html):
  `esp_restart()` restarts CPUs and leaves most peripherals unreset. It does not
  remove power from an external SD card.
- [Pinned card-init implementation](https://github.com/espressif/esp-idf/blob/v5.5.4/components/sdmmc/sdmmc_init.c):
  the SDK already issues CMD52 and CMD0 before OCR negotiation. Ordinary remount
  already performs a protocol reset attempt; adding another unconditional CMD0
  would duplicate the existing sequence without explaining this failure.
- [Pinned FAT/SD mount cleanup](https://github.com/espressif/esp-idf/blob/v5.5.4/components/fatfs/vfs/vfs_fat_sdmmc.c):
  failed mounts deinitialize an initialized host. The application's mounted flag
  remains false after failed initialization, which is expected.
- [Pinned host implementation](https://github.com/espressif/esp-idf/blob/v5.5.4/components/esp_driver_sdmmc/src/sdmmc_host.c):
  slot setup starts at400kHz and one-bit width before selecting the normal clock.
  Lowering only the configured20MHz operational maximum does not directly target
  the early OCR timeout.
- [Espressif upstream retained-card restart report](https://github.com/espressif/esp-idf/issues/14000):
  demonstrates this general failure class, but its specific defect is SPI CMD52
  CRC handling. Our native SDMMC OCR timeout is different; do not transplant its
  workaround as if that issue diagnosed this board.
- [ESP-IDF5.5.4 SD pull-up requirements](https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32/api-reference/peripherals/sd_pullup_requirements.html):
  one-bit mode still requires CMD/DAT pull-ups. This remains a hardware audit
  consideration, not a proven explanation of the interrupted-reboot result.
  Flash/PSRAM/application boot succeeds, so boot-strap failure is not the observed
  failure stage.

## Narrow prevention decision

Refuse intentional console reboot while a show, tone, startup speech, or the
SD audio producer is active. Do not close another task's file or unmount a
filesystem while a producer may be reading it. Do not add card/SDK reset
workarounds based on this single failure.

A show-idle/audio-not-busy check alone has a small asynchronous hole: the worker
publishes its last PCM block before closing the file, while the callback can mark
playback complete. Explicit producer file quiescence is required in addition to
show/audio state. The file flag must become active before opening and clear only
after close returns; an acquire/release read prevents console reboot during that
window. Normal playback scheduling is controlled by the same diagnostic task
that processes console reboot, so it cannot launch a new producer request
between a successful guard and `esp_restart()`.

Waiting for initial speaker discovery is permitted: the startup WAV is loaded
and closed synchronously before the diagnostic task begins. Pending startup
speech can therefore be distinguished from currently playing speech/preroll;
`startup::playing()` is the appropriate speech guard, rather than its broader
`busy()` which includes waiting for the first speaker connection.

The implemented `reboot` command now checks show state, tone frames, active
startup speech and `audio::quiescent()`. The latter combines the producer state
with a worker-owned atomic file-active flag covering opening through completed
close, including error paths. The audio provider gained no work or synchronization.
It does not force cancellation, close a worker file, or unmount during playback.

A production-source host test holds the worker inside its actual final-close
call while the provider consumes the last block. It verifies playback can report
complete/not-busy while quiescence remains false, then becomes true only after
close is released. Existing cancellation tests also verify nonquiescence during
callback-held cancellation; failed-open testing verifies quiescence recovers.
ASan/UBSan audio host suite passes. No native build or flash was performed for
this change by this agent.

## Remaining uncertainty and next proof

The prevention guard needs production compilation and a targeted device test:
reboot during real SD playback must be visibly refused, while idle reboot must
still mount and initialize. Cold-cycle and idle-reset success do not prove a
recovery path from panic, watchdog, brownout, or externally forced reset during
an SD transaction. Those cases may require explicit hardware card-power control
or separately researched protocol recovery; neither is implemented by this
narrow prevention change.
