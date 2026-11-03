# Home Assistant prop integration — AI implementation and operations guide

Owner: TwoSkulls project. Last updated: November 2, 2026.
Audience: an AI implementing this approach in another prop project, including
Death, or maintaining TwoSkulls. This is the handoff entry point; keep it current
as the implementation and its qualification change.

## The result to reproduce

Use Home Assistant as the local prop control application. The firmware exposes
typed status and bounded commands through MQTT discovery; a native HA dashboard
turns those entities into a useful interface. The phone app connects to the HA
server. Neither the phone app nor MQTT discovery alone creates this finished UI.

Cam tested and strongly approved the **Halloween** dashboard, after finding the
separate generic Current skit / Skit / Play entity cards confusing. This is the
preferred approach for future HA work:

- One **player/control card per prop**, with an inline script/skit dropdown, a
  visible **▶ Play** action, and **Now playing** underneath. Selection does not
  start playback. Idle is explicit. The operator should not need a history dialog
  or Home Assistant's separate Scripts feature to play a prop's script.
- A separate **status card** with understandable connection, storage, error and
  signal information, plus saved-log access and a complete diagnostics link.
- Make this dashboard the default and leave it easy to find in the sidebar.
  Test what the operator actually sees and does, including the phone app when
  available. Discovery/device pages remain useful for deeper diagnosis.
- Start with built-in HA cards. This implementation needs no custom website,
  HACS card, custom frontend JavaScript or replacement prop-control framework.

The working dashboard source is
[home_assistant/halloween-dashboard.yaml](../home_assistant/halloween-dashboard.yaml).
The short [operator/dashboard guide](../home_assistant/README.md) describes its
current installation. Home Assistant calls its automation objects *scripts*;
our skits are firmware assets exposed through a Select entity, not HA Scripts.

## Quick handoff for another AI

Read this guide and the destination project's own instructions first. Inventory
its firmware, existing network provisioning, HA server/broker and script catalog.
Reuse the existing server/broker when appropriate. Implement discovery and status,
then bounded selection/play commands through that project's existing control
owner. Adapt the dashboard YAML to the discovered entity IDs and the prop's
terminology. Install it and set it as default. Demonstrate selection without
playback, one actual Play, the displayed active title, return to Idle and offline
behavior. Add saved-log retrieval only if its protocol and receiver are supported.
Record exactly what was demonstrated and what still needs hardware observation.

Do not assume this guide grants permission to alter another project's hardware,
network, accounts or public exposure. Follow that project's current authorization
and evidence-preservation rules. Do not flash first and inspect logs afterward.

## Architecture and source map

```text
Prop firmware <-- MQTT --> authenticated broker <-- MQTT --> Home Assistant
   |                                                      |
   +-- existing radio/show/storage/control owner           +-- native dashboard
                                                          +-- phone/browser app
                                                          +-- optional log bridge
```

| Owning source | Responsibility / reuse boundary |
| --- | --- |
| [skull_ha_model.h](../firmware/main/skull_ha_model.h), [implementation](../firmware/main/skull_ha_model.cpp) | Portable bounded discovery/state JSON, exact validated catalog mapping. Device name, role, fields and topics are TwoSkulls-specific. |
| [skull_network.cpp](../firmware/main/skull_network.cpp) | Native ESP-IDF Wi-Fi/MQTT, identity, subscriptions, reconnect, mailbox, readiness and log streaming. Adapt to the destination's SDK and control ownership. |
| [network configuration](../firmware/main/skull_network_config.h) | Read-only optional SD configuration and explicit input limits. Do not replace an existing provisioning system blindly. |
| [skull_show.cpp](../firmware/main/skull_show.cpp) | Existing local/paired skit execution, exact asset validation, readiness and cooldown. Pairing semantics are not generic. |
| [dashboard YAML](../home_assistant/halloween-dashboard.yaml) | Built-in grouped player and status cards. Adapt actual entity IDs and installation-specific diagnostics URL. |
| [saved-log component](../home_assistant/custom_components/twoskulls_diagnostics/README.md) | Optional custom **backend integration**, not a frontend card. Strict TwoSkulls namespace and transfer contract. |
| [Story 007](stories/story-007-home-assistant.md) | Implementation history and private evidence pointers. Later acceptance supersedes early no-speaker limitations. |
| [MQTT throughput research](research/2026-11-02-mqtt-log-export-throughput.md) | Source-backed explanation and measured correction for slow asynchronous log transfer. |
| [native firmware guide](../firmware/README.md#home-assistant-mqtt) | TwoSkulls networking, maintenance and resource details. |

MQTT plus a native dashboard is the reusable pattern. Copying the entire radio,
Matter, show or SD implementation into Death is not required. Its firmware should
retain its own tested physical behavior and task ownership.

## 1. Reuse or establish the HA server and broker

1. Identify the actual HA server URL, version, MQTT broker host/port, discovery
   prefix, authentication and transport. A phone app requires a running server.
   An ESP32 cannot use the Mac's `localhost` or a Docker-only hostname as its
   broker address: use an address reachable from the prop's network.
2. If starting fresh, follow the official [HA installation guide](https://www.home-assistant.io/installation/)
   for the chosen host. Check disk/RAM first; the HA image and its unpacked data
   required several GB of free space in this Mac experiment. Container and HA OS
   installations differ; do not assume a Container installation has add-on apps.
3. Configure an authenticated broker, such as Mosquitto. Its
   [authentication guide](https://mosquitto.org/documentation/authentication-methods/)
   covers password files; preserve an existing file rather than recreating it
   with an overwrite option. Set topic access appropriately for the destination.
   Keep secrets in private configuration, not source, screenshots, chat or shell
   arguments/history. Authentication on plain `mqtt://` does not encrypt traffic.
4. In HA: **Settings → Devices & services → Add integration → MQTT**. Configure
   the same broker the prop uses. Verify MQTT discovery is enabled with the
   expected prefix (here `homeassistant`). Broker connectivity alone does not
   prove that discovery, state, commands and device execution all work.
5. Complete HA location/time-zone/country settings. This test instance uses
   Canada and America/Edmonton; another installation must use its own settings.
6. Add the server to the Companion app on the same reachable network and sign
   into the intended user. LAN operation needs no public DNS, port forwarding or
   custom domain. Off-LAN remote access is a separate deployment decision.

### Current whole-house hub: Mac Mini

Cam selected the always-on Mac Mini as the primary whole-house Home Assistant
hub. The live HA/MQTT setup has been migrated there; the laptop's two temporary
server containers/images were removed during cleanup, with private data and
cold backups retained for deliberate rollback. See the
[migration record](stories/story-009-mac-mini-home-assistant-hub.md) for evidence,
remaining qualification and the Mini-side coordinating chat.

Last verified November 2, 2026:

| Item | Current installation |
| --- | --- |
| Host | Mac Mini, `The-Octagon.local`, user `occam` |
| HA | Container `home-assistant`, HA **2026.10.4**, pinned amd64 image |
| Broker | Container `home-mosquitto`, Mosquitto **2.1.2-alpine**, pinned amd64 image |
| HA address | [http://192.168.1.80:8123](http://192.168.1.80:8123); [hostname URL](http://The-Octagon.local:8123) also resolved and returned HTTP200 from the laptop |
| Broker for props | `mqtt://192.168.1.80:1883`, existing private authenticated credentials |
| Instance name / locale | Home; Canada; America/Edmonton |
| Dashboard | `/dashboard-halloween/props`, preserved as the useful current default |
| Runtime owner | Existing Colima Linux VM and its existing Mini startup supervisor; no competing VM |
| Service reconstruction | Mini repository `/Users/occam/Developer/openai-bishop/services/home-assistant/compose.yaml` and its `README.md` |
| Private live data root | `/Users/occam/Library/Application Support/HomeAssistant/` — HA in `config/`, broker config/passwords in `mosquitto/config/`, persistence in `mosquitto/data/` |

The grouped dashboard, account, mobile registration, registry UUIDs and private
report cache were preserved. Jeff's separate NVS broker URI now points to the
Mini; SD Wi-Fi settings, assets, stored logs and firmware were not rewritten.
A new prop should target this broker, not the retired laptop IP. Other offline
props still need their own explicit endpoint/configuration update and testing.

This is a LAN installation. The numeric address is currently DHCP-assigned;
a router reservation has not been verified. Use the verified hostname for HA
clients when suitable, and establish a reservation before relying on the numeric
MQTT address indefinitely. Do not claim configured permanent addressing, public
DNS, off-LAN access or all multicast/HomeKit/Matter discovery is qualified.
No unrelated household devices were enrolled as part of migration.

The Mini's small internal disk remains tight. Image download/startup were
monitored against a **2 GiB physical-host-free cutoff** and existing household
service health. Guest virtual-disk free space is not physical host reserve. The
existing 10% free-space monitor is a warning, not an HA minimum or blanket
prohibition. Measure space and memory again before updates; preserve a bounded
rollback rather than pruning unrelated containers/Books data or compacting the
live shared VM. A second always-running HA clone can issue duplicate commands:
keep the retired laptop runtime inactive during normal operation.

The Mini's existing startup supervisor starts Colima at login; container policies
provide the next layer. Service restart proof is distinct from a host reboot.
That existing owner currently waits for the NAS, and the shared Colima profile
mounts `/Volumes/Data`. Do not simply move its NAS gate: Bookshelf's restart
policy and writable NAS binds could then start it against an absent mount.
NAS-independent cold startup requires a reviewed shared-mount/ownership change
and controlled startup test, not an extra competing HA supervisor. The live
migration leaves that existing behavior unchanged.
The host has a previously documented AC-return startup failure, so do not promise
unattended recovery after a power outage. Follow its existing reboot-resilience
procedure for any future host test; do not improvise reboots or change NAS topology.

The Mini owns its protected live configuration; inspect its operating README
before maintenance. Private migration/validation artifacts remain under ignored
`build/validation/story-007-ha/mac-mini-migration/` on the laptop. The coherent
cold backup includes HA auth/configuration and separately preserved broker data.
A temporary source-restricted upload key was revoked after checksum verification
and its laptop private key removed. Do not commit any archives, passwords or
HA tokens. The public implementation snapshot has a reconstruction base commit
because current TwoSkulls HA work was not yet pushed when it was transferred.

Backup operations were updated on November 2 by the Mini owner (peer commit
`d0846bd`): existing daily jobs now run guarded stable/eligible HA/MQTT update
maintenance at 02:25 Edmonton with a verified cold backup and latest-two local
backup/rollback retention, followed by 03:05 health checks. The actual
1,367,629-byte backup `20261102T193613586393Z-cc6b0c35` was verified; no image
upgrade occurred in that check. Offsite coverage and isolated restore remain
unverified. Maintain the Mini's current operating notes rather than inferring
full disaster-recovery acceptance from these local backup checks.

For authorized checks from this laptop, the private migration helper
`python3 build/validation/story-007-ha/mac-mini-migration/mini_api.py` tests the
new HA API using preserved private auth, with direct LAN requests. It is not a
portable installation dependency. Use the logged-in browser or protected local
authentication; do not ask Cam to paste credentials into chat.

### Legacy laptop runtime — rollback only

The retired containers were `twoskulls-story007-ha` and
`twoskulls-story007-mqtt`; both and their unshared images were removed during
the November 2 cleanup. Old HA was `http://192.168.1.99:8123` on the LAN.
The ignored `build/validation/story-007-ha/` tree retains their private configuration
and helpers. Its original `ha_api.py` targets localhost and will not reach the
current Mini server. Do not recreate this runtime casually or treat its
old MQTT URI as the current hub. The source broker used persistence=false;
the permanent target uses persistent broker storage. A deliberate rollback must
coordinate source/target activation and restore Jeff's broker URI together.
Protected container definitions, image digests and retained logs are under
`build/validation/cleanup-20261102/` in the `jaw-articulation` worktree. Rollback
requires pulling the recorded images and recreating containers with the retained
bind configuration; existing `docker start` commands no longer apply. See the
[cleanup record](research/2026-11-02-project-storage-cleanup.md) for build/debug
archive recovery and retention boundaries.

## 2. Give the firmware a clear HA contract

### Identity, discovery and availability

Use a stable hardware-derived identifier, distinct from its editable display
name and IP address. TwoSkulls uses `twoskulls_<12 lowercase MAC hex digits>` from
the station MAC; Jeff currently is `twoskulls_f42dc978ad78`. Give another prop its
own identity and unique MQTT client ID; two devices must not impersonate each
other. HA registry entity IDs and device UUIDs are separate identifiers.

Publish retained device discovery at
`homeassistant/device/<device-id>/config`. Group related components in its `cmps`
object, with stable per-component `unique_id`, device metadata, readable names
and an application version plus build fingerprint. Republish after MQTT reconnect
and HA birth (`homeassistant/status` payload `online`). Discovering a device
does not automatically populate our desired dashboard layout.

The current base topic is `halloween/<device-id>`:

| Topic suffix | Payload / ownership | Persistence |
| --- | --- | --- |
| `state` | Firmware typed JSON snapshot, approximately every 5 s | QoS0, non-retained |
| `availability` | Firmware `online`; broker Last Will `offline` | Retained; Last Will QoS1 |
| `play_available` | Firmware `online` only when a new play is permitted, else `offline` | Retained readiness gate |
| `select/set` | Exact advertised catalog option, e.g. `Skit - pumpkin flowers.wav` | Command QoS0, retain=false |
| `play` | Exact literal `PLAY` | Command QoS0, retain=false |
| `logs_available` | Firmware `online` only when read-only SD export is permitted | Retained readiness gate |
| `logs/request` | HA dashboard/button sends literal `REFRESH`; **HA bridge consumes it** | Non-retained |
| `logs/get` | HA bridge sends a scoped fixed-slot GET; **firmware consumes it** | Non-retained |
| `jaw/set` | Story010 bounded tuning actions; see below | Command QoS0, retain=false |
| `jaw_available` | Story010 `online` only during manual control with command subscriptions ready | Retained readiness gate |
| `logs/<request-id>/…` | Firmware begin/chunk/end/error response | QoS1, non-retained |

Top-level device availability governs normal components. Play requires both node
availability and `play_available`, with `availability_mode: all`. Log reads use
the equivalent combination with `logs_available`. Current diagnostic sensors
also use `expire_after: 20` to avoid presenting frozen telemetry as live.
The actual broker keepalive is 15 s; the observed offline transition was about
23 s, not instantaneous. Readiness is enforced again in firmware.

For a new prop, a Select needs its `options`, `command_topic: <base>/select/set`,
`state_topic: <base>/state`, `value_template: "{{ value_json.selected_skit }}"`
and `retain: false`. A Button needs `command_topic: <base>/play`,
`payload_press: PLAY` and `retain: false`. The current title sensor reads
`value_json.current_skit`. The production model above is the complete working
discovery example; it handles inherited topics, availability and bounded escaping.
Verify compatibility with the installed HA version before changing its schema.

### Selection, action and state must remain separate

| Meaning | Current TwoSkulls field/control | UI label |
| --- | --- | --- |
| Requested next item | `selected_skit`, MQTT Select | Skit (or Script for another prop) |
| Execute that item | MQTT Button, `PLAY` command | ▶ Play |
| What is actually running | `current_skit` sensor | Now playing |
| Internal lifecycle | `show_state` sensor | Detailed diagnostics |
| Last command acknowledgement | `command_result`: `selected`, `accepted`, `not_ready`, etc. | Detailed diagnostics |

Publish the validated selection back from the device. Do not treat a browser's
optimistic selection as an execution acknowledgement. In TwoSkulls, options are
exact basenames of catalog-validated scripted WAV assets, preserving case and
extension. Startup/Marco/Polo and invalid/ambiguous paths are excluded. Selection
is RAM-only and resets to the first valid catalog entry on reboot. It is not
the last-running title. An empty `current_skit` becomes `Idle` only when
`show_state` is idle; empty during another lifecycle state becomes `Unknown`.
Missing measurements are `null`/Unknown rather than invented zero values.

A different prop can expose scripts or routines instead of WAVs. Adapt the
catalog/validation and terminology while preserving these three meanings.
Never silently convert display labels into executable paths. Do not expose an
arbitrary file path, shell, UART console or unaudited command executor through HA.

### Command execution belongs to the existing control owner

MQTT callbacks copy a bounded command; the prop's existing owner validates and
executes it. No SD I/O, show execution or actuator work runs in the callback.
TwoSkulls requires successful command SUBACKs before enabling controls, permits
one pending command shared across selection/play/log reads, rejects stale
commands older than 2 s or from a previous connection epoch, and rejects incoming
fragmented, duplicate or retained-at-subscribe controls. Extra commands are
dropped, not queued for later playback. Validate exact catalog membership again
at execution time. Test pending-work cleanup on disconnect/reconnect.

With MQTT v3, a broker can clear RETAIN when forwarding a live retained publish;
the current filter prevents stored replay, not every live publish originally
sent with retain enabled. HA actions explicitly publish retain=false. Do not
claim stronger replay protection than the protocol and tested implementation.

TwoSkulls Play requires initialized local control, completed startup/greeting,
idle/no pending show, ready speaker stream, no competing audio and elapsed
cooldown. For Death, implement its actual motor/actuator/content readiness rather
than copying speaker assumptions. Keep local controls functional when HA is down.

Anthony plays A and requests Jeff's B part only if Jeff is ready when accepting
the skit; otherwise Anthony can play A alone. Direct Jeff control plays B. A
failed already-started paired transaction aborts without an automatic solo
replay. These are TwoSkulls semantics, not a universal prop protocol.

### Diagnostics to expose

Map only measurements the destination actually has. The Story007 baseline exposes
23 diagnostic entities plus Select, Play and Open saved reports (26 with valid
skits). Story010 is now deployed on Jeff, adding 21 jaw entities: all 47 live
entities and the expected jaw names were verified. Anthony deployment is pending. `report_slots` is an additional raw state field, not a separate entity.

| Group | Raw state fields / interpretation |
| --- | --- |
| Connections | `speaker`, `peer`, `stream`, `sd`, `error_pending` are JSON booleans |
| Activity | `show_state`, `current_skit`, `selected_skit`, `command_result`, `role`, `firmware` |
| Runtime | `uptime_s`, `free_heap`, `min_free_heap` (internal heap bytes) |
| Radio | `wifi_rssi_dbm` in dBm; `bluetooth_rssi_delta` is a controller-relative delta, **not dBm** |
| Startup | `control_ready_ms`, `sd_mount_ms`, `sd_config_ms`, `speaker_connected_ms`; unknown until measured |
| Counters | `sd_attempts`, `sd_failures`, `speaker_disconnects`, `report_count` |
| Saved evidence | `report_id` as a hex string, `report_slots` integer bitmask 0–255 |

Do not put full logs into sensor state/attributes or recorder history. Retain
startup/lifecycle context in bounded firmware memory and attach it to saved faults
as appropriate. This is separate from continuous telemetry and from core dumps.

### Per-prop jaw tuning — Story010 deployed on Jeff

[Story010](stories/story-010-sd-jaw-calibration.md) extends the same MQTT/native
dashboard pattern with a dedicated maintenance subview. The checked-in example
links Jeff's main view to `/dashboard-halloween/jeff-jaw`; adapt `jeff_jaw_*` to
`anthony_jaw_*` for Anthony after discovery. Keep this feature visibly separate
from the everyday skit player. Jeff now has matching firmware and live dashboard: a
compact Tune jaw row opens the numeric/fine/coarse maintenance view, preserving
the existing Props cards. Configuration was backed up before the live edit.
STOP/Enter/STOP verified disabled 0 → manual 544 → disabled 0 applied us; endpoint
steps were then owner-accepted: Cam saved closed/open 654/1404 us and resumed
normal movement. A read-only follow-up confirmed those active values, matching
drafts and no unsaved changes. Cam then confirmed reboot persistence. Anthony's
deployment/calibration remains pending; the final agent probe found Jeff offline.

The portable policy is [skull_jaw_control.h](../firmware/main/skull_jaw_control.h),
the strict command parser is [skull_jaw_command.h](../firmware/main/skull_jaw_command.h),
and the PWM owner is [skull_actuators.cpp](../firmware/main/skull_actuators.cpp).
Configuration/storage retain their own per-prop SD files. Add backend adapters
to another project rather than copying this prop's PWM/radio ownership blindly.

`jaw/set` accepts exact literals `ENTER`, `STOP`, `RESUME`, `CLOSED`, `OPEN`,
`SAVE`, `DISCARD`, `STEP:OPEN1`, `STEP:CLOSE1`, `STEP:OPEN10`, `STEP:CLOSE10`,
and `TARGET:<integer>`. Signed `STEP:+1/-1/+10/-10` is supported for raw pulse
direction. Open/Close uses saved endpoint orientation. No commands are retained;
firmware rejects duplicates, fragmented/invalid payloads, wrong connection epochs
and commands older than 2 seconds. STOP replaces queued mailbox work.

Enter requires finished startup, idle show/audio/tone and no active report
transfer. Speaker-off commissioning before the initialization announcement
finishes is intentionally unavailable. Enter during automatic breathing may return
`entry_rejected` while PWM is pending; STOP then Enter is the demonstrated path.
Do not weaken that conservative hardware guard. All playback entry points are inhibited
while manual or disabled; STOP also cancels local active/prepared playback.
Another skull's already-transmitted command cannot be recalled by this action.
Only the existing control task touches PWM or persists endpoints.

Sensors expose `jaw_mode`, `jaw_result`, `jaw_command_us`, `jaw_applied_us`,
`jaw_closed_us`, `jaw_open_us`, `jaw_draft_closed_us`, `jaw_draft_open_us`;
`jaw_dirty` is binary. These pulse values are commands, never measured jaw
angles. The `jaw_target` Number uses an inline numeric box and acknowledged
device state, with fine/coarse action buttons. Manual state publishes every 250 ms
and after commands; normal state retains the five-second interval. Availability
requires the node plus manual mode and all four command subscriptions for manual
actions. New jaw discovery sets explicit `default_entity_id` to avoid UI guessing.

SD keys `jaw_closed_us`/`jaw_open_us` default544/1368 on old cards. Normal
gestures interpolate through these endpoints, including reversed mappings.
Manual commands move at most 25 us each, within a fixed session envelope of the
starting saved span plus 50 us, clipped to 544..2400. These limits do not establish
the linkage's safe travel. Captures change drafts; explicit Save verifies both
endpoints on SD before activating them without moving the held position. Keep
unsaved drafts, saved endpoints and failure state distinct. No automatic sweep
or boot-time default-migration write. Preserve other card settings and unresolved
`config.jaw.*` recovery evidence; FAT replacement is not power-fail atomic.

STOP, Wi-Fi/MQTT loss or 90 seconds without accepted manual input requests zero
PWM and leaves the mode disabled. Reconnect never silently resumes. The browser
can disappear while MQTT stays online, so inactivity is the fallback; instruct
the operator to STOP/Resume before leaving. Explicit Resume discards drafts and
slews toward normal movement at most 10 us/20 ms. Re-entering from disabled mode
explicitly re-enables the last held command, not a sensed physical position.
Driver errors latch a fault until reboot, while zero-duty retries continue even
after an eye failure. Pending/failing PWM may prevent disable; the applied pulse
and result must remain truthful. A LAN control is not a physical emergency stop.

The discovery buffer is 24 KiB in PSRAM, with a 32 KiB MQTT outbox limit; sample
discovery is about 14.7 KiB/47 components and maximal-catalog host encoding passes.
Jeff deployment observed free/minimum internal heap 46,075/28,439 bytes. Live HA
rendering/discovery and bounded control transitions passed; see Story010 for
flash/rollback and SD reset-recovery evidence. Cam's later control/calibration
acceptance, live saved 654/1404 us and owner-confirmed reboot persistence are
recorded there. Qualify Anthony's own mechanism separately.

## 3. Configure TwoSkulls networking, or adapt existing provisioning

The active implementation is native ESP-IDF 5.5.4 / ESP-Matter 1.5.0 under
`firmware/`, not the historical Arduino runtime. Optional existing SD
`/sdcard/config.txt` fields are:

```ini
wifi_ssid=YOUR_NETWORK
wifi_password=YOUR_PASSWORD
mqtt_uri=mqtt://BROKER_LAN_HOST:1883
mqtt_username=YOUR_MQTT_USER
mqtt_password=YOUR_MQTT_PASSWORD
```

These are placeholders. Preserve role/speaker and other existing settings. The
parser trims values without quote/escape syntax, limits input to 4096 bytes and
lines to 255 bytes, and never logs values. Wi-Fi SSID/password limits are 32/64
bytes; additional broker field limits are in `skull_network_config.h`. The broker
URI has a host and optional port, without embedded userinfo or a path.

Jeff starts a Wi-Fi station from SD settings with RAM SDK storage. Anthony's
existing Matter stack remains the primary Wi-Fi owner; do not instantiate a
second owner or erase/copy fabrics. No new AP or Matter node is added to Jeff.
Missing optional network configuration leaves ordinary local operation available.

If SD has no MQTT URI, an idle USB maintenance command can store a separate NVS
broker fallback: `mqtt-config mqtt://BROKER_HOST:1883 USERNAME PASSWORD`, then
reboot. `-` means an empty auth field; this USB format cannot represent spaces.
SD broker settings take precedence. Do not paste real command values into public
notes or logs. `mqtts://` uses SDK certificate-bundle verification, but a production
TLS broker was not device-qualified in these tests.

For another prop, prefer its existing secure provisioning/config mechanism.
Inspect the actual board, partitions, resource margin and logs before flashing.
TwoSkulls application-only updates use its verified native layout; never copy
the offset or flash commands to another board without verifying that layout.

## 4. Install the operator dashboard — this is part of the feature

1. Wait for real discovery and fresh telemetry. Open **Settings → Devices &
   services → MQTT → the prop** and verify actual entity IDs and catalog options.
   The HA registry can retain old IDs or add suffixes; do not infer IDs solely
   from display names. Find the destination device page's actual URL as well.
2. Copy [the dashboard YAML](../home_assistant/halloween-dashboard.yaml) as a
   starting point. Replace every Jeff entity with its real counterpart, the
   card titles and script labels, and the diagnostics navigation URL. That
   device-registry UUID is installation-specific and must not be copied to Death.
   Adapt or remove status rows for unsupported hardware; never show fake values.
3. **Settings → Dashboards → Add dashboard → New dashboard from scratch**.
   Use a useful name (here Halloween), a URL containing a hyphen (here
   `dashboard-halloween`), show it in the sidebar and allow its intended users.
4. Open the dashboard → Edit → dashboard menu → **Raw configuration editor**.
   Paste the adapted YAML, Save, close the editor and Done. The current layout is
   two built-in **entities** cards. The player contains the inline Select,
   conditional button row labeled **▶ Play**, and a read-only **Now playing** row.
   The status card contains diagnostics and explicit Open reports / View actions.
5. The Play row shows the actionable button only while its entity is available;
   otherwise a read-only unavailable row replaces it. The button calls
   `button.press` on the actual Play entity. Do not accidentally use the default
   more-info action or require a user to hunt for Press inside a history dialog.
6. In **Settings → Dashboards → Halloween menu → Set as default**, confirm the
   system default. In an existing user's Profile → Appearance, a specifically
   chosen dashboard overrides that setting; use **Auto (use system settings)**
   to follow it. Check the phone under the intended account and refresh if needed.
7. Reload and verify the saved result. All relevant operations should work from
   the visible card without opening generic History/Activity dialogs. Keep deep
   diagnostics accessible, rather than crowding the player with technical state.

HA's built-in Home Overview supports favorites but limited card customization.
Use a separate native dashboard for this grouped layout. Existing discovery
continues to work; new props still require their intended operator cards to be
added. The old Overview remains accessible through dashboard settings and may
remain visible when a user has a personal override.

## 5. Optional saved SD log access

Standard MQTT discovery does **not** fetch files or implement this log viewer.
Use the [receiver installation and protocol guide](../home_assistant/custom_components/twoskulls_diagnostics/README.md)
with its owning Python implementation and tests. To install on HA, copy the
`twoskulls_diagnostics` directory to `custom_components`, configure MQTT, add the
empty `twoskulls_diagnostics:` section to `configuration.yaml`, check configuration
and restart HA. No frontend resource registration is required.

Press **Open reports**, then open HA **Notifications** for an index and full
literal report text. This snapshots existing fixed SD fault files while idle; it
does not delete/modify the card, stream live logs or create an HTTP file server.

**Death reuse boundary:** the receiver currently validates only
`twoskulls_[0-9a-f]{12}` IDs and `halloween/...` topics. A `death_...` device does
not become supported by changing discovery or dashboard names alone. Either
implement a compatible consciously chosen contract, or adapt/configure its ID
validation, state/request/response regexes, MQTT subscriptions and service/cache
namespace as a coherent backend change with tests. Do not masquerade as Jeff.
Also adapt the receiver's role-to-display-name mapping (currently primary →
Anthony, secondary → Jeff), notification branding, and manifest/service metadata;
renaming topics alone must not leave a Death prop displaying Jeff's name.
The service domain is `twoskulls_diagnostics`; inspect the source before claiming
it is a universal receiver. A new prop can ship scalar diagnostics/control first
and explicitly defer this optional log feature.

The current collector requires fresh non-retained state (≤20 s old) with the
`report_slots` bitmask. It reads slots 0–7 sequentially, one fetch per prop and up
to four props concurrently. The HA service's `device_id` is the stable MQTT ID,
not the HA registry UUID. It sends `GET <16 lowercase hex nonce> <slot>` to
`logs/get`. Firmware closes the SD file after making a bounded immutable PSRAM
snapshot, then sends `begin` JSON (slot/bytes), `chunk/<offset>` raw bytes, and
`end` JSON (bytes/8-hex standard CRC32), or an explicit error.

Limits: 14,335 report bytes, ≤256 bytes per chunk, one QoS1 frame in flight,
20-second device deadline, 30-second collector deadline. Identical QoS1 duplicates
are accepted idempotently; wrong request/slot, changed duplicates, offset errors,
corruption, incomplete or oversized data fail closed. PUBACK acknowledges the
broker, not verified HA consumption. No automatic retry or reconnect collection
is added. The receiver verifies full bytes before updating its private
content-addressed cache under `.storage/twoskulls_reports/<device>/` and displaying
escaped text. Earlier verified versions survive slot rotation and repeated reads
deduplicate. The cache has no automatic deletion policy. Notifications can be
dismissed; HA restart may clear the view while cached bytes remain.

### Known asynchronous-transfer pitfall

The first real 5,689-byte transfer timed out with the SDK's default 1000 ms MQTT
poll cadence. The owner enqueued each next frame after PUBACK while the worker
had already gone back to polling. A metadata-only trace and pinned source review
identified throughput, rather than a reason to add blind retries. The supported
ESP-MQTT configuration used here is:

```ini
CONFIG_MQTT_USE_CUSTOM_CONFIG=y
CONFIG_MQTT_POLL_READ_TIMEOUT_MS=50
```

Changing defaults alone does not override an existing generated `sdkconfig`.
Verify the effective setting, build/resources and SDK applicability. Afterward
the same report took about 2.049 s and matched the SD bytes. Lower polling means
more wakeups; active-audio CPU/coexistence still needs qualification. Do not copy
this setting to an unrelated SDK as a universal fix. See the linked research for
sources, unchanged deadlines, config diff and remaining limits.

## 6. Acceptance: demonstrate the complete workflow

Use deterministic production-source tests for parsing, catalog validation,
payload bounds, readiness, command ownership and protocol failures. For TwoSkulls,
run `python3 scripts/run_host_tests.py` (C++17, ASan/UBSan), then the native build
and linker-size check from [the firmware build guide](../firmware/README.md#build).
Another project needs corresponding tests/builds, not a claim based on ours.

Also complete an actual instance/device/UI pass:

| Check | Required observation |
| --- | --- |
| Discovery | One correctly named device, stable identity, complete supported entities, accurate version/build and full validated option list. |
| Selection | Use the **visible dropdown**; device acknowledges the exact option and stays idle. |
| Play | Use the **visible ▶ Play button** once; command accepted; actual current title appears; expected physical action occurs; return to Idle. Record human audio/motion observation separately from telemetry. |
| Readiness | Missing speaker/content, startup, busy or cooldown prevents invalid starts. UI follows the gate; firmware independently rejects invalid requests. |
| Failure inputs | Unknown/path-traversal option, stale/duplicate/fragmented/retained stored commands and command bursts do not start unintended work. |
| Reconnect | HA restart and broker/device reconnection restore discovery/fresh state without replaying an old action. Offline/expired state becomes unavailable. |
| Dashboard | Persist after reload, system default set, profile override checked, clear labels and no required history dialogs. Verify on the user's app when available. |
| Diagnostics | Values/units/unknowns are honest; IP/name/registry identity are not confused; state changes are live. |
| Logs, if included | Fresh fixed-slot listing, original SD bytes vs verified HA cache by hash, complete literal notification text, explicit busy/timeout/failure; no partial report presented as complete. |
| Resources | Measured flash/IRAM/internal heap and playback CPU/audio behavior on the actual target; no assumptions from idle-only success. |

Capture existing status, pending faults and saved SD evidence before resets,
flashes or destructive experiments. Preserve rollback binaries and verify actual
board/layout first. Keep raw evidence private/ignored. Reversible dashboard-only
changes do not require another firmware flash or needless reboot.

**Server readiness is not entity readiness.** On the Mini's two-service recovery
test, HTTP200 preceded usable discovered entities by about three minutes. The
broker had retained online/discovery plus fresh non-retained state while HA's
entities initially appeared unavailable. They recovered without another device,
service or credential change. Before accepting a restart, wait for fresh numeric
uptime and an available Select with the actual option, then acknowledge a harmless
selection while idle. Do not misdiagnose the early unavailable window as a
permanent device fault, or declare success from a retained online/canary alone.
HA's documented discovery/birth/state startup behavior is the relevant reference;
the duration here is an observation, not a guaranteed delay for other systems.

## 7. Troubleshooting by layer

| Symptom | Check next |
| --- | --- |
| Phone has no controls | Correct HA server/account/network; app is a client, not the server. Then select Halloween and check profile/default override. |
| Three generic cards / a history dialog | Old Home Overview or more-info action. Open the grouped dashboard; ensure button uses `button.press` and selector is inline. |
| Broker connected, no device | Discovery enabled/prefix, actual retained config, payload validity/size, unique IDs and HA birth/republication. A valid broker login is not discovery acceptance. |
| Wrong/missing options | Inspect the prop's validated catalog and exact strings, not a guessed display-label transform or HA Scripts list. |
| Play unavailable | Real readiness, speaker/media connection, startup, show/audio busy, cooldown, node availability and successful command subscriptions. Do not bypass the firmware gate. |
| Click seems to do nothing | Check command topic/payload, device acknowledgement and fresh actual state. Confirm the button action is not more-info and target entity ID is real. |
| Title blank or stale | Explicit Idle/current distinction, periodic non-retained state, sensor expiry and current entity binding. |
| Logs never arrive | HA backend installed/restarted, matching ID/namespace, fresh report_slots, idle SD gate, request direction, chunk offsets/deadlines and effective MQTT worker poll. |
| “Works” only on one browser | System default vs user profile override, intended app server/account, persisted YAML and cached view. |

When a nontrivial failure appears, frame its general class (e.g. asynchronous
ownership, stale data, throughput or readiness), inspect a few primary sources,
choose one bounded experiment and retain its result. Do not accumulate retries
or per-prop exceptions without understanding the layer that failed.

## Current evidence and limits

- November 2: real Jeff discovery with 26 entities / 11 skits, selection,
  invalid/not-ready commands, stored retained-command rejection, offline
  indication and broker recovery passed. Earlier no-speaker results are history,
  not the final state of the feature.
- The grouped dashboard's actual dropdown changed selection without playback.
  Its Play button started Pumpkin flowers; telemetry showed preparing → playing
  → idle, the correct active title and no new pending fault/report. Cam then
  reported testing it successfully and strongly approved the clear interface.
  This is not exhaustive audio quality, performance or paired-device acceptance.
- Actual SD log retrieval verified both preserved reports (413 and 5,689 bytes)
  byte-for-byte and as complete HA notifications. A separate 14,335-byte synthetic
  HA/MQTT receiver fixture passed; maximum-size impaired-wireless transfer from
  Jeff is not thereby proven.
- Latest speaker-eye-warning application is 2,315,472 bytes; static IRAM has
  only **85 bytes free**. Internal-heap pressure and simultaneous audio/network
  behavior need measurement before adding features. An earlier idle heap result
  is not active-audio headroom. No OTA capability follows from this integration.
- Anthony is not flashed/physically qualified for this HA change. Production TLS,
  all Wi-Fi failures and exhaustive callback timing/load are not device-qualified.
- Private evidence lives under `build/validation/story-007-ha/`, especially
  `player-dashboard/`, `fixes/` and its manifests/validation JSON. Speaker warning
  evidence is under `build/validation/story-008-speaker-eyes/`. The portable guide
  and source must remain useful without access to credentials or those captures.

## Maintenance contract

Whenever this project changes HA topics, discovery schema/entity names,
readiness/control semantics, dashboard layout, configuration/provisioning, log
protocol/limits, dependencies or acceptance evidence:

1. Update this guide and the relevant owning source/example in the same change.
2. Keep the short dashboard/operator guide and log receiver README consistent.
3. Preserve the distinction between a reusable pattern, TwoSkulls-specific code,
   an inferred expectation and something demonstrated on hardware/by the user.
4. Record new compatibility failures and primary-source decisions; remove stale
   “pending” claims only when the corresponding observation exists.
5. Never put secrets, installed tokens or unreviewed raw captures in this guide.

No scheduled upkeep service is required: this is an ongoing responsibility of
future work on the feature. Record future Death adaptations here when they change
the shared recipe, while keeping Death's own device-specific instructions in its
project.

## Primary references

- [HA installation](https://www.home-assistant.io/installation/)
- [MQTT discovery, naming, availability and birth](https://www.home-assistant.io/integrations/mqtt/)
- [MQTT Select](https://www.home-assistant.io/integrations/select.mqtt/) and [Button](https://www.home-assistant.io/integrations/button.mqtt/)
- [Native entities/button rows](https://www.home-assistant.io/dashboards/entities/)
- [Dashboards and system/personal defaults](https://www.home-assistant.io/dashboards/dashboards/)
- [Persistent notifications](https://www.home-assistant.io/integrations/persistent_notification/)
- [Mosquitto authentication](https://mosquitto.org/documentation/authentication-methods/)
- [ESP-IDF 5.5 MQTT API](https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32/api-reference/protocols/mqtt.html), plus the pinned SDK/source evidence in the throughput research.
