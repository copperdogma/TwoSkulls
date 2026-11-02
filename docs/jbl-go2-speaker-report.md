# JBL GO 2 connection report and 2027 reuse guide

Experiment date: November 1, 2026. Hardware: Jeff and Cam’s JBL GO 2.

Final state after the successful demonstration: Jeff is back on
`speaker_name=JBL Flip 6`, verified on SD and after reboot. The reusable USB
`speaker-name` utility and startup mitigations remain installed, with no
experimental override. Before restoration, SD selection of GO 2 survived reboot
and Cam heard both startup speech and Polo clearly. To use GO 2 next year, change
the setting with the utility below. See [the installed-card writing record](research/2026-11-01-speaker-config-writing.md)
and [SD startup recovery record](research/2026-11-01-sd-startup-recovery.md).

Jeff successfully connected to the GO 2 using the current native ESP32 firmware.
Cam heard the diagnostic tone and clear “Polo” speech. The speaker is a promising
candidate for a more self-contained skull because of its small size, but mounting
it inside the head remains an idea to test. No internal fit or mounting method
was established in this experiment.

## What got it working

Jeff’s SD card still selected `JBL Flip 6`. The current firmware selects a speaker
by its exact Bluetooth name, so Jeff first needed to target `JBL GO 2`.

For this reversible experiment, a build option changed the secondary skull’s
radio selection to `JBL GO 2` without editing the card. After flashing, the first
scans found no candidates. Cam pressed the GO 2’s Bluetooth button again and
confirmed its pairing light was flashing. Jeff then discovered and connected to
it automatically. The pairing window may have expired during the build; that
explanation was not independently established.

The existing discovery, pairing and audio code worked. No Bluetooth library
upgrade, codec change, special PIN workaround or speaker reset was needed.
This does not explain the earlier unsuccessful attempts with historical code:
that older failure was not reproduced during this experiment.

## Results

| Check | Observed result |
| --- | --- |
| Discovery | An unnamed result was followed by the exact name `JBL GO 2` at the same address |
| Authentication | Success, status 0 |
| A2DP connection and media start | Successful; `speaker=1`, `stream=1` |
| Audio format | SBC, 44.1 kHz stereo, bitpool 2–53 |
| Initial startup announcement | Firmware delivered its PCM, but Cam did not hear speech |
| Explicit diagnostic tone | Cam heard it |
| Explicit `Polo.wav` playback | Cam confirmed clear speech; 24,289 frames, zero reported underruns |
| Status after Polo | SD mounted, zero dropped radio events, radio fault 0, actuator stuck 0 |

The audible tone and speech establish that this GO 2 can work with Jeff. The
unheard first announcement still needs investigation. Successful streaming and
source PCM completion alone do not prove that a speaker has unmuted in time.

## Repeat the setup next year

With the retained utility firmware, use 115200-baud USB serial:

```text
speaker-name
speaker-name JBL GO 2
speaker-name
reboot
```

Wait for the saved/readback-verified message before rebooting. Put the GO 2 in
pairing mode if needed, then confirm startup speech and an explicit
`play /audio/Polo.wav`. The first original config is saved on the card; unrelated
settings remain intact. A failed-card boot permits only `status`, `fault-log`
and `sd-retry`; it cannot change config until the card mounts and config is valid.

The following steps describe the original temporary-image experiment for
historical reference. Its build option has since been removed from retained
source; use the saved `experiment.bin` if the original test image is needed.
Use the USB utility above with current source.

Use the native `firmware/` implementation and its build instructions. Historical
Arduino instructions on `main` describe a different runtime. The tested source
was branch `codex/jaw-articulation`, base commit `757d5ac`, with the local GO 2
experiment changes.

1. Verify which skull is attached and capture `status` and `fault-log` before
   changing anything. Preserve its current application and settings. Jeff’s
   verified base MAC for this experiment was `f4:2d:c9:78:ad:78`.
2. Select the exact name `JBL GO 2`. To reproduce this experiment, build with
   `SKULL_JBL_GO2_EXPERIMENT=ON`. It affects only the secondary role and leaves
   the SD configuration untouched.
3. Power on the GO 2, press its Bluetooth button and confirm the pairing light
   is flashing **when Jeff is scanning**. Seeing it on a phone was a useful
   cross-check; leave the phone disconnected during the test. Cam had forgotten
   its old phone pairing, but this experiment did not establish that forgetting
   it was required.
4. Watch for exact-name discovery, authentication success, A2DP connected and
   media start success. An initial candidate with no name is normal in the
   observed discovery sequence; wait for the subsequent name result.
5. After startup has finished, send `tone` at 115200 baud. Once it finishes,
   send `play /audio/Polo.wav`. Confirm both by listening and inspect completion
   and `status`. A connected indicator alone is insufficient.

For this existing native partition layout, the tested build command was:

```sh
source /Users/cam/esp/esp-idf-v5.5.4/export.sh
idf.py -C firmware -DSKULL_JBL_GO2_EXPERIMENT=ON build
```

Use an **application-only** upload to the verified board, following
[the native firmware guide](../firmware/README.md#board-and-first-upload).
The tested application offset was `0x30000`. Do not copy that offset to an
unverified board or erase NVS as a routine pairing step.

For eventual normal use, the intended configuration route is to set only
`speaker_name=JBL GO 2` in that skull’s existing SD `config.txt`, preserving its
role, volume and other settings, and build with the experiment option OFF.
That route uses the same radio selection input but was not exercised here.
The old experiment option defaulted OFF and affected only secondary skulls.
It has now been removed; current source always takes the speaker name from SD.

## State after the initial experiment and firmware rollback

Immediately after the first experiment Jeff ran the GO 2 test application. His card still said
`JBL Flip 6`, with secondary role and 60% software volume. Anthony and Death were
not changed. Successful pairing saved the normal name-bound speaker cache;
the experiment did not erase Bluetooth bonds or other NVS data.

The pre-experiment application was verified against Jeff’s flash before the
test. Reflashing that saved application at the verified application offset
restores SD-based selection of Flip 6. Alternatively, rebuild with the option
explicitly OFF and application-only flash. No factory reset is required.

The final state at the top supersedes that state: SD again selects Flip 6 and
the override has been removed. Utility and startup source changes are retained on
`codex/jaw-articulation`; a pre-utility application rollback also removes those
new commands. To restore just the speaker after the demo, use the retained
utility rather than rolling back firmware.

Private logs, the original application and matching ELF, and the experiment
application and ELF are under `build/validation/jbl-go2-20261101/` in the active
worktree. The initial capture is
`build/validation/2026-11-01-jbl-go2-initial.log`. These are ignored local build
artifacts, not a remote backup. Preserve them with this report before deleting
the worktree or cleaning its build directory. Source and documentation are
retained on the working branch; build artifacts need separate preservation.

## Making the skull more self-contained

The proposed next step is a reversible dry fit with the complete speaker before
designing a mount. Check head clearance, jaw travel, access to the SD card and
electronics, and access to the speaker’s charging port, power, Bluetooth and
volume buttons. Keep the speaker removable for charging and maintenance.

Compare speech with the speaker outside the head, loosely positioned inside,
and then in a trial mount. Listen for muffling, shell rattles and interaction
with servo noise. Check balance and how the skull attaches to its support.
Battery life, charging access and the actual power-on procedure will determine
how self-contained the arrangement is; internal placement alone does not
establish those properties. No speaker disassembly or shared-power modification
was attempted or is needed to repeat the Bluetooth result.

Before adopting this for the 2027 show, qualify first-announcement audibility,
speaker and skull power cycles, speaker-off/late-power-on recovery, a full skit,
paired Anthony/Jeff playback and a representative outdoor run. Repeat the
acoustic checks after mounting, since the shell may change the result.

## Supporting evidence

See the [dated research and device evidence](research/2026-11-01-jbl-go2.md)
for source links, exact firmware hashes, connection timings and private capture
locations. The practical conclusion is to keep the GO 2 approach available for
next year: audible Bluetooth speech works, while internal mounting and reliable
startup behavior remain the next experiments.
