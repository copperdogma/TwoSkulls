# Perfboard learning review — 2026-10-16

Cam asked for a candid assessment of the design direction and missing concepts. This is a review, not authorization to rewire, change LED resistors or fabricate a board.

## Findings

The proposed removable controller, protected underside components, flange-supported flush socket, organized supply/return rails and antenna overhang are sensible packaging directions. Latest suggestion to move the servo connector north is plausible but has not been implemented in v3. Keep antenna clearance ahead of saving a few centimetres of low-rate servo signal wire; connector and cable placement can affect the antenna.

Electrical priorities before fabrication: actual servo peak/stall supply demand and cable/wire drop; controller voltage after D1 during servo motion; servo return paths that do not use controller header wiring; controller/servo behavior when one supply is absent; polarity and continuity. Bulk capacitance does not replace adequate source/cable capacity. Actual servo model/current remains unidentified in the design record; do not invent a supply rating from the component arrangement.

The existing eye harness uses one shared100Ω return resistor. Preserved in drafts to avoid silently changing the existing harness. For a clean rebuild, one appropriately calculated resistor per LED branch is the preferred arrangement; confirm LED type, forward voltage, desired current and GPIO drive limits before selecting values. Shared resistance can couple branch brightness/current. No resistor or harness change applied.

Mechanical plug-body interference remains a convenience to validate with the actual cables in both orders, not electrical source isolation. Flange retention, cable pull force, cutout weakening, underside insulation/support and shell flex are as important as fitting the nominal component bodies. Do not mark a physically arranged drawing as electrically load-qualified.

## Bounded primary references

- [Pololu: servo electrical characteristics](https://www.pololu.com/blog/16/electrical-characteristics-of-servos-and-introduction-to-the-servo-control-interface): size the supply for servo maximum/stall demand rather than idle consumption. Applies to the general load class; exact owned servo remains unknown.
- [Adafruit: Circuits From Scratch](https://learn.adafruit.com/lets-put-leds-in-things/from-scratch): separate resistors for parallel LED branches. Our branches are separately GPIO-driven with a shared return resistor, so the source supports independent branch limiting; exact current still depends on the actual harness and drive states.
- [Espressif: module/baseboard positioning](https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32/pcb-layout-design.html#general-principles-of-pcb-layout-for-modules-positioning-a-module-on-a-base-board): antenna beyond baseboard where practical, adequate housing clearance, and final communication testing. These module-placement principles apply to the carrier/perfboard arrangement; chip-level multilayer layout rules should not be copied wholesale onto a handwired devboard carrier.

No new local electrical test was run: the useful next discriminating experiment is an unsoldered physical mock-up followed by actual supply-voltage/current checks with the selected servo. Existing v3 graph checks establish connectivity only. Learning sequence: schematic and current paths first, component limits second, layout/mechanics third, measurements last.
