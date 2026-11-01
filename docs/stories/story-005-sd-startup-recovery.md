# Story 005 SD startup recovery and boot diagnostics

Status: Implemented and bench checked; hardware power-control follow-up open.
Date: November 1, 2026.

## Goal

Initialize and verify real SD access before starting playback, recover through
supported software mechanisms where possible, and keep useful diagnostics
available when the installed card cannot initialize. Preserve card contents and
failure evidence. The speaker-name utility is installed, and GO 2 is selected
on Jeff's SD card for Cam's demonstration.

## Evidence and recovery experiment

See [the research and device evidence](../research/2026-11-01-sd-startup-recovery.md).
The original mount failed a sector read; its follow-up CMD13 reported
Sending-data state. Later tracing established native command communication but
ACMD41 initialization did not become ready within the SDK's bounded polling
window. This is not evidence that editing config.txt caused the problem: no
speaker configuration write had run.

One CMD12 abort before the ordinary CMD0 initialization path did not recover
this powered fault. Native CMD8/CMD55 replies remained correct, while ACMD41's
ready bit stayed clear. The DAT3 pull-up now reads high, but did not clear the
existing fault. Additional clock/wait/reset variants require fresh evidence;
repeated polling is not a demonstrated recovery mechanism. Card power control
is unverified for the actual board.

## Boot console behavior

The UART driver is installed before the first SD mount attempt. During a failed
boot, the console accepts only:

- `status`: boot attempt, mount/config distinction, last error, time to retry,
  and safe snapshots of firmware diagnostics before workers start.
- `fault-log`: available retained failure evidence.
- `sd-retry`: interrupt the waiting interval and request another ordinary mount
  attempt. This does not reset or power-cycle the card.

The first failure receives one quick retry after its normal eye pattern. Later
failures wait 30 seconds between completed attempts. The eye diagnostic and
actuator control remain active during the wait; fault retention can use internal
flash, while SD report writes are disabled in this path. A mount transaction
itself still occupies the boot task until its SDK timeout; console handling
resumes during the following wait.

Three flashes indicate SD initialization/filesystem mount unavailable. Five
indicate a mounted card whose configuration is missing, invalid or unavailable;
the log identifies which case. Repeated timeouts explicitly report that standard
reinitialization has not restored access and card power recovery may be needed,
without claiming it is the sole possible remedy.

Playback, config writing, reboot and factory reset commands are unavailable in
this boot console. Overlong lines are discarded through their delimiter,
including across automatic retry boundaries. Once valid configuration loads,
queued boot input is discarded before the ordinary runtime console starts.

## Verification boundary

Source review confirmed existing status snapshots guard absent workers or read
initialized default state. No radio/audio workers are started by the boot
console. Native build and portable sanitizer suites passed. An isolated helper
harness checked quick/30-second timing, dispatch, manual retry, invalid input
and missing/invalid config distinctions. Live failed-card console checks passed
for status/fault-log, manual retry, write/reset refusal and overlong-line
rejection. Cam then power cycled Jeff; the card mounted on attempt 1. The utility
saved GO 2, an orderly reboot mounted on attempt 1 again, and Cam heard startup
speech and Polo clearly (zero reported Polo underruns). Retained fault export
to SD succeeded. These checks do not establish software recovery of the original
internal not-ready state or a GPIO-controlled card-power path. Full show and
arbitrary power-interruption qualification remain separate.
