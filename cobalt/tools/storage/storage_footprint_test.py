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
"""Unit tests for cobalt/tools/storage/storage_footprint.py."""

import json
import os
import sys
import tempfile
import unittest

_REPO_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "..", "..", ".."))
if _REPO_ROOT not in sys.path:
  sys.path.insert(0, _REPO_ROOT)

# pylint: disable=wrong-import-position
from cobalt.tools.storage import storage_footprint as sf
from cobalt.tools.storage import storage_monitor
# pylint: enable=wrong-import-position

PKG = "com.example.app"
APP = f"/data/data/{PKG}"
UD = f"{APP}/app_content_shell/content_shell"
DE = f"/data/user_de/0/{PKG}"


def _file(path, size, allocated=None):
  return sf.StatEntry(
      path, False, True, size, allocated if allocated is not None else
      ((size + 4095) // 4096 + 1) * 4096, 0)


def _dir(path):
  return sf.StatEntry(path, True, False, 4096, 8192, 0)


def _base_entries():
  return [
      _dir(APP),
      _dir(f"{APP}/app_content_shell"),
      _dir(f"{APP}/app_content_shell/BrowserStabilityMetrics"),
      _file(f"{APP}/app_content_shell/BrowserStabilityMetrics/1.pma", 135168),
      _dir(UD),
      _file(f"{UD}/Cookies", 24576),
      _file(f"{UD}/DIPS", 4096),
      _dir(f"{UD}/Local Storage"),
      _dir(f"{UD}/Local Storage/leveldb"),
      _file(f"{UD}/Local Storage/leveldb/000003.log", 1000),
      _dir(f"{UD}/Session Storage"),
      _file(f"{UD}/Session Storage/LOG", 300),
      _dir(f"{APP}/cache"),
      _dir(f"{APP}/cache/Cache"),
      _dir(f"{APP}/cache/Cache/Cache_Data"),
      _file(f"{APP}/cache/Cache/Cache_Data/index", 3000),
      _file(f"{APP}/cache/Variations", 85),
      _dir(f"{APP}/no_backup"),
      _file(f"{APP}/no_backup/androidx.work.workdb", 106496),
      _dir(DE),
      _dir(f"{DE}/cache"),
  ]


class BucketTableTest(unittest.TestCase):
  """Tests parsing the shared table and classifying paths."""

  def test_parses_the_shared_table(self):
    table = sf.load_bucket_table()
    self.assertEqual(len(table.buckets), 23)
    self.assertEqual(table.buckets[0], "Cookies")
    self.assertIn(sf.USER_DE, table.all_buckets())
    for root in ("UserData", "DirCache", "AppData"):
      self.assertTrue(
          any(r.root == root and not r.path for r in table.rules), root)

  def test_parses_wrapped_macros_and_comments(self):
    table = sf.parse_bucket_table("""
// COBALT_STORAGE_BUCKET(NotABucket)
COBALT_STORAGE_BUCKET(A)  // Trailing comment.
COBALT_STORAGE_BUCKET(B)
COBALT_STORAGE_PATH(UserData,
                    "x/y",
                    A)
COBALT_STORAGE_PATH(UserData, "", B)
""")
    self.assertEqual(table.buckets, ("A", "B"))
    self.assertEqual(table.classify("UserData", "x/y/z"), "A")
    self.assertEqual(table.classify("UserData", "x"), "B")
    with self.assertRaises(ValueError):
      sf.parse_bucket_table("COBALT_STORAGE_BUCKET(A)\n"
                            'COBALT_STORAGE_PATH(UserData, "x", A)')
    with self.assertRaises(ValueError):
      sf.parse_bucket_table("COBALT_STORAGE_BUCKET(A)\n"
                            'COBALT_STORAGE_PATH(UserData, "", Missing)')

  def test_classifies_like_the_browser(self):
    # Same cases as CobaltStorageFootprintTest.ClassifiesPaths.
    table = sf.load_bucket_table()
    cases = [
        ("UserData", "", "OtherUserData"),
        ("DirCache", "", "OtherCache"),
        ("AppData", "", "OtherAppData"),
        ("UserData", "Cookies-journal", "Cookies"),
        ("UserData", "Network/Cookies-journal", "Cookies"),
        ("UserData", "Network/TransportSecurity", "NetworkState"),
        ("UserData", "Local Storage/leveldb/000003.log", "LocalStorage"),
        ("UserData", "WebStorage/QuotaManager-journal", "QuotaManager"),
        ("UserData", "DIPS-wal", "DIPS"),
        ("UserData", "Cache/Cache_Data/index", "CleanupTargets"),
        ("UserData", "old_Code Cache_000/js/index", "CleanupTargets"),
        ("UserData", "SharedStorage-wal", "CleanupTargets"),
        ("UserData", "BrowserStabilityMetrics/1.pma", "StabilityPma"),
        ("UserData", "Cookiesx", "OtherUserData"),
        ("UserData", "Cookies-journalx", "OtherUserData"),
        ("UserData", "Cookies-wal/x", "OtherUserData"),
        ("UserData", ".org.chromium.Chromium.abc", "OtherUserData"),
        ("DirCache", "Cache/Cache_Data/index", "HttpCache"),
        ("DirCache", "Code Cache/js/index-dir/the-real-index", "CodeCacheJs"),
        ("DirCache", "Code Cache/wasm/abc_0", "CodeCacheWasm"),
        ("DirCache", "Code Cache", "OtherCache"),
        ("DirCache", "ShaderCache/data_0", "ShaderCache"),
        ("DirCache", "crashpad/pending/1.dmp", "Crashpad"),
        ("DirCache", "Experiment Config", "OtherCache"),
        ("AppData", "app_content_shell", "OtherAppData"),
        ("AppData", "app_content_shell/BrowserStabilityMetrics/1.pma",
         "StabilityPma"),
        ("AppData", "files/x", "AndroidFiles"),
        ("AppData", "no_backup/androidx.work.workdb-wal", "AndroidNoBackup"),
        ("AppData", "databases/a.db", "AndroidDbAndPrefs"),
        ("AppData", "shared_prefs/b.xml", "AndroidDbAndPrefs"),
        ("AppData", "code_cache/c", "OtherAppData"),
        (sf.USER_DE, "cache/x", sf.USER_DE),
    ]
    for root, path, bucket in cases:
      self.assertEqual(table.classify(root, path), bucket, f"{root}:{path}")


class CollectionTest(unittest.TestCase):
  """Tests parsing stat output and aggregating a footprint."""

  def test_parse_stat_entries(self):
    entries = sf.parse_stat_entries("\n".join([
        f"41c0 4096 16 512 1794555816 {APP}",
        f"81b0 24576 56 512 1794555816 {UD}/Cookies",
        f"41c0 4096 16 512 1794555816 {UD}/Local Storage",
        "garbage line",
        f"zz 1 1 512 1 {APP}/bad_mode",
    ]))
    self.assertEqual([e.path for e in entries],
                     [APP, f"{UD}/Cookies", f"{UD}/Local Storage"])
    self.assertTrue(entries[0].is_dir)
    self.assertTrue(entries[1].is_file)
    self.assertEqual(entries[1].allocated, 56 * 512)
    self.assertEqual(entries[1].size, 24576)
    self.assertIn("\\( -type f -o -type d \\)", sf.find_stat_command([APP, DE]))

  def test_aggregate_attributes_nested_roots(self):
    table = sf.load_bucket_table()
    entries = _base_entries()
    footprint = sf.aggregate_footprint(entries, sf.android_roots(PKG), table)

    def alloc(prefixes):
      return sum(
          e.allocated
          for e in entries
          if any(e.path == p or e.path.startswith(p + "/") for p in prefixes))

    roots = footprint["roots"]
    self.assertEqual(roots["AppData"]["allocated_bytes"], alloc([APP]))
    self.assertEqual(roots["UserData"]["allocated_bytes"], alloc([UD]))
    self.assertEqual(roots["DirCache"]["allocated_bytes"],
                     alloc([f"{APP}/cache"]))
    self.assertEqual(roots["UserDe"]["allocated_bytes"], alloc([DE]))
    self.assertEqual(roots["UserData"]["files"], 4)
    self.assertEqual(roots["UserData"]["logical_bytes"],
                     24576 + 4096 + 1000 + 300)
    buckets = footprint["buckets"]
    # Every entry is in exactly one bucket.
    self.assertEqual(
        sum(b["allocated_bytes"] for b in buckets.values()),
        roots["AppData"]["allocated_bytes"] +
        roots["UserDe"]["allocated_bytes"])
    self.assertEqual(buckets["Cookies"]["allocated_bytes"],
                     alloc([f"{UD}/Cookies"]))
    self.assertEqual(buckets["Cookies"]["root"], "UserData")
    self.assertEqual(buckets["HttpCache"]["root"], "DirCache")
    self.assertEqual(buckets["StabilityPma"]["root"], "AppData")
    self.assertEqual(buckets["CleanupTargets"]["allocated_bytes"],
                     alloc([f"{UD}/Session Storage"]))
    self.assertEqual(buckets["IndexedDB"]["allocated_bytes"], 0)
    self.assertIsNone(buckets["IndexedDB"]["root"])
    folders = footprint["folders"]
    self.assertEqual(folders[""][0], roots["AppData"]["allocated_bytes"])
    self.assertEqual(folders["app_content_shell/content_shell"][0],
                     roots["UserData"]["allocated_bytes"])
    self.assertEqual(folders["user_de"][0], roots["UserDe"]["allocated_bytes"])
    self.assertEqual(folders["cache/Cache"][2], 1)
    self.assertEqual(footprint["files"][0][0],
                     "app_content_shell/BrowserStabilityMetrics/1.pma")

  def test_keeps_sqlite_companions_and_their_database(self):
    table = sf.load_bucket_table()
    entries = _base_entries() + [
        _file(f"{APP}/cache/Cache/Cache_Data/{i:04x}_0", 500000)
        for i in range(sf.NOTABLE_FILE_COUNT)
    ] + [_file(f"{UD}/DIPS-wal", 100000)]
    files = {
        f[0] for f in sf.aggregate_footprint(entries, sf.android_roots(PKG),
                                             table)["files"]
    }
    self.assertIn("app_content_shell/content_shell/DIPS-wal", files)
    self.assertIn("app_content_shell/content_shell/DIPS", files)


class TimelineTest(unittest.TestCase):
  """Tests the JSONL timeline, analysis, findings and reports."""

  def setUp(self):
    self.temp_dir = tempfile.TemporaryDirectory()  # pylint: disable=consider-using-with
    self.path = os.path.join(self.temp_dir.name, "run.jsonl")

  def tearDown(self):
    self.temp_dir.cleanup()

  def _write_run(self, extra_args=""):
    table = sf.load_bucket_table()
    timeline = sf.FootprintTimeline(
        self.path,
        package=PKG,
        platform="android",
        cuj="cold_start",
        interval_sec=1.0,
        extra_args=extra_args,
        roots=sf.android_roots(PKG),
        table=table)
    timeline.event(0.0, "cuj_phase", "cold_start")
    legacy = [_dir(f"{UD}/Cache")
             ] + [_file(f"{UD}/Cache/f{i}", 1 << 20) for i in range(3)]
    for t in range(0, 61):
      entries = _base_entries()
      if t < 32:
        entries += legacy
      if t >= 3:
        entries.append(_file(f"{UD}/DIPS-wal", 100000 * (t - 2)))
        entries += [
            _file(f"{APP}/cache/Cache/Cache_Data/{i}_0", 200000)
            for i in range(t - 2)
        ]
      if 40 <= t < 43:
        entries.append(_file(f"{UD}/Local Storage/leveldb/000005.ldb", 2 << 20))
      if t == 1:
        timeline.event(1.0, "launch", "https://www.youtube.com/tv")
      pid = 100 if t >= 3 else None
      timeline.sample(
          float(t), entries, pid=pid, io=(t * 4096, t * 8192) if pid else None)
    timeline.close()
    return timeline

  def test_round_trip_and_events(self):
    timeline = self._write_run(extra_args="--enable-features=X")
    self.assertEqual(timeline.sample_count, 61)
    loaded = sf.load_timeline(self.path)
    self.assertEqual(loaded.header["package"], PKG)
    self.assertEqual(loaded.header["extra_args"], "--enable-features=X")
    self.assertEqual(len(loaded.samples), 61)
    events = [(e["event"], e["t"]) for e in loaded.events]
    self.assertIn(("launch", 1.0), events)
    self.assertIn(("process_start", 3.0), events)
    # A truncated last line (interrupted run) is ignored.
    with open(self.path, "a", encoding="utf-8") as f:
      f.write('{"type": "sample", "t"')
    self.assertEqual(len(sf.load_timeline(self.path).samples), 61)
    with open(self.path, encoding="utf-8") as f:
      header = json.loads(f.readline())
    self.assertEqual(header["type"], "header")
    self.assertEqual(header["roots"]["UserData"], UD)

  def test_analysis_moments_groups_and_findings(self):
    self._write_run()
    analysis = sf.analyze(sf.load_timeline(self.path))
    labels = [(m.label, analysis.times[m.index]) for m in analysis.moments]
    self.assertEqual(labels, [("Start", 0.0), ("+10 s after launch", 13.0),
                              ("+30 s after launch", 33.0), ("End", 60.0)])
    self.assertEqual([g.name for g, _ in analysis.groups],
                     ["UserData", "DirCache", "AppData", sf.USER_DE])
    user_data_rows = [r.name for r in analysis.groups[0][1]]
    self.assertEqual(user_data_rows[0], "DIPS")  # The largest peak first.
    self.assertIn("IndexedDB", analysis.empty_buckets)
    self.assertEqual(analysis.total.series[-1],
                     sf.total_bytes(analysis.timeline.samples[-1]))

    titles = [f.title for f in analysis.findings]
    severities = [f.severity for f in analysis.findings]
    self.assertEqual(severities,
                     sorted(severities, key=["high", "medium", "info"].index))
    self.assertTrue(
        any(t.startswith("UserData is ") and "budget" in t for t in titles),
        titles)
    self.assertTrue(
        any(
            t.startswith("DIPS-wal is ") and "its database" in t
            for t in titles), titles)
    self.assertTrue(
        any(t.startswith("HttpCache grew by") for t in titles), titles)
    self.assertTrue(
        any(t.startswith("LocalStorage peaked") for t in titles), titles)
    drop = next(
        f for f in analysis.findings
        if f.title.startswith("CleanupTargets dropped"))
    self.assertIn("storage cleanup", drop.details[0])
    self.assertFalse(any(t.startswith("LocalStorage dropped") for t in titles))

  def test_drop_events(self):
    mb = 1 << 20
    # pylint: disable=protected-access
    # A fall over several polls is one drop.
    self.assertEqual(
        [(e[1], e[2]) for e in sf._drop_events([4 * mb, 3 * mb, 2 * mb, mb])],
        [(0, 3)])
    # Drop, regrow, drop: churn.
    churn = [4 * mb, 2 * mb, 2 * mb, 4 * mb, 4 * mb, 2 * mb]
    self.assertEqual([(e[1], e[2]) for e in sf._drop_events(churn)], [(0, 1),
                                                                      (3, 5)])
    # Relatively small (2% of 100 MB) or small (200 KB) changes are not drops.
    self.assertEqual(sf._drop_events([100 * mb, 99 * mb, 98 * mb]), [])
    self.assertEqual(sf._drop_events([200 << 10, 0]), [])

  def test_multi_cuj_moments_use_cujs(self):
    timeline = sf.FootprintTimeline(
        self.path,
        package=PKG,
        platform="android",
        cuj="all",
        interval_sec=1.0,
        extra_args="",
        roots=sf.android_roots(PKG),
        table=sf.load_bucket_table())
    for t, name, detail in ((0.0, "cuj", "pause_flush"),
                            (0.5, "cuj_phase", "pre_pause"), (5.0, "cuj_phase",
                                                              "pause_flush"),
                            (10.0, "cuj", "warm_relaunch"), (10.5, "cuj_phase",
                                                             "warm_relaunch_1"),
                            (15.0, "cuj_phase", "warm_relaunch_2")):
      timeline.event(t, name, detail)
    for t in range(21):
      timeline.sample(float(t), _base_entries())
    timeline.close()
    analysis = sf.analyze(sf.load_timeline(self.path))
    self.assertEqual(
        [(m.label, analysis.times[m.index]) for m in analysis.moments],
        [("Start", 0.0), ("end of pause_flush", 9.0), ("End", 20.0)])

  def test_reports(self):
    self._write_run()
    html_path = os.path.join(self.temp_dir.name, "run.html")
    md_path = os.path.join(self.temp_dir.name, "run.md")
    analysis = sf.write_reports(self.path, html_path, md_path)
    with open(md_path, encoding="utf-8") as f:
      markdown = f.read()
    for text in ("## Storage Footprint by Bucket", "### Findings",
                 "### Footprint at key moments", "+10 s after launch",
                 "### Largest folders at the end", "### Largest files",
                 "| `DIPS` |", "**Total**"):
      self.assertIn(text, markdown)
    with open(html_path, encoding="utf-8") as f:
      page = f.read()
    for text in ("<svg", "id='findings'", "id='tree'", "UserData root",
                 "DIPS-wal"):
      self.assertIn(text, page)
    # The legacy cache deleted at t=32 s, and the LevelDB compaction peak.
    self.assertIn("<s>Cache</s><span class='tag'>deleted at t=32s (was 3.0",
                  page)
    self.assertIn("<span class='peak'>peak 2.", page)
    self.assertNotIn("<script", page)
    self.assertNotIn("src=", page)

    comparison = sf.format_comparison(analysis, analysis)
    self.assertIn("# Storage footprint comparison", comparison)
    self.assertIn("| `DIPS` |", comparison)
    self.assertIn("### Findings in B", comparison)

  def test_format_size(self):
    self.assertEqual(sf.fmt_size(0), "0")
    self.assertEqual(sf.fmt_size(512), "512 B")
    self.assertEqual(sf.fmt_size(4096), "4 KB")
    self.assertEqual(sf.fmt_size(3 << 20), "3.00 MB")
    self.assertEqual(sf.fmt_size(25 << 20), "25.0 MB")
    self.assertEqual(sf.fmt_delta(-4096), "-4 KB")
    self.assertEqual(sf.fmt_delta(4096), "+4 KB")


class StorageMonitorIntegrationTest(unittest.TestCase):
  """Tests that storage_monitor feeds both its tracker and the timeline."""

  def test_poll_feeds_tracker_and_timeline(self):
    with tempfile.TemporaryDirectory() as temp_dir:
      path = os.path.join(temp_dir, "run.jsonl")
      timeline = sf.FootprintTimeline(
          path,
          package=PKG,
          platform="android",
          cuj="manual",
          interval_sec=1.0,
          extra_args="",
          roots=sf.android_roots(PKG),
          table=sf.load_bucket_table())
      tracker = storage_monitor.StorageTracker(package=PKG)
      proc_io = storage_monitor.ProcIoSample(
          pid=7, read_bytes=4096, write_bytes=8192)
      # pylint: disable=protected-access
      storage_monitor._poll_for_duration(
          tracker,
          duration_sec=0.0,
          interval_sec=1.0,
          run_start_time=0.0,
          snapshot_fn=lambda: self.fail("uses entries instead"),
          proc_io_fn=lambda: proc_io,
          phase_label="manual",
          timeline=timeline,
          entries_fn=_base_entries)
      timeline.close()
      self.assertIn("app_content_shell/content_shell/Cookies",
                    tracker.file_records)
      self.assertEqual(tracker.pids_seen, {7})
      loaded = sf.load_timeline(path)
      self.assertEqual(len(loaded.samples), 1)
      self.assertEqual(loaded.samples[0]["pid"], 7)
      self.assertEqual(loaded.samples[0]["io"]["write_bytes"], 8192)
      self.assertEqual([e["event"] for e in loaded.events],
                       ["cuj_phase", "process_start"])

  def test_snapshot_from_entries_keeps_files(self):
    snapshot = storage_monitor.snapshot_from_entries(_base_entries(), PKG)
    self.assertEqual(snapshot["app_content_shell/content_shell/Cookies"],
                     (24576, 0))
    self.assertNotIn("app_content_shell/content_shell", snapshot)

  def test_parse_args_timeline_and_compare(self):
    args = storage_monitor.parse_args(
        ["--timeline", "/tmp/a.jsonl", "--compare", "a.jsonl", "b.jsonl"])
    self.assertEqual(args.timeline, "/tmp/a.jsonl")
    self.assertEqual(args.compare, ["a.jsonl", "b.jsonl"])


if __name__ == "__main__":
  unittest.main()
