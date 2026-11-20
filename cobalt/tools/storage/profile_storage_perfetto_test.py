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
"""Unit tests for cobalt/tools/storage/profile_storage_perfetto.py."""

import hashlib
import io
import json
import os
import re
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

_REPO_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "..", "..", ".."))
if _REPO_ROOT not in sys.path:
  sys.path.insert(0, _REPO_ROOT)

# pylint: disable=wrong-import-position
from cobalt.tools.storage import profile_storage_perfetto as psp
# pylint: enable=wrong-import-position

_HEALTH_CSV = ('"kind","name","idx","severity","value"\n'
               '"meta","tracing_disabled_ns","[NULL]","[NULL]",70000000000\n'
               '"meta","duration_ns","[NULL]","[NULL]",65000000000\n')


class FakeSubprocess:
  """Fakes adb, --cuj-cmd and trace_processor_shell for subprocess.run.

  Lifetime/Ownership: Created and owned by a single test.
  Threading Model: Single-threaded test execution.
  """

  def __init__(self,
               drop_caches_rc: int = 0,
               failing_query: str = "",
               health_csv: str = _HEALTH_CSV,
               cuj_cmd_rc: int = 0):
    self.commands = []
    self.inputs = []
    self.uptime = 100.0
    self.drop_caches_rc = drop_caches_rc
    self.failing_query = failing_query
    self.health_csv = health_csv
    self.cuj_cmd_rc = cuj_cmd_rc
    self.perfetto_alive_polls = 1

  def run(self, cmd, **kwargs):
    self.commands.append(cmd)
    self.inputs.append(kwargs.get("input"))
    rc, out = 0, ""
    if isinstance(cmd, str):  # --cuj-cmd (shell=True).
      rc = self.cuj_cmd_rc
    elif cmd[0] == "adb":
      shell = cmd[-1]
      if shell == "get-state":
        out = "device\n"
      elif "pull" in cmd:
        with open(cmd[-1], "wb") as f:
          f.write(b"trace-bytes")
      elif "versionName" in shell:
        out = ("perfetto: Perfetto v44.0-abc (abc)\n"
               "    versionName=8.03.1-DEVELOPER\n"
               "fingerprint: google/sabrina/sabrina:14/user/release-keys\n")
      elif "drop_caches" in shell:
        rc = self.drop_caches_rc
      elif "perfetto --background-wait" in shell:
        out = "4242\n"
      elif shell.startswith("test -d /proc/"):
        rc = 0 if self.perfetto_alive_polls > 0 else 1
        self.perfetto_alive_polls -= 1
      elif "/proc/uptime" in shell:
        out = (f"{self.uptime:.2f} 9.00\nStarting: Intent\n"
               f"{self.uptime + 0.5:.2f} 9.00\n")
        self.uptime += 10
    elif "--version" in cmd:
      out = "Perfetto v50.1-abc (abc)\nTrace Processor RPC API version: 14\n"
    else:
      with open(cmd[2], encoding="utf-8") as f:
        sql = f.read()
      if "FROM stats" in sql:
        rc, out = (0, self.health_csv) if self.health_csv else (1, "")
      elif self.failing_query and sql.startswith(self.failing_query):
        rc = 1
      else:
        out = '"col","value"\n"a",1.500000\n'
    return subprocess.CompletedProcess(cmd, rc, out, "")


class ProfileStoragePerfettoTest(unittest.TestCase):
  """Tests for the Perfetto config, CUJ driver, queries, and reports.

  Lifetime/Ownership: Instantiated and managed by the unittest runner per test.
  Threading Model: Single-threaded test execution.
  """

  def test_build_perfetto_config(self):
    config = psp.build_perfetto_config(185)
    self.assertIn("duration_ms: 185000", config)
    # The 3 Chromium data sources only go to the app's Chromium producer.
    filters = re.findall(r'producer_name_regex_filter: "(.*)"', config)
    self.assertEqual(
        filters,
        ["org[.]chromium-com[.]google[.]android[.]youtube[.]tv-"
         "[0-9]+"] * 3)
    # Producers seen with `perfetto --query` on sabrina.
    for producer, expected in (
        ("org.chromium-com.google.android.youtube.tv-11190", True),
        ("cronet-com.google.android.youtube.tv-11190", False),
        ("cronet-com.google.android.gms-1337", False),
        ("cronet-com.google.android.katniss-4357", False),
        ("cronet-com.google.android.apps.tv.launcherx-1526", False),
        ("org.chromium-1234", False),
    ):
      self.assertEqual(bool(re.fullmatch(filters[0], producer)), expected)
    self.assertIn(
        'producer_name_regex_filter: "org[.]chromium-'
        'dev[.]cobalt[.]coat-[0-9]+"',
        psp.build_perfetto_config(185, "dev.cobalt.coat"))
    self.assertIn("write_into_file: true", config)
    self.assertIn("file_write_period_ms: 10000", config)
    self.assertIn("flush_timeout_ms: 30000", config)
    self.assertIn("buffers { size_kb: 8192 fill_policy: RING_BUFFER }", config)
    self.assertIn("buffers { size_kb: 32768 fill_policy: RING_BUFFER }", config)
    self.assertRegex(config, r'name: "linux.ftrace"\s+target_buffer: 1')
    self.assertIn("compact_sched { enabled: true }", config)
    for event in psp.FTRACE_EVENTS:
      self.assertIn(f'ftrace_events: "{event}"', config)
    for category in psp.TRACK_EVENT_CATEGORIES:
      self.assertIn(f'enabled_categories: "{category}"', config)
    for name in ("org.chromium.histogram_sample", "org.chromium.trace_metadata",
                 "linux.process_stats", "linux.sys_stats"):
      self.assertIn(f'name: "{name}"', config)
    for dropped in ("toplevel", "v8", "blink", "gpu", "memory-infra",
                    "memory_instrumentation"):
      self.assertNotIn(dropped, config)
    chrome_config = json.loads(
        json.loads(re.search(r"trace_config: (\".*\") }", config).group(1)))
    self.assertEqual(chrome_config["record_mode"], "record-continuously")
    self.assertEqual(
        chrome_config["included_categories"],
        [c for c in psp.TRACK_EVENT_CATEGORIES if c != "__metadata"])

  def test_build_perfetto_config_with_memory_infra(self):
    config = psp.build_perfetto_config(185, include_memory_infra=True)
    self.assertIn("buffers { size_kb: 16384 fill_policy: RING_BUFFER }", config)
    self.assertIn('enabled_categories: "disabled-by-default-memory-infra"',
                  config)
    self.assertIn('name: "org.chromium.memory_instrumentation"', config)
    self.assertIn("periodic_interval", config)
    self.assertEqual(config.count("producer_name_regex_filter"), 4)

  def test_duration_cap(self):
    for argv, cap in (([], 185), (["--duration-s",
                                   "10"], 130), (["--cuj", "lifecycle"], 242),
                      (["--cuj", "manual"],
                       3600), (["--cuj", "manual", "--cuj-cmd", "true"], 3600)):
      self.assertEqual(psp.duration_cap_s(psp.parse_args(argv)), cap, argv)

  def test_command_line_args_inject_tracing_feature(self):
    self.assertEqual(
        psp.build_command_line_args(""),
        "--enable-features=EnablePerfettoSystemTracing")
    self.assertEqual(
        psp.build_command_line_args(" enable-features=A;B , --v=1,"),
        "--enable-features=EnablePerfettoSystemTracing,"
        "--enable-features=A;B,--v=1")
    cmd = psp.build_launch_command("dev.cobalt.coat", "--enable-features=A;B")
    self.assertTrue(
        cmd.startswith("am start -S -a android.intent.action.VIEW "
                       "-d 'https://www.youtube.com/tv' "))
    self.assertIn(
        "--esa commandLineArgs '--enable-features=EnablePerfettoSystemTracing,"
        "--enable-features=A;B' ", cmd)
    self.assertTrue(
        cmd.endswith("--esa url_params 'force_startup_screen=none,hl=en,gl=US' "
                     "-p dev.cobalt.coat"))

  def test_load_queries_splits_eight_statements(self):
    queries = psp.load_queries()
    self.assertEqual([q[0] for q in queries], list(range(1, 9)))
    for number, title, sql in queries:
      self.assertTrue(title, f"Q{number}")
      code = "\n".join(line for line in sql.splitlines()
                       if not line.lstrip().startswith("--"))
      self.assertEqual(code.count(";"), 1, f"Q{number}")
      self.assertTrue(code.rstrip().endswith(";"), f"Q{number}")
    for _, _, sql in queries[6:]:
      self.assertIn(
          "WHERE (p.name = 'com.google.android.youtube.tv' OR p.upid IN "
          "(SELECT upid FROM thread WHERE name = 'CrBrowserMain'))", sql)
    for _, _, sql in psp.load_queries(package="dev.cobalt.coat")[6:]:
      self.assertIn("p.name = 'dev.cobalt.coat'", sql)
      self.assertNotIn("youtube", sql)

  def test_lifecycle_records_phases(self):
    fake = FakeSubprocess()
    args = psp.parse_args(["--cuj", "lifecycle", "--serial", "s1"])
    events = []
    with mock.patch.object(psp.subprocess, "run", side_effect=fake.run), \
        mock.patch.object(psp.time, "sleep") as sleep:
      self.assertEqual(psp.run_cuj(args, events), 0)
    self.assertEqual([e["name"] for e in events], ["launch"] +
                     ["KEYCODE_DPAD_DOWN", "KEYCODE_DPAD_RIGHT"] * 8 +
                     ["watch", "KEYCODE_HOME"])
    self.assertEqual(
        sum(c.args[0] for c in sleep.call_args_list), 30 + 16 * 2 + 45 + 15)
    shells = [cmd[-1] for cmd in fake.commands]
    self.assertTrue(
        all(cmd[:4] == ["adb", "-s", "s1", "shell"] for cmd in fake.commands))
    self.assertTrue(
        all(
            s.startswith("cat /proc/uptime; ") and
            s.endswith("; rc=$?; cat /proc/uptime; exit $rc") for s in shells))
    self.assertIn("am start -S ", shells[0])
    self.assertIn(
        "-d 'https://www.youtube.com/tv#/watch?v=1La4QzGeaaQ' "
        "-p com.google.android.youtube.tv", shells[17])
    self.assertNotIn(" -S ", shells[17])
    self.assertIn("input keyevent KEYCODE_HOME", shells[18])
    self.assertEqual((events[0]["start_s"], events[0]["end_s"]), (100.0, 100.5))

    events.append({
        "phase": "stop",
        "name": "stop_trace",
        "start_s": 400.0,
        "end_s": 400.5
    })
    phases = psp.phase_bounds(events)
    self.assertEqual([p["name"] for p in phases],
                     ["cold_start", "scroll", "play", "exit"])
    self.assertEqual((phases[0]["start_s"], phases[0]["end_s"]), (100.5, 110.5))
    self.assertEqual((phases[2]["start_s"], phases[2]["end_s"]), (270.5, 280.5))
    self.assertEqual((phases[3]["start_s"], phases[3]["end_s"]), (280.5, 400.5))

  def test_record_trace_restores_golden_and_brackets_cuj(self):
    fake = FakeSubprocess(drop_caches_rc=1)
    args = psp.parse_args([
        "--serial", "s1", "--golden-tar", "/data/local/tmp/golden.tar",
        "--drop-caches", "--duration-s", "5"
    ])
    with tempfile.TemporaryDirectory() as tmp, \
        mock.patch.object(psp.subprocess, "run", side_effect=fake.run), \
        mock.patch.object(psp.time, "sleep"), \
        mock.patch("sys.stdout", new_callable=io.StringIO), \
        mock.patch("sys.stderr", new_callable=io.StringIO) as stderr:
      trace = os.path.join(tmp, "cold.pftrace")
      phases_path = os.path.join(tmp, "cold.phases.json")
      capture = psp.record_trace(args, "CONFIG", trace, phases_path)
      with open(phases_path, encoding="utf-8") as f:
        self.assertEqual(json.load(f), capture)
    self.assertIn("--drop-caches needs root", stderr.getvalue())
    shells = [" ".join(cmd[3:]) for cmd in fake.commands]

    def index(text):
      return next(i for i, s in enumerate(shells) if text in s)

    restore = shells[index("tar xf")]
    self.assertIn(
        "run-as com.google.android.youtube.tv sh -c 'cd "
        "/data/data/com.google.android.youtube.tv && rm -rf ./cache "
        "./app_content_shell", restore)
    self.assertIn(
        "tar xf /data/local/tmp/golden.tar -C . && rm -f "
        "app_content_shell/BrowserStabilityMetrics/*.pma' && sync", restore)
    perfetto = index("perfetto --background-wait")
    self.assertEqual(
        shells[perfetto], "shell perfetto --background-wait --txt -c - "
        "-o /data/misc/perfetto-traces/cold.pftrace")
    self.assertEqual(fake.inputs[perfetto], "CONFIG")
    self.assertLess(index("am force-stop"), index("tar xf"))
    self.assertLess(index("tar xf"), index("drop_caches"))
    self.assertLess(index("drop_caches"), perfetto)
    self.assertLess(perfetto, index("am start -S"))
    self.assertLess(index("am start -S"), index("kill -INT 4242"))
    self.assertLess(index("kill -INT 4242"), index("test -d /proc/4242"))
    pull = index("pull")
    self.assertEqual(shells[pull - 2:pull], ["shell test -d /proc/4242"] * 2)
    self.assertEqual(shells[pull],
                     f"pull /data/misc/perfetto-traces/cold.pftrace {trace}")
    self.assertEqual(shells[-1],
                     "shell rm -f /data/misc/perfetto-traces/cold.pftrace")
    self.assertFalse(capture["caches_dropped"])
    self.assertEqual(capture["error"], "")
    self.assertEqual(capture["device"]["app_version_name"], "8.03.1-DEVELOPER")
    self.assertEqual([e["name"] for e in capture["events"]],
                     ["launch", "stop_trace"])
    self.assertEqual(capture["phases"], [{
        "name": "cold_start",
        "start_s": 100.5,
        "end_s": 110.5
    }])

  def test_interrupted_cuj_still_saves_trace(self):
    fake = FakeSubprocess()
    with tempfile.TemporaryDirectory() as tmp:
      trace_processor = os.path.join(tmp, "trace_processor_shell")
      with open(trace_processor, "w", encoding="utf-8") as f:
        f.write("fake")
      trace = os.path.join(tmp, "cold.pftrace")
      # Ctrl+C during the 65 s cold_start sleep.
      with mock.patch.object(psp.subprocess, "run", side_effect=fake.run), \
          mock.patch.object(psp.time, "sleep",
                            side_effect=[KeyboardInterrupt, None]), \
          mock.patch("sys.stdout", new_callable=io.StringIO), \
          mock.patch("sys.stderr", new_callable=io.StringIO) as stderr:
        rc = psp.main([
            "--serial", "s1", "--out", trace, "--trace-processor",
            trace_processor
        ])
      with open(os.path.join(tmp, "cold.phases.json"), encoding="utf-8") as f:
        capture = json.load(f)
      self.assertTrue(os.path.isfile(trace))
    self.assertEqual(rc, 1)
    self.assertIn("KeyboardInterrupt", stderr.getvalue())
    self.assertEqual(capture["error"], "KeyboardInterrupt: ")
    self.assertEqual([e["name"] for e in capture["events"]],
                     ["launch", "stop_trace"])
    shells = [" ".join(cmd[3:]) for cmd in fake.commands]
    self.assertEqual(shells[-5:], [
        ("shell cat /proc/uptime; kill -INT 4242; rc=$?; "
         "cat /proc/uptime; exit $rc"),
        "shell test -d /proc/4242",
        "shell test -d /proc/4242",
        f"pull /data/misc/perfetto-traces/cold.pftrace {trace}",
        "shell rm -f /data/misc/perfetto-traces/cold.pftrace",
    ])

  def test_parse_health(self):
    rows = psp.parse_csv(
        '"kind","name","idx","severity","value"\n'
        '"stat","traced_flushes_failed","[NULL]","data_loss",13\n'
        '"stat","traced_buf_chunks_overwritten",1,"data_loss",2\n'
        '"stat","sorter_push_event_out_of_order","[NULL]","error",54\n'
        '"meta","ftrace_setup_errors","[NULL]","[NULL]",'
        '"Ftrace event unknown: f2fs/a\nFtrace event unknown: f2fs/b\n"\n'
        '"meta","duration_ns","[NULL]","[NULL]",65000000000\n')
    health, metadata = psp.parse_health(rows)
    self.assertFalse(health["healthy"])
    self.assertEqual(
        health["counters"], {
            "traced_flushes_failed": 13,
            "traced_buf_chunks_overwritten": 2,
            "traced_buf_chunks_discarded": 0,
            "ftrace_cpu_overrun_end": 0,
            "sorter_push_event_out_of_order": 54,
        })
    self.assertEqual(
        health["data_loss"],
        ["traced_flushes_failed=13", "traced_buf_chunks_overwritten[1]=2"])
    self.assertEqual(
        health["ftrace_setup_errors"],
        ["Ftrace event unknown: f2fs/a", "Ftrace event unknown: f2fs/b"])
    self.assertEqual(metadata["duration_ns"], "65000000000")
    self.assertTrue(health["truncated"])
    self.assertIn("truncated: no `tracing_disabled_ns`",
                  psp.health_banner(health))
    health, _ = psp.parse_health(psp.parse_csv(_HEALTH_CSV))
    self.assertTrue(health["healthy"])
    self.assertFalse(health["truncated"])

  def test_format_markdown_from_canned_csvs(self):
    health, _ = psp.parse_health(psp.parse_csv(_HEALTH_CSV))
    summary = {
        "trace": {
            "path": "/out/lifecycle.pftrace",
            "sha256": "abc",
            "size_bytes": 2500000,
            "duration_s": 140.0
        },
        "versions": {
            "trace_processor": "Perfetto v50.1",
            "app_version_name": ""
        },
        "trace_health": health,
        "capture": {
            "cuj":
                "lifecycle",
            "phases": [
                {
                    "name": "cold_start",
                    "start_s": 100.0,
                    "end_s": 131.0
                },
                {
                    "name": "exit",
                    "start_s": 220.0,
                    "end_s": 235.5
                },
            ],
        },
    }
    results = [{
        "number":
            5,
        "title":
            "Engines",
        "csv":
            "lifecycle.q5.csv",
        "row_count":
            2,
        "error":
            "",
        "rows":
            psp.parse_csv('"engine","last_end_boottime_s"\n'
                          '"LocalStorage",230.250000\n'
                          '"CodeCache",150.000000\n'),
    }, {
        "number": 8,
        "title": "Overlap",
        "csv": "lifecycle.q8.csv",
        "row_count": 0,
        "error": "boom",
        "rows": [],
    }]
    md = psp.format_markdown(summary, results)
    self.assertIn("2.5 MB, 140.0 s, sha256 `abc`", md)
    self.assertIn("- Versions: trace_processor `Perfetto v50.1`\n", md)
    self.assertIn("**HEALTHY** (no data loss)", md)
    self.assertIn("| exit | 220.00 | 235.50 | 120.0 | 15.5 |", md)
    self.assertIn("Last storage I/O after HOME (Q5): LocalStorage +10.25 s.",
                  md)
    self.assertIn("| LocalStorage | 230.25 |", md)
    self.assertIn("`lifecycle.q8.csv`: 0 rows, ERROR: boom", md)

  def test_parse_args(self):
    with mock.patch.dict(os.environ, {"ANDROID_SERIAL": "env-serial"}):
      args = psp.parse_args([])
    self.assertEqual(args.serial, "env-serial")
    self.assertEqual(args.package, "com.google.android.youtube.tv")
    self.assertEqual(args.cuj, "cold_start")
    self.assertEqual(args.duration_s, 65.0)
    self.assertIsNone(psp.parse_args(["--cuj", "manual"]).duration_s)
    for bad in (
        ["--cuj-cmd", "true"],
        ["--cuj", "lifecycle", "--duration-s", "5"],
        ["--package", "a;rm"],
        ["--cuj", "manual", "--cuj-cmd", "true", "--duration-s", "5"],
        ["--cuj", "manual", "--cuj-cmd", "true", "--duration-s", "0"],
        ["--duration-s", "0"],
        ["--duration-s", "-5"],
        ["--duration-s", "nan"],
        ["--duration-s", "inf"],
        ["--analyze-only", "a.pftrace", "--out", "b.pftrace"],
    ):
      with self.assertRaises(SystemExit), \
          mock.patch("sys.stderr", new_callable=io.StringIO):
        psp.parse_args(bad)

  def test_print_config(self):
    with mock.patch("sys.stdout", new_callable=io.StringIO) as stdout:
      self.assertEqual(psp.main(["--print-config"]), 0)
    self.assertEqual(stdout.getvalue(), psp.build_perfetto_config(185))

  def _analyze_only(self, fake):
    with tempfile.TemporaryDirectory() as tmp:
      trace = os.path.join(tmp, "cold.pftrace")
      trace_processor = os.path.join(tmp, "trace_processor_shell")
      for path in (trace, trace_processor):
        with open(path, "wb") as f:
          f.write(b"trace-bytes")
      report_dir = os.path.join(tmp, "reports")
      with mock.patch.object(psp.subprocess, "run", side_effect=fake.run), \
          mock.patch("sys.stdout", new_callable=io.StringIO), \
          mock.patch("sys.stderr", new_callable=io.StringIO):
        rc = psp.main([
            "--analyze-only", trace, "--report-dir", report_dir,
            "--trace-processor", trace_processor
        ])
      with open(
          os.path.join(report_dir, "cold.summary.json"), encoding="utf-8") as f:
        summary = json.load(f)
      return rc, sorted(os.listdir(report_dir)), summary

  def test_analyze_only(self):
    fake = FakeSubprocess()
    rc, files, summary = self._analyze_only(fake)
    self.assertEqual(rc, 0)
    self.assertFalse([cmd for cmd in fake.commands if cmd[0] == "adb"])
    self.assertEqual(
        files,
        sorted([f"cold.q{n}.csv" for n in range(1, 9)] +
               ["cold.md", "cold.summary.json"]))
    self.assertEqual(summary["trace"]["sha256"],
                     hashlib.sha256(b"trace-bytes").hexdigest())
    self.assertEqual(summary["trace"]["duration_s"], 65.0)
    self.assertEqual(summary["versions"]["trace_processor"],
                     "Perfetto v50.1-abc (abc)")
    self.assertTrue(summary["trace_health"]["healthy"])
    self.assertEqual([q["row_count"] for q in summary["queries"]], [1] * 8)
    self.assertIsNone(summary["capture"])

  def test_analyze_only_reports_query_errors(self):
    rc, _, summary = self._analyze_only(FakeSubprocess(failing_query="-- Q8:"))
    self.assertEqual(rc, 1)
    self.assertEqual([q["number"] for q in summary["queries"] if q["error"]],
                     [8])

  def test_unhealthy_trace_exits_1_with_all_outputs(self):
    truncated_csv = _HEALTH_CSV.replace("tracing_disabled_ns", "x")
    for health_csv in (truncated_csv, ""):  # "" makes the health query fail.
      rc, files, summary = self._analyze_only(
          FakeSubprocess(health_csv=health_csv))
      self.assertEqual(rc, 1)
      self.assertEqual(len(files), 10)
      self.assertFalse(summary["trace_health"]["healthy"])
      self.assertTrue(summary["trace_health"]["truncated"])
      self.assertEqual(
          bool(summary["trace_health"].get("error")), not health_csv)

  def test_failing_cuj_cmd_exits_1(self):
    fake = FakeSubprocess(cuj_cmd_rc=3)
    with tempfile.TemporaryDirectory() as tmp:
      trace_processor = os.path.join(tmp, "trace_processor_shell")
      with open(trace_processor, "w", encoding="utf-8") as f:
        f.write("fake")
      with mock.patch.object(psp.subprocess, "run", side_effect=fake.run), \
          mock.patch.object(psp.time, "sleep"), \
          mock.patch("sys.stdout", new_callable=io.StringIO), \
          mock.patch("sys.stderr", new_callable=io.StringIO) as stderr:
        rc = psp.main([
            "--serial", "s1", "--cuj", "manual", "--cuj-cmd", "exit 3", "--out",
            os.path.join(tmp, "manual.pftrace"), "--trace-processor",
            trace_processor
        ])
      with open(
          os.path.join(tmp, "manual.summary.json"), encoding="utf-8") as f:
        summary = json.load(f)
    self.assertEqual(rc, 1)
    self.assertIn("exit 3", fake.commands)
    self.assertIn("--cuj-cmd exited with 3", stderr.getvalue())
    self.assertEqual(summary["capture"]["cuj_cmd_returncode"], 3)
    self.assertTrue(summary["trace_health"]["healthy"])

  def test_device_info_parses_prefixed_and_empty_fields(self):
    responses = [
        subprocess.CompletedProcess([], 0, "device\n", ""),
        subprocess.CompletedProcess(
            [], 0, "perfetto: \n    versionName=8.03.1-DEVELOPER\n"
            "fingerprint: \n", ""),
    ]
    with mock.patch.object(psp.subprocess, "run", side_effect=responses):
      info = psp._device_info("s1", "com.google.android.youtube.tv")  # pylint: disable=protected-access
    self.assertEqual(
        info, {
            "perfetto": "",
            "app_version_name": "8.03.1-DEVELOPER",
            "build_fingerprint": "",
        })

  def test_uptime_action_checks_exit_code(self):
    ok_uptimes = "10.00 1.00\n10.25 1.00\n"
    with mock.patch.object(
        psp.subprocess,
        "run",
        return_value=subprocess.CompletedProcess([], 2, ok_uptimes,
                                                 "quiet failure")):
      with self.assertRaisesRegex(RuntimeError,
                                  r"launch failed \(exit code 2\)"):
        psp._uptime_action("s1", "cold_start", "launch", "false")  # pylint: disable=protected-access
      event = psp._uptime_action(  # pylint: disable=protected-access
          "s1",
          "stop",
          "stop_trace",
          "kill -INT 42",
          check=False)
    self.assertEqual((event["start_s"], event["end_s"]), (10.0, 10.25))

  def test_malformed_health_and_duration_outputs(self):
    bad_rows = [
        ["kind", "name", "idx", "severity", "value"],
        ["meta", "tracing_disabled_ns", "[NULL]", "[NULL]", "[NULL]"],
        ["meta", "duration_ns", "[NULL]", "[NULL]", "[NULL]"],
        ["meta", "android_build_fingerprint", "[NULL]", "[NULL]", "[NULL]"],
        ["stat", "traced_flushes_failed", "[NULL]", "data_loss", "not-an-int"],
        ["short", "row"],
        ["unknown", "x", "[NULL]", "[NULL]", "1"],
    ]
    health, metadata = psp.parse_health(bad_rows)
    self.assertFalse(health["healthy"])
    self.assertTrue(health["truncated"])
    self.assertEqual(metadata, {})
    self.assertIn("invalid duration_ns: [NULL]", health["error"])
    self.assertIn("invalid stat traced_flushes_failed", health["error"])
    self.assertIn("malformed health row (2 columns)", health["error"])
    self.assertIn("unknown health row kind", health["error"])
    for raw in ("[NULL]", "", "abc", "-5", None):
      self.assertEqual(
          psp._duration_s({"duration_ns": raw}),  # pylint: disable=protected-access
          0.0)

    bad_header_health, _ = psp.parse_health([["wrong", "header"]])
    self.assertFalse(bad_header_health["healthy"])
    self.assertIn("unexpected health header", bad_header_health["error"])

    missing_dur_health, _ = psp.parse_health([
        ["kind", "name", "idx", "severity", "value"],
        ["meta", "tracing_disabled_ns", "[NULL]", "[NULL]", "100"],
    ])
    self.assertFalse(missing_dur_health["healthy"])
    self.assertEqual(missing_dur_health["error"], "missing duration_ns")

    for bad_health_csv in (
        '"kind","name","idx","severity","value"\n'
        '"meta","tracing_disabled_ns","[NULL]","[NULL]",70000000000\n'
        '"meta","duration_ns","[NULL]","[NULL]","[NULL]"\n',
        '"kind","name"\n"unterminated',
    ):
      rc, files, summary = self._analyze_only(
          FakeSubprocess(health_csv=bad_health_csv))
      self.assertEqual(rc, 1)
      self.assertEqual(len(files), 10)
      self.assertEqual(summary["trace"]["duration_s"], 0.0)
      self.assertFalse(summary["trace_health"]["healthy"])
      self.assertTrue(summary["trace_health"]["error"])

  def test_malformed_query_csv_and_q5_outputs(self):
    self.assertEqual(
        psp.parse_csv('"col","val"\n"a\x00b",1\n'),
        [["col", "val"], ["ab", "1"]])
    phases = [{"name": "exit", "start_s": 200.0}]
    self.assertEqual(
        psp._after_home(  # pylint: disable=protected-access
            phases, [["wrong_col"], ["LocalStorage"]]),
        "")
    self.assertEqual(
        psp._after_home(  # pylint: disable=protected-access
            phases, [
                ["engine", "last_end_boottime_s"],
                ["ShortRow"],
                ["NullEnd", "[NULL]"],
                ["NanEnd", "nan"],
                ["ValidEngine", "205.5"],
            ]),
        "Last storage I/O after HOME (Q5): ValidEngine +5.50 s.")
    self.assertEqual(
        psp._markdown_table([["a", "b"], [], ["only_one"], ["1", "2", "3"]]),  # pylint: disable=protected-access
        ["| a | b |", "| --- | --- |", "| only_one |  |", "| 1 | 2 |"])

    fake = FakeSubprocess()
    orig_run = fake.run

    def broken_q8_run(cmd, **kwargs):
      res = orig_run(cmd, **kwargs)
      if isinstance(cmd, list) and len(cmd) >= 3 and cmd[1] == "-q":
        with open(cmd[2], encoding="utf-8") as f:
          sql = f.read()
        if sql.startswith("-- Q7:"):
          return subprocess.CompletedProcess(cmd, 0, "", "")
        if sql.startswith("-- Q8:"):
          return subprocess.CompletedProcess(cmd, 0, '"a","b"\n"unclosed', "")
      return res

    with mock.patch.object(fake, "run", side_effect=broken_q8_run):
      rc, _, summary = self._analyze_only(fake)
    self.assertEqual(rc, 1)
    errors = {q["number"]: q["error"] for q in summary["queries"] if q["error"]}
    self.assertIn("empty CSV output", errors[7])
    self.assertIn("malformed CSV output", errors[8])

  def test_malformed_inputs_and_sidecar_files(self):
    with tempfile.TemporaryDirectory() as tmp:
      trace_processor = os.path.join(tmp, "trace_processor_shell")
      with open(trace_processor, "wb") as f:
        f.write(b"fake")
      empty_trace = os.path.join(tmp, "empty.pftrace")
      with open(empty_trace, "wb"):
        pass
      missing_dir = os.path.join(tmp, "never_created")
      missing_trace = os.path.join(missing_dir, "missing.pftrace")
      for bad_trace in (missing_trace, empty_trace):
        with mock.patch("sys.stderr", new_callable=io.StringIO) as stderr:
          self.assertEqual(
              psp.main([
                  "--analyze-only", bad_trace, "--trace-processor",
                  trace_processor
              ]), 1)
        self.assertIn("Error:", stderr.getvalue())
      self.assertFalse(os.path.exists(missing_dir))

      bad_sql = os.path.join(tmp, "empty.sql")
      with open(bad_sql, "w", encoding="utf-8") as f:
        f.write("-- no queries here\n")
      with self.assertRaisesRegex(ValueError, "No '-- Q<n>: <title>' queries"):
        psp.load_queries(path=bad_sql)

      trace = os.path.join(tmp, "valid.pftrace")
      with open(trace, "wb") as f:
        f.write(b"trace-bytes")
      with mock.patch.object(
          psp.subprocess, "run", side_effect=PermissionError("denied")), \
          mock.patch("sys.stderr", new_callable=io.StringIO) as stderr:
        self.assertEqual(
            psp.main(
                ["--analyze-only", trace, "--trace-processor",
                 trace_processor]), 1)
      self.assertIn("failed to run", stderr.getvalue())

      report_dir = os.path.join(tmp, "sidecar_reports")
      os.makedirs(report_dir, exist_ok=True)
      phases_path = os.path.join(report_dir, "valid.phases.json")
      for bad_sidecar in (
          "{not json",
          "[]",
          json.dumps({"device": "not-a-dict"}),
          json.dumps({"phases": [{
              "name": "cold_start",
              "start_s": "bad"
          }]}),
      ):
        with open(phases_path, "w", encoding="utf-8") as f:
          f.write(bad_sidecar)
        fake = FakeSubprocess()
        with mock.patch.object(psp.subprocess, "run", side_effect=fake.run), \
            mock.patch("sys.stdout", new_callable=io.StringIO), \
            mock.patch("sys.stderr", new_callable=io.StringIO) as stderr:
          rc = psp.main([
              "--analyze-only", trace, "--report-dir", report_dir,
              "--trace-processor", trace_processor
          ])
        self.assertEqual(rc, 0)
        self.assertIn("WARNING: ignoring malformed phases file",
                      stderr.getvalue())
        with open(
            os.path.join(report_dir, "valid.summary.json"),
            encoding="utf-8") as f:
          self.assertIsNone(json.load(f)["capture"])

      # Empty file from `adb pull` raises RuntimeError cleanly.
      fake = FakeSubprocess()
      orig_run = fake.run

      def empty_pull(cmd, **kwargs):
        if isinstance(cmd, list) and cmd[:1] == ["adb"] and "pull" in cmd:
          with open(cmd[-1], "wb"):
            pass
          return subprocess.CompletedProcess(cmd, 0, "", "")
        return orig_run(cmd, **kwargs)

      with mock.patch.object(psp.subprocess, "run", side_effect=empty_pull), \
          mock.patch.object(psp.time, "sleep"), \
          mock.patch("sys.stdout", new_callable=io.StringIO):
        with self.assertRaisesRegex(RuntimeError,
                                    "Pulled trace is missing or empty"):
          psp.record_trace(
              psp.parse_args(["--serial", "s1", "--duration-s", "1"]), "CFG",
              os.path.join(tmp, "pulled.pftrace"),
              os.path.join(tmp, "pulled.phases.json"))


if __name__ == "__main__":
  unittest.main()
