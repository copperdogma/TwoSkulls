# Current communication paths — 2026-10-14

**Current planning decision:** retain the direct BLE peer protocol because yard
Wi-Fi is unreliable. The Wi-Fi/TCP proposal later in this chronological audit
is superseded, not selected for implementation. See
[Story 001](stories/story-001-single-wrover-matter-prototype.md) and its
[research record](research/2026-10-14-single-wrover-research.md): ESP-Matter has a
Bluedroid backend, but shared controller/callback ownership remains a prototype
gate. Current implementation results are in the dated implementation log below.

**2026-10-15 update:** implementation/flashing is now authorized and the first
native diagnostic probe is running; see its dated implementation log. Cam also
confirmed only Anthony had a C3. Anthony's original PCB is v2.1 without onboard
SD; the v3.0 board borrowed from Jeff is being used with Anthony's card. The
physical secondary is unavailable for the pair test, and the borrowed board's
original image is backed up. These facts supersede earlier physical assumptions.

Read-only audit of main e4b48ff and sibling matter-skull-switch source; not a
verification of firmware flashed on either physical skull. No firmware changed.

- TwoSkulls.ino: primary/secondary comes from config.txt. Both instantiate A2DP
  and BLE on the WROVER. Only the primary consumes matterTriggerDetected to pick
  a skit and write its file path to the secondary BLE characteristic. Secondary
  callback queues that local file. Primary checks response/readback then queues
  its own copy. This is a start handshake, not sample-accurate clock synchronization.
- bluetooth_controller.cpp: initializeA2DP explicitly selects ESP_BT_MODE_BTDM;
  initializeBLE selects primary client or secondary server on the same WROVER.
- matter-skull-switch/firmware/main/app_main.cpp: C3 receives Matter OnOff and
  emits a 500 ms pulse on GPIO4. No skull-to-skull transport or UART command
  parser found in that application. UART REPL is a debug console.
- Pin discrepancy: TwoSkulls README says C3 GPIO1 to WROVER GPIO1, while sibling
  C3 source emits GPIO4. Establish actual flashed version/wiring before changing
  hardware. WROVER current input is GPIO1.

Conclusion limited to checked source: Anthony primary needs C3 for existing
Matter trigger architecture; Jeff secondary does not use its C3 for coordination.
Do not remove Jeff's board based solely on source without confirming deployed
firmware and checking power/ground wiring dependencies.

Primary references verified:
https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-guides/ble/overview.html
C3 has BLE, no Classic Bluetooth, so cannot replace WROVER A2DP source.
https://docs.espressif.com/projects/esp-matter/en/latest/esp32/introduction.html
Original ESP32 can implement a Matter Wi-Fi accessory; C3 is an architectural
separation, not an absolute hardware requirement for Matter. Accessory is the
role here; it is not an Apple Home hub.

User recollects A2DP/BLE interleaving problems. Source explicitly attempts their
coexistence, but inspection cannot confirm the cause or deployed workaround.
Rebuild choices remain open: preserve working BLE with one primary C3, or design
and independently test a C3-based coordination channel if coexistence is still
a problem. No communication redesign authorized or implemented in this audit.

## Single-WROVER feasibility research — 2026-10-14

User asks whether two cores and one radio could handle A2DP source, Matter Wi-Fi
accessory, and BLE coordination together. Decision: plausible experimental target,
not an established replacement for the C3. Do not rebuild around it without a
bench prototype. CPU affinity does not remove RF contention or internal RAM needs.

Primary references:
- https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/coexist.html
  Documents time division among Wi-Fi, BT, BLE and supported STA/BT and STA/BLE
  scenarios. Pairwise support is not an acceptance test of this application.
- https://docs.espressif.com/projects/esp-faq/en/latest/software-framework/coexistence.html
  Documents audio loss/lag for A2DP sink + Wi-Fi + BLE scanning. This is related
  failure evidence, not direct evidence about TwoSkulls' A2DP source path.
- https://docs.espressif.com/projects/esp-matter/en/release-v1.4/esp32/faq.html
  BLE normally released after commissioning; custom ongoing BLE needs retention
  and stack integration. FAQ's examples specifically use NimBLE.
- https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/bluetooth/index.html
  A2DP/Classic requires Bluedroid, while NimBLE is LE-only. Must resolve Matter
  commissioning stack compatibility rather than assume independent stacks can
  be combined. Firmware currently uses Arduino-ESP32 2.0.17, so adding current
  ESP-Matter is not a mere task-affinity change.

Candidate simplification: Matter and skull coordination both over Wi-Fi, A2DP
on Classic Bluetooth; eliminates persistent custom BLE, but requires a new
reliable coordinated-start protocol. This is a proposal, not implementation.

Small next experiment: spare WROVER streams representative SD audio, runs a
minimal Matter trigger and either current BLE link or proposed Wi-Fi control;
measure audio underruns, internal free/minimum heap, and start skew during normal
traffic, reconnections, and commissioning. Then add jaw/LED work and test pair
for an hour. No board or radio experiment run in this research pass.

## Proposed Wi-Fi control transport — 2026-10-14

Recommendation for first prototype: both skulls join existing home Wi-Fi in STA
mode, Anthony hosts a small persistent TCP control service, Jeff connects using
mDNS-discovered service plus configured pair identity. Keep SD audio local;
only commands/status cross network. Use standard sockets, bounded length framing,
command/session IDs, readiness and deadlines. No broker/cloud dependency.

Separate delivery from timing: PREPARE asset/version -> READY after prebuffer ->
START_AT future timestamp mapped into each monotonic clock. Estimate offset and
RTT using four-timestamp exchanges (NTP method), reject uncertain/stale estimates;
never start expired commands on reconnect. Unknown readiness blocks a new skit.
A failed connection mid-commit cannot guarantee atomic paired start, so timeout
and cancellation behavior require explicit tests. Playback after a successful
start should not require a continuous network tick.

Sources:
https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/lwip.html
https://docs.espressif.com/projects/esp-idf/en/release-v4.4/esp32/api-reference/protocols/mdns.html
https://www.rfc-editor.org/info/rfc5905/

Not tested/implemented. Bench proof must distinguish local playback start skew,
clock drift across a skit, and independent Bluetooth speaker latency. Measure
actual sound; network scheduling alone does not align two A2DP output pipelines.
