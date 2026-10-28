# Continuous speaker reconnect — 2026-10-27

Problem class: asynchronous recovery when a remote device becomes available
after a finite discovery window. Repeat paced attempts using completion events;
a finite per-episode budget must not become a lifetime stop.

## Reused evidence and sources

- Death's native radio was copied from this worktree and remained byte-identical
  before today's fix. On Death, late speaker power-on failed after3 cached
  attempts plus inquiry. Explicit retry connected immediately. Updated firmware
  repeated multiple failed episodes and automatically connected the speaker when
  powered on later; Cam confirmed the startup announcement. This is sibling
  hardware evidence, not an updated TwoSkulls paired-device test.
- [ESP-IDF5.5.4 A2DP API](https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32/api-reference/bluetooth/esp_a2dp.html): ESP_OK means queued request, not completed connection/disconnection.
- [ESP-IDF5.5.4 Classic GAP API](https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32/api-reference/bluetooth/esp_gap_bt.html): inquiry is finite and reports state/completion; the application owns rearming it.
- Installed pinned SDK components/bt/host/bluedroid/btc/profile/std/a2dp/btc_av.c,
  btc_av_state_opening_handler, has no disconnect-request case. Do not infer that
  requesting a disconnect cancels an in-flight open. Preserve the pending gate.

## Decision

Reuse Death's patch exactly for the shared radio module: repeat3 cached attempts
and one inquiry, restart with5second backoff, including API failures, discovery
misses and failed discovered opens. Remove lifetime connection/ACL timeout stop
latches; continue waiting for confirmed completion and then resume automatically.
Do not reset the shared Matter/Bluetooth host. Repeat media readiness/start too.
Preserve configured-address/name selection and both role-specific peer flows.

Verify locally with the actual radio source, fake async SDK and virtual clock
for both primary and secondary, plus all existing host tests/native build.
A permanently wedged SDK operation remains a limitation; unsafe overlapping opens
are not an acceptable workaround. Hardware verification on Anthony/Jeff is still
required; Death's successful live check does not certify their paired behavior.

Local result: all host suites including both-role production-radio scenarios
PASS under ASan/UBSan; native ESP32 build PASS. Radio source matches Death
exactly. Physical paired-device update/verification remains pending.
