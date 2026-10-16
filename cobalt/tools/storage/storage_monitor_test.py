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
from unittest import mock

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
    self.assertEqual(
        storage_monitor.classify_storage_path(
            "/data/data/com.google.android.youtube.tv/"
            "no_backup/androidx.work.workdb-wal",
            package="com.google.android.youtube.tv",
        ),
        ("NO_BACKUP", "androidx.work.workdb"),
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
    self.assertEqual(journal["start_kb"], 0.0)
    self.assertEqual(journal["peak_kb"], 16.0)
    self.assertEqual(journal["final_kb"], 0.0)
    self.assertEqual(journal["delta_kb"], 0.0)

    old_log = by_file[
        "app_content_shell/content_shell/Local Storage/leveldb/000003.log"]
    self.assertFalse(old_log["existed_at_end"])
    self.assertEqual(old_log["start_kb"], 64.0)
    self.assertEqual(old_log["peak_kb"], 64.0)
    self.assertEqual(old_log["final_kb"], 0.0)
    self.assertEqual(old_log["delta_kb"], -64.0)

    # Verify Markdown formatting renders Proposal 1 headers, Start/Delta KB,
    # and status
    md = storage_monitor.format_markdown_report(report)
    self.assertIn("## Section A: Storage Type Summary", md)
    self.assertIn("## Section B: Transient & High-Churn File Hotspots", md)
    self.assertIn(
        "| Relative File Path | Storage Type | Status | Lifespan | "
        "Start KB | Peak KB | Final KB | Delta KB | Writes (`mtime_changes`) |",
        md,
    )
    self.assertIn("-64.00", md)
    self.assertIn("**Transient (Deleted)**", md)

  def test_parse_proc_io_output(self):
    sample = storage_monitor.parse_proc_io_output("\n".join([
        "pid: 1234",
        "rchar: 102400",
        "wchar: 204800",
        "read_bytes: 40960",
        "write_bytes: 81920",
        "cancelled_write_bytes: 4096",
    ]))
    self.assertEqual(
        sample,
        storage_monitor.ProcIoSample(
            pid=1234, read_bytes=40960, write_bytes=81920))
    # No output (process not running) or no pid line.
    self.assertIsNone(storage_monitor.parse_proc_io_output(""))
    self.assertIsNone(
        storage_monitor.parse_proc_io_output("read_bytes: 1\nwrite_bytes: 2"))

  def test_tracks_block_io_across_restarts(self):
    tracker = storage_monitor.StorageTracker(
        package="com.google.android.youtube.tv")
    sample = storage_monitor.ProcIoSample
    # PID 100 is already running at t=0 (e.g. manual mode), so its counters
    # predate monitoring and only set the baseline.
    tracker.record_proc_io(0.0,
                           sample(pid=100, read_bytes=4096, write_bytes=8192))
    # +8 KB read and +16 KB written in 1 s.
    tracker.record_proc_io(1.0,
                           sample(pid=100, read_bytes=12288, write_bytes=24576))
    # The app is force-stopped, then relaunched as PID 200.
    tracker.record_proc_io(2.0, None)
    tracker.record_proc_io(3.0,
                           sample(pid=200, read_bytes=4096, write_bytes=8192))
    # PID 200 restarts with the same (recycled) PID, so its counters go back.
    tracker.record_proc_io(4.0,
                           sample(pid=200, read_bytes=2048, write_bytes=1024))

    report = tracker.build_summary_data(
        platform="android",
        cuj="warm_relaunch",
        duration_sec=4.0,
        interval_sec=1.0,
    )
    pio = report["process_io_summary"]
    self.assertEqual(pio["pids_tracked"], 2)
    # Read: 8 + 4 + 2 KB. Write: 16 + 8 + 1 KB.
    self.assertEqual(pio["read_kb"], 14.0)
    self.assertEqual(pio["write_kb"], 25.0)
    self.assertEqual(pio["peak_read_kbps"], 8.0)
    self.assertEqual(pio["peak_read_at_sec"], 1.0)
    self.assertEqual(pio["peak_write_kbps"], 16.0)
    self.assertEqual(pio["peak_write_at_sec"], 1.0)

    md = storage_monitor.format_markdown_report(report)
    self.assertIn("- **Platform**: `android` (`com.google.android.youtube.tv`)",
                  md)
    self.assertIn(
        "- **Block I/O (`/proc/<pid>/io`)**: "
        "Read: `14.00 KB` (peak `8.00 KB/s` at `t=1.0s`) | "
        "Write: `25.00 KB` (peak `16.00 KB/s` at `t=1.0s`) | "
        "Processes: `2`", md)

  def test_counts_all_io_of_process_started_after_first_poll(self):
    tracker = storage_monitor.StorageTracker()
    # Not running when monitoring starts (e.g. right before cold_start).
    tracker.record_proc_io(0.0, None)
    tracker.record_proc_io(
        1.0,
        storage_monitor.ProcIoSample(pid=7, read_bytes=10240, write_bytes=2048))
    pio = tracker.build_summary_data(
        platform="android",
        cuj="cold_start",
        duration_sec=1.0,
        interval_sec=1.0)["process_io_summary"]
    self.assertEqual(pio["read_kb"], 10.0)
    self.assertEqual(pio["write_kb"], 2.0)

  def test_parse_args_defaults_and_custom_package(self):
    args = storage_monitor.parse_args([])
    self.assertEqual(args.cuj, "manual")
    self.assertEqual(args.package, "dev.cobalt.coat")

    custom = storage_monitor.parse_args([
        "--cuj",
        "cold_start",
        "--package",
        "com.google.android.youtube.tv",
    ])
    self.assertEqual(custom.cuj, "cold_start")
    self.assertEqual(custom.package, "com.google.android.youtube.tv")

  @mock.patch("cobalt.tools.storage.storage_monitor._poll_for_duration")
  def test_manual_cuj_polls_passively(self, mock_poll_for_duration):
    tracker = storage_monitor.StorageTracker()
    duration = storage_monitor.run_cuj_android(
        cuj="manual",
        interval_sec=1.0,
        tracker=tracker,
        package="dev.cobalt.coat",
        snapshot_fn=lambda: {},
        proc_io_fn=lambda: None,
    )
    self.assertGreaterEqual(duration, 0.0)
    mock_poll_for_duration.assert_called_once()
    self.assertEqual(
        mock_poll_for_duration.call_args.kwargs["duration_sec"],
        float("inf"),
    )
    self.assertEqual(
        mock_poll_for_duration.call_args.kwargs["phase_label"],
        "manual",
    )

  @mock.patch("cobalt.tools.storage.storage_monitor.time.sleep")
  @mock.patch("cobalt.tools.storage.storage_monitor.run_adb_shell")
  def test_launch_other_package_restricts_intent(self, mock_adb, _):
    # pylint: disable=protected-access
    storage_monitor._launch_cobalt_android(
        "https://www.youtube.com/tv", package="com.google.android.youtube.tv")
    mock_adb.assert_called_with(
        "am start -a android.intent.action.VIEW "
        "-d 'https://www.youtube.com/tv' -p com.google.android.youtube.tv",
        package=None)

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
