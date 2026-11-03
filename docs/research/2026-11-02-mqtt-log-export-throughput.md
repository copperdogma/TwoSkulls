# Asynchronous MQTT log-export throughput

Problem class: stop-and-wait application flow control over an asynchronous SDK
outbox whose worker sleeps while polling an otherwise quiet TCP connection.
Observed report from the first HA read: a 413-byte SD log completed, but the
5,689-byte log ended with the native 20-second timeout. Do not add blind retries
or extend the deadline before distinguishing steady low throughput from lost
PUBACK correlation.

## Bounded primary-source pass

- [ESP-IDF 5.5 MQTT API](https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32/api-reference/protocols/mqtt.html)
  documents enqueue as deferred MQTT-task transmission and PUBLISHED as broker
  acknowledgement of QoS1/2. It is not an application delivery confirmation.
- Actual SDK checkout is ESP-IDF 5.5.4 with bundled ESP-MQTT commit
  `6af4446a48ea7fa54948edc4e62277ed70abb6e1`.
- [Pinned MQTT worker](https://github.com/espressif/esp-mqtt/blob/6af4446a48ea7fa54948edc4e62277ed70abb6e1/mqtt_client.c#L1716-L1755)
  processes incoming packets, then sends a queued outbox item. After unlocking,
  [line 1819](https://github.com/espressif/esp-mqtt/blob/6af4446a48ea7fa54948edc4e62277ed70abb6e1/mqtt_client.c#L1817-L1823)
  calls transport poll-read. A PUBACK callback can finish before the control
  owner gets its next 20 ms tick to enqueue the following frame, leaving the MQTT
  worker sleeping with no new incoming bytes.
- [Pinned mqtt_config.h](https://github.com/espressif/esp-mqtt/blob/6af4446a48ea7fa54948edc4e62277ed70abb6e1/lib/include/mqtt_config.h#L21-L25)
  defaults that poll to **1000 ms** unless configured. The generated current
  sdkconfig has MQTT_USE_CUSTOM_CONFIG disabled and no poll-timeout override.
- [Pinned Kconfig](https://github.com/espressif/esp-mqtt/blob/6af4446a48ea7fa54948edc4e62277ed70abb6e1/Kconfig#L127-L139)
  exposes MQTT_POLL_READ_TIMEOUT_MS, dependent on MQTT_USE_CUSTOM_CONFIG. The
  event-queue option is a different mechanism; it does not immediately wake a
  poll that is already blocked when an owner enqueues a payload.

## Decision-oriented hypothesis and next experiment

The small report requires four frames (begin, two chunks, end); the larger log
requires 25 frames. At roughly one second per stop-and-wait frame, the first fits
and the second cannot fit a 20-second transfer deadline even with every PUBACK
received. Existing five-second telemetry/QoS0 packets may add outbox delay.

Capture one controlled larger-slot request, logging only monotonic receive times,
response topic suffixes and payload lengths (not log contents). Monotonically
advancing offsets at approximately one-second intervals would confirm the SDK
poll cadence hypothesis. A fixed last offset and then no progress would instead
call for expected-ID/PUBACK/outbox instrumentation. No firmware edit was made in
this research pass.

If cadence is confirmed, the smallest supported experiment is:

```text
CONFIG_MQTT_USE_CUSTOM_CONFIG=y
CONFIG_MQTT_POLL_READ_TIMEOUT_MS=50
```

Retain the same single in-flight frame, protocol, existing 20-second deadline and
bounded outbox. The 14,335-byte maximum needs 58 frames; a 50 ms poll plus 20 ms owner
cadence has a reasonable theoretical margin. This is an estimate, not measured
worst-case latency. Enabling custom config also materializes MQTT defaults; diff
the generated SDK config to confirm no unexpected priority, stack, buffer or
protocol change. The client explicitly sets its stack/buffers at initialization.

Qualification after change: native build/resource check, exact CRC/byte comparison
for both real logs and a bounded synthetic max-size fixture if appropriate, one
reconnect/no-replay check, and later active-audio CPU/timing measurement. Lowering
poll timeout increases worker wakeups; do not claim it is free merely because it
does not allocate an additional task or buffer. Preserve the existing bound and
persistence/read-only ownership rather than expanding retries or timeouts.

## Trace-confirmed decision and implementation

Parent's controlled larger-slot trace confirmed continuing roughly one-second
frame progression, with occasional roughly three-second retransmission gaps. It
reached offset 3072 before the native timeout at about 21.6 seconds. Evidence:
`build/validation/story-007-ha/fixes/log-throughput-before.txt`.

Adopt the supported 50 ms poll setting as the first measured correction. Tracked
sdkconfig.defaults and the existing ignored sdkconfig were updated; changing only
defaults would not override the already-generated explicit disabled setting.
No SDK source patch, extra task, protocol change or retransmission retry was
introduced. Preserve the 20-second native/30-second collector deadlines for this
first experiment. A maximum 58-frame report has about 4.1 seconds of theoretical
50 ms poll + 20 ms owner cadence, leaving space for several observed 3-second retry
gaps. This is a budget estimate, not a guaranteed wireless latency bound.
Post-change actual reports and maximum-bound transport checks determine whether
the existing deadline remains appropriate. Do not increase it without measured
need, and retain a bounded failure when the wireless link is unhealthy.

## Native build and local post-change result

The 50 ms configuration built successfully. Final checked native resources were
unchanged at IRAM 130,987/131,072 (85 bytes free), static DRAM 102,476 bytes. The
application image was 2,315,248 bytes padded. Enabling custom config materialized
standard ports, buffer 1024, stack 6144, priority 5, event queue 1 and outbox expiry
30000 ms; no effective change to those fallback defaults was intended. The only
transport behavior changed was poll timeout 1000→50 ms. Evidence:
`fixes/native-pacing-build.log`, `fixes/native-pacing-size.log`, and
`fixes/mqtt-config-diff.txt` under the private Story 007 evidence directory.

A single user-interface refresh then produced these observed broker traces:

| Report | Result | Begin-to-terminal time | Frames | Median frame gap | Largest gap |
| --- | --- | --- | --- | --- | --- |
| 5,689 bytes, before | Native timeout; last offset 3072 | 21.594 s | 16 received (one duplicate chunk) | 1.039 s | 3.233 s |
| 413 bytes, after | END received, 413 chunk bytes | 0.354 s | 4 | 0.070 s | 0.215 s |
| 5,689 bytes, after | END received, 5,689 chunk bytes | 2.049 s | 25 | 0.074 s | 0.190 s |

No duplicate chunks appeared in that post-change run. This supports the
poll-cadence diagnosis rather than an ACK-ID correlation stall. Metadata traces
are `fixes/log-throughput-before.txt` and `fixes/log-throughput-after-ui.txt`;
computed values are `fixes/log-throughput-comparison.json`.

These observations prove frame-level progress/completion for the two actual
reports, not their byte/hash identity; the receiver/cache comparison separately
checks CRC and exact bytes. Maximum-size codec tests are portable host evidence,
not an on-device maximum-size transfer benchmark. No speaker was present, so
increased MQTT wakeup/audio coexistence remains unqualified. The unchanged
20-second native bound now has substantial margin for the observed reports.

## Local result

The supported setting corrected the measured bottleneck: the same actual UI read
completed the 413-byte report in 0.354 seconds (4 frames) and the 5,689-byte report
in 2.049 seconds (25 frames). Both cached files matched pre-flash SD SHA256 and
complete native HA notification bodies. Evidence: `fixes/log-throughput-after-ui.txt`
and `fixes/ha-saved-reports-validation.json` under the ignored Story007 runtime.
The installed image then recovered from a broker restart with live increasing
uptime and no command replay. No extra retries or deadline increase were needed.
Native build passed with unchanged 85-byte IRAM margin. A 14,335-byte synthetic
receiver/HA transport fixture passed separately; this was not a maximum-size SD
transfer on Jeff. Active-audio CPU/coexistence and maximum-size native transfer on
an impaired wireless link remain unqualified.
