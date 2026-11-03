# Agent guidelines

## Home Assistant integration and operator UI

Read [the AI prop-integration guide](docs/home-assistant-prop-integration.md)
when implementing or maintaining Home Assistant features. Cam's approved pattern
is a native dashboard with a grouped per-prop selection/Play/Now playing card and
a separate status/logs card, set as the useful default. Raw discovered entities
and generic history dialogs are not the finished operator interface.
Update that guide, the dashboard example and relevant setup documentation when
changing the integration, protocol, UI, provisioning or qualification. Keep
reusable behavior distinct from TwoSkulls-specific code and tested evidence.


## Debugging native firmware

Read the root [README debugging guide](README.md#debugging-the-native-firmware)
before investigating hardware/runtime failures. Capture `status`, `fault-log`
and available SD `diagnostics/fault-*.log` evidence before rebooting or changing
firmware/card contents. Use the active `firmware/` implementation and its native
README; historical Arduino instructions refer to a different runtime. Keep raw
captures private, preserve unrelated work, and record experiments in the relevant
story. Follow the guide’s evidence-first, one-change-at-a-time process.
