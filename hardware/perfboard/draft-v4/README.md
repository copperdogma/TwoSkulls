# TwoSkulls perfboard v4 — northern servo and antenna clearance

2026-10-16. Current planning draft; supersedes v3 routing/placement and preserves it for reference. No cutting, soldering, rewiring or flashing. Physical dimensions, RF performance and load behavior remain unverified.

Open `twoskulls-v4.perfboarder.zip` with Import → Perfboarder board at https://app.perfboarder.com/. Use `assembly-plan.png` / `.svg` for physical side/layer interpretation. `wiring.csv` lists all segments; `mechanical-check.json` records the nominal clearance calculation and assumptions.

## Changes

Servo J3 moves to column26, rows6–8: signal6, external5V7, ground8. Under the assumed mating-plug envelope and NE reserve, this is the northernmost whole-row origin that fits: row5 collides with the NE access reserve; row6 leaves approximately1.19mm.

The signal polyline shrinks from approximately68.6mm in v3 to30.5mm. Power and ground branch from the northern portions of the underside rails using two insulated flying links across the controller socket area. C1 moves5.08mm north, with its positive/negative leads at row14 and its flat body extending north. C2 moves to row16. Central rails now begin at row8 (+5V) and row9 (ground), south of the modeled antenna clearance region.

Keep the servo mating plug and cable east of the board, or route the cable south after it exits. Do not loop the harness north across the antenna. Exact housing, cable bend and service-removal space must be checked with the actual connector.

## RF model and applicability

[Espressif's module-positioning guidance](https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32/pcb-layout-design.html#general-principles-of-pcb-layout-for-modules-positioning-a-module-on-a-base-board) recommends antenna overhang, housing clearance of at least15mm around the antenna, and final communication testing. V4 applies a rounded15mm offset around an illustrative antenna rectangle. This is a planning target, not an RF acceptance claim.

The [WROVER-E datasheet](https://documentation.espressif.com/esp32-wrover-e_esp32-wrover-ie_datasheet_en.html) gives an18mm module width. V4 uses the full18mm width as the antenna envelope, nominally centred at column13. Its north/south edges remain assumed at rows−1.5/0.9; actual Freenove header-to-antenna offsets are unmeasured. V3's antenna illustration used the carrier width; v4 distinguishes the narrower module antenna from the carrier.

Nominal separations from that antenna rectangle:

| Item / assumption | Separation |
| --- | ---: |
| Servo mating housing, assumed4mm across ×10mm along the pin row | 24.39mm |
| Reserved NE mount/access disk, centre(25,3), radius4mm | 18.13mm |
| Existing NW mount/access disk, centre(2,3), radius4mm | 15.68mm |
| C1 north body edge at row7.2 | 16.00mm |

The NW margin is only0.68mm beyond the target, and C1's only1mm. Actual antenna position and component sizes can invalidate those margins. The housing calculation includes its body, not only pin centres. Required soldering-tool and cable-removal clearances are not measured.

GPIO23 is fixed near the top of the existing carrier, inside this15mm planning buffer. Its necessary short escape wire initially occupies that buffer; it runs away along the carrier side, never under the illustrated actual antenna area. This is an explicit limitation, not a silently ignored rule or a claim that all added wiring has15mm clearance. Confirm wireless operation with the complete harness and skull installed.

## NE standoff interpretation

V3 drew three mounts: NW(2,3), SW(2,31), SE(25,31). To honor Cam's NE-standoff constraint, v4 additionally reserves a4mm-radius NE mounting/tool-access disk at(25,3). It is drawn dashed. This does not mandate a fourth rigid foot on a curved flexible shell; the existing three mounts remain available. Choose the actual support set after dry fitting and keep the board flat. Standoff material, screw heads, washers and tool size are unmeasured.

## Component-side coordinates and actual mounting side

Columns count from the left and rows from the top. On the real underside the view is mirrored. The companion plan deliberately shows underside parts through the board using component-side coordinates.

| Ref | Electrical holes | Mounting |
| --- | --- | --- |
| J1 | Col8 and18, rows1–20 | Top, removable raised ESP32 sockets; antenna north, programming USB south. Assumed25.4mm row spacing. |
| J2 | (1,7) GPIO32/left; (1,8) GPIO33/right; (1,9) GND | Top eye harness header. Existing shared100Ω resistor retained; per-LED redesign is separate work. |
| J3 | (26,6) GPIO23/SIG; (26,7) external5V; (26,8) GND | Top servo header. Confirm actual mating plug order/envelope. |
| J4 | (12,23) external5V; (14,23) GND | Underside wire landings, not the actual USB socket footprint. |
| D1 | A(6,24); K/band(6,28) | Underside1N5819 candidate; band south toward controller supply. |
| C1 | +(12,14), −(14,14) | Underside470µF ≥10V, flat north, assumed10mm diameter ×16mm body. |
| C2 | (12,16), (14,16), nonpolar | Underside100nF; assumed6×3mm plan envelope and6mm height. |
| C3 | +(3,20), −(5,20) | Underside100µF ≥10V, flat north, assumed6.3mm diameter ×12mm body. |

C1's body spans approximately cols11.03–14.97 and rows7.2–13.5. C3 retains the v3 body envelope. C1 rests on a separate insulating support above the underside rails; its leads carry no mounting load. Actual capacitor sizes/pitches, end-seal/vent clearance, support material, socket tail depth and shell flex determine the required riser height. A flat cap needs approximately its full diameter plus support/wire clearance.

The blank remains a proposed26×32 grid on2.54mm pitch, approximately67.78×83.02mm with editor margins. These are not measured stock dimensions.

## Flying links and electrical boundaries

The external5V branch uses `(17,8) → (19,9)` and the ground branch `(17,10) → (19,11)` as insulated flying leads. These cross the socket area diagonally in plan, with the physical wires held clear **below the exposed socket pin ends and solder joints**. They are not bare bridges or a claim that an insulated wire comfortably fits between2.54mm solder pads. Retain and insulate them; verify their vertical clearance in the actual riser stack. The drawing's layer inset explains this requirement.

Perfboarder records the links' endpoints but may display its own rounded/orthogonal route. The companion plan specifies their intended physical route. Do not use automatic rerouting as an assembly instruction, or change the flying links into solder tracks. Other arched crossings are unjoined insulated crossovers, listed in `insulated-crossings.json`.

External regulated5V feeds the servo directly and the ESP32 through D1. Anode is on the external rail; cathode/band toward controller5V. C1/C2 bridge external rails, C3 is after D1. Common ground; servo current must not flow through the controller's header or narrow feed. GPIO14/15/2 remain onboard-SD only.

Use ordinary programming USB with external power disconnected. The single diode blocks direct USB-fed controller5V from energizing the servo rail; it does not establish ordinary USB plus external-source protection for the Mac or prevent all GPIO phantom power. Actual load, voltage after D1, source/cable capacity, USB-C CC terminations and servo-off behavior remain electrical checks.

## Retained v3 mechanical arrangement

The power USB-C socket remains centred at(13,26), opening upward, flange flush on top, small PCB through a measured-later slot. Nominal flange20.1×6.7mm runs across the board. The dashed planning slot is not a cutting template. Flange retention, actual PCB depth, cable-body interference and cutting effects remain unverified. Its leads run north to J4; routed wires avoid the illustrative slot area. Capacitors/diode remain underneath, ESP32/headers above. See [v3 assembly notes](../draft-v3/README.md) for unchanged flange, capacitor-support and cutout details; v4 coordinates/routing above take precedence.

## Verification

All56 pins resolve; six connected net graphs preserve earlier terminal membership;36 segments have no foreign pin or explicit-junction pass-through under the specified geometry. Independent review confirmed polarity, pin map and nominal mechanical calculations. Local search over rows1–6 confirms row6 as the first noncolliding mating-body position relative to the NE disk, under stated assumptions.

The actual editor import/export was checked against the generated geometry/circuit. Four editor findings remain: intentional unused WROVER power pins, an underside rail junction inside J1's box, and C1/C2 below J1. The editor does not model those assembly sides/vertical levels or physical flying-link clearance. Keepouts/notes remain external in the companion drawing and README.

Next fit gate: measure carrier/header/antenna geometry, actual plug and screw/tool envelopes, inspect the flying-link layer, and dry-fit the unchanged flush socket/cables and underside stack. Adjust coordinates if needed, then test RF and power behavior before claiming build acceptance.
