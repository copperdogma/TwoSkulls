# Speech jaw response — 2026-10-30

## Problem and bounded research

Cam reports that audio speech jaw motion is too small and smoothed, merging
successive syllables into a slow closing movement. Scope clarified to Anthony /
TwoSkulls only; no Death changes. The native `firmware/` implementation is the
active target; root Arduino animator is historical.

This is envelope tracking / cascaded low-pass latency. Native PCM already provides
20 ms RMS windows; the actuator then applies amplitude EMA alpha 0.1 followed by
jaw EMA alpha 0.2. At nominal 50 Hz their time constants are about 190 ms and
90 ms. Both stages blur onsets and inter-syllable dips; integer position feedback
also loses fractional progress.

Primary sources inspected:

- [Bram, envelope follower with different attack and release](https://www.musicdsp.org/en/latest/Analysis/136-envelope-follower-with-different-attack-and-release.html):
  one follower with separate rising/falling coefficients. Its published time
  convention reaches 1%, so do not confuse it with a one-time-constant duration.
- [MathWorks, Envelope Extraction](https://www.mathworks.com/help/signal/ug/envelope-extraction-using-the-analytic-signal.html):
  RMS window size trades fine ripple against lost envelope detail. Keep the
  existing 20 ms analysis rather than adding a new longer window.

## Decision and local evidence

Adopt a single follower on normalized RMS-derived jaw target, retaining fractional
state. Rising updates cover 85% of remaining distance per accepted control tick;
falling updates cover 70%. Below RMS 200 the target is closed; RMS 2400 reaches
full opening, versus legacy full-scale RMS 3000. The linear mapping between
these points preserves dips rather than compressing everything toward open.

These are an initial physical-test tuning choice, not values prescribed by the
sources. Keep existing 0..80 degree / 544..1368 us mapping, 50 Hz PWM, output
pending protection, idle breathing, peer role mask, and PCM delivery timing.
Non-speaking resets the speech follower and closes immediately during audio.
No phoneme detector or phrase-specific choreography is introduced.

Production driver host tests verify three consecutive syllable-like bursts:
first onset >=65 degrees within 20 ms, >=77 within 40 ms; a quiet gap closes to
<=3 degrees within 60 ms, even with speaking=true. Noise, moderate amplitude,
role-stop reset, saturation bounds and existing gesture/output cases pass.
All portable host suites pass with address/undefined-behavior sanitizers.
Independent lower-cost review found no actionable issue.

`scripts/jaw_preflight.py` reads real repository PCM, computes matching 20 ms RMS
windows, compiles/runs the production actuator source against SDK/time stubs, and
writes command traces and a summary. Example:

```sh
python3 scripts/jaw_preflight.py --output-directory build/validation/jaw-trial \
  'sd_card_files/audio/Initialized - Primary.wav' \
  'sd_card_files/audio/Initialized - Secondary.wav' sd_card_files/audio/Marco.wav
```

At 100% source volume, legacy/new commanded peak angles are 76/80 (primary
startup), 55/80 (secondary startup), 58/80 (Marco). Total command travel is
191/792, 160/380 and 95/292 degrees respectively. Increased travel demonstrates
more tracking activity; it does not by itself establish natural physical motion.

## Remaining uncertainty

Human observation must judge whether the faster response is sufficiently
articulated or too twitchy. Recorded energy peaks do not guarantee specific
openings at every written vowel. Physical servo speed, load, speaker buffering
and source level remain relevant. Jeff is not attached; shared-source host
coverage does not qualify his hardware or a paired show. Do not expand endpoint
travel to compensate for envelope filtering.

## Anthony physical result

2026-10-30: application-only flashed and verified, original application image
preserved with validated checksum/hash, SD recovered after a full cold power
cycle. Cam liked startup/Marco and then requested an extended local test.
`Skit - ju jubes.wav` completed 1710213 source frames with zero reported
underruns, actuator stuck=0 and no radio fault. Cam observed the jaw and said
"Looks good! Let's keep it." Keep this candidate as Anthony's accepted tuning.
This is human-observed motion plus device playback telemetry, not a measured
acoustic-offset or Jeff/paired-show qualification. No autoplay loop was added.

## Softer follow-up after extended observation

Cam later found the extra activity caused too much servo noise and requested
about 20% smoothing back. Interpret this as 20% less following response per
accepted control update: attack 0.85 -> 0.68; release 0.70 -> 0.56. Keep the
single-follower architecture, noise floor, full-open RMS and travel limits.
This changes requested movement; it does not assert a 20% reduction in sound.

Revised production-driver tests preserve distinct reopenings and reach >=50,
>=70, >=77 degrees after 20/40/60 ms of full-level input; a quiet speaking gap
closes to <=3 degrees in 80 ms. Full host suites PASS. Real-PCM driver preflight
retains the 80-degree peak, reducing commanded total travel versus the first
candidate: primary startup 792 -> 702 degrees, secondary 380 -> 356, Marco
292 -> 266. These are command traces; physical noise/motion acceptance remains
the owner's decision.

Softer image device check: flash hash verified; new ELF `6cd2543e7` boots,
mounts SD, reconnects the configured speaker, delivers startup PCM and completes
Marco with zero reported underruns. Radio fault=0 and actuator stuck=0. Normal
scheduling retained. Servo-noise improvement still needs human assessment.

### Final softer acceptance

Cam observed the softer Ju Jubes trial and on 2026-10-30 approved keeping it,
then requested validation and push. Retain attack 0.68 / release 0.56. Final
host sanitizer suites and native build pass; rebuilt binary SHA256 exactly
matches Anthony's flashed softer image. Independent final review found no code
issues. Human motion/noise suitability is accepted for Anthony; no numerical
decibel reduction or Jeff/paired hardware acceptance is claimed. Jeff is next.
