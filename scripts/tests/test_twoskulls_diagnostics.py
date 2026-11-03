"""Production Python receiver tests; no Home Assistant installation required."""
import asyncio
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "home_assistant/custom_components/twoskulls_diagnostics/__init__.py"
SPEC = importlib.util.spec_from_file_location("twoskulls_diagnostics_test_target", SOURCE)
bridge_module = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = bridge_module
SPEC.loader.exec_module(bridge_module)
Transfer = bridge_module.Transfer
ProtocolError = bridge_module.ProtocolError
DEVICE = "twoskulls_001122334455"


def packet(info):
    return json.dumps(info).encode()


def begin(transfer, data, slot=1):
    transfer.feed("begin", packet({"slot": slot, "bytes": len(data)}))


def end(data):
    return packet({"bytes": len(data), "crc32": f"{zlib.crc32(data):08x}"})


class ProtocolTests(unittest.TestCase):
    def test_roundtrip_maximum_and_qos_duplicates(self):
        data = bytes(range(256)) * 55 + b"x" * 255
        self.assertEqual(len(data), bridge_module.MAX_REPORT_BYTES)
        transfer = Transfer(1)
        begin(transfer, data)
        begin(transfer, data)  # must not reset receive position
        for offset in range(0, len(data), 256):
            part = data[offset:offset+256]
            transfer.feed(f"chunk/{offset}", part)
            transfer.feed(f"chunk/{offset}", part)
        self.assertEqual(transfer.feed("end", end(data)), data)
        self.assertEqual(transfer.feed("end", end(data)), data)

    def test_order_correlation_size_and_corruption(self):
        transfer = Transfer(1)
        with self.assertRaises(ProtocolError):
            transfer.feed("chunk/0", b"x")
        with self.assertRaises(ProtocolError):
            begin(transfer, b"x", slot=2)
        with self.assertRaises(ProtocolError):
            transfer.feed("begin", packet({"slot": 1, "bytes": 14336}))
        with self.assertRaises(ProtocolError):
            transfer.feed("begin", packet({"slot": True, "bytes": 1}))
        begin(transfer, b"abcd")
        with self.assertRaises(ProtocolError):
            transfer.feed("chunk/1", b"b")
        transfer.feed("chunk/0", b"ab")
        with self.assertRaises(ProtocolError):
            transfer.feed("chunk/0", b"xy")
        with self.assertRaises(ProtocolError):
            transfer.feed("begin", packet({"slot": 1, "bytes": 3}))
        with self.assertRaises(ProtocolError):
            transfer.feed("end", end(b"abcd"))
        transfer.feed("chunk/2", b"cd")
        with self.assertRaises(ProtocolError):
            transfer.feed("end", packet({"bytes": 4, "crc32": "00000000"}))
        self.assertEqual(transfer.feed("end", end(b"abcd")), b"abcd")

    def test_error_and_malformed_frames(self):
        for reason in ("busy", "missing", "read_failed", [], "<b>untrusted</b>"):
            with self.assertRaises(ProtocolError):
                Transfer(0).feed("error", packet({"error": reason}))
        with self.assertRaises(ProtocolError):
            Transfer(0).feed("begin", b"[]")
        with self.assertRaises(ProtocolError):
            Transfer(0).feed("begin", b"\xff")
        empty = Transfer(0)
        begin(empty, b"", slot=0)
        self.assertEqual(empty.feed("end", end(b"")), b"")


class Notifications:
    def __init__(self):
        self.items = {}

    def async_create(self, hass, message, title=None, notification_id=None):
        self.items[notification_id] = {"message": message, "title": title}


class Hass:
    def __init__(self, root):
        self.config = SimpleNamespace(path=lambda *parts: str(Path(root).joinpath(*parts)))

    def async_create_task(self, coroutine, name):
        return asyncio.create_task(coroutine, name=name)

    async def async_add_executor_job(self, function, *args):
        return await asyncio.to_thread(function, *args)


class Mqtt:
    def __init__(self):
        self.bridge = None
        self.requests = []
        self.data = {0: b"original <script>unsafe()</script>\n```markdown\n", 1: b"second report\n"}
        self.respond = True
        self.error = None

    async def async_publish(self, hass, topic, payload, qos, retain):
        self.requests.append((topic, payload, qos, retain))
        if not self.respond:
            return
        _, nonce, slot = payload.split()
        slot = int(slot)
        prefix = f"halloween/{DEVICE}/logs/{nonce}/"
        def deliver(kind, data):
            self.bridge.receive_response(SimpleNamespace(topic=prefix+kind, payload=data, retain=False))
        if self.error:
            deliver("error", packet({"error": self.error}))
            return
        data = self.data[slot]
        # A different request nonce may not start or finish our transfer.
        self.bridge.receive_response(SimpleNamespace(topic=f"halloween/{DEVICE}/logs/0000000000000000/end", payload=end(data), retain=False))
        deliver("begin", packet({"slot": slot, "bytes": len(data)}))
        for offset in range(0, len(data), 256):
            deliver(f"chunk/{offset}", data[offset:offset+256])
        deliver("end", end(data))


class BridgeTests(unittest.IsolatedAsyncioTestCase):
    async def asyncSetUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.hass = Hass(self.temporary.name)
        self.mqtt = Mqtt()
        self.notifications = Notifications()
        self.bridge = bridge_module.ReportBridge(self.hass, self.mqtt, self.notifications)
        self.mqtt.bridge = self.bridge
        self.bridge.receive_state(SimpleNamespace(topic=f"halloween/{DEVICE}/state", payload=packet({"role": "secondary", "report_slots": 3}), retain=False))

    async def asyncTearDown(self):
        self.temporary.cleanup()

    async def test_refresh_stores_exact_bytes_and_native_notifications(self):
        self.bridge.start(DEVICE)
        self.bridge.start(DEVICE)  # duplicate click does not queue another fetch
        task = self.bridge.active[DEVICE]
        await task
        await asyncio.sleep(0)
        self.assertEqual(len(self.mqtt.requests), 2)
        self.assertTrue(all(qos == 0 and retain is False for _, _, qos, retain in self.mqtt.requests))
        cache = Path(self.temporary.name) / ".storage/twoskulls_reports" / DEVICE
        self.assertEqual({p.read_bytes() for p in cache.glob("*.log")}, set(self.mqtt.data.values()))
        self.assertTrue(all((p.stat().st_mode & 0o777) == 0o600 for p in cache.glob("*.log")))
        notice = self.notifications.items[f"{DEVICE}_fault_0"]["message"]
        self.assertIn("&lt;script&gt;", notice)
        self.assertNotIn("<script>", notice)
        self.assertIn("fault-1.log", self.notifications.items[f"{DEVICE}_reports"]["message"])
        self.assertFalse(self.bridge.pending)
        self.assertFalse(self.bridge.active)
        # Content-addressed copies deduplicate and preserve earlier versions.
        await self.bridge.refresh(DEVICE)
        self.assertEqual(len(list(cache.glob("*.log"))), 2)

    async def test_timeout_and_device_busy_leave_no_active_transfer(self):
        original = bridge_module.REQUEST_SECONDS
        bridge_module.REQUEST_SECONDS = .01
        try:
            self.mqtt.respond = False
            await self.bridge.refresh(DEVICE)
        finally:
            bridge_module.REQUEST_SECONDS = original
        self.assertFalse(self.bridge.pending)
        self.assertIn("timed out", self.notifications.items[f"{DEVICE}_reports"]["message"])
        self.mqtt.respond = True
        self.mqtt.error = "busy"
        await self.bridge.refresh(DEVICE)
        self.assertIn("busy", self.notifications.items[f"{DEVICE}_reports"]["message"])
        self.assertFalse(self.bridge.pending)

    async def test_stale_unknown_and_invalid_states_do_not_publish(self):
        self.bridge.states.clear()
        for payload in (b"{", packet({"report_slots": True}), packet({"report_slots": 256})):
            self.bridge.receive_state(SimpleNamespace(topic=f"halloween/{DEVICE}/state", payload=payload, retain=False))
        await self.bridge.refresh(DEVICE)
        self.assertFalse(self.mqtt.requests)
        with self.assertRaises(ValueError):
            self.bridge.start("../other")
        self.bridge.receive_request(SimpleNamespace(topic=f"halloween/{DEVICE}/logs/request", payload=b"REFRESH", retain=True))
        self.assertFalse(self.bridge.active)


if __name__ == "__main__":
    unittest.main()
