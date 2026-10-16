# Perfboard layout tool selection - 2026-10-15

## Scope and decision

Cam asked to research a better tool before beginning the new perfboard layout in installed KiCad. This pass evaluates the general problem: placing socketed modules and hand wiring on a 2.54 mm hole grid, including buildable wire paths, underside orientation, connector access and mechanical keepouts. It does not create a final board or validate electrical behavior.

Try **Perfboarder** first for the physical layout, with **DIYLC** as the offline fallback. KiCad remains appropriate for a formal schematic or future manufactured PCB. This is a provisional workflow choice, pending a small placement/wiring/export trial. Do not infer that advertised checks establish electrical correctness or usable exports.

## Primary sources and assessment

| Tool | Primary source | Fit / limitation |
| --- | --- | --- |
| Perfboarder | https://perfboarder.com/ | Free browser editor; custom hole footprints, component/solder-side views, connection checks, ZIP export and local agent bridge. Best candidate for direct agent assistance. Actual bridge connection, routing, custom module geometry and export are not tested yet. |
| DIYLC | https://github.com/bancika/diy-layout-creator | Free open-source offline perfboard/stripboard/point-to-point editor; custom components, image/PDF export; current docs require Java 21+. Strong desktop fallback; not installed or exercised in this pass. |
| KiCad | https://www.kicad.org/discover/pcb-design/ | Formal schematic/PCB workflow with footprints and net checks. Can represent a 2.54 mm grid, but ordinary PCB tracks do not by themselves specify real perfboard solder joints, jumpers or lead clearance. |
| PerfBoard.app | https://perfboard.app/about and https://perfboard.app/pricing | Dedicated routing, wire cut/bend dimensions and solder-side guides. Full exports require Pro; advertised remote MCP also requires a recurring subscription. Prices did not render in text, so no amount is claimed. Less suitable for this small free/local workflow. Distinct product from Perfboarder. |
| VeroRoute | https://sourceforge.net/projects/veroroute/ | Perfboard/stripboard router with KiCad netlist import and connectivity checking. Publisher lists Windows/Linux prebuilt packages; extra setup on this Mac was not justified for the first trial. |
| Perfboard Studio | https://github.com/medinstech/perfboard-studio | Promising netlist/CLI/MCP approach; upstream explicitly distinguishes software claims from built-hardware proof. Not selected or installed for this pass. |

## Small local verification

- Installed `/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli version` returned **9.0.6**. This verifies CLI availability, not a produced layout.
- Fritzing.app is also present in `/Applications`; it was not evaluated in depth.
- Opened `https://app.perfboarder.com/` in a temporary background browser tab. Editor loaded without a login prompt, with Start a board, Open a file, Parts catalogue, routing/rearrangement, build tracking and Agent controls visible.
- Expanded Agent panel: it exposes a per-board enable toggle, localhost port 4870, a Codex CLI setup tab and stdio package instructions. Toggle remained off; no server was installed, configured or connected.
- Read npm registry metadata for `@perfboarder/mcp`: version **0.1.2**, Node >=20, executable `perfboarder-mcp`. Package availability is not proof that the bridge works.
- No circuit files were uploaded, no accounts/subscriptions created, and no software installed. No complete board, footprint, route or export was tested.

## Next discriminating experiment

Create a disposable board containing two socket-header rows with a custom module outline, one diode and connector. Check hole spacing and body/underside clearance, route a named net, inspect both sides, then export and reopen the file. Proceed with TwoSkulls only if this succeeds. Use actual Freenove v3.0 header geometry, keep antenna/USB/SD access clear, and measure the intended USB-C breakout and perfboard before claiming a build-ready layout. Existing pinout images alone do not establish header-row spacing.

## Electrical scope carried forward from this chat

Latest user workflow supersedes the earlier preference for mandatory VBUS blocking: ordinarily disconnect external perfboard power for normal-USB programming; alternatively use Cam's VBUS-disconnected cable while external power remains on. Put a reverse-blocking diode in the external 5 V branch feeding the WROVER, cathode/stripe toward WROVER, while the servo stays on the external rail. This prevents significant USB-fed power from energizing the main perfboard/servo rail. It does not establish that normal USB and external power can be connected together safely. Choose actual diode and capacitor parts and qualify supply behavior under load before build acceptance.


## Completed Perfboarder trial - 2026-10-15

**Verdict:** usable for an agent-assisted layout, with independent endpoint, geometry and physical review. The bounded placement/wiring/export/reopen trial passed. No final TwoSkulls hardware layout was produced.

Installed the official npm package `@perfboarder/mcp@0.1.2` and dependencies into `/tmp/twoskulls-perfboarder-trial`, with scripts disabled. No global install or Codex configuration change. The standard MCP SDK worked from the Node CLI; node_repl could not import its node:process dependency. A temporary localhost-only driver invoked the official MCP client. MCP listener127.0.0.1:4870 and temporary driver127.0.0.1:4871 were shut down after testing. Chrome's native local-device access prompt had to be accepted before the tab could connect; tab screenshots alone did not expose that prompt. No security flags or origin checks were disabled.

Created a 30x30/2.54mm-grid trial named `TwoSkulls tool trial - NOT FOR BUILD`. It contains a custom 40-pin socket-pair outline, custom axial diode footprint, power connector and servo connector: four parts, four named/coloured nets, six wires. Socket spacing25.4mm and diode lead spacing10.16mm are illustrative assumptions, not measured parts. L/R pin numbers are arbitrary trial references, not a verified WROVER pin map. No LEDs, eye connector, capacitor, USB-C module envelope, antenna keepout, actual board size or current capacity is qualified by this trial.

Demonstrated custom kind creation, component placement and movement, named pin connection, net naming/colouring, rerouting, mirrored front/back display, and overlap refusal. Moving connector origins to the edge cleared the two edge-access warnings. Final `problems` returned no findings. Native diode-do41 catalogue pins were only one grid step apart; replaced that trial part with a deliberately specified custom footprint. Library names alone do not prove physical dimensions.

A reference error exposed a more significant checker gap: after deleting D1, the replacement reused D1; connect calls using mistaken J4.A/J4.K nonetheless succeeded and did not produce a corresponding hard error. Those wires were explicitly cut and correctly reconnected to D1. The final export was independently checked to ensure every net endpoint names an existing part and a real pin. Never use a zero-problem badge alone as wiring acceptance.

Exported through the editor's Export button to ZIP. A browser download-event wait timed out despite the actual download succeeding; verified the completed file in Downloads rather than repeating the export. Import first opens a format selector, then the `Perfboarder board .zip` choice opens the chooser. Uploaded only the generated non-sensitive test ZIP to the browser-local editor. It reopened alongside the existing project, with `(2)` added to its name and a new ID. Its board geometry, kinds, parts, nets, keepouts, notes and solderedLinks matched the original export exactly. All final endpoints independently resolve. These checks do not validate buildability of arbitrary future routes or electrical correctness.

Retained under `hardware/perfboard/tool-trial/`:

- `perfboarder-trial.zip`: actual editor download, reopenable project.
- `perfboarder-trial.json`: initial MCP project export.
- `reopened-project.json`: MCP export after importing ZIP.
- `verification.json`: bounded verification results and outstanding limitations.
- `perfboarder-trial.png`: screenshot of the reopened project, both sides.
- `README.md`: applicability and how to open the trial.

The open Chrome board is retained for visual review with its agent bridge off. Temporary test activity and source research do not authorize building, rewiring or flashing. Next: obtain the actual perfboard dimensions, socket row spacing, USB-C breakout and passive component geometry; then create the real layout with verified v3.0 pin labels and independent net checks.

## Actual TwoSkulls draft and mounting decision — 2026-10-15

Created `hardware/perfboard/draft-v1/` in the onboard-SD worktree. This is a provisional eight-part/six-net design with official v3 GPIO labels, external servo rail, reverse-blocking controller diode and three capacitor positions. Its actual downloaded project ZIP matches the exported JSON; all56 pins resolve and38 wire segments match the intended connected graphs. Independent review caught and prompted correction of an initial external5V segment passing through the input ground pad. Latest review finds no foreign-pin/junction crossings. Crossings still require insulated point-to-point wiring.

Cam selected the Lord of the Tools180-EP USB-C panel socket and then revised cable entry from horizontal edge entry to perpendicular entry, socket face up. Adopt a raised, flange-supported cradle with flying power leads. Reserve three board mounts and separate compliant feet on the curved flexible skull. The companion drawing and assembly notes carry mechanical reservations absent from the editor JSON. No physical fit or electrical load acceptance is claimed.

Bounded mounting research addressed load transfer and bonding to flexible curved substrates. [3M VHB design guide](https://multimedia.3m.com/mws/media/1732017O/3m-vhb-tape-design-guide-en.pdf) and [5925 technical data](https://multimedia.3m.com/mws/media/2523009O/3m-vhb-tape-5925.pdf?fn=3M-VHB-Tape-5925.pdf) describe substrate/preparation dependence and viscoelastic bonding behavior. Decision: separate broad pads, avoid peel and a rigid bridge across the shell; defer product selection until plastic and loads are known. These sources do not demonstrate adhesion to this skull. [Espressif PCB guidance](https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32/pcb-layout-design.html) supports keeping the antenna beyond the carrier/baseboard where practical and checking enclosure clearance; this draft does not establish RF performance.

Remaining fit uncertainty: blank board dimensions, Freenove header-row spacing and carrier offset, socket flange hole centres/tail depth, caps' exact stock geometry, screw/tool access and shell material. Next experiment: physical dry fit before final bracket dimensions or soldering. Nothing was fabricated, rewired or flashed.

Final draft handoff: saved editableZIP, JSON, wiringCSV, independently checked verification record, actual editor screenshot and rendered mechanical concept. Left the browser draft open with its agent bridge disabled; stopped the temporary localhost driver/MCP processes. No global configuration changes. Work remains an uncommitted design draft.

## Straight-rail revision — 2026-10-16

Cam requested the next draft use his prior construction pattern: removable ESP32 on raised headers, parallel5V/ground rails beneath it, perpendicular branches and capacitors across the rails. Created `hardware/perfboard/draft-v2/`, preserving v1. Rails are underside insulated wiring at columns9/11; servo moved to lower right, bulk/bypass caps bridge the rails below the module, and a separate after-diode branch supplies the controller. Diagram distinguishes the real raised socket/flange/vertical PCB from J4's wire landing pads and shows plug travel explicitly.

Independent review found a C1/C2 body overlap and excessive cradle envelope; moved C2 to row25 and narrowed the cradle to leave the servo connector clear. Drew body assumptions and measured nominal clearances from those assumptions; this is not measured-part fit. All five cross-net wire crossings are marked as insulated jumps. Same electrical terminal membership as v1, six connected graphs,56 resolved pins,32 segments and no foreign-pin/junction pass-throughs. Actual browser import/export preserved geometry/circuit; only identity, timestamp and coordinate-display preferences changed.

Perfboarder reports intentional unused power pins and an overlap between J1's bounding box and an underside rail junction. Do not hide that finding or treat the checker as a multilayer mechanical model: rails are beneath the perfboard, while the module is raised on removable sockets; actual clearance must be checked. No additional third-party research was needed: existing flange-load-transfer and substrate-bonding research still applies. Exact bracket height, hole centres, shell adhesive and real component/blank dimensions remain open. No physical work performed.

## V3 flush socket and underside capacitors — 2026-10-16

Cam clarified that the socket's tiny PCB should pass through a perfboard slot while its flange rests on the top surface; no raised cradle. The external socket is centered south of the ESP32's programming connector, with interference intended between inserted cable bodies. C1/C3 lie flat underneath, and risers protect the underside stack. V3 adopts this arrangement, preserves earlier drafts, and explicitly keeps slot dimensions, plug-body interference, antenna offset and riser depth as unmeasured fit gates.

General mounting problem: supporting a horizontal radial electrolytic on a handwired board without stressing lead entries or rubbing against rails. Bounded primary pass: [Nichicon application guidelines](https://www.nichicon.co.jp/english/products/pdf/aluminum.pdf) call for low-stress lead forming before soldering, correct lead spacing and care around body/end-seal placement. Decision: show a separate insulating saddle/support and unobstructed seal/vent, rather than resting the body directly against the underside rails. The manufacturer guidance is not an acceptance test of unknown stock, adhesive or this assembly. Local geometric check finds no mutual cap-body overlap; actual support and material compatibility still need a dry fit.

Actual editor import/export preserved all28 segments and six net graphs; independent review confirms unchanged terminal membership, polarities, no foreign-pin/junction crossing, no route into the illustrative slot box, and nominal flange/mount clearance. Four editor findings are retained and explained as intentionally unused supply pins plus underside component/rail overlaps with the raised module in the editor's single-plane model. Companion drawing is required to interpret mounting side. No global configuration changes, bridge processes, cutting, soldering, rewiring or flashing in this revision.

## V4 northern servo and RF target — 2026-10-16

Cam requested the servo connector move as far north as antenna and NE-standoff clearance allow. Bounded primary refresh: [Espressif module-positioning guidance](https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32/pcb-layout-design.html#general-principles-of-pcb-layout-for-modules-positioning-a-module-on-a-base-board) recommends overhang,15mm housing clearance and final RF testing. The [WROVER-E datasheet](https://documentation.espressif.com/esp32-wrover-e_esp32-wrover-ie_datasheet_en.html) gives18mm module width; do not use28mm carrier width as the antenna width. Nominal centring and antenna/header offsets remain unmeasured. Use a rounded15mm offset around the assumed antenna rectangle; do not claim measured compliance.

Reserved NE(25,3)4mm access radius in addition to v3's three drawn mount choices. Servo header atcol26 rows6–8 is the northernmost integer-row candidate under an assumed4×10mm housing envelope: row5 overlaps the NE reserve, row6 gives1.19mm nominal clearance. Connector-to-antenna24.39mm; NE-to-antenna18.13mm; NW-to-antenna15.68mm. C1 moved5.08mm north with a16mm north-edge separation. Small NW/C1 margins and the unavoidable GPIO23 escape inside the buffer remain explicit RF measurement/test holds. Cable exits east/south.

General routing obstacle: a hole-grid editor represents nets in one plane, while insulated perfboard wiring can span socket tails on another level. Chose two explicitly drawn insulated flying links held below exposed socket tails, rather than squeezing wire between pads; actual vertical clearance/retention must be verified. The companion layer detail is authoritative for construction; editor autoroutes do not establish that physical path. Local graph/pin checks and independent review pass36segments with unchanged net membership. The servo signal's specified polyline is30.5mm versus68.6mm in v3. No electrical or physical prototype test was performed.
