# TwoSkulls saved reports for Home Assistant

This small Home Assistant integration reads the fixed SD report slots through
MQTT and presents their complete contents in Home Assistant's built-in
**Notifications**. It does not add a website, expose a public file server, or put
log bodies into sensor attributes or recorder history.

## Installation

1. Copy this `twoskulls_diagnostics` directory into your HA configuration's
   `custom_components` directory.
2. Configure Home Assistant's normal MQTT integration for the same broker as the
   props.
3. Add this empty section to `configuration.yaml`:

   ```yaml
   twoskulls_diagnostics:
   ```

4. Check configuration and restart Home Assistant.

With a compatible prop firmware, press **Open saved reports** on its device page.
Open HA's Notifications sidebar to see a report index and one notification per
saved report. Text is rendered literally; logs cannot inject HTML or links.
The native service is also available:

```yaml
action: twoskulls_diagnostics.refresh_logs
data:
  device_id: twoskulls_001122334455
```

The identifier is the prop's stable MQTT device identifier, not an IP address.
It must have the form `twoskulls_` followed by twelve lowercase hex digits.

## Transfer and privacy boundaries

The receiver requires a fresh non-retained state snapshot (at most 20 seconds
old) containing an integer `report_slots` bitmask. It reads only those fixed slots
0–7, sequentially, with one active fetch per prop and at most four simultaneous
props. A second click while the same prop is being fetched is not queued.

Requests are non-retained `GET <16-hex-request-id> <slot>` messages to
`halloween/<device>/logs/get`. Responses are scoped under
`halloween/<device>/logs/<request-id>/`:

- `begin`: JSON `{"slot":0,"bytes":413}`
- `chunk/<offset>`: raw binary data, 1–256 bytes
- `end`: JSON `{"bytes":413,"crc32":"12345678"}`
- `error`: JSON `{"error":"busy"}` or another bounded device error

Reports are limited to 14,335 bytes. The receiver validates the slot, exact byte
count, ordered offsets, identical QoS1 duplicates and standard zlib CRC32 before
saving or displaying content. Conflicting, oversized, corrupt or incomplete
responses fail without replacing a previously cached report. Each request has a
30-second deadline; late or unrelated request IDs are ignored. There is no
automatic retry or automatic new collection after reconnect.

Cache files are private content-addressed `.log` files under
`.storage/twoskulls_reports/<device>/`. Identical reports deduplicate; earlier
versions are preserved when an SD slot later rotates. The cache is not publicly
served. It does not automatically delete older verified files. Notifications can
be dismissed through the normal HA UI; refresh recreates them from the device.
A Home Assistant restart may clear the notification view; the verified cache
remains. Full binary contents are saved exactly; malformed UTF-8 uses replacement
characters only in the human-readable notification view.

MQTT uses the existing broker's authentication and transport configuration.
The integration does not install credentials, put tokens in URLs, expose files
outside Home Assistant, or write/delete anything on the prop's SD card. A listing
of zero reports, stale connection, busy device, transfer failure or timeout is
reported explicitly. Reading is gated by the firmware's idle policy.

## Sources and validation

The integration reuses Home Assistant's supported MQTT subscription/publish
helpers, async executor pattern, and persistent-notification API:

- https://contributors.home-assistant.io/core/internals/asyncio/
- https://www.home-assistant.io/integrations/persistent_notification/
- https://github.com/home-assistant/core/tree/2026.10.4/homeassistant/components/mqtt

The installed HA 2026.10.4 helper signatures were also inspected directly.
Portable production-source tests cover protocol bounds, correlation, duplicate
frames, corruption, timeout/device errors, exact private caching and notification
escaping. An isolated fixture also passed through the installed HA 2026.10.4 service,
real MQTT, exact 14,335-byte caching and native notifications with every escaped
character verified. The fixture deliberately repeated begin/chunk frames and
sent an unrelated request ID; neither corrupted the report. Its two disposable
notifications and cache copy were removed after verification. Actual prop and
visible-UI qualification remain separate checks.
