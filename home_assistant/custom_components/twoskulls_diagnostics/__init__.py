"""Read bounded TwoSkulls SD reports through MQTT into native HA notifications.

No log body is stored in a sensor attribute or recorder history. MQTT callbacks
only validate/assemble bounded transfers; filesystem work runs in HA's executor.
"""
from __future__ import annotations

import asyncio
from dataclasses import dataclass, field
import hashlib
import html
import json
import os
from pathlib import Path
import re
import secrets
import time
import zlib

DOMAIN = "twoskulls_diagnostics"
MAX_REPORT_BYTES = 14335
MAX_CHUNK_BYTES = 256
MAX_DEVICES = 64
MAX_ACTIVE = 4
FRESH_SECONDS = 20
REQUEST_SECONDS = 30
DEVICE_RE = re.compile(r"twoskulls_[0-9a-f]{12}\Z")
STATE_RE = re.compile(r"halloween/(twoskulls_[0-9a-f]{12})/state\Z")
REQUEST_RE = re.compile(r"halloween/(twoskulls_[0-9a-f]{12})/logs/request\Z")
RESPONSE_RE = re.compile(
    r"halloween/(twoskulls_[0-9a-f]{12})/logs/([0-9a-f]{16})/(begin|end|error|chunk/(0|[1-9][0-9]{0,4}))\Z"
)


class ProtocolError(ValueError):
    """A response failed bounded-transfer validation."""


@dataclass
class Transfer:
    """One exact slot/request. Repeated identical QoS1 frames are idempotent."""

    slot: int
    expected: int | None = None
    data: bytearray = field(default_factory=bytearray)
    complete: bytes | None = None

    @staticmethod
    def _json(payload: bytes) -> dict:
        if len(payload) > 256:
            raise ProtocolError("oversized metadata")
        try:
            value = json.loads(payload)
        except (ValueError, UnicodeError, RecursionError) as error:
            raise ProtocolError("invalid metadata") from error
        if not isinstance(value, dict):
            raise ProtocolError("invalid metadata")
        return value

    def feed(self, kind: str, payload: bytes) -> bytes | None:
        if not isinstance(payload, bytes):
            raise ProtocolError("non-binary payload")
        if kind == "error":
            info = self._json(payload)
            reason = info.get("error")
            allowed = {"busy", "missing", "not_found", "read_failed", "unavailable",
                       "invalid_request", "disconnected", "timeout", "no_memory",
                       "sd_unavailable", "oversized", "expired"}
            raise ProtocolError(reason if isinstance(reason, str) and reason in allowed else "device_error")
        if kind == "begin":
            info = self._json(payload)
            size = info.get("bytes")
            if type(info.get("slot")) is not int or info["slot"] != self.slot:
                raise ProtocolError("wrong report slot")
            if type(size) is not int or not 0 <= size <= MAX_REPORT_BYTES:
                raise ProtocolError("invalid report size")
            if self.expected is not None and self.expected != size:
                raise ProtocolError("conflicting begin")
            self.expected = size
            return None
        if kind.startswith("chunk/"):
            if self.expected is None:
                raise ProtocolError("chunk before begin")
            offset_text = kind[6:]
            if not re.fullmatch(r"0|[1-9][0-9]{0,4}", offset_text):
                raise ProtocolError("invalid chunk offset")
            offset = int(offset_text)
            if not 1 <= len(payload) <= MAX_CHUNK_BYTES or offset + len(payload) > self.expected:
                raise ProtocolError("invalid chunk size")
            if offset < len(self.data):
                if offset + len(payload) <= len(self.data) and self.data[offset:offset+len(payload)] == payload:
                    return None
                raise ProtocolError("conflicting chunk")
            if offset != len(self.data):
                raise ProtocolError("out-of-order chunk")
            self.data.extend(payload)
            return None
        if kind == "end":
            info = self._json(payload)
            size, checksum = info.get("bytes"), info.get("crc32")
            if self.expected is None or type(size) is not int or size != self.expected or len(self.data) != size:
                raise ProtocolError("incomplete report")
            if not isinstance(checksum, str) or not re.fullmatch(r"[0-9a-fA-F]{8}", checksum):
                raise ProtocolError("invalid checksum")
            if zlib.crc32(self.data) != int(checksum, 16):
                raise ProtocolError("checksum mismatch")
            self.complete = bytes(self.data)
            return self.complete
        raise ProtocolError("unknown response frame")


@dataclass
class Pending:
    transfer: Transfer
    future: asyncio.Future


class ReportBridge:
    """HA-loop-owned subscriptions and per-device fetches."""

    def __init__(self, hass, mqtt, notifications):
        self.hass = hass
        self.mqtt = mqtt
        self.notifications = notifications
        self.states: dict[str, tuple[int, float, str]] = {}
        self.pending: dict[tuple[str, str], Pending] = {}
        self.active: dict[str, asyncio.Task] = {}

    def receive_state(self, message):
        match = STATE_RE.fullmatch(message.topic)
        if not match or message.retain or not isinstance(message.payload, bytes) or len(message.payload) > 8192:
            return
        try:
            info = json.loads(message.payload)
        except (ValueError, UnicodeError, RecursionError):
            return
        if not isinstance(info, dict):
            return
        mask = info.get("report_slots")
        if type(mask) is not int or not 0 <= mask <= 255:
            return
        device = match[1]
        if device not in self.states and len(self.states) >= MAX_DEVICES:
            return
        role = info.get("role")
        label = "Anthony" if role == "primary" else "Jeff" if role == "secondary" else device
        self.states[device] = (mask, time.monotonic(), label)

    def receive_response(self, message):
        match = RESPONSE_RE.fullmatch(message.topic)
        if not match or message.retain or not isinstance(message.payload, bytes):
            return
        key = (match[1], match[2])
        pending = self.pending.get(key)
        # Late/stale frames cannot create a transfer or overwrite accepted data.
        if pending is None or pending.future.done():
            return
        try:
            data = pending.transfer.feed(match[3], message.payload)
        except ProtocolError as error:
            pending.future.set_exception(error)
        else:
            if data is not None:
                pending.future.set_result(data)

    def receive_request(self, message):
        match = REQUEST_RE.fullmatch(message.topic)
        if match and not message.retain and message.payload == b"REFRESH":
            self.start(match[1])

    def _notify(self, device, message, title, suffix="reports"):
        self.notifications.async_create(
            self.hass, message, title=title,
            notification_id=f"{device}_{suffix}",
        )

    def start(self, device: str):
        if not isinstance(device, str) or not DEVICE_RE.fullmatch(device):
            raise ValueError("Invalid TwoSkulls device ID")
        if device in self.active:
            return
        if len(self.active) >= MAX_ACTIVE:
            self._notify(device, "Other saved-report requests are still running. Try again shortly.", "Saved reports busy")
            return
        task = self.hass.async_create_task(self.refresh(device), f"{DOMAIN} {device}")
        self.active[device] = task
        task.add_done_callback(lambda done: self.active.pop(device, None))

    async def _fetch(self, device: str, slot: int) -> bytes:
        nonce = secrets.token_hex(8)
        future = asyncio.get_running_loop().create_future()
        key = (device, nonce)
        self.pending[key] = Pending(Transfer(slot), future)
        try:
            await self.mqtt.async_publish(
                self.hass, f"halloween/{device}/logs/get", f"GET {nonce} {slot}",
                qos=0, retain=False,
            )
            return await asyncio.wait_for(future, REQUEST_SECONDS)
        finally:
            self.pending.pop(key, None)

    def _store(self, device: str, slot: int, data: bytes) -> dict:
        # Controlled identifiers and content-addressed names; no arbitrary paths.
        root = Path(self.hass.config.path(".storage", "twoskulls_reports", device))
        root.mkdir(parents=True, exist_ok=True, mode=0o700)
        root.chmod(0o700)
        digest = hashlib.sha256(data).hexdigest()
        destination = root / f"{digest}.log"
        if not destination.exists():
            temporary = root / f".{digest}.tmp"
            fd = os.open(temporary, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
            try:
                with os.fdopen(fd, "wb") as output:
                    output.write(data)
                    output.flush()
                    os.fsync(output.fileno())
                os.replace(temporary, destination)
            finally:
                if temporary.exists():
                    temporary.unlink()
        return {"slot": slot, "bytes": len(data), "sha256": digest}

    async def refresh(self, device: str):
        state = self.states.get(device)
        label = state[2] if state else device
        if state is None or time.monotonic() - state[1] > FRESH_SECONDS:
            self._notify(device, "No fresh report listing from this prop. Check its connection and try again.", f"{label} saved reports unavailable")
            return
        mask = state[0]
        self._notify(device, "Reading saved SD reports. Open Notifications to view each report when it arrives.", f"{label} saved reports")
        if not mask:
            self._notify(device, "There are no saved SD reports on this prop.", f"{label} saved reports")
            return
        results = []
        for slot in range(8):
            if not mask & (1 << slot):
                continue
            try:
                data = await self._fetch(device, slot)
                metadata = await self.hass.async_add_executor_job(self._store, device, slot, data)
            except TimeoutError:
                results.append(f"- fault-{slot}.log: timed out; the report was not updated. Existing cached reports are unchanged. Try again when the prop is available.")
                break
            except ProtocolError as error:
                results.append(f"- fault-{slot}.log: {html.escape(str(error))}; the report was not updated. Existing cached reports are unchanged.")
            except OSError:
                results.append(f"- fault-{slot}.log: local cache could not be saved.")
            except Exception:
                # HA can reject publish while MQTT is reconnecting. Do not leak
                # exception payloads or leave a forever-running progress notice.
                results.append(f"- fault-{slot}.log: connection unavailable; try again shortly.")
                break
            else:
                results.append(f"- fault-{slot}.log: **{len(data):,} bytes**, verified and saved. Open its notification below.")
                # pre + escaped text renders literal content, including markdown,
                # without turning report text into links, HTML, or commands.
                content = html.escape(data.decode("utf-8", errors="replace"), quote=False)
                message = f"Read-only SD report · {metadata['bytes']:,} bytes · checksum verified\n\n<pre>{content}</pre>"
                self._notify(device, message, f"{label} · fault-{slot}.log", f"fault_{slot}")
        self._notify(device, "\n".join(results), f"{label} saved reports")


def CONFIG_SCHEMA(config):
    """Validate the empty YAML section using HA's own current schema helper."""
    from homeassistant.helpers import config_validation as cv
    return cv.empty_config_schema(DOMAIN)(config)


async def async_setup(hass, config):
    from homeassistant.components import mqtt, persistent_notification
    from homeassistant.core import callback
    from homeassistant.exceptions import HomeAssistantError

    bridge = ReportBridge(hass, mqtt, persistent_notification)
    hass.data[DOMAIN] = bridge
    @callback
    def receive_state(message):
        bridge.receive_state(message)

    @callback
    def receive_request(message):
        bridge.receive_request(message)

    @callback
    def receive_response(message):
        bridge.receive_response(message)

    await mqtt.async_subscribe(hass, "halloween/+/state", receive_state, qos=1, encoding=None)
    await mqtt.async_subscribe(hass, "halloween/+/logs/request", receive_request, qos=1, encoding=None)
    await mqtt.async_subscribe(hass, "halloween/+/logs/+/#", receive_response, qos=1, encoding=None)

    async def refresh_service(call):
        device = call.data.get("device_id")
        try:
            bridge.start(device)
        except ValueError as error:
            raise HomeAssistantError(str(error)) from error

    hass.services.async_register(DOMAIN, "refresh_logs", refresh_service)
    return True
