# Agent guidelines


## Debugging native firmware

Read the root [README debugging guide](README.md#debugging-the-native-firmware)
before investigating hardware/runtime failures. Capture `status`, `fault-log`
and available SD `diagnostics/fault-*.log` evidence before rebooting or changing
firmware/card contents. Use the active `firmware/` implementation and its native
README; historical Arduino instructions refer to a different runtime. Keep raw
captures private, preserve unrelated work, and record experiments in the relevant
story. Follow the guide’s evidence-first, one-change-at-a-time process.
