# Story 004 — More expressive speech jaw

Status: Complete — Cam accepted the final softer Anthony tuning on 2026-10-30. Jeff is now flashed and boot/config checks pass; Jeff physical and paired-show qualification remain separate gates.

## Intent and acceptance

Cam wants wider and crisper syllable articulation: open promptly at speech
energy peaks, close quickly between them, and reopen on subsequent peaks.
The original mention of Death was corrected to TwoSkulls; Anthony is attached.
Keep servo endpoints, breathing, audio/peer/provisioning behavior intact.
Human visual/audio acceptance is required before completion. Jeff and paired
physical acceptance remain separate.

## 2026-10-30 work log

- Current main checkout is historical Arduino code. Located active native code
  on `codex/onboard-sd-migration`; it belongs to another chat, so created a new
  managed worktree / `codex/jaw-articulation` at that branch's committed head.
  Preserved unrelated research/perfboard edits in the original checkout.
- Read the native debugging guides and captured private serial evidence before
  the upload. Anthony reports primary, JBL Flip 5, volume 100%, original
  Oct 15 firmware with ELF prefix `003f1adab`; `fault-log` is unknown on that old
  build. Status showed SD mounted, idle show, no stream, radio fault=0 and
  actuator stuck=0. User then turned on the speaker for this trial.
- [Research and tuning decision](../research/2026-10-30-speech-jaw-response.md):
  replaced two slow EMAs with one fast fractional jaw follower; added noise
  floor / full-open RMS tuning. Active native implementation only.
- Added real-driver synthetic onset/gap/reopen/noise/role-stop checks and a
  reproducible PCM preflight utility. Full portable host suites PASS; real
  primary/secondary startup and Marco driver traces stay within legacy bounds.
- Independent lower-cost review: no actionable correctness issue found.
- Private captures and host/PCM reports are under ignored
  `build/validation/2026-10-30-*` in this worktree.
- Native build and application-only Anthony upload completed below. No commit or push.

### Build / upload preflight

- ESP-IDF 5.5.4 build PASS: app size `0x202b20`, 47% app partition free.
  Build log: `build/validation/2026-10-30-build.log`.
- Attempted read-only backup of existing app partition at `0x30000`, length
  `0x3d0000`, before any write. 460800-baud read reached the ROM/stub, identified
  base MAC `a0:a3:b3:80:16:38`, then failed with an invalid packet header. The
  USB device disappeared. No backup is claimed and no firmware was written.
- General problem class: serial transport loss/corruption. Inspected
  [Espressif esptool troubleshooting](https://docs.espressif.com/projects/esptool/en/release-v4/esp32/troubleshooting.html),
  which recommends reducing baud for packet corruption. Selected 115200 for the
  next bounded attempt; it could not open the now-absent port. Asked Cam to
  reconnect USB. Preserve failed logs rather than retry absent hardware.

### Reconnected USB attempt

- Cam reconnected Anthony. The serial device reappeared and a 115200-baud
  read-only app-partition backup began successfully on the same base MAC.
- After 331776 / 3997696 bytes (8%), pySerial returned `Errno 6: Device not
  configured`, and both `/dev/cu.usbserial-10` and its tty device disappeared.
  Read failed; no complete backup and no firmware writes. Failed capture:
  `build/validation/2026-10-30-backup-reconnected.log`.
- Lower baud did not prevent host USB device loss. Revisited the transport
  framing: this is now USB enumeration/connection loss rather than evidence
  against the jaw implementation. Espressif's troubleshooting identifies
  hardware/driver problems for pySerial errors and recommends a stable direct
  data connection. Await a different cable/direct connection rather than
  repeatedly starting flash writes over a disappearing device.

### Power-source clarification

- Cam identified that Anthony was powered by a battery pack whose low-current
  auto-shutoff was turning the board off. Cam restored power and requested
  another attempt. Treat the earlier disappearance as owner-reported supply
  shutdown, not demonstrated cable/driver failure.
- Retrying the read-only backup at 460800 baud with the restored supply, before
  application-only upload. Preserve earlier failed captures.

- With restored power the 460800-baud attempt kept its USB device but failed
  at 3604480 bytes with a truncated protocol packet (expected 0x1000,
  received 0xfa4). No write. This distinguishes a remaining fast-transfer
  integrity issue from the earlier battery shutdown.
- Selected the standard lower-baud remedy already researched: 115200. Back up
  0x220000 bytes starting at the factory app offset 0x30000, enough for the
  expected ~2.1 MiB current image, then require complete segment/checksum/hash
  validation before using it as a rollback image. This is an application image
  backup with padding, not a complete app-partition or full-device backup.

### Rollback image validated

- At 115200 baud, read 2228224 bytes successfully in 203.9 seconds. Esptool
  image-info validates all six segments, checksum and appended SHA256. Saved
  application ELF SHA256 is
  `003f1adabab1a027002db841756f4685d13b09c8dbb8d0253d8f71f4551d005a`,
  matching the pre-change boot evidence. Backup and SHA256 manifest live in
  ignored `build/validation/pre-jaw-tuning-20261030/`.
- Started application-only upload at 115200 baud to `0x30000`. New binary SHA256
  `fbfd2001be6115bb1d7bb2d66942c3237321980ab283abbd5631536fae8a17ec`.
  NVS, bootloader and partition table are outside the write range. This build
  also contains already-committed reconnect/logger changes inherited from the
  native branch; those had not been present on Anthony's older October 15 image.

### Application upload and first boot

- Application-only upload PASS at 115200 baud: 2108192 bytes to 0x30000 in
  128.4 seconds; esptool reports `Hash of data verified`. Boot reports the new
  ELF SHA256 prefix `291b8017a`; full build ELF SHA256:
  `291b8017a7aa2384e35fad3181bc183e60e28f5009680ce95b4b8eaea02c9cd8`.
- First boot initializes PSRAM and actuator outputs, then repeats SD OCR
  `send_op_cond` timeout 0x107 before config/radio startup. No audio test is
  claimed. Console `status` / `fault-log` are unavailable before the normal
  diagnostic task starts; preserved complete private boot capture instead:
  `build/validation/2026-10-30-jaw-after-boot.log`. SD diagnostics cannot be
  retrieved while mounting fails. Recovered fault report is retained; archive
  attempt reports errno 19. No card removal/format/file change.
- Revisited peripheral state across processor resets using the existing
  [SD soft-reset research](../research/2026-10-15-sd-soft-reset-recovery.md),
  plus Espressif's SD example documentation. Prior local OCR timeout recovered
  after full card power removal. This is an applicable bounded experiment,
  not proof that every 0x107 timeout has the same cause. Asked Cam for a full
  ESP32/card cold power cycle before further playback tests.

### Cold start / speaker gate

- Cam cold-cycled the board/card. SD mount recovered: 960 MB, primary config,
  JBL Flip 5, volume 100%, 15 WAVs / 11 scripts. Startup PCM loaded. Actual
  `status` / `fault-log` captured privately; pending recovered fault report
  includes the earlier SD timeout. Actuator stuck=0, pending channels=0,
  radio fault=0, no PCM played yet.
- Speaker was powered on per Cam, but saved-address attempts failed (ACL status
  260), then a full 10.24 s inquiry found no configured-name match. Requested
  a single Bluetooth-button press to make the configured Flip 5 discoverable.
  No bond erase, factory reset, speaker configuration or firmware change.
- Successive short captures each contain a power-on boot marker. To avoid
  serial-open side effects while observing Bluetooth, switched to one longer
  interactive monitor with the existing helper. pySerial's official API notes
  possible RTS/DTR glitches on open even with states set before opening:
  https://pyserial.readthedocs.io/en/latest/pyserial_api.html . This is a
  possible observation effect, not a proven diagnosis of the power-on markers.

### Earlier handoff before speaker / softer-trial acceptance

- New tuning is installed and verified on Anthony. Cold start mounted the SD
  card, loaded primary configuration and startup PCM. Speaker saved-address
  retries and discovery continue but no connection/PCM playback is established
  in this trial. Cam confirmed the speaker is on; Bluetooth-button / configured
  name discovery check is pending. No physical jaw/audio acceptance claimed.
- Final source checks pass; all original host suites and the native build had
  passed before upload. No further source changes after qualification. No
  commit/push, Death modification, Jeff flash, bond erase or card alteration.

### Speaker connection confirmed by Cam

- Cam pressed the speaker Bluetooth button and reported Anthony connected.
  This is owner-observed speaker connection; audible/visual jaw acceptance is
  still pending. Asked Cam to assess startup / Marco as more animated, still
  too smooth/small, or too twitchy/abrupt. Avoided reopening the serial monitor
  just to recheck the confirmed connection during the visual trial.

### Longer speech trial requested

- Cam reports the tuning looks good and requests a temporary automatic skit
  with mostly Anthony dialogue for a proper motion test.
- Selected `Skit - ju jubes.wav`: authored A intervals total 28.0 s versus B
  10.73 s (~72% A), with multiple extended A lines. Use the existing local-play
  console command once; no persistent auto-start/loop firmware change needed.
- Opening serial again coincided with a new power-on boot and speaker loss.
  The monitor stays open for the request/playback instead of reopening between
  commands. Asked for one Bluetooth-button press to restore the old link.

### Longer trial and human acceptance

- Cam restored speaker connectivity with the monitor held open. Startup PCM
  delivered and Marco completed with zero underruns. The card's actual Ju
  Jubes script was inspected before playback and matches the A/B timing used
  for selection.
- Sent the existing one-shot `play /audio/Skit - ju jubes.wav` command. Accepted
  at uptime 120941 ms, playing at 121071 ms, complete at 159881 ms:
  1710213 source frames, zero reported underruns. Anthony's A-role dialogue
  plays while B-role replies are muted. Status during/after the trial shows
  speaker=1, stream=1, radio fault=0, actuator stuck=0 and pending_channels=0.
  Marco resumed afterward under existing behavior.
- Cam observed the extended test and said: "Looks good! Let's keep it."
  Anthony's requested motion tuning is accepted. Preserve the current values:
  RMS floor 200, full-open RMS 2400, attack 0.85, release 0.70 per accepted
  20 ms tick. Legacy travel/PWM limits and idle breathing remain unchanged.
- One-shot playback used no temporary firmware/autoplay changes, so no test
  mode needs removal. No Death change or Jeff upload. No commit or push was
  requested in this exchange. Raw trial capture remains private at
  `build/validation/2026-10-30-jaw-ju-jubes.log`.

### Follow-up: reduce audible servo activity

- Cam subsequently reports excessive servo noise with the extra activity and
  requests about 20% smoothing back. Keep speech gain/endpoints, use 20% less
  target-following response per tick: attack 0.85 -> 0.68, release 0.70 -> 0.56.
  This is an explicit interpretation of the approximate request, not a physical
  noise-reduction percentage. Quieter real motion still needs human assessment.
- Preserved the accepted first-trial binary/ELF/flash args and SHA256 manifest
  under ignored `build/validation/accepted-jaw-20261030/` before rebuilding.
- Cam also requests command-triggered arbitrary skit tests. Verified installed
  console already implements `play <path>` (local, role-masked, one shot) and
  `pair <path>` (named paired test, normal readiness gates). Used that existing
  `play` implementation for the earlier Ju Jubes test; no skit was hardcoded.
  Added explicit paired-command and repeatable test instructions to the native
  README. No additional firmware playback mode is necessary.

- Follow-up full host suites PASS (sanitizers, 44 show-controller scenarios,
  existing storage/audio/radio coverage). Softer production-driver real-PCM
  traces preserve full travel and reduce aggregate primary startup command
  travel from 792 to 702 degrees; remaining noise needs a hardware observation.
- Pre-upload serial `status` / `fault-log` captured privately: SD mounted,
  show idle, no pending fault report, actuator stuck=0 / pending_channels=0.
  Speaker awaiting reconnection after serial-open boot. Earlier saved SD
  diagnostics remain on the card; card files were not removed or rewritten.

- Softer native build PASS; application-only Anthony upload at 115200 baud
  PASS, 2108192 bytes / 128.3 s, flash hash verified. New ELF SHA256:
  `6cd2543e7b65a3b6381e8fb8787005350f0cad6d3b9bcb51e11c33fe7889054a`.
  New binary SHA256 and coefficients captured in
  `build/validation/2026-10-30-jaw-softer-image.json`. Original and first-trial
  rollback images retained; no partition table/NVS/bootloader write.

- Softer device boot confirmed ELF `6cd2543e7`, SD mounted, primary configuration
  and startup PCM loaded, saved speaker reconnected and stream=1. Startup PCM
  delivered; Marco completed with 37675 frames / zero reported underruns.
  Status: pending fault=0, radio fault=0, actuator stuck=0, pending_channels=0.
  Normal show/Marco scheduling remains active. No forced skit/loop installed.
- This verifies the requested update and normal runtime, not an acoustic noise
  reduction measurement. Human softer-motion/noise acceptance remains open.

### Softer Ju Jubes trial

- Cam requests another Ju Jubes test. Kept one interactive serial session for
  the trial. After a serial-open boot and the user making the Flip 5 available,
  speaker connection and startup succeeded on the softer build.
- Sent `play /audio/Skit - ju jubes.wav`; accepted at uptime 144931 ms, playing
  at 145031 ms. This is a one-shot local A-role test, with Jeff's intervals
  silent. No firmware/config/autoplay change. Private live capture:
  `build/validation/2026-10-30-jaw-softer-ju-jubes.log`.
- Cam subsequently accepted this softer trial; see final qualification below.

### Final qualification and push authorization

- After observing the softer Ju Jubes trial, Cam said: "I think that's good!
  Validate, fix any issues, then push. Next we'll update Jeff."
- Final `python3 scripts/run_host_tests.py` PASS: all Python and production-source
  C++ sanitizer suites, including actuator onset/gap/reopen/noise/role-reset,
  saturation/pending-output behavior and 44 show-controller scenarios.
- Final ESP-IDF 5.5.4 native `idf.py -C firmware build` PASS. Built application
  SHA256 `b2aaa5163be6811b437e270bdb70afc5e03a5b0d125a770ff9b538201e65d013`
  exactly matches the softer image manifest already flashed to Anthony. No
  additional upload/reset is needed for this qualification.
- Independent final review found no code issue; corrected stale pending-human-
  acceptance wording in the story/index/research. Whitespace checks PASS.
- Softer-trial serial telemetry recorded zero underruns through the last sampled
  source frame 1419392, actuator stuck=0 and radio fault=0. The bounded monitor
  ended before the skit's final completion line, so final source-frame completion
  is not inferred from that capture. Cam's subsequent visual/audio acceptance
  establishes the requested physical result; it is not a measured noise level.
- Keep final attack 0.68 / release 0.56, RMS floor 200, full-open RMS 2400,
  existing 0..80-degree travel. No persistent autoplay or phrase-specific code.
  One-shot named local/paired playback commands already existed and are now
  explicitly documented. Normal scheduling remains unchanged.
- Scope for commit/push: Story 004 code/tests/preflight utility and documentation
  only, on `codex/jaw-articulation` descended from `codex/onboard-sd-migration`.
  Raw logs, backups, firmware binaries, SDK dependencies and unrelated changes
  remain outside Git. Jeff has not been flashed or physically qualified here;
  the shared accepted binary is ready for that next authorized session.

### Jeff update

Cam attached Jeff and requested the update. Identified secondary role / Flip 6 /
volume 60%, verified the original app exactly against the saved rollback image,
then flashed the identical Anthony-accepted binary application-only. Flash hash
and new boot/config/SD/PSRAM checks pass. See the
[Jeff update record](../validation/2026-10-30-jeff-jaw-update.md) for identity,
rollback verification, exact image and playback/physical acceptance boundary.

Jeff then reconnected to the Flip 6 and completed startup PCM delivery. Device
status has speaker=1 / stream=1 / radio fault=0. Physical jaw and coordinated
paired acceptance remain separate; no user observation of those is inferred.
