# Anthony: loose LED header and SD mount failure

2026-10-14. Investigation only; no firmware or hardware changes for this symptom.

User reports LED female header detached from soldered perfboard; startup appears
to repeat three flashes until header is held firmly, then startup proceeds.

Inspected original/main firmware: GPIO32/33 drive LEDs; no LED presence detection.
Three flashes are executed inside the failed SD.begin retry loop in setup.
A mount failure does not establish a brownout. Serial distinguishes repeated
mount retries from reset/startup banners.

Inspected sibling TwoSkulls - Instructable/TwoSkulls_schematic.png and
TwoSkulls-LEDs_schematic.png plus original Fritzing archive and assembly photos
IMG_4370.jpeg and IMG_3999.jpeg. LED circuits are GPIO -> LED -> resistor -> GND;
external SD uses GPIO5/18/19/23, 3.3 V, GND. The drawing still includes ultrasonic
hardware, so it is an older design, not a verified as-built map of Anthony today.
LED drawing shows separate 100-ohm resistors; README describes a shared resistor.
Photos show hand-built solder interconnects but cannot identify today's broken
joint or reliably map the damaged header. Do not infer a particular short or
failed joint from these old images.

Inference: an electrically open LED branch should not prevent card access.
Pressure-dependent behavior makes disturbed shared ground/interconnect, nearby
cracked SD power/signal joint, or unintended contact the leading suspects.
An LED-dependent marginal supply is possible but lower priority; not demonstrated.

Primary reference:
https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32/schematic-checklist.html
Espressif emphasizes stable power and decoupling; this is general advice and does
not diagnose this board. No voltage or continuity measurements taken.

Recommended discriminating check: with both feeds off, isolate the loose header
and inspect/measure SD GND-to-ESP GND continuity, 3.3 V path, and nearby solder
interconnects. Once connector is mechanically sound, boot with the LED harness
unplugged: successful mounting separates loss of LED load from mechanical joint
damage. Capture serial at 115200 for exact mount/reset evidence. Do not short
GPIO pins or use a loose header as a live jumper. Current photographs of both
sides around the damaged header are needed to locate a specific repair.

Status: hypothesis only; physical inspection, continuity tests, and acceptance
pending. Onboard SD migration remains deferred and separate from this fault.
