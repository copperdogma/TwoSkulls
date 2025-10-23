<!-- 85e8757f-5ad6-49b1-bd3d-8b6fcb1f8a3c ee4db718-dac7-4ac9-a870-2f96280914af -->
# Investigate A2DP Reconnection Delay

## Phase 1: Research ESP32-A2DP Library

1. Search for ESP32-A2DP library configuration options that can be set from user code (not library modifications)

- Look for methods like `set_reconnect_interval()`, `set_connection_timeout()`, or similar
- Check if there are any initialization flags or parameters that affect reconnection behavior

2. Search online for known solutions to ESP32-A2DP reconnection delays after reboot

- Check GitHub issues on pschatzmann/ESP32-A2DP repository
- Look for discussions about NVS pairing data, Bluetooth stack initialization delays
- Identify any workarounds that don't require library modifications

3. Document findings and assess feasibility

- If simple configuration changes exist, note them for implementation
- If solutions are too complex/brittle, proceed to Phase 2

## Phase 2: Add Diagnostic Logging

If no easy solutions found, add timestamps to pinpoint where the delay occurs:

1. Modify `bluetooth_controller.cpp`:

- Add timestamp logging at the start and end of `initializeA2DP()`
- Add timestamps to `connection_state_changed()` callback for all state transitions
- Log the time between A2DP initialization and first connection attempt

2. Modify `TwoSkulls.ino`:

- Add timestamp at the start of `setup()`
- Add timestamp right before `bluetoothController.initializeA2DP()` call
- Add timestamp right after `bluetoothController.initializeA2DP()` call

3. Test and analyze

- Power cycle skull and capture serial logs
- Identify which stage has the 10-20 second delay
- Document findings

## Expected Outcomes

- Identify if delay is in ESP32 boot, your code initialization, or A2DP library
- Determine if there's a simple configuration fix or if this is inherent behavior
- Provide clear recommendation on whether the issue is fixable without library modifications

### To-dos

- [ ] 