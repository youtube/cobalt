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

  def test_parse_and_track_proc_io(self):
    raw_proc_io = "\n".join([
        "pid: 1234",
        "rchar: 102400",
        "wchar: 204800",
        "syscr: 50",
        "syscw: 100",
        "read_bytes: 40960",
        "write_bytes: 81920",
        "cancelled_write_bytes: 4096",
        "---DISKSTATS---",
        " 179 0 mmcblk0 1000 50 8000 400 2000 100 16000 800 0 600 1200",
        " 179 1 mmcblk0p1 500 25 4000 200 1000 50 8000 400 0 300 600",
        " 179 32 mmcblk0boot0 10 0 80 5 0 0 0 0 0 5 5",
        "---PSI_IO---",
        "some avg10=2.50 avg60=1.00 avg300=0.20 total=150000",
        "full avg10=0.75 avg60=0.30 avg300=0.05 total=40000",
    ])
    sample = storage_monitor.parse_proc_io_output(raw_proc_io)
    self.assertIsNotNone(sample)
    self.assertEqual(sample.pid, 1234)
    self.assertEqual(sample.rchar, 102400)
    self.assertEqual(sample.wchar, 204800)
    self.assertEqual(sample.syscr, 50)
    self.assertEqual(sample.syscw, 100)
    self.assertEqual(sample.read_bytes, 40960)
    self.assertEqual(sample.write_bytes, 81920)
    self.assertEqual(sample.cancelled_write_bytes, 4096)
    self.assertEqual(sample.disk_reads_completed, 1000)
    self.assertEqual(sample.disk_writes_completed, 2000)
    self.assertEqual(sample.disk_io_time_ms, 600)
    self.assertEqual(sample.psi_some_total_us, 150000)
    self.assertEqual(sample.psi_full_total_us, 40000)
    self.assertEqual(sample.psi_some_avg10, 2.50)
    self.assertEqual(sample.psi_full_avg10, 0.75)

    self.assertIsNone(storage_monitor.parse_proc_io_output(""))

    tracker = storage_monitor.StorageTracker(
        package="com.google.android.youtube.tv")
    # PID 100 already running at t=0.0s (e.g. manual mode): baseline subtracted
    tracker.record_proc_io(
        0.0,
        storage_monitor.ProcIoSample(
            pid=100,
            rchar=10240,
            wchar=20480,
            syscr=10,
            syscw=20,
            read_bytes=4096,
            write_bytes=8192,
            disk_reads_completed=100,
            disk_writes_completed=200,
            disk_io_time_ms=50,
            psi_some_total_us=10000,
            psi_full_total_us=2000,
            psi_some_avg10=1.2,
            psi_full_avg10=0.3,
        ),
    )
    tracker.record_proc_io(
        1.0,
        storage_monitor.ProcIoSample(
            pid=100,
            rchar=30720,
            wchar=61440,
            syscr=30,
            syscw=60,
            read_bytes=12288,
            write_bytes=24576,
            disk_reads_completed=140,
            disk_writes_completed=280,
            disk_io_time_ms=110,
            psi_some_total_us=35000,
            psi_full_total_us=7000,
            psi_some_avg10=3.5,
            psi_full_avg10=1.1,
        ),
    )
    # PID 200 spawned during warm_relaunch at t=2.0s: full counter accrued
    tracker.record_proc_io(
        2.0,
        storage_monitor.ProcIoSample(
            pid=200,
            rchar=10240,
            wchar=20480,
            syscr=15,
            syscw=25,
            read_bytes=4096,
            write_bytes=8192,
            disk_reads_completed=160,
            disk_writes_completed=310,
            disk_io_time_ms=150,
            psi_some_total_us=45000,
            psi_full_total_us=9000,
            psi_some_avg10=2.0,
            psi_full_avg10=0.5,
        ),
    )

    report = tracker.build_summary_data(
        platform="android",
        cuj="warm_relaunch",
        duration_sec=2.0,
        interval_sec=1.0,
    )
    self.assertEqual(report["metadata"]["package"],
                     "com.google.android.youtube.tv")
    pio = report["process_io_summary"]
    self.assertEqual(pio["pids_tracked"], 2)
    # PID 100 delta (20 KB rchar, 40 KB wchar) + PID 200 total (10 KB, 20 KB)
    self.assertEqual(pio["rchar_kb"], 30.0)
    self.assertEqual(pio["wchar_kb"], 60.0)
    self.assertEqual(pio["read_kb"], 12.0)
    self.assertEqual(pio["write_kb"], 24.0)
    self.assertEqual(pio["syscr"], 35)
    self.assertEqual(pio["syscw"], 65)
    self.assertEqual(pio["peak_read_kbps"], 8.0)
    self.assertEqual(pio["peak_read_at_sec"], 1.0)
    self.assertEqual(pio["peak_write_kbps"], 16.0)
    self.assertEqual(pio["peak_write_at_sec"], 1.0)
    self.assertEqual(pio["write_amp_factor"], 0.4)

    dio = report["disk_io_summary"]
    self.assertEqual(dio["reads_completed"], 60)
    self.assertEqual(dio["writes_completed"], 110)
    self.assertEqual(dio["avg_read_iops"], 30.0)
    self.assertEqual(dio["avg_write_iops"], 55.0)
    self.assertEqual(dio["peak_read_iops"], 40.0)
    self.assertEqual(dio["peak_read_iops_at_sec"], 1.0)
    self.assertEqual(dio["peak_write_iops"], 80.0)
    self.assertEqual(dio["peak_write_iops_at_sec"], 1.0)
    self.assertEqual(dio["io_busy_ms"], 100)

    psi = report["psi_io_summary"]
    self.assertEqual(psi["some_stall_ms"], 35.0)
    self.assertEqual(psi["full_stall_ms"], 7.0)
    self.assertEqual(psi["peak_some_avg10"], 3.5)
    self.assertEqual(psi["peak_full_avg10"], 1.1)

    md = storage_monitor.format_markdown_report(report)
    self.assertIn("- **Platform**: `android` (`com.google.android.youtube.tv`)",
                  md)
    self.assertIn("**Process I/O (`/proc/<pid>/io`)**:", md)
    self.assertIn("VFS Write (`wchar`): `60.00 KB`", md)
    self.assertIn("Block Write (`write_bytes`): `24.00 KB`", md)
    self.assertIn("Peak: `16.00 KB/s` at `t=1.0s`", md)
    self.assertIn("Write Amp (`write_bytes/wchar`): `0.40x`", md)
    self.assertIn("**Block Device IOPS (`/proc/diskstats`)**:", md)
    self.assertIn("Read IOPS: avg `30.00`, peak `40.00`", md)
    self.assertIn("**I/O Pressure Stall (`/proc/pressure/io`)**:", md)
    self.assertIn("Some Stall: `35.00 ms` (peak avg10: `3.50%`)", md)

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
