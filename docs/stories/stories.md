# Implementation stories

| Story | Status | Next gate |
| --- | --- | --- |
| [001 — Single-WROVER Anthony prototype](story-001-single-wrover-matter-prototype.md) | Identical image; all 11 paired skits clean, Marco/Polo ordering, Home and bounded recovery demonstrated | Measured acoustic timing, Jeff actuators, primary resource margin and yard/reset limits |
| [002 — Continuous speaker reconnect](story-002-continuous-speaker-reconnect.md) | In Review | Paired-device delayed-speaker acceptance |
| [003 — Retained fault logs](story-003-retained-fault-logs.md) | In Review | Both-role device export and paired playback acceptance |
| [004 — Expressive speech jaw](story-004-expressive-speech-jaw.md) | Complete — softer Anthony tuning accepted | Jeff flashed; physical and paired qualification remain separate |
| [006 — Lifecycle fault context](story-006-lifecycle-fault-context.md) | Jeff SD export/readback/reboot passed | Anthony and speaker/paired checks |
| [007 — Home Assistant diagnostics and skits](story-007-home-assistant.md) | Jeff HA/MQTT validated; In Review | Positive audio and Anthony/paired hardware |
| [008 — Missing-speaker eye warning](story-008-speaker-eye-warning.md) | Implemented; host tests and Jeff boot warning passed | Connection-loss visual check and Anthony |
| [009 — Mac Mini whole-house HA hub](story-009-mac-mini-home-assistant-hub.md) | Live; Mini/Jeff UI, logs and service recovery passed | Phone URL, addressing and host power/reboot qualification |
| [010 — SD jaw calibration and HA tuning](story-010-sd-jaw-calibration.md) | Complete for Jeff; calibrated 654/1404 us saved and owner-verified after reboot | Anthony deployment/calibration; paired qualification |

Supporting [research record](../research/2026-10-14-single-wrover-research.md).
The earlier [Arduino onboard-SD migration](../onboard-sd-migration.md) is a separate
baseline in this worktree: onboard SD/config was observed; its original audio
and full-show acceptance remain separate from native local-playback proof. See
the [current application port evidence](../validation/2026-10-15-native-application-port.md).

Follow-up [catalogue and recovery qualification](../validation/2026-10-15-code-qualification.md) records the failed experiments, fixes, final-image proof and remaining limits.
