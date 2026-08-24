#!/usr/bin/env python3
# Copyright 2026 The Cobalt Authors. All Rights Reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for verify_memory_trace.py."""

import json
import os
import sys
import tempfile
import unittest

script_dir = os.path.dirname(os.path.realpath(__file__))
if script_dir not in sys.path:
  sys.path.insert(0, script_dir)

# pylint: disable=wrong-import-position
import verify_memory_trace


class VerifyMemoryTraceTest(unittest.TestCase):
  """Test suite for memory trace verification and noise detection."""

  def setUp(self):
    # pylint: disable=consider-using-with
    self.temp_dir = tempfile.TemporaryDirectory()

  def tearDown(self):
    self.temp_dir.cleanup()

  def test_clean_memory_trace(self):
    """Verifies that a pure memory trace triggers the PROCEED signal."""
    clean_events = [
        {
            "cat": "__metadata",
            "name": "process_name",
            "args": {
                "name": "Cobalt"
            }
        },
        {
            "cat": "disabled-by-default-memory-infra",
            "name": "periodic_interval",
            "args": {
                "dumps": {
                    "heaps_v2": {
                        "allocators": {
                            "malloc": {}
                        }
                    },
                    "process_mmaps": {
                        "vm_regions": []
                    },
                }
            },
        },
    ]
    metrics = verify_memory_trace.analyze_memory_events(clean_events)
    self.assertEqual(metrics["memory_dumps_count"], 1)
    self.assertTrue(metrics["has_heaps_v2"])
    self.assertTrue(metrics["has_process_mmaps"])
    self.assertFalse(metrics["has_redundant_data"])
    self.assertEqual(metrics["noise_categories"], set())
    self.assertTrue(verify_memory_trace.print_verdict(metrics))

  def test_clean_memory_trace_with_process_totals(self):
    """Verifies parsing of process_totals (Private Footprint and Peak RSS)."""
    events = [{
        "cat": "disabled-by-default-memory-infra",
        "name": "periodic_interval",
        "args": {
            "dumps": {
                "heaps_v2": {},
                "process_mmaps": {},
                "process_totals": {
                    "private_footprint_bytes": "5025000",
                    "peak_resident_set_size": "a5b6000",
                    "is_peak_rss_resettable": True,
                },
            }
        },
    }]
    metrics = verify_memory_trace.analyze_memory_events(events)
    self.assertTrue(metrics["has_process_totals"])
    self.assertAlmostEqual(
        metrics["latest_private_footprint_mb"], 80.14, places=1)
    self.assertAlmostEqual(metrics["latest_peak_rss_mb"], 165.71, places=1)

  def test_parse_bytes_to_mb(self):
    """Verifies byte parsing logic and type guards."""
    # pylint: disable=protected-access
    self.assertIsNone(verify_memory_trace._parse_bytes_to_mb(None))
    self.assertIsNone(verify_memory_trace._parse_bytes_to_mb(True))
    self.assertIsNone(verify_memory_trace._parse_bytes_to_mb(False))
    self.assertIsNone(verify_memory_trace._parse_bytes_to_mb("invalid_hex"))
    self.assertAlmostEqual(
        verify_memory_trace._parse_bytes_to_mb(1048576), 1.0, places=4)
    self.assertAlmostEqual(
        verify_memory_trace._parse_bytes_to_mb("100000"), 1.0, places=4)

  def test_noisy_memory_trace(self):
    """Verifies that CPU categories trigger PROCEED_WITH_WARNING."""
    noisy_events = [
        {
            "cat": "__metadata",
            "name": "process_name"
        },
        {
            "cat": "toplevel,blink",
            "name": "TaskQueue::RunTask"
        },
        {
            "cat": "netlog",
            "name": "URLRequest"
        },
        {
            "cat": "disabled-by-default-memory-infra",
            "name": "periodic_interval",
            "args": {
                "dumps": {
                    "heaps_v2": {
                        "allocators": {
                            "malloc": {}
                        }
                    },
                    "process_mmaps": {
                        "vm_regions": []
                    },
                }
            },
        },
    ]
    metrics = verify_memory_trace.analyze_memory_events(noisy_events)
    self.assertEqual(metrics["memory_dumps_count"], 1)
    self.assertTrue(metrics["has_heaps_v2"])
    self.assertTrue(metrics["has_process_mmaps"])
    self.assertTrue(metrics["has_redundant_data"])
    self.assertEqual(metrics["noise_categories"],
                     {"toplevel", "blink", "netlog"})
    # Should still return True (profileable), but flagged as noisy
    self.assertTrue(verify_memory_trace.print_verdict(metrics))

  def test_incomplete_trace_missing_heaps(self):
    """Verifies that missing heaps_v2 triggers ABORT."""
    incomplete_events = [{
        "cat": "disabled-by-default-memory-infra",
        "name": "periodic_interval",
        "args": {
            "dumps": {
                "process_mmaps": {
                    "vm_regions": []
                }
            }
        },
    }]
    metrics = verify_memory_trace.analyze_memory_events(incomplete_events)
    self.assertFalse(metrics["has_heaps_v2"])
    self.assertFalse(verify_memory_trace.print_verdict(metrics))

  def test_incomplete_trace_missing_mmaps(self):
    """Verifies that missing process_mmaps triggers ABORT."""
    incomplete_events = [{
        "cat": "disabled-by-default-memory-infra",
        "name": "periodic_interval",
        "args": {
            "dumps": {
                "heaps_v2": {
                    "allocators": {}
                }
            }
        },
    }]
    metrics = verify_memory_trace.analyze_memory_events(incomplete_events)
    self.assertFalse(metrics["has_process_mmaps"])
    self.assertFalse(verify_memory_trace.print_verdict(metrics))

  def test_parse_valid_json_file(self):
    """Tests parsing a standard trace file."""
    trace_path = os.path.join(self.temp_dir.name, "trace.json")
    trace_content = {
        "traceEvents": [{
            "cat": "__metadata",
            "name": "process_name"
        }, {
            "cat": "disabled-by-default-memory-infra",
            "name": "periodic_interval",
            "args": {
                "dumps": {
                    "heaps_v2": {},
                    "process_mmaps": {}
                }
            }
        }]
    }
    with open(trace_path, "w", encoding="utf-8") as f:
      json.dump(trace_content, f)

    events = verify_memory_trace.parse_trace_file(trace_path)
    self.assertIsNotNone(events)
    self.assertEqual(len(events), 2)
    self.assertTrue(verify_memory_trace.verify_trace(trace_path))

  def test_parse_missing_file(self):
    """Tests parsing a non-existent file."""
    non_existent = os.path.join(self.temp_dir.name, "missing.json")
    self.assertIsNone(verify_memory_trace.parse_trace_file(non_existent))
    self.assertFalse(verify_memory_trace.verify_trace(non_existent))


if __name__ == "__main__":
  unittest.main()
