# TwoSkulls perfboard draft v2 — straight rails

2026-10-16. Supersedes v1 placement/routing. This is a reviewable design draft, not a fabrication release. Both skulls use the same proposed board. No soldering, rewiring or flashing performed.

Open `twoskulls-v2.perfboarder.zip` via Import → Perfboarder board at https://app.perfboarder.com/. `assembly-plan.png` / `.svg` show the mechanical arrangement; `wiring.csv` gives every electrical segment. The editor's footprint for J4 is explicitly **wire landings**, not the physical USB socket.

## Changes from v1

- Straight external +5V rail in column9 and common-ground rail in column11 run under the removable ESP32. All rail wiring is on the **underside of the perfboard**, not loose between the ESP32 and its sockets.
- The external positive rail runs rows6–28; ground runs rows6–30. Both stop short of the antenna end. The actual antenna boundary remains a fit/RF hold.
- Servo connector moved to the lower right: its power and ground branches run horizontally from the rails below the ESP32's socket pins.
- C1 and C2 bridge the parallel rails below the ESP32 body. Their bodies stay outside the proposed central programming-USB corridor.
- D1 now has its band/cathode **DOWN**. Follow the v2 polarity and coordinates, not the v1 drawing.
- USB socket is drawn in both plan and side view, elevated on a bracket, with its opening facing up and cable moving down perpendicular to the perfboard. Its small PCB hangs below the horizontal flange. Flexible wires reach J4; the flange/bracket carries cable forces.

## Coordinates and parts

Coordinates below are component-side column,row, starting at upper left. Perfboarder displays A–Z across: column9=I, column11=K. Its back view is mirrored. Use the wiring CSV when working from the underside.

The blank remains a proposed26×32 grid, 2.54mm pitch, approximately67.78×83.02mm with the editor margins. Actual blank dimensions and WROVER socket-row spacing (assumed25.4mm) are not measured.

| Ref | Placement and polarity | Provisional body assumptions |
| --- | --- | --- |
| J1 | Left socket col8/right col18, rows1–20; antenna up, programming USB down. Removable Freenove WROVER v3.0. | Verify carrier outline, underside components, header spacing, SD access and socket height. |
| J2 | (1,7) left eye/GPIO32; (1,8) right eye/GPIO33; (1,9) ground. | Existing shared100Ω eye-harness resistor retained. |
| J3 | (26,23) servo signal/GPIO23; (26,24) external5V; (26,25) ground. | Confirm plug order and actual header/mating-plug body. |
| J4 | (20,28) external5V; (20,29) ground. | Wire solder landings only; actual raised socket shown separately. |
| D1 | Candidate1N5819: anode(6,22); cathode/band(6,26). | 10.16mm lead pitch; DO41 stock identity/voltage drop still to qualify. |
| C1 | 470µF ≥10V: +(9,28), −(11,28). | ≤10mm diameter, height16mm assumed. |
| C2 | 100nF ceramic: (9,25) to(11,25), nonpolar. | Top-view envelope6×3mm, height6mm assumed. This bypasses the rail supply point; noise at the actual servo still needs testing. |
| C3 | 100µF ≥10V: +(2,23), −(4,23). | ≤6.3mm diameter, height12mm assumed. |

Nominal C1–C2 gap is about1.12mm with these body assumptions. C1 clears the proposed programming-cable corridor (cols12.5–16.5, rows22–32) by about1.35mm. These small margins require measurement. Capacitors are not placed beneath the controller. GPIO14/15/2 remain reserved for onboard SD; no new external connections.

## Electrical construction

External regulated5V feeds the servo directly and feeds the ESP32 through D1. Diode anode is on the external rail; band/cathode feeds J1's5V pin. Ground is common. C1/C2 are across the external rails; C3 is across the controller supply after D1. The servo current must not flow through a WROVER header or the controller's narrow supply/ground leads.

Use short, adequately sized insulated bus wires on the underside, exposing conductor only at intended solder joints. A drawn crossing is **not** a joint. `insulated-crossings.json` lists the five cross-net intersections: (8,21), (8,22), (9,21), (11,24), (18,25). The assembly drawing uses arches at these locations. Keep insulation between crossing wires/rails and between wiring and mounting hardware. Do not implement the horizontal/vertical crossings as bare solder bridges.

C1's position across the rails makes assembly straightforward; it does not compensate for an inadequate5V source or a long/thin servo cable. Select actual wire, capacitors and supply for the real load; verify controller voltage after the diode during operation. The exact capacitor stock and supply current are not qualified by this drawing.

Programming: external power OFF with an ordinary USB cable, or external power ON with Cam's VBUS-disconnected cable. D1 blocks the direct ESP32-5V-to-servo-rail path. It does not establish ordinary-USB/external simultaneous-source protection for the Mac, zero leakage, or prevention of GPIO phantom powering. Test servo-off behavior separately.

Use the Lord of the Tools180-EP socket's V/G pads only for power. D+/D− stay unused. Check the actual unit's CC1/GND and CC2/GND sink terminations before relying on USB-C-to-C power; do not add duplicate resistors if already fitted. Existing hardware-dossier uncertainty about exact purchased-board protection remains.

## Mechanical arrangement

The USB flange is horizontal, its opening faces up, and its plug axis is perpendicular to the board. The bracket supports the flange; the tiny PCB and its solder pads carry no mechanical attachment load. Add cable strain relief to the bracket. Its height must clear the connector PCB, solder joints and flying wires. No final bracket height or hole centres are specified.

The revised illustrative bracket reserve is roughly cols19.3–24.7, rows22–31.7, approximately14×25mm. This narrows the v1 reserve to leave the servo connector at col26 accessible. The nominal20.1×6.7mm flange is drawn with its long axis along the rows, offset left within that bracket. The actual mounting hole centres and screw clearances remain unknown. The socket drawing is a placement concept, not a drill template.

Board mounting centres remain (2,3), (2,31), (25,31), each with a4mm radius reserve. The lower-right support shares the bracket region: design those parts together. The flange is deliberately offset left in the concept, but screw heads, washers, tools and plug removal still require a physical check. Do not drill from an unverified drawing.

Use separate broad standoff feet against the curved skull so the board stays flat. Screw the board to the standoffs and bond the feet through a compliant interface suited to the actual shell plastic. Adhesive, shell flex, temperature and insertion load are not qualified. No exact adhesive or fabricated bracket is selected here.

Mechanical reservations live in the companion drawing and these notes; Perfboarder's `keepouts`/`notes` arrays remain empty. The editor cannot establish the actual vertical clearance of the raised ESP32, underside wiring or bracket.

## Verification and next action

The generated file was imported into the actual editor and exported again. Geometry, footprints, placements, net memberships and all32 wire segments matched. Import changed project identity/timestamp, and the display was set to columns counting from the left. All56 part pins resolve; six net graphs are connected. Independent review confirmed unchanged electrical terminal membership from v1, correct GPIOs and diode/capacitor polarity, with no segment through a foreign pin or explicit junction.

Two editor findings remain: intentionally unused WROVER power pins, and J1 overlapping a rail junction beneath it. The latter reflects the planned underside rail and raised module, which this editor does not model as separate physical layers. Neither explanation establishes actual mechanical clearance.

Next: place actual sockets, controller, capacitors and socket/flange on the intended blank without soldering. Check board fit in the skull, programming-plug and SD access, capacitor envelopes, flange hole centres/depth and bracket/standoff clearances. Update the geometry from those measurements before fabrication or wiring.
