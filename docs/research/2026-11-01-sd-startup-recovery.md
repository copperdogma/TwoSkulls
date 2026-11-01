# Recovering SD access after a host reset

Cam requested that startup initialize the installed card, verify real access,
and recover from improper startup where software can do so. Preserve the card's
powered failure state for diagnosis. The earlier power-cycle request is
superseded while this investigation is active. The speaker utility and GO 2
demonstration remain the final device task afterward.

## Evidence and standard mechanisms

Initial utility boot timed out reading sectors, then CMD13 returned 0xb00.
In pinned ESP-IDF 5.5.4 this is a card status response, not a host interrupt
register. It decodes to Sending-data state 5 with READY_FOR_DATA set. Later
initializations timed out at OCR negotiation. No speaker utility write ran.
An interrupted read is plausible; the later current protocol state is unknown.

- [Pinned card initialization](https://github.com/espressif/esp-idf/blob/v5.5.4/components/sdmmc/sdmmc_init.c)
  already sends CMD52 reset, CMD0, CMD8, CMD5 and then OCR negotiation.
- [Pinned native host](https://github.com/espressif/esp-idf/blob/v5.5.4/components/esp_driver_sdmmc/src/sdmmc_host.c)
  begins at 400 kHz/1 bit; the controller/FIFO/DMA reset and failed-mount cleanup
  already occur. Repeating those calls alone adds no new recovery mechanism.
- [Pinned transaction construction](https://github.com/espressif/esp-idf/blob/v5.5.4/components/esp_driver_sdmmc/src/sdmmc_transaction.c)
  uses the initialization clock train for CMD0 and a stop/abort command for CMD12.
- [Pinned command/OCR implementation](https://github.com/espressif/esp-idf/blob/v5.5.4/components/sdmmc/sdmmc_cmd.c)
  keeps clock running during ACMD41 and retries. Native ACMD41 timeout causes
  an MMC CMD1 fallback before the final `send_op_cond (1)` error.
- [SD physical specification 6.00](https://www.sdcard.org/cms/wp-content/themes/sdcard-org/dl.php?f=Part1_Physical_Layer_Simplified_Specification_Ver6.00.pdf)
  specifies CMD12 for aborting reads; [7.10 card reset rules](https://www.sdcard.org/cms/wp-content/themes/sdcard-org/dl.php?f=Part1_Physical_Layer_Simplified_Specification_Ver7.10.pdf)
  distinguish ordinary CMD0 reset from Inactive state requiring power-on recovery.
- [Espressif SD pull-up requirements](https://docs.espressif.com/projects/esp-idf/en/v5.1/esp32/api-reference/peripherals/sd_pullup_requirements.html)
  require DAT3 high even in 1-bit mode. A low DAT3 during reset can select SPI.
  Current application explicitly pulls CMD/DAT0 up but leaves DAT3/GPIO13 alone.
  Pinned host code drives DAT3 high for >=4-bit slot configuration, not 1-bit.
  Actual DAT3 voltage and physical pull-up network are not yet verified.

Decision: capture command/opcode/error/response sequence without editing the
SDK; then test one bounded CMD12 abort before normal reinitialization. Observe
DAT3 without first changing its pulls. Do not add arbitrary delays/retries,
format, force a writable filesystem or modify card data to diagnose this.
The current 20 MHz cap applies after initialization, so reducing it cannot
explain/fix an OCR stage that already runs at 400 kHz.

## Card power control uncertainty

The slot is on the board, not inside the ESP32 chip. Software power control
depends on the actual board circuit. [Freenove's FNK0060 guide](https://docs.freenove.com/projects/fnk0060/en/latest/fnk0060/codes/C/30_Read_and_Write_the_Sdcard.html)
describes the integrated slot and signal pins, but no power switch.
[Its public repository](https://github.com/Freenove/Freenove_ESP32_WROVER_Board)
links a generic ESP32 module datasheet, not a matching v3.0 board-level SD power
schematic. No GPIO-controlled card-power path is established from these sources.
Do not claim that such control exists or definitely does not exist without a
matching schematic or board-level verification. Unmounting is not power-off.

If board inspection confirms the card is tied directly to the shared rail,
automatic true power recovery would require a hardware change, such as a
dedicated card-supply switch with discharge and signal back-power prevention.
Choose no pin or part until the actual power path is verified. No wiring or
card-power modification was performed in this task.

## Powered failure experiments

Baseline `sd-trace-live.log` (ELF prefix 193e05472) showed CMD8 echo 0x1aa,
CMD55 response 0x120 and ACMD41 argument 0x40ff8000 with response 0x00ff8000.
This is native Idle state with APP_CMD accepted, but initialization-complete
OCR bit31 clear. The voltage window and HCS request are appropriate; native
responses rule out current SPI/Inactive mode as the cause of this failure.
The SDK already uses 300 polls with 10 ms delays, exceeding the specification's
one-second initialization expectation. More arbitrary polling is not justified.
The saved baseline BIN is usable; its matching ELF was not retained.

`sd-abort-live.log` records the single CMD12 attempt before mount 2: host API
returned ESP_OK but command.error was ESP_ERR_TIMEOUT, so the command failed.
Normal CMD0/CMD8/CMD55 then worked but ACMD41 remained not ready. CMD/DAT0
read high and DAT3 sampled low in these traces. Reading DAT3 does not verify
analog voltage or the resistor network, and native communication rules out
asserting that DAT3 caused this observed fault.

Decision: no further speculative reset variants. Retain a bounded read-abort
attempt as a standards-based recovery stage, add the documented DAT3 pull-up as
startup prevention, expose boot diagnostics and slow retries, then test a true
power cycle after those paths are captured. No format or config write during
the failed state. Software recovery of this particular not-ready card is not
claimed. No matching GPIO-controlled card-power path has been verified.

Portable ASan/UBSan suites passed after the DAT3 pull-up and utility changes.
An isolated harness of the actual boot-wait helper passed quick/30-second retry
timing, status/fault-log dispatch, manual retry interruption, refusal of write
or reset commands, malformed-line rejection across retry boundaries and
missing/invalid configuration distinction. This harness is a one-off local
verification, not a checked-in regression suite. Device checks remain required.

## Startup mitigation device check

Application-only final candidate was hash-verified on Jeff. BIN SHA256
`1711c82db548b9708163ffef9f5612d27753d63e922b57b854b4e0da939aee39`;
ELF SHA256 `474104357acd3ca7df2fa13f400a18228f560d686635fbcd4c435ba9013d8c3d`.
Capture `startup-failure-console.log` shows DAT3 now reads high with its pull-up,
but native ACMD41 still exhausts 300 polls, last OCR 00ff8000, ready=0.
One quick retry then 30-second waiting intervals are logged. Live `status` and
`fault-log` worked with no card mounted; compact original fault evidence remained
available from NVS. `speaker-name ...` and `reboot` were refused. `sd-retry`
interrupted the wait and ran attempt 3; it did not restore readiness. An overlong
line ending in a valid command was discarded, and subsequent status still worked.
Main-task minimum unused stack observed 536 bytes; no watchdog/reset observed.

Requested the true power-cycle experiment only after saving those observations.
No config write or formatting occurred in the unavailable-card state.

## Power recovery and normal startup

Cam confirmed all Jeff power removed for about 10 seconds and reconnected with
the card installed. `post-power-cycle.log` shows the card mounted on attempt 1
and valid Flip 6 configuration was read. This cleared the observed failure
after software abort/reset attempts had failed; it does not identify the card's
internal cause or prove that every such failure requires power removal.

The writing utility then changed only the configured speaker to GO 2 and
verified the result. An orderly console reboot again mounted SD on attempt 1
and read GO 2. Both startup and explicit Polo were heard clearly by Cam.
Polo completed with zero reported underruns. The retained initialization fault
was successfully archived to SD and pending became 0. Live runtime malformed
command checks also passed without changing the selected speaker.

Current startup therefore verifies mount and parsed config, has a bounded
read-abort/reinitialization stage, corrects the documented DAT3 pull state,
retains diagnostics and provides manual/slow retries. Automatic true power
cycling remains unavailable without a verified card-power control circuit.
Never equate software unmount/host reset with measured removal of SD VDD.
