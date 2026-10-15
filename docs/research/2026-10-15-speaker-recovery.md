# Established speaker connection loss — 2026-10-15

## Problem and decision

This is an asynchronous connection recovery problem: a successfully established
A2DP session can disappear after the speaker powers off. Recovery must distinguish
that event from a failed connection attempt, and must wait for a surviving Classic
ACL to close before opening another connection.

The legacy controller enables `set_auto_reconnect(true)`. The native port only
scheduled recovery for a failed remembered-address connection attempt; after a
successful connection, its `direct_fallback` flag was false, so a later disconnect
could leave the installation silent until a serial `retry` or reboot.

The pinned [Espressif A2DP source example](https://github.com/espressif/esp-idf/blob/v5.5.4/examples/bluetooth/bluedroid/classic_bt/a2dp_source/main/main.c)
handles established DISCONNECTED by returning to its unconnected state, whose
heartbeat requests a new connection. Adopt that state transition with this
project's stricter existing bounds: reset the three-attempt cached-address budget
on a successful connection; on subsequent session loss, wait for actual ACL
teardown if still connected, otherwise schedule a direct retry after five seconds.
Failed retries consume the budget rather than resetting it. After three direct
attempts, retain one finite configured-name inquiry. The existing 60-second ACL
wait, 75-second connection watchdog, and explicit-retry terminal holds remain.

Only the radio worker mutates recovery state. No Bluetooth stack restart, NVS
erase, forced cancellation of an opening ACL, or replay of interrupted audio is
added. Initial inquiry-selected connection failures retain the finite existing
policy; this change specifically restores recovery after a successful session.

## Verification and remaining uncertainty

The actual `skull_radio.cpp` compiles with the project's recorded Xtensa command.
An isolated host harness extracted its actual connection-event branch and passed
eight cases: successful connection resets the episode budget; established loss
with ACL already down schedules a five-second retry; loss with ACL still up waits
for teardown; failed direct attempt preserves its consumed budget; exhausted
direct attempts seek once; initial inquiry-selected failure does not recurse;
a stopped recovery episode remains stopped; an unselected speaker is ignored.
This harness checks decisions, not Bluetooth controller ordering or timing.

Physical speaker off/on recovery and subsequent paired playback remain required.
An interruption cancels local audio through the existing show controller; recovery
must not automatically replay the interrupted skit. A lost BLE peer during an
already-started skit remains the legacy finish-local policy. Coordinated abort of
both skulls would require a separate protocol decision and is not introduced here.
