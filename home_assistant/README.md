# Home Assistant dashboard

For the complete AI handoff and reuse procedure (including another prop such as
Death), read [the prop-integration guide](../docs/home-assistant-prop-integration.md).
Keep it updated alongside this operator guide and dashboard YAML.

The **Halloween** dashboard groups Jeff's controls in one **Jeff player** card:

1. Choose a skit from the inline **Skit** dropdown.
2. Click **▶ Play** once to start it.
3. **Now playing** shows the active title, then returns to **Idle**.

Selection alone never plays audio. Play follows the firmware's readiness gate;
while playback/startup/cooldown or a missing speaker prevents another start, the
card shows Play as unavailable. Firmware still validates every command.
The separate **Jeff status** card includes speaker/Anthony connectivity, SD,
signal, pending errors, saved reports and a link to the complete device page.
**Open reports** reads saved SD logs into Home Assistant's **Notifications**.

## Current hub

The live server is now on the always-on Mac Mini:
[http://192.168.1.80:8123/dashboard-halloween/props](http://192.168.1.80:8123/dashboard-halloween/props).
The hostname `The-Octagon.local:8123` also passed a LAN check. The instance is
named **Home**; the grouped **Halloween** dashboard remains its current default.
The original account, device/entity IDs, mobile registration and logs were
preserved. Edit the existing phone-app server URL to the Mini address; the old
laptop address is retired. Its containers/images were removed during cleanup;
private configurations and cold rollback backups remain available.
See the [hub migration record](../docs/stories/story-009-mac-mini-home-assistant-hub.md)
for operational paths, storage/restart limits and remaining qualification.

## Jaw tuning (Story010)

**Jeff firmware/UI deployed, owner-accepted and reboot persistence confirmed.**
The compact **Tune jaw** row in Jeff's player opens the dedicated native subview at
`/dashboard-halloween/jeff-jaw`. Live discovery contains 47 entities, including all
21 jaw entities. STOP/Enter/STOP transitions passed. Cam subsequently tested and
accepted the controls; live state shows saved closed/open 654/1404 µs, matching
drafts, no unsaved changes and normal automatic movement resumed. Cam confirmed
these values survived reboot. Jeff was offline during the final closeout probe;
reboot persistence is owner-observed acceptance, not a second agent measurement.
For Anthony, deploy and verify his `anthony_jaw_*` entities before using an
equivalent card. Each skull keeps independent limits on its SD card.

1. Finish startup with the speaker connected, wait for playback to be idle,
   open Jaw tuning, and choose **Take manual control**. Enter holds the current
   command and blocks skits, greetings and idle breathing. It does not require
   selecting a particular skit. If Enter reports `entry_rejected` during automatic
   breathing, choose **Disable jaw output**, then Enter again. The pending-PWM
   guard intentionally remains conservative; Enter from disabled re-enables the
   last held command.
2. Watch the actual linkage. First choose **Open 1 µs** once and check the
   displayed microseconds and physical movement. Continue with deliberate
   **Open 1 µs / Close 1 µs** steps; 10 µs buttons
   are available once you know the direction. Jeff's unattached rod only pushes
   open; teeth contact followed by free rod retraction during closing is normal.
   Back off if opening causes binding or servo strain. There is no automatic
   endpoint search; verify another prop's linkage rather than assuming this design.
3. The numeric **Exact target** is in microseconds, and each accepted change is
   at most 25 µs. Use repeated deliberate small steps for larger distances. The
   session window extends 50 µs beyond the saved range; the firmware rejects
   requests outside that window. Rejections appear in Last tuning result.
4. At a physically acceptable closed position choose **Set closed**. Find an
   acceptable open position and choose **Set open**. These are drafts, shown
   separately from saved endpoints. **Discard** changes drafts without motion.
5. Choose **Save** after checking both drafts. Look for `saved_to_sd` and the
   updated saved endpoints. It applies to normal movement and later boots,
   without moving the held manual position. A failed save leaves drafts visible
   and does not claim persistence. Keep power connected until it returns. Verify
   the saved values after a later controlled reboot. Cam completed this check
   for Jeff; repeat it independently for another skull.
6. Use **Disable jaw output** when finished inspecting, or explicitly **Resume
   normal movement** after saving. Resume discards unsaved drafts and gradually
   returns to normal movement. Closing the app is not a stop command.

STOP, Wi-Fi/MQTT loss or 90 seconds of no accepted tuning input disables pulses
and latches the mode; reconnect does not resume movement. A driver fault can
prevent the disable from completing, so inspect the last applied pulse/result.
Values are software PWM commands, not physical angle feedback or a guarantee
that torque has released. A server/LAN button is not a physical emergency stop.
Stop affects this skull; it cannot recall a peer command already transmitted.

Existing SD cards without keys use 544/1368 µs until the first explicit save.
Normal movement uses `jaw_closed_us`/`jaw_open_us`; different servos may reverse
the numeric direction. The Open/Close buttons account for the saved direction.

## Installation / migration

`halloween-dashboard.yaml` uses only built-in Home Assistant entities and button
rows. No custom frontend, website, HACS plugin or extra ESP32 firmware is needed.
Create a dashboard from scratch under Settings → Dashboards; open its editor's
Raw configuration editor and paste this YAML. Set it as the default under
Settings → Dashboards → Halloween → Set as default. A personal dashboard override
must be set to Auto in the user's profile to follow the system default.

The current instance serves it at `/dashboard-halloween/props`. Entity names are
those discovered on this test instance. On migration, verify the entity IDs and
replace the **All diagnostics** navigation path with Jeff's device-page URL on
the new instance; the registry device UUID is installation-specific.

Primary references: [native entities/button rows](https://www.home-assistant.io/dashboards/entities/)
and [dashboard creation/defaults](https://www.home-assistant.io/dashboards/dashboards/).
The built-in Home Overview has limited customization; a separate native dashboard
supports the grouped player and leaves automatic discovery intact.

## November 2, 2026 verification

Applied through Cam's logged-in Chrome UI, saved and set as system default.
All 11 options appeared inline. Selecting another skit changed Jeff's acknowledged
selection while he stayed idle, and the original Pumpkin flowers choice was
restored. Clicking the actual visible Play button was accepted: telemetry passed
through preparing → playing → idle, showed the selected title and then Idle,
kept the speaker connected and produced no pending fault or additional SD report.
This verifies the UI/device command path, not independently heard audio quality.
Anthony was disconnected; only Jeff's local B part was tested.

Private screenshots and read-only API observation are under
`build/validation/story-007-ha/player-dashboard/`. No firmware flash was needed.
