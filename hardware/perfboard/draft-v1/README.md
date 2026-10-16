# TwoSkulls perfboard draft v1 — fit check required

2026-10-15. Planning artifact for both skulls; no hardware built, rewired or flashed. Editable project: `twoskulls-v1.perfboarder.zip` (Import → Perfboarder board in https://app.perfboarder.com/). The ZIP was downloaded from the actual editor and its project data compared with `twoskulls-v1.json`. See `mechanical-concept.svg` for mechanical reservations **not encoded in Perfboarder's keepouts**.

## Layout and orientation

Component side, antenna at top, WROVER programming USB at bottom. Coordinates are column,row, counted from the upper-left hole. The solder-side view is mirrored; do not copy component-side coordinates visually onto the underside.

Draft blank: 26 columns × 32 rows on a 2.54 mm grid. With the editor's 2.14 mm margins, the envelope is 67.78 × 83.02 mm. This is a proposed board, not a measurement of existing stock. J1 header-row spacing is provisionally 25.4 mm; verify against the actual Freenove carrier before soldering.

| Ref | Part | Component-side placement / connection |
| --- | --- | --- |
| J1 | Freenove ESP32-WROVER v3.0, socketed | Left row col8, right row col18; rows1–20. Antenna up, USB down. |
| J2 | Eye harness | (1,7) GPIO32 left, (1,8) GPIO33 right, (1,9) ground. Retain existing shared 100Ω harness resistor. |
| J3 | Servo header | (26,12) GPIO23 signal, (26,13) external +5V, (26,14) ground. Verify actual plug order. |
| J4 | Flying leads from USB-C socket | (20,23) +5V, (20,24) ground. These are solder landings, not the socket's physical footprint. |
| D1 | Candidate 1N5819 Schottky | Cathode/band at (5,21), anode at (5,25); 10.16 mm lead pitch. |
| C1 | Servo bulk capacitor | 470µF, ≥10V; + at (23,19), − at (25,19). Proposed body ≤10 mm diameter; confirm stock. |
| C2 | Servo bypass | 100nF ceramic, (23,9) to (25,9); provisional 5.08 mm pitch. |
| C3 | Controller bulk capacitor | 100µF, ≥10V; + at (2,28), − at (4,28). Proposed body ≤6.3 mm diameter; confirm stock. |

GPIO order was checked against the official Freenove v3 pinout in the hardware dossier, not the arbitrary pin numbering in the earlier tool trial. Onboard SD pins14/15/2 remain unused externally. No additional tall parts beneath the WROVER or its SD access. Reserve cols10–16, rows21–32 for programming-cable access. Verify actual carrier outline, antenna overhang and cable access by dry fitting.

## Power and hand wiring

External regulated5V → servo directly, and → D1 anode → D1 cathode/band → WROVER5V. All grounds common. The servo's load current must not pass through the WROVER. Run short adequately sized insulated power and ground wires; route the servo's power/return branch back toward the input, rather than through controller wiring. The drawing is a hole-level wiring plan, not a copper-track design.

All crossing wires are insulated. Only named same-net terminals/junctions are joined. Intermediate holes crossed by a line are not solder connections. `wiring.csv` lists every endpoint and hole. Insulate exposed diode leads as necessary. C1/C2 connect to the external rail; C3 is after D1. Caps reduce local disturbances but cannot compensate for an inadequate supply/cable.

D1 blocks the direct WROVER5V-to-servo-rail path when programming with external power off. Candidate1N5819 voltage drop must be checked with the real controller load, especially its5V-fed peripherals. This does not prove zero leakage or prevent all GPIO phantom powering. Check servo behavior with external power off; signal isolation can be added if that test exposes a problem.

Programming workflows: ordinary USB with external perfboard power OFF; or Cam's VBUS-disconnected USB cable while external power remains ON. The perfboard diode does **not** establish that ordinary USB plus external power simultaneously is safe for the Mac. Do not claim dual-source USB isolation from this layout.

USB-C socket: Lord of the Tools180-EP, documented flange20.1 ×6.7 ×1.5 mm. Use V/G only, leave D+/D− unused. Verify CC1-to-ground and CC2-to-ground each have the appropriate approximately5.1kΩ sink termination on the actual unit; do not add duplicate pull-downs if already present. Source/cable current capacity and servo load remain to be checked.

## Socket and skull mounting

Cam's latest choice supersedes the initial edge-entry preference: **USB opening faces up; the cable enters perpendicular to the perfboard.** The tiny connector PCB hangs downward from its flange. Support the two-hole flange on a raised cradle, with flexible insulated leads to J4; its solder pads are not structural attachments. Add cable strain relief to that support.

Reserve roughly cols20–26, rows22–32 for a provisional cradle envelope. Do not drill its flange holes from this drawing: actual hole centres, PCB depth, screw clearance and required elevation are unmeasured. Listed nominal2 mm flange holes do not establish M2 screw clearance; check screw fit.

Reserve three board mounting centres at (2,3), (2,31), (25,31), each with a4 mm radius for screw head, washer and tool access. These are proposed drill locations requiring the actual blank board's edge and hole geometry to be checked. The lower-right mounting region also supports the cradle: design them together, rather than stacking conflicting independent screws. C3 has little spare clearance under its provisional body size.

Use three separate broad standoff feet against the curved skull, independently positioned so the board remains flat. Screw the board to the standoffs; bond the feet to the shell with a compliant interface compatible with the actual plastic. Do not bridge the curved flexible wall with a long rigid bonded block. Identify the shell plastic and test adhesion on a hidden area before choosing adhesive. Foam tape/elastic adhesive performance depends on substrate, preparation, load, temperature and peel; no specific adhesive has been qualified here.

## Verification and next fit gate

Eight parts, six connected nets, 56 valid part pins and38 orthogonal wire segments. Downloaded ZIP matches the JSON; actual edges match intended paths. Independent review found no remaining line through a foreign pin/junction and confirmed diode polarity, capacitor polarity and GPIO mapping. Perfboarder reports one soft warning for intentionally unused WROVER supply pins, with no hard findings. These results establish drawing consistency, not physical/electrical acceptance.

Before a build-ready revision, measure the blank's hole count/usable area, WROVER row spacing/outline, capacitor bodies/leads, and USB flange hole centres/PCB depth. Dry-fit board and plug inside the skull. Then finalize cradle and standoffs, update geometry, and perform continuity/power checks before plugging in the controller. The practical next action is a paper-scale or unsoldered placement check with the actual parts.

`mcp-log.json` is a historical creation/iteration log, including superseded references and routes. It is not the final assembly specification; use the saved project, wiring CSV and notes. Later drafts supersede this revision.
