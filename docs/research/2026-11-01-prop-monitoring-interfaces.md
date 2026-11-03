# Established interfaces for Halloween prop monitoring

Date: November 1, 2026. Research only; no service installation, infrastructure
changes, firmware build/upload, account access or new hardware activity.
Cam requested an alternative to inventing a fleet dashboard/device protocol,
including Home Assistant and established prop/show-control systems. Three parallel
research passes covered HA, show/prop ecosystems and telemetry standards.

## Decision

Use **Home Assistant as the central monitoring/dashboard platform**. Prefer
**MQTT with Home Assistant discovery** as the fleet integration convention while
keeping each prop's firmware and show engine independent. MQTT standardizes
transport; HA discovery adds a platform-specific device/entity model. Retain an
optional local webpage/HTTP JSON interface. A small gateway adapter can publish
HA discovery/state for an HTTP-only prop if native MQTT costs too much RAM/code.
A minimal HTTP endpoint consumed by HA REST is a valid first alternative, not a
requirement to add both transports to every ESP32.

Do not migrate the existing TwoSkulls runtime to ESPHome, WLED, ESPixelStick or a
show-control framework solely for monitoring. Those are stronger candidates for
new compatible props. Keep the existing Matter/Apple Home trigger path.

## Candidate comparison and primary sources

| Candidate | What it supplies | Decision for this project |
| --- | --- | --- |
| [HA MQTT integration](https://www.home-assistant.io/integrations/mqtt/) | Discovery, grouped entities, availability, device metadata and local configuration URL | Preferred fleet convention; publish neutral application topics separately from HA discovery |
| [HA REST integration](https://www.home-assistant.io/integrations/rest/) | Several sensors/binary sensors from one polled endpoint | Strong minimal-HTTP alternative; explicit HA config rather than MQTT discovery |
| [ESPHome web API](https://esphome.io/web-api/) | Device page, REST and event stream | Close to desired feature set for new/simple devices; existing skull firmware needs a port/integration |
| [ESPHome native API](https://esphome.io/components/api/) | Native HA transport based on TCP/Protobuf | Not a documented drop-in server for an independently owned native-IDF runtime |
| [FPP MQTT](https://github.com/FalconChristmas/fpp/blob/master/docs/MQTT.md) | Show player status, warnings, playlist/position, availability and commands | Useful future show-engine peer on same MQTT bus; not ESP32 diagnostics library |
| [xSchedule](https://github.com/xLightsSequencer/xSchedule/blob/main/README.md) | Sequencing, schedules, triggers, web/API | Use if coordinated yard shows become a goal; separate from prop health |
| [WLED JSON API](https://kno.wled.ge/interfaces/json-api/) | Lighting state, uptime/heap/Wi-Fi info and local web control | Good for compatible future lighting props; own API convention, not generic prop standard |
| [ESPixelStick interfaces](https://github.com/forkineye/ESPixelStick/wiki/Supported-Inputs-and-Outputs) | Sequence/controller firmware, MQTT state and HA light discovery | Good future pixel/servo/relay role; discovery models a light, not arbitrary skull diagnostics |
| [Bottango REST API](https://docs.bottango.com/learn-bottango/external-control/rest-api/) | Desktop animation readiness/playback/effector API | Animatronic authoring lead; REST endpoint is desktop-side, not a drop-in on current firmware |
| [Bottango supported boards](https://docs.bottango.com/learn-bottango/hardware-drivers/supported-microcontrollers/) | ESP32 hardware driver workflow | Requires its firmware integration/Arduino workflow; not validated against skull radio/audio stack |
| [Fright Ideas Director](https://www.frightideas.com/director/software) | Animation/audio authoring for BooBox controller family | Established prop workflow, but no generic ESP32 diagnostics adapter found in inspected docs |

## Other genuine standards

- [Homie 5](https://homieiot.github.io/specification/spec-core-v5_0_0/) is a
  controller-neutral, self-describing MQTT convention with typed properties and
  lifecycle. It is not HA discovery; compatible adapter/controller still needed.
  Its transient log topics do not automatically retrieve retained SD files.
- [Eclipse Sparkplug 3.0](https://sparkplug.eclipse.org/specification/version/3.0/documents/sparkplug-specification-3.0.0.pdf)
  adds industrial Protobuf metrics and birth/death/sequence semantics. More
  machinery than justified for this small fleet unless industrial SCADA is a goal.
- [Prometheus data model](https://prometheus.io/docs/concepts/data_model/) suits
  numeric trends and counters. Prefer an optional gateway exporter; it is not a
  convenient primary interface for arbitrary skit state and SD documents.
- [OSC](https://opensoundcontrol.stanford.edu/spec-1_0.html) is an open multimedia
  messaging format, but application address/schema/history would still be ours.
- Entertainment does have real diagnostics standards: RDM and
  [RDMnet/ANSI E1.33](https://etclabs.github.io/RDMnetDocs/0.3.0/index.html).
  [ETC RDM documentation](https://www.etcconnect.com/webdocs/controls/colorsource_onlinehelp/en/Content/HelpAboutRDM.html)
  describes device discovery/configuration/status. Appropriate for theatrical
  fixture compatibility; larger model/tooling commitment for DIY props, with no
  automatic arbitrary SD-report retrieval established here. DMX/sACN/DDP-style
  show-output protocols alone do not supply our application diagnostics.

## Existing Matter opportunity

[Released HA Core 2026.10.4 sensor mappings](https://github.com/home-assistant/core/blob/2026.10.4/homeassistant/components/matter/sensor.py#L1616-L1708)
support Wi-Fi RSSI, reboot count, uptime and optional boot-reason attributes as
diagnostic sensors (disabled by default). Local pinned native SDK source creates
General Diagnostics and Wi-Fi Network Diagnostics on the root node, including
reboot count, uptime and nullable RSSI. The local factory creates a boot-reason
**event**, not the attribute expected by that HA sensor: do not promise that
sensor without checking. Anthony creates the Matter node; Jeff does not.

This establishes plausible standard reuse at source level, not that Anthony's
live values or discovery have been tested. Query existing Matter diagnostics
when Anthony is available before adding another route for those same values.
Custom skit/Bluetooth/SD fields still require an adapter; arbitrary custom Matter
clusters do not automatically become HA entities.

## Availability, logs and coexistence

[HA MQTT sensors](https://www.home-assistant.io/integrations/sensor.mqtt/) support
expiry. Combine connection availability with application freshness; Last Will
alone cannot prove the control loop is healthy. Avoid blindly mixing retained
sensor state with expire_after, since retained replay can revive stale readings.
Retained discovery configuration is separate. Publish boot/session identity and
heartbeat/sequence information; use null/unavailable for absent signal samples.

Expose report count/latest ID/size as sensors and keep full SD reports as files.
[HA state/attribute guidance](https://contributors.home-assistant.io/core/entities/state-attributes/)
documents limited state size and recorder implications; do not treat entity
history as a general file archive. File listing/download/collection remains a
small custom capability even if the dashboard is reused. Device configuration
links do not proxy local device files to an off-site browser. A gateway/VPN or
server-side collector is still needed for remote log access.

[HA remote access](https://www.home-assistant.io/docs/configuration/remote/)
supports cloud, VPN and reverse-proxy arrangements; the proposed domain could
front the HA dashboard, with routing/authentication selected later. No DNS or
hosting changes are authorized by this research. HA can coexist with Apple Home:
[HomeKit Bridge](https://www.home-assistant.io/integrations/homekit/) exports
supported HA entities, while [Matter sharing](https://www.home-assistant.io/integrations/matter/)
can retain existing controllers. Do not automatically re-pair existing devices.

## Resource boundary and bounded local check

[ESP-MQTT](https://docs.espressif.com/projects/esp-mqtt/en/latest/esp32/) supplies
supported SDK MQTT functionality, but uses buffers/tasks/outbox memory. Likewise
[ESPHome web server](https://esphome.io/components/web_server/) documents flash
and per-client heap costs. Neither HTTP nor MQTT fit is established on this
firmware. Last-recorded linker IRAM margin was only 681 bytes; Flash app room is
not evidence of internal-memory/audio timing safety. Jeff also needs Wi-Fi
initialization/provisioning for independent IP connectivity whichever route wins.

Small local applicability check: mapped the actual 5,689-byte Jeff SD report
previously read back after reboot into a typed JSON fixture and checked JSON
roundtrip for an HTTP/MQTT-style payload. SD mount 111 ms and mount+config 141 ms,
boolean absence of speaker/peer, null unavailable Wi-Fi/BT readings and distinct
signal units survived; full report text stayed outside status. Missing fields
from a truncated status line were not inferred. Private fixture/result:
`build/validation/2026-11-01-interface-research/`.

This is only field/type mapping validation, not HA schema validation, a running
integration, broker test, network service, or device footprint/performance proof.
No HA/Python dependencies were installed just for the research.

Next experiment, only after choosing a direction: read Anthony's existing Matter
diagnostics when available; compare isolated minimal MQTT/discovery and HTTP/REST
builds on the current skull firmware, then runtime heap/stack/radio/audio checks.
Discovery can run on a host adapter if the ESP32 should remain HTTP-only. Trial
HA with recorded data on existing suitable infrastructure before designing a
custom central site or migrating prop firmware.
