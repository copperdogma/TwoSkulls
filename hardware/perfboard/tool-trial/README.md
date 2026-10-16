# Perfboarder tool trial - NOT FOR BUILD

Created 2026-10-15. This is a tool qualification example, not the TwoSkulls perfboard design. Custom geometry and pin numbers are illustrative; do not wire hardware from this file.

Open https://app.perfboarder.com, open the project menu, select Import, then Perfboarder board .zip, and select `perfboarder-trial.zip`. This was exported and reopened successfully; board/kinds/parts/nets/keepouts/notes/build-progress data matched exactly. IDs and display name can change on import.

The PNG shows front and mirrored back views. Four parts, four nets, six wires; all final exported endpoints resolve and the app reports no problems. Its checker previously accepted nonexistent pin references in this trial, so external net verification is mandatory for a real design. See `verification.json` and the project research note `docs/research/2026-10-15-perfboard-layout-tools.md`.

Not verified: actual WROVER footprint/pin map, perfboard dimensions, USB-C breakout, capacitor/LED/diode selections, current capacity, insulation, mechanical clearance or physical behavior.
