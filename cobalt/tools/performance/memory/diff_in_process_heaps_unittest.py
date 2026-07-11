#!/usr/bin/env python3
# Copyright 2026 The Cobalt Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Unit tests for diff_in_process_heaps.py."""

import json
import os
import shutil
import tempfile
import unittest

import diff_in_process_heaps as diff_tool


class DiffInProcessHeapsTest(unittest.TestCase):

  def setUp(self):
    self.temp_dir = tempfile.mkdtemp()

  def tearDown(self):
    shutil.rmtree(self.temp_dir, ignore_errors=True)

  def _create_trace_file(self, filename: str, trace_data: dict) -> str:
    path = os.path.join(self.temp_dir, filename)
    with open(path, "w", encoding="utf-8") as f:
      json.dump(trace_data, f)
    return path

  def test_parse_snapshot_with_trace_events(self):
    mock_dumps = {
        "heaps_v2": {
            "maps": {
                "strings": [{
                    "id": 1,
                    "string": "main"
                }],
                "nodes": [{
                    "id": 10,
                    "name_sid": 1,
                    "parent_id": 0
                }],
            },
            "allocators": {
                "malloc": {
                    "nodes": [{
                        "node_id": 10,
                        "size": 1024,
                        "count": 1
                    }]
                }
            },
        }
    }
    trace_data = {
        "traceEvents": [{
            "name": "periodic_interval",
            "ts": 100000,
            "args": {
                "dumps": json.dumps(mock_dumps)
            },
        }]
    }
    path = self._create_trace_file("c26_mock.json", trace_data)
    res = diff_tool.parse_snapshot(path, snapshot_index=-1)
    self.assertIsNotNone(res)
    heaps, desc = res
    self.assertIn("maps", heaps)
    self.assertIn("allocators", heaps)
    self.assertIn("periodic_interval at ts=100000 µs", desc)

  def test_extract_allocations(self):
    heaps_v2 = {
        "maps": {
            "strings": [
                {
                    "id": 1,
                    "string": "v8::internal::Heap::Allocate"
                },
                {
                    "id": 2,
                    "string": "cobalt::dom::Document::CreateElement"
                },
            ],
            "nodes": [
                {
                    "id": 10,
                    "name_sid": 1,
                    "parent_id": 0
                },
                {
                    "id": 20,
                    "name_sid": 2,
                    "parent_id": 0
                },
            ],
        },
        "allocators": {
            "v8": {
                "nodes": [{
                    "node_id": 10,
                    "size": 50000,
                    "count": 50
                }]
            },
            "partition_alloc": {
                "nodes": [{
                    "node_id": 20,
                    "size": 120000,
                    "count": 12
                }]
            },
        },
    }
    records = diff_tool.extract_allocations(heaps_v2)
    self.assertEqual(len(records), 2)

    subsystems = {r.subsystem for r in records}
    self.assertIn("V8 / JS Engine", subsystems)
    self.assertIn("DOM / Layout (PartitionAlloc)", subsystems)

  def test_compare_aggregates_and_threshold(self):
    base_agg = {
        "DOM / Layout": (100000, 10),
        "V8 / JS Engine": (200000, 20),
        "Tiny / Noise": (5000, 1),
    }
    target_agg = {
        "DOM / Layout": (150000, 15),  # +50,000 bytes
        "V8 / JS Engine": (190000, 19),  # -10,000 bytes
        "Tiny / Noise": (5100, 1),  # +100 bytes (should be filtered)
    }

    # Threshold 1 KB (1024 bytes)
    diffs = diff_tool.compare_aggregates(
        base_agg, target_agg, threshold_bytes=1024)
    self.assertEqual(len(diffs), 2)
    keys = {d["key"] for d in diffs}
    self.assertIn("DOM / Layout", keys)
    self.assertIn("V8 / JS Engine", keys)
    self.assertNotIn("Tiny / Noise", keys)

  def test_generate_report_markdown(self):
    base_records = [
        diff_tool.AllocationRecord(
            "malloc",
            "DOM / Layout",
            "CreateNode",
            ["CreateNode", "main"],
            size_bytes=100000,
            count=10,
        )
    ]
    target_records = [
        diff_tool.AllocationRecord(
            "malloc",
            "DOM / Layout",
            "CreateNode",
            ["CreateNode", "main"],
            size_bytes=150000,
            count=15,
        )
    ]
    report = diff_tool.generate_report(
        "/tmp/c26_trace.json",
        "Base Desc",
        base_records,
        "/tmp/c27_trace.json",
        target_desc="Target Desc",
        target_records=target_records,
        threshold_bytes=1024,
    )
    self.assertIn("# Cobalt In-Process Heap Differential Bloat Analysis",
                  report)
    self.assertIn("DOM / Layout", report)
    self.assertIn("CreateNode", report)
    self.assertIn("+48.83 KB", report)  # 50000 bytes is ~48.83 KB


if __name__ == "__main__":
  unittest.main()
