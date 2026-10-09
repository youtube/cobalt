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
"""Unit tests for cobalt/tools/storage/storage_monitor.py."""

import os
import sys
import unittest

_REPO_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "..", "..", ".."))
if _REPO_ROOT not in sys.path:
  sys.path.insert(0, _REPO_ROOT)

# pylint: disable=wrong-import-position
from cobalt.tools.storage import storage_monitor
# pylint: enable=wrong-import-position


class StorageMonitorTest(unittest.TestCase):
  """Tests for path classification, snapshot tracking, and report generation.

  Lifetime/Ownership: Instantiated and managed by the unittest runner per test.
  Threading Model: Single-threaded test execution.
  """

  def test_classify_storage_path(self):
    self.assertEqual(
        storage_monitor.classify_storage_path(
            "/data/data/dev.cobalt.coat/app_content_shell/content_shell/"
            "Local Storage/leveldb/000003.log"),
        ("USER_DATA", "Local Storage"),
    )
    self.assertEqual(
        storage_monitor.classify_storage_path(
            "/data/user/0/dev.cobalt.coat/app_content_shell/content_shell/"
            "Network/Cookies-journal"),
        ("USER_DATA", "Network/Cookies"),
    )
    self.assertEqual(
        storage_monitor.classify_storage_path(
            "/data/data/dev.cobalt.coat/app_content_shell/content_shell/"
            "Cookies-journal"),
        ("USER_DATA", "Cookies"),
    )
    self.assertEqual(
        storage_monitor.classify_storage_path(
            "app_content_shell/content_shell/WebStorage/QuotaManager-journal"),
        ("USER_DATA", "WebStorage/QuotaManager"),
    )
    self.assertEqual(
        storage_monitor.classify_storage_path(
            "app_content_shell/content_shell/DIPS-wal"),
        ("USER_DATA", "DIPS"),
    )
    self.assertEqual(
        storage_monitor.classify_storage_path(
            "app_content_shell/content_shell/SharedStorage-wal"),
        ("USER_DATA", "SharedStorage"),
    )
    self.assertEqual(
        storage_monitor.classify_storage_path(
            "app_content_shell/content_shell/Cache/Cache_Data/index"),
        ("USER_DATA", "USER_DATA:Cache"),
    )
    self.assertEqual(
        storage_monitor.classify_storage_path("cache/Cache/Cache_Data/index"),
        ("DIR_CACHE", "DIR_CACHE:Cache"),
    )
    self.assertEqual(
        storage_monitor.classify_storage_path(
            "app_content_shell/content_shell/ShaderCache/data_0"),
        ("USER_DATA", "ShaderCache"),
    )
    self.assertEqual(
        storage_monitor.classify_storage_path(
            "app_content_shell/BrowserStabilityMetrics/1234.pma"),
        ("APP_DATA", "BrowserStabilityMetrics"),
    )
    self.assertEqual(
        storage_monitor.classify_storage_path(
            "files/storage/.storage_record.abc"),
        ("FILES_DIR", "storage"),
    )
    self.assertEqual(
        storage_monitor.classify_storage_path(
            "/data/user_de/0/dev.cobalt.coat/cache/"
            "com.android.opengl.shaders_cache"),
        ("DE_CACHE", "com.android.opengl.shaders_cache"),
    )
    self.assertEqual(
        storage_monitor.classify_storage_path(
            "/data/data/dev.cobalt.coat/shared_prefs/prefs.xml"),
        ("SHARED_PREFS", "prefs.xml"),
    )
    self.assertEqual(
        storage_monitor.classify_storage_path(
            "app_content_shell/content_shell/.org.chromium.Chromium.RZDMJu"),
        ("USER_DATA", ".org.chromium.Chromium.*"),
    )

  def test_tracker_detects_transient_journal_and_compaction_spikes(self):
    tracker = storage_monitor.StorageTracker()

    # Tick 0 (t=0s): 3-column stat output (%s %Y %n)
    # Cookies: 32768 B (32 KB)
    # 000003.log: 65536 B (64 KB)
    # BTM: 73728 B (72 KB) — remains static
    base = "/data/data/dev.cobalt.coat/app_content_shell/content_shell"
    snap0 = storage_monitor.parse_stat_output("\n".join([
        f"32768 1000 {base}/Network/Cookies",
        f"65536 1000 {base}/Local Storage/leveldb/000003.log",
        f"73728 1000 {base}/BTM",
    ]))
    tracker.record_snapshot(0.0, snap0)

    # Tick 1 (t=1s): Transient Cookies-journal (16 KB) + LevelDB compaction
    # where 000003.log (64 KB) and 000005.ldb (48 KB) temporarily coexist
    snap1 = storage_monitor.parse_stat_output("\n".join([
        f"32768 1001 {base}/Network/Cookies",
        f"16384 1001 {base}/Network/Cookies-journal",
        f"65536 1001 {base}/Local Storage/leveldb/000003.log",
        f"49152 1001 {base}/Local Storage/leveldb/000005.ldb",
        f"73728 1000 {base}/BTM",
    ]))
    tracker.record_snapshot(1.0, snap1)

    # Simulate a transient ADB timeout returning an empty snapshot (ignored)
    tracker.record_snapshot(1.5, {})

    # Tick 2 (t=2s): Journal committed & old .log deleted after compaction
    snap2 = storage_monitor.parse_stat_output("\n".join([
        f"32768 1002 {base}/Network/Cookies",
        f"49152 1001 {base}/Local Storage/leveldb/000005.ldb",
        f"73728 1000 {base}/BTM",
    ]))
    tracker.record_snapshot(2.0, snap2)

    report = tracker.build_summary_data(
        platform="android",
        cuj="video_playback",
        duration_sec=2.0,
        interval_sec=1.0)

    # Verify metadata start, peak, and final persistent KB
    meta = report["metadata"]
    self.assertEqual(meta["start_persistent_kb"], 168.0)
    self.assertEqual(meta["peak_persistent_kb"], 232.0)
    self.assertEqual(meta["peak_persistent_at_sec"], 1.0)
    self.assertEqual(meta["final_persistent_kb"], 152.0)
    self.assertNotIn("final_persistent_disk_kb", meta)

    # Verify Section A includes all types (even static BTM)
    by_type = {r["storage_type"]: r for r in report["storage_type_summary"]}
    self.assertIn("BTM", by_type)
    ls_row = by_type["Local Storage"]
    self.assertEqual(ls_row["start_kb"], 64.0)
    self.assertEqual(ls_row["peak_kb"], 112.0)
    self.assertEqual(ls_row["final_kb"], 48.0)
    self.assertNotIn("disk_kb", ls_row)
    self.assertNotIn("transient_spike_kb", ls_row)
    self.assertNotIn("net_delta_kb", ls_row)
    self.assertNotIn("recommendation", ls_row)

    # Verify Section B (files_inventory) includes transient/active files
    # and excludes untouched static baseline files (BTM)
    by_file = {f["relative_path"]: f for f in report["files_inventory"]}
    self.assertNotIn("app_content_shell/content_shell/BTM", by_file)

    journal = by_file["app_content_shell/content_shell/Network/Cookies-journal"]
    self.assertFalse(journal["existed_at_end"])
    self.assertEqual(journal["status"], "**Transient (Deleted)**")
    self.assertEqual(journal["peak_kb"], 16.0)
    self.assertEqual(journal["final_kb"], 0.0)

    old_log = by_file[
        "app_content_shell/content_shell/Local Storage/leveldb/000003.log"]
    self.assertFalse(old_log["existed_at_end"])
    self.assertEqual(old_log["peak_kb"], 64.0)

    # Verify Markdown formatting renders Proposal 1 headers and status
    md = storage_monitor.format_markdown_report(report)
    self.assertIn("## Section A: Storage Type Summary", md)
    self.assertIn("## Section B: Transient & High-Churn File Hotspots", md)
    self.assertIn("**Transient (Deleted)**", md)

  def test_format_adb_command(self):
    # pylint: disable=protected-access
    self.assertEqual(
        storage_monitor._format_adb_command(["run-as dev.cobalt.coat"],
                                            "ls /data"),
        "run-as dev.cobalt.coat ls /data",
    )
    self.assertEqual(
        storage_monitor._format_adb_command(["su 0 -c"], "echo \"hello\""),
        "su 0 -c \"echo \\\"hello\\\"\"",
    )
    self.assertEqual(
        storage_monitor._format_adb_command([], "ls /data"),
        "ls /data",
    )


if __name__ == "__main__":
  unittest.main()
