# TwoSkulls perfboard v3 — centered flush USB-C and flat underside parts

2026-10-16. Current review draft for both skulls. Supersedes v2 placement and routing; previous drafts remain preserved. No physical fabrication, rewiring or flashing performed. This is not yet a dimensioned cutting or assembly release.

- `assembly-plan.png` / `.svg`: physical arrangement, underside parts, socket section and proposed cable-body interference.
- `twoskulls-v3.perfboarder.zip`: actual editor export; open with Import → Perfboarder board at https://app.perfboarder.com/.
- `wiring.csv`: every connection and hole coordinate.
- `verification.json`: checks and remaining physical holds.

## What changed

The external power socket is centered south of the ESP32's programming USB, rather than off to the right. Its flange rests directly on the top of the perfboard. The connector's small PCB passes downward through a slot; it no longer sits on a raised cradle. The upward-facing opening rises only by the socket/flange's actual thickness above the perfboard; that exact height is unmeasured.

Short flexible leads join the socket V/G pads to J4's underside landing points north of the flange. Straight external5V and ground rails head north underneath the removable ESP32. All discrete components—D1 and C1/C2/C3—are on the perfboard underside. C1 and C3 lie flat with gently formed leads and separate mechanical support. Headers and the removable ESP32 remain accessible on top.

The antenna end is intended to project beyond the north edge. Its illustrated carrier offset/overhang is provisional; the official pinout does not establish the physical header-to-antenna offset. Dry-fit the real module before choosing the final edge position.

## Coordinates and placement

Coordinates are component-side column,row, counting from upper left. The physical underside is mirrored when you turn the board over. The assembly drawing deliberately shows underside bodies **through** the board using the component-side coordinate system. Perfboarder's front/back labels do not encode which side these custom parts are mounted on; use this table and companion drawing for assembly side.

Proposed blank remains26×32 holes, 2.54mm pitch, approximately67.78×83.02mm using the editor's margins. Header-row spacing is provisionally25.4mm. Neither blank nor carrier spacing has been physically measured.

| Ref | Electrical holes | Physical arrangement |
| --- | --- | --- |
| J1 | Left col8, right col18; rows1–20. | Top, raised removable sockets; antenna north, programming USB south. |
| J2 | (1,7) GPIO32/left eye; (1,8) GPIO33/right eye; (1,9) ground. | Top harness header. Keep the existing shared100Ω eye-harness resistor. |
| J3 | (26,21) GPIO23 servo signal; (26,22) external5V; (26,23) ground. | Top/right header; check actual mating-plug order. |
| J4 | (12,23) external5V, (14,23) ground. | Underside wire landings only. These are NOT the USB socket's physical terminals or footprint. |
| D1 | Anode(6,24), cathode/band(6,28). | Underside, candidate1N5819. Band points south/down in component-side coordinates, toward the controller supply connection. |
| C1 | +(12,16), −(14,16). | Underside470µF ≥10V, laid north; body assumed10mm diameter ×16mm length. |
| C2 | (12,18) to(14,18), nonpolar. | Underside100nF ceramic; body assumption6×3mm in plan, height6mm. |
| C3 | +(3,20), −(5,20). | Underside100µF ≥10V, laid north; body assumed6.3mm diameter ×12mm length. |

C1's illustrative body spans cols11.03–14.97, rows9.2–15.5; C3 spans cols2.76–5.24, rows14.8–19.5. These envelopes include no claimed measured lead-forming dimensions. Actual capacitor body, lead pitch and bend radius must replace these assumptions before assembly. No capacitor bodies overlap under the stated assumptions.

## Flush socket and cutout

The Lord of the Tools180-EP flange is nominally20.1×6.7mm. The proposed flange centre is(13,26), with its long axis across the board. Its nominal plan envelope spans cols9.04–16.96 and rows24.68–27.32. The upward-facing opening is directly south of the ESP32's programming USB.

The dashed slot region in the drawing is an **illustrative planning area**, not a cut template. Actual PCB thickness, width, onboard components, solder pads and wire clearance are not measured. Cut only after measuring the actual unit and checking that its flange spans the opening with adequate supporting board material. Leave the flange supported; retain it with an appropriate bond or mechanical retainer so insertion/removal loads do not reach the tiny PCB pads. Retention hardware/adhesive is not selected here.

No routed wire enters the illustrative slot keep-clear box (cols11–15, rows25–27). Rails stop at J4 on row23. The controller supply return route stays west of the flange/slot. A larger real slot could require rerouting. Slot dimensions, flange-hole centres, clearances and cutting effects are not encoded in Perfboarder `keepouts`; the companion plan carries these constraints.

After any eventual cutting, remove burrs/loose copper and verify isolation before powering. No board has been cut during this work.

## Cable interference

The intended obstruction is between the **inserted cable bodies**, not the two empty receptacles. Orange dashed bodies show a conceptual horizontal ESP32 programming plug overlapping the vertical external-power plug. This is not a proven interlock: actual ESP32 socket height, plug length/width, cable strain relief and insertion paths must be tested in both orders. Different low-profile/right-angle cables can change the result. No fabricated connector spacing is claimed to prevent simultaneous connection.

The normal workflow now intended by Cam is one power/programming cable at a time. Disconnect external power before using ordinary programming USB. Earlier discussion of a VBUS-cut cable remains an available separate technique, but this draft does not rely on concurrent plugs or claim electrical dual-source isolation for the Mac.

## Underside components and risers

C1 lies over the underside bus-wire region. Use an insulating saddle/support so its body cannot rub or press against solder joints or rails. Do not treat its outer jacket as the sole insulation, or its leads as its mechanical support. Keep end seals/vents unobstructed, avoid stressing lead entry points, and form leads before soldering rather than pushing a soldered capacitor down afterward. Support method/material and actual parts remain to be selected.

[Nichicon's mounting guidance](https://www.nichicon.co.jp/english/products/pdf/aluminum.pdf) advises forming leads before soldering without stressing the capacitor, checking lead spacing, and avoiding problematic circuit/body/end-seal placement. This supports the construction constraints; it does not qualify these anonymous stock capacitors, support materials or this handwired assembly. This is hand soldering, not a wave-solder process.

A flat round capacitor projects approximately its full diameter below the board, plus support and wiring clearance. Riser clearance must exceed the greatest of capacitor/support depth, connector PCB/tail/wire depth, and other underside components, with additional allowance for shell flex. C1's provisional diameter is10mm and C3's6.3mm; the connector tail could be deeper. No final riser height is specified.

Proposed board mounts remain(2,3), (2,31), (25,31), with4mm-radius reserves. The centered flange clears these nominal reserves. Use separate broad standoff feet so the board remains flat against the curved skull; adhesive must accommodate the actual shell material and flex. The socket's exact retention and the feet's bond are still physical choices, not demonstrated attachment strength.

## Wiring and power boundaries

External regulated5V feeds the servo directly and the ESP32 through D1; anode on external rail, band/cathode toward ESP32. C1/C2 bridge the external rails, C3 is after D1. Ground is common. Servo current must return through the substantial external rails, not through a WROVER header or narrow controller branch. GPIO14/15/2 remain reserved for onboard SD.

Use adequately sized insulated underside wiring; expose conductor only at intended joints. Five cross-net intersections are insulated jumps, never solder bridges: (3,21), (8,21), (8,23), (12,21), (14,22). They are arched in the companion drawing and listed in `insulated-crossings.json`. Routing lines crossing other holes do not imply solder joints.

D1 blocks the direct controller5V-to-servo-rail feed during USB programming. It does not establish zero leakage, all GPIO phantom-power prevention, or ordinary USB plus external-power safety. Actual controller voltage after D1, servo-off behavior, source/cable capacity and noise remain electrical load tests. Use the power socket's V/G only; D+/D− stay unused. Check actual CC sink terminations before relying on USB-C-to-C power, without adding duplicate resistors if already present.

## Verification and next fit gate

Actual browser import/export preserved the board geometry, custom footprints, placements, all six connected net graphs and28 wire segments. All56 pin references resolve. Independent review confirmed the same terminal memberships as earlier drafts, correct GPIOs/diode/cap polarity, no line through a foreign pin or explicit junction, no mutual capacitor-body overlap and no route into the illustrative slot box.

Four editor findings are recorded: unused WROVER power pins, a rail junction inside J1's bounding box, and C1/C2 overlapping J1 in plan. These reflect the deliberately different physical sides/levels: rails and capacitors beneath the perfboard, removable module above. Perfboarder does not model those assembly layers. They are not proof of either a collision or adequate clearance; verify the actual stack.

Next: physical dry fit with the actual blank, ESP32/header pair, socket and both cables. Measure the slot/tail and capacitors, confirm antenna overhang and SD access, test both plug orders, and size risers/supports from the real underside stack before cutting or soldering.
