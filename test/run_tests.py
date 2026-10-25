#!/usr/bin/env python3
"""
HostelHive IoT Gate Attendance Scanner - Automated Test Suite
Academic Reference: SENG 34213 - System Development Project
Requirement Traceability: SRS FR011, FR012, FR013, FR014, FR015 & SDS Section 6.3
"""

import unittest
import json
import time
import sys

# ============================================================================
# 1. QR Payload Parser Logic (Mirrors gate::isValidQrPayload in gate_policy.h)
# ============================================================================
import re

QR_PATTERN = re.compile(r"HH1:[0-9a-f]{64}")


class QrParser:
    @staticmethod
    def parse_payload(raw_data: str):
        sanitized = (raw_data or "").strip()
        if len(sanitized) == 0:
            return {"is_valid": False, "qr": "", "error": "Empty QR payload"}
        if not QR_PATTERN.fullmatch(sanitized):
            return {"is_valid": False, "qr": "", "error": "Not a HostelHive pass"}
        return {"is_valid": True, "qr": sanitized, "error": ""}


# ============================================================================
# 2. Cooldown & Deduplication Logic (Mirrors FR014 in qr_scanner.cpp)
# ============================================================================
class CooldownManager:
    def __init__(self, cooldown_ms: int = 15000, max_entries: int = 16):
        self.cooldown_ms = cooldown_ms
        self.max_entries = max_entries
        self.entries = {}  # student_id -> last_timestamp_ms

    def is_cooldown_active(self, student_id: str, current_time_ms: int):
        if student_id in self.entries:
            elapsed = current_time_ms - self.entries[student_id]
            if elapsed < self.cooldown_ms:
                remaining_sec = (self.cooldown_ms - elapsed + 999) // 1000
                return True, remaining_sec
        return False, 0

    def register_scan(self, student_id: str, current_time_ms: int):
        if len(self.entries) >= self.max_entries and student_id not in self.entries:
            # Evict oldest
            oldest_id = min(self.entries, key=self.entries.get)
            del self.entries[oldest_id]
        self.entries[student_id] = current_time_ms


# ============================================================================
# 3. Fail-closed gate decision (Mirrors gate::decide in gate_policy.h)
# ============================================================================
ACCEPTED_OUTCOMES = {"recorded", "cooldown", "already_in", "already_out"}


def gate_decision(http_code: int, outcome=None, error_code=None):
    if http_code == 200:
        return "UNLOCK" if outcome in ACCEPTED_OUTCOMES else "DENY_UNAVAILABLE"
    if http_code == 409 and error_code not in (None, "scanner_direction_required"):
        return "DENY_BAD_REQUEST"
    return {
        422: "DENY_INVALID_PASS",
        401: "DENY_DEVICE_UNAUTHORIZED",
        409: "DENY_GATE_NOT_CONFIGURED",
        400: "DENY_BAD_REQUEST",
        413: "DENY_BAD_REQUEST",
        415: "DENY_BAD_REQUEST",
    }.get(http_code, "DENY_UNAVAILABLE")


# ============================================================================
# Test Cases
# ============================================================================

class TestQrScannerParsing(unittest.TestCase):
    """Only backend-issued HH1 passes are accepted (H2)."""

    GOOD = "HH1:" + "0123456789abcdef" * 4

    def test_backend_issued_payload_accepted(self):
        res = QrParser.parse_payload(self.GOOD + "\r\n")
        self.assertTrue(res["is_valid"])
        self.assertEqual(res["qr"], self.GOOD)

    def test_empty_payload_rejection(self):
        res = QrParser.parse_payload("   \r\n")
        self.assertFalse(res["is_valid"])
        self.assertEqual(res["error"], "Empty QR payload")

    def test_legacy_and_arbitrary_payloads_rejected(self):
        for raw in ["HH-STU-2021-CS-045", "HH:STU-001", "STU12345", "AB", "any barcode"]:
            self.assertFalse(QrParser.parse_payload(raw)["is_valid"], raw)

    def test_case_length_and_charset_enforced(self):
        self.assertFalse(QrParser.parse_payload(self.GOOD.upper())["is_valid"])
        self.assertFalse(QrParser.parse_payload(self.GOOD[:-1])["is_valid"])
        self.assertFalse(QrParser.parse_payload(self.GOOD + "0")["is_valid"])
        self.assertFalse(QrParser.parse_payload(self.GOOD[:-1] + "g")["is_valid"])


class TestScanCooldownDeduplication(unittest.TestCase):
    """Test suite for SRS FR014: Duplicate scan prevention within 15 seconds."""

    def setUp(self):
        self.mgr = CooldownManager(cooldown_ms=15000, max_entries=4)

    def test_initial_scan_allowed(self):
        active, _ = self.mgr.is_cooldown_active("HH-STU-001", current_time_ms=1000)
        self.assertFalse(active)

    def test_rapid_rescan_within_cooldown_blocked(self):
        self.mgr.register_scan("HH-STU-001", current_time_ms=1000)
        # Attempt scan after 3 seconds
        active, remaining = self.mgr.is_cooldown_active("HH-STU-001", current_time_ms=4000)
        self.assertTrue(active)
        self.assertEqual(remaining, 12)

    def test_rescan_allowed_after_cooldown_expires(self):
        self.mgr.register_scan("HH-STU-001", current_time_ms=1000)
        # Attempt scan after 16 seconds
        active, _ = self.mgr.is_cooldown_active("HH-STU-001", current_time_ms=17000)
        self.assertFalse(active)

    def test_multiple_distinct_students_independent(self):
        self.mgr.register_scan("HH-STU-001", current_time_ms=1000)
        # Another student scans immediately
        active, _ = self.mgr.is_cooldown_active("HH-STU-002", current_time_ms=2000)
        self.assertFalse(active)

    def test_cooldown_capacity_eviction(self):
        for i in range(4):
            self.mgr.register_scan(f"HH-STU-00{i}", current_time_ms=1000 * (i + 1))
        # Adding 5th entry evicts oldest (HH-STU-000)
        self.mgr.register_scan("HH-STU-999", current_time_ms=5000)
        self.assertNotIn("HH-STU-000", self.mgr.entries)
        self.assertIn("HH-STU-999", self.mgr.entries)


class TestFailClosedGateDecision(unittest.TestCase):
    """The gate opens only on a confirmed backend result (H1)."""

    def test_unlock_on_accepted_outcomes(self):
        for outcome in ACCEPTED_OUTCOMES:
            self.assertEqual(gate_decision(200, outcome), "UNLOCK")

    def test_unparseable_200_denies(self):
        self.assertEqual(gate_decision(200, None), "DENY_UNAVAILABLE")
        self.assertEqual(gate_decision(200, "granted"), "DENY_UNAVAILABLE")

    def test_network_failures_deny(self):
        for code in (0, -1, -11):
            self.assertEqual(gate_decision(code), "DENY_UNAVAILABLE")

    def test_error_statuses_deny(self):
        self.assertEqual(gate_decision(422), "DENY_INVALID_PASS")
        self.assertEqual(gate_decision(401), "DENY_DEVICE_UNAUTHORIZED")
        self.assertEqual(gate_decision(409, None, "scanner_direction_required"), "DENY_GATE_NOT_CONFIGURED")
        self.assertEqual(gate_decision(409, None, "event_id_conflict"), "DENY_BAD_REQUEST")
        for code in (201, 302, 403, 500, 503):
            self.assertEqual(gate_decision(code), "DENY_UNAVAILABLE")


class TestApiContractConformance(unittest.TestCase):
    """Request shape matches POST /api/v1/device/scans."""

    def test_payload_json_structure(self):
        payload = {"event_id": "3f2a8c4e-1b6d-4e9a-9c7f-2d5e8a1b4c6f", "qr": "HH1:" + "ab" * 32}
        decoded = json.loads(json.dumps(payload))
        self.assertEqual(set(decoded), {"event_id", "qr"})
        self.assertTrue(QrParser.parse_payload(decoded["qr"])["is_valid"])


if __name__ == "__main__":
    print("==================================================================")
    print("  HostelHive Hardware & IoT Automated Test Suite (SENG 34213)   ")
    print("==================================================================")
    suite = unittest.TestSuite()
    for tc in [TestQrScannerParsing, TestScanCooldownDeduplication, TestFailClosedGateDecision, TestApiContractConformance]:
        suite.addTests(unittest.TestLoader().loadTestsFromTestCase(tc))

    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    if not result.wasSuccessful():
        sys.exit(1)
    print("\n[SUCCESS] All unit tests passed (100% pass rate).")
    sys.exit(0)
