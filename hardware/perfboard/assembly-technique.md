# Agreed perfboard construction approach

2026-10-16. Construction guidance from the design discussion; no assembled-board acceptance implied. Follow the current revision's coordinates and polarity.

## Rails and insulation

Cam plans to use thick straight bare solid wire, believed to be tinned copper, for external5V and ground. Confirm material/diameter and suitability for the actual servo current; appearance alone does not establish a current rating.

Anchor each rail by soldering to three or four isolated perfboard pads along its length. Let the wire rest without spring tension; do not load or lift pads while forming branches. Use electronics flux and solder.

Complete rail branches, inspect them and check isolation before covering the rails/joints with electrical-grade polyimide tape. Then route insulated wires above the covering. Crossings need insulation during construction; a final covering cannot separate conductors already touching. Secure rails and branches mechanically; tape insulation does not replace support. Keep sharp wire ends and screw hardware from pressing into thin film.

The actual v4 flying links must be insulated, retained and held clear below the exposed socket tails. Neither the logical editor drawing nor tape coverage establishes that vertical clearance.

## Branch joints

Route the insulated solid-core branch without tension. Strip without nicking copper. Form a small hook around the rail where accessible; otherwise use a supported short parallel overlap, typically3–5mm, rather than a point contact. Lightly tin the exposed branch and use fresh electronics-grade flux. Heat both conductors with an appropriately sized clean, tinned tip; feed solder into the heated joint and let it wet both surfaces. Keep it still while cooling. Inspect wetting and bridges, and secure the insulated wire so movement does not flex its lead at the joint.

Practice on scrap rail/branch wire before heating the real anchoring pads. Sources: [Adafruit joint technique](https://learn.adafruit.com/adafruit-guide-excellent-soldering/making-a-good-solder-joint), [example soldering faults](https://learn.adafruit.com/adafruit-guide-excellent-soldering/common-problems), [3M polyimide electrical tape example](https://www.3m.com/3M/en_US/p/d/b00034565/). These are general references, not qualification of Cam's actual wire/tape.

## Capacitors

C1 is the external-rail bulk reservoir; C2 is the smaller ceramic bypass. C3 on the west side is an additional controller-side reservoir after D1, not an LED-eye filter. C3 is provisional: the Freenove board already has supply capacitors, and benefit from this extra100µF is unmeasured.

Capacitors reduce disturbances, not all servo-induced effects. Supply capacity, cable drop and common-ground return impedance still matter. A diode blocks reverse feed but is not complete electrical/noise isolation.

Lay electrolytics flat with gently formed leads and a separate insulating support; do not force a soldered body downward or use leads as its mounting. C1 needs separation from the underside rails. Required riser space includes the full capacitor diameter, support/wiring, connector tail and shell flex. Keep polarity, end seals and vents clear. No fixed height or adhesive is qualified.
