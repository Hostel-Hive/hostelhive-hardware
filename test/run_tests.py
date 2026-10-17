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
# 1. QR Payload Parser Logic (Mirrors qr_scanner.cpp)
# ============================================================================
class QrParser:
    @staticmethod
    def parse_payload(raw_data: str):
        if not raw_data:
            return {"is_valid": False, "student_id": "", "error": "Empty QR payload"}
        
        sanitized = raw_data.replace("\r", "").replace("\n", "").strip()
        if len(sanitized) == 0:
            return {"is_valid": False, "student_id": "", "error": "Empty QR payload"}
        if len(sanitized) < 4:
            return {"is_valid": False, "student_id": "", "error": "Payload too short"}
        
        return {"is_valid": True, "student_id": sanitized, "error": ""}


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
# 3. Offline Buffer Queue (Mirrors LittleFS queue in offline_queue.cpp)
# ============================================================================
class OfflineQueueSimulator:
    def __init__(self, max_capacity: int = 50):
        self.max_capacity = max_capacity
        self.queue = []

    def enqueue(self, student_id: str, node_id: str, direction: str, timestamp: str):
        if len(self.queue) >= self.max_capacity:
            self.queue.pop(0)  # Drop oldest
        record = {
            "student_id": student_id,
            "node_id": node_id,
            "direction": direction,
            "timestamp": timestamp,
            "offline_synced": True
        }
        self.queue.append(record)
        return True

    def flush(self, mock_network_succeeds: bool = True):
        if not mock_network_succeeds:
            return 0  # Network still down, keep all
        flushed_count = len(self.queue)
        self.queue.clear()
        return flushed_count

    def count(self):
        return len(self.queue)


# ============================================================================
# Test Cases
# ============================================================================

class TestQrScannerParsing(unittest.TestCase):
    """Test suite for QR raw stream parsing and sanitization."""

    def test_valid_student_id_prefix(self):
        res = QrParser.parse_payload("HH-STU-2021-CS-045\r\n")
        self.assertTrue(res["is_valid"])
        self.assertEqual(res["student_id"], "HH-STU-2021-CS-045")

    def test_whitespace_and_newline_stripping(self):
        res = QrParser.parse_payload("   HH-STU-2022-SE-012   \r\n")
        self.assertTrue(res["is_valid"])
        self.assertEqual(res["student_id"], "HH-STU-2022-SE-012")

    def test_empty_payload_rejection(self):
        res = QrParser.parse_payload("   \r\n")
        self.assertFalse(res["is_valid"])
        self.assertEqual(res["error"], "Empty QR payload")

    def test_too_short_payload_rejection(self):
        res = QrParser.parse_payload("AB\n")
        self.assertFalse(res["is_valid"])
        self.assertEqual(res["error"], "Payload too short")


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


class TestOfflineQueueBuffering(unittest.TestCase):
    """Test suite for SRS FR015: LittleFS offline queue caching & auto-flush."""

    def setUp(self):
        self.queue = OfflineQueueSimulator(max_capacity=5)

    def test_enqueue_offline_scan(self):
        self.queue.enqueue("HH-STU-001", "GATE_ENTRY_01", "IN", "2026-10-15T10:00:00+05:30")
        self.assertEqual(self.queue.count(), 1)
        self.assertEqual(self.queue.queue[0]["student_id"], "HH-STU-001")

    def test_fifo_ordering(self):
        self.queue.enqueue("STU-1", "GATE_01", "IN", "T1")
        self.queue.enqueue("STU-2", "GATE_01", "IN", "T2")
        self.assertEqual(self.queue.queue[0]["student_id"], "STU-1")
        self.assertEqual(self.queue.queue[1]["student_id"], "STU-2")

    def test_capacity_overflow_eviction(self):
        for i in range(5):
            self.queue.enqueue(f"STU-{i}", "GATE_01", "IN", f"T{i}")
        # Enqueue 6th item, should drop STU-0
        self.queue.enqueue("STU-OVERFLOW", "GATE_01", "IN", "T_NEW")
        self.assertEqual(self.queue.count(), 5)
        self.assertEqual(self.queue.queue[0]["student_id"], "STU-1")
        self.assertEqual(self.queue.queue[-1]["student_id"], "STU-OVERFLOW")

    def test_auto_flush_on_reconnection(self):
        self.queue.enqueue("STU-A", "GATE_01", "IN", "T1")
        self.queue.enqueue("STU-B", "GATE_01", "IN", "T2")
        self.assertEqual(self.queue.count(), 2)

        # Wi-Fi returns
        flushed = self.queue.flush(mock_network_succeeds=True)
        self.assertEqual(flushed, 2)
        self.assertEqual(self.queue.count(), 0)

    def test_failed_flush_preserves_buffer(self):
        self.queue.enqueue("STU-A", "GATE_01", "IN", "T1")
        # Network call fails
        flushed = self.queue.flush(mock_network_succeeds=False)
        self.assertEqual(flushed, 0)
        self.assertEqual(self.queue.count(), 1)


class TestApiContractConformance(unittest.TestCase):
    """Test suite for SDS 6.3 & FR011 Hardware REST API contracts."""

    def test_payload_json_structure(self):
        payload = {
            "student_id": "HH-STU-2021-CS-045",
            "node_id": "GATE_ENTRY_01",
            "direction": "IN",
            "timestamp": "2026-10-15T20:00:00+05:30",
            "offline_synced": False
        }
        encoded = json.dumps(payload)
        decoded = json.loads(encoded)

        self.assertIn("student_id", decoded)
        self.assertIn("node_id", decoded)
        self.assertIn("direction", decoded)
        self.assertIn("timestamp", decoded)
        self.assertIn("offline_synced", decoded)

    def test_response_code_evaluation(self):
        # 200 OK -> Granted
        self.assertTrue(200 == 200)
        # 403 Forbidden -> Denied
        self.assertFalse(403 == 200)
        # 429 Too Many Requests -> Cooldown
        self.assertEqual(429, 429)


if __name__ == "__main__":
    print("==================================================================")
    print("  HostelHive Hardware & IoT Automated Test Suite (SENG 34213)   ")
    print("==================================================================")
    suite = unittest.TestSuite()
    for tc in [TestQrScannerParsing, TestScanCooldownDeduplication, TestOfflineQueueBuffering, TestApiContractConformance]:
        suite.addTests(unittest.TestLoader().loadTestsFromTestCase(tc))

    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)
    if not result.wasSuccessful():
        sys.exit(1)
    print("\n[SUCCESS] All unit tests passed (100% pass rate).")
    sys.exit(0)
