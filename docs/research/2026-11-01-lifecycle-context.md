# Lifecycle context retained with fault reports

Problem class: bounded embedded observability, concurrent log capture, and
preserving startup context after routine status chatter fills a rolling buffer.
Cam requested startup/SD/speaker timing, connection losses and wireless strength
in RAM, exported as context on an existing selected error. Remote access and
panic/core dumps are outside this change.

## Primary sources and decision

- [ESP-IDF logging](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32/api-reference/system/log.html):
  the vprintf hook must be reentrant because multiple task contexts invoke it.
  Reuse the existing locked, bounded RAM hook; no filesystem/network I/O there.
- [Classic GAP RSSI](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32/api-reference/bluetooth/esp_gap_bt.html):
  `esp_bt_gap_read_rssi_delta` completes asynchronously. The result is a delta
  relative to the Golden Receive Power Range, not absolute dBm. Check address,
  connection and outstanding request; bound requests to one every 30 seconds,
  with a five-second response deadline. Failed requests also respect cadence.
- [Wi-Fi station AP information](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32/api-reference/network/esp_wifi.html):
  `esp_wifi_sta_get_ap_info` reports the associated AP's RSSI. Query only the
  existing primary stack; do not start/provision Wi-Fi on the secondary.

The API contracts were also checked against the pinned local ESP-IDF 5.5.4
headers. Keep first milestones in dedicated slots, first successful SD mount and
config recovery separately, twelve recent lifecycle events and latest signal/loss
observations. This avoids startup eviction by ten-second status output or later
signal samples. Retain selected application/driver history separately. New reports
are v2, bounded to 14,335 bytes in the same eight rotating SD paths. NVS compact
layout stays v1 and about 1 KiB; its tail-only recovery cannot retain full context.

Freeze the ordinary event ring at the first error, then seal lifecycle context
under its capture lock when the existing control owner materializes the report.
That small separation includes a measured failed SD result emitted after the
low-level driver error. Format the report outside the lock; subsequent lifecycle
updates cannot change the sealed report. Healthy telemetry never triggers SD/NVS
writes. Existing idle-only persistence and retry cadence remain unchanged.

## Applicability checks and remaining uncertainty

Production-source host tests exercise pinned startup and recovered mount timing
surviving long status/signal churn, latest signal/disconnect preservation, context
sealing, immutable pending reports and worst-case report bounds/fault-tail
preservation. Radio tests cover both roles, sampling cadence, asynchronous replies,
timeout, ignored late disconnected replies and request failure backoff.

Physical audio/radio qualification is separate. Measure the one-off lifecycle
snapshot copy under the capture critical section and periodic RSSI effect during
paired playback before claiming timing acceptance. Thirty-second Wi-Fi sampling
can miss shorter outages. SDK boot uptime is not an external power-on measurement;
terminal startup completion does not establish audible speech. The logger starts
after NVS/actuator initialization, and no RAM-only history survives power loss
until ordinary fault export or compact persistence occurs.

Machine results: full host sanitizer suites and ESP-IDF 5.5.4 native build/size
passed; private logs and manifest in `build/validation/lifecycle-context-20261101/`.
Final host Core layout is 31,792 bytes; Lifecycle is 5,592 bytes. The RAM allocation
is PSRAM. Firmware grew 6,816 bytes to 2,120,864 bytes; IRAM has only 681 bytes
free, so build fit does not qualify future additions or runtime headroom.
Independent review found no blocker. No hardware was reset or flashed.

## Jeff-only SD export test preparation

General issue found before device test: the old idle predicate classified startup
waiting for an absent speaker as busy forever, preventing even synthetic error
export. [Espressif SDMMC thread-safety guidance](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32/api-reference/storage/sdmmc.html)
recommends ownership/serialization above the protocol driver. Local source shows
startup WAV is fully preloaded/closed before radio begins, and only the existing
control task can change waiting startup into playing. Exclude actual startup
playing from logger I/O; permit silent waiting, preserving show/audio quiescence,
tone and worker-file-close gates. Synchronous SD I/O returns before that same
owner can start playback. Independent review verified this ownership argument.

Added idle-only `fault-saved` serial readback of fixed slots using bounded rb
reads and 64-byte hex chunks, with offsets and successful END/length framing.
Pinned IDF FatFS fstat supplies regular-file mode/size. Host tests verify binary
roundtrip, bounds, nonregular/oversized/missing files and consumer failure without
modifying logs/audio. This is local USB readback for evidence, not network access.
Use the existing fault-test command scheduled by the host at 60 seconds; no
permanent timed-fault code is needed. Device results are recorded in Story 006.

Jeff physical test passed for silent startup, scheduled synthetic-error export,
exact SD readback and byte-identical persistence across reboot. The original
report remained unchanged. See Story 006 for timings, hashes, resource status,
installed image and untested speaker/Anthony boundaries. No automatic test fault
remains in firmware; the host sent the existing fault-test command once.
