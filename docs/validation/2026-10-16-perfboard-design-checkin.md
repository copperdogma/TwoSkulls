# Perfboard design check-in — 2026-10-16

## Scope

Save the Perfboarder tool trial, v1–v4 design history, editable exports, assembly/clearance drawings, wiring tables, recorded assumptions, research and agreed rail/soldering technique. V4 is the current planning draft. No firmware changes, fabrication, rewiring or hardware acceptance.

## Verification

- `python3 hardware/perfboard/verify-designs.py` passed for all four drafts: real pin references, connected graphs, expected routing, unchanged terminal membership, no foreign pin/junction pass-through under specified geometry, editor/import ZIP consistency, wiring CSV agreement, SVG parsing and PNG header/dimension checks. The separate trial's export/reimport fields match.
- Draft segment counts: v1=38, v2=32, v3=28, v4=36; each has8 parts,6 nets and56 part pins. V4's two specified diagonal links are insulated flying leads on a separate physical level, not verified between-pad routes.
- `python3 hardware/perfboard/draft-v4/check-mechanics.py` passed the nominal row search and recorded clearances. Servo housing to NE reserve≈1.19mm; antenna separations≈24.39mm servo,18.13mm NE,15.68mm NW and16mm C1.
- Python authoring/check scripts syntax-checked. No generator was rerun during check-in, preserving saved project IDs and review records.
- Browser preview files originally contained JPEG data with PNG filenames. Re-encoded those images as PNG without changing the pictured design; editable artifacts and assembly drawings were unchanged.
- `git diff --check` passed. Only hardware planning files and associated research/validation documentation are in scope.

## Limits and next gate

All clearances depend on unmeasured carrier/antenna positions, connector housings, screw access and capacitor bodies. The fixed GPIO23 escape begins within the antenna buffer. Four editor findings remain documented as unused supply pins and overlapping parts on separate physical sides/levels; the editor is not a3D clearance checker.

Actual slot size, underside flying-link clearance, capacitor support, standoff height, cable interference, current capacity, diode drop, servo disturbances and complete-skull RF behavior remain unverified. Next action: unsoldered dry fit and measurements of the actual parts before cutting/soldering; subsequent electrical/load tests remain separate.
