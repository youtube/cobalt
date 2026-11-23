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
"""Records and analyzes a Perfetto storage trace of Cobalt on Android TV.

Records Chromium storage trace events (SQLite, LevelDB, base::File and
ScopedBlockingCall slices, histogram samples) together with kernel scheduling,
fsync and block-layer ftrace events while running a CUJ, then runs the queries
in perfetto_storage_queries.sql with trace_processor_shell and writes CSV, JSON
and Markdown reports.
"""

import argparse
import csv
from datetime import datetime
import hashlib
import io
import json
import math
import os
import re
import shlex
import string
import subprocess
import sys
import tempfile
import time
from typing import Any, Dict, List, Optional, Tuple

try:
  from cobalt.tools.storage.storage_monitor import (DEFAULT_HOME_URL,
                                                    DEFAULT_VIDEO_URL)
except ImportError:  # Run as a script, so storage_monitor.py is on sys.path.
  from storage_monitor import DEFAULT_HOME_URL, DEFAULT_VIDEO_URL

DEFAULT_PACKAGE = "com.google.android.youtube.tv"
DEFAULT_TRACE_PROCESSOR = (
    "~/.local/share/perfetto/prebuilts/trace_processor_shell")
COLD_START_DURATION_S = 65.0
# Sum of the lifecycle CUJ sleeps (30 s + 8 x 4 s + 45 s + 15 s).
LIFECYCLE_DURATION_S = 122.0
# duration_ms safety cap, so that a lost adb connection cannot leave the trace
# running: the expected trace length plus DURATION_MARGIN_S, or
# MANUAL_MAX_DURATION_S when the length is unknown (manual, --cuj-cmd).
DURATION_MARGIN_S = 120.0
MANUAL_MAX_DURATION_S = 3600.0
QUERIES_SQL = os.path.join(
    os.path.dirname(os.path.abspath(__file__)), "perfetto_storage_queries.sql")
# Chromium only connects to the system tracing service with this feature,
# which is disabled by default on Android.
PERFETTO_FEATURE_ARG = "--enable-features=EnablePerfettoSystemTracing"
URL_PARAMS = "force_startup_screen=none,hl=en,gl=US"
REMOTE_TRACE_DIR = "/data/misc/perfetto-traces"
# The queries select the app process with this literal (see load_queries()).
SQL_PACKAGE_LITERAL = f"'{DEFAULT_PACKAGE}'"
REPORT_ROWS = 10

TRACK_EVENT_CATEGORIES = (
    "sql",
    "leveldb",
    "base",
    "IndexedDB",
    "ServiceWorker",
    "disabled-by-default-file",
    "disabled-by-default-histogram_samples",
    "__metadata",
    "startup",
)
MEMORY_INFRA_CATEGORY = "disabled-by-default-memory-infra"
FTRACE_EVENTS = (
    "sched/sched_switch",
    "sched/sched_waking",
    "sched/sched_wakeup_new",
    "sched/sched_blocked_reason",
    # Make perfetto re-read the process name after zygote renames the app.
    "task/task_newtask",
    "task/task_rename",
    "ext4/ext4_sync_file_enter",
    "ext4/ext4_sync_file_exit",
    "f2fs/f2fs_sync_file_enter",
    "f2fs/f2fs_sync_file_exit",
    "block/block_rq_issue",
    "block/block_rq_complete",
    "android_fs/android_fs_dataread_start",
    "android_fs/android_fs_dataread_end",
    "android_fs/android_fs_datawrite_start",
    "android_fs/android_fs_datawrite_end",
)
HEALTH_COUNTERS = (
    "traced_flushes_failed",
    "traced_buf_chunks_overwritten",
    "traced_buf_chunks_discarded",
    "ftrace_cpu_overrun_end",
    "sorter_push_event_out_of_order",
)
_HEALTH_NAMES = ", ".join(f"'{name}'" for name in HEALTH_COUNTERS)
HEALTH_SQL = f"""SELECT 'stat' AS kind, name, idx, severity, value
FROM stats
WHERE value > 0 AND (name IN ({_HEALTH_NAMES}) OR severity = 'data_loss')
UNION ALL
SELECT 'meta', name, NULL, NULL, COALESCE(str_value, CAST(int_value AS TEXT))
FROM metadata
WHERE name IN ('android_build_fingerprint', 'cr-user-agent',
               'ftrace_setup_errors', 'trace_time_clock_id',
               'tracing_disabled_ns')
UNION ALL
SELECT 'meta', 'duration_ns', NULL, NULL, end_ts - start_ts FROM trace_bounds;
"""

_CONFIG_TEMPLATE = string.Template("""\
# Safety cap; the tool normally stops the trace with SIGINT before this.
duration_ms: $duration_ms
write_into_file: true
file_write_period_ms: 10000
flush_timeout_ms: 30000

# Buffer 0: Chromium storage trace events and histogram samples.
buffers { size_kb: $track_event_buffer_kb fill_policy: RING_BUFFER }
# Buffer 1: ftrace, process and memory stats.
buffers { size_kb: 32768 fill_policy: RING_BUFFER }

# The Chromium data sources only go to the app's producer: Cronet in other
# apps (GMS, the launcher) also registers track_event, and a frozen one makes
# the start and every flush time out.
data_sources {
  config {
    name: "track_event"
    target_buffer: 0
    chrome_config { trace_config: $chrome_trace_config }
    track_event_config {
      disabled_categories: "*"
$enabled_categories
    }
  }
  $producer_filter
}
data_sources {
  config { name: "org.chromium.histogram_sample" target_buffer: 0 }
  $producer_filter
}
data_sources {
  config { name: "org.chromium.trace_metadata" target_buffer: 0 }
  $producer_filter
}
$memory_infra_data_source
data_sources {
  config {
    name: "linux.ftrace"
    target_buffer: 1
    ftrace_config {
      compact_sched { enabled: true }
$ftrace_events
    }
  }
}
data_sources {
  config {
    name: "linux.process_stats"
    target_buffer: 1
    process_stats_config {
      scan_all_processes_on_start: true
      proc_stats_poll_ms: 1000
    }
  }
}
data_sources {
  config {
    name: "linux.sys_stats"
    target_buffer: 1
    sys_stats_config {
      meminfo_period_ms: 1000
      vmstat_period_ms: 1000
    }
  }
}
""")

_MEMORY_INFRA_TEMPLATE = string.Template("""\
data_sources {
  config {
    name: "org.chromium.memory_instrumentation"
    target_buffer: 0
    chrome_config { trace_config: $chrome_trace_config }
  }
  $producer_filter
}""")


def _chrome_trace_config(categories: List[str], **extra: Any) -> str:
  """Returns a Chrome JSON trace config as a quoted textproto string."""
  config = {"record_mode": "record-continuously"}
  config["included_categories"] = categories
  config.update(extra)
  return json.dumps(json.dumps(config, separators=(",", ":")))


def duration_cap_s(args: argparse.Namespace) -> float:
  """Returns the trace duration_ms safety cap for the CUJ, in seconds."""
  if args.cuj == "lifecycle":
    return LIFECYCLE_DURATION_S + DURATION_MARGIN_S
  if args.duration_s:
    return args.duration_s + DURATION_MARGIN_S
  return MANUAL_MAX_DURATION_S


def build_perfetto_config(duration_s: float,
                          package: str = DEFAULT_PACKAGE,
                          include_memory_infra: bool = False) -> str:
  """Returns the Perfetto text config for a storage trace."""
  # The app's Chromium producer is org.chromium-<package>-<pid>
  # (base/tracing/perfetto_platform.cc); the app's own Cronet registers as
  # cronet-<package>-<pid>. Perfetto full-matches the regex, and [.] avoids
  # textproto escaping.
  package_regex = package.replace(".", "[.]")
  producer_filter = ("producer_name_regex_filter: "
                     f"\"org[.]chromium-{package_regex}-[0-9]+\"")
  categories = list(TRACK_EVENT_CATEGORIES)
  memory_infra = ""
  if include_memory_infra:
    categories.append(MEMORY_INFRA_CATEGORY)
    memory_infra = _MEMORY_INFRA_TEMPLATE.substitute(
        chrome_trace_config=_chrome_trace_config(
            [MEMORY_INFRA_CATEGORY],
            memory_dump_config={
                "allowed_dump_modes": ["background", "light", "detailed"],
                "triggers": [{
                    "min_time_between_dumps_ms": 10000,
                    "mode": "detailed",
                    "type": "periodic_interval",
                }],
            }),
        producer_filter=producer_filter)
  return _CONFIG_TEMPLATE.substitute(
      duration_ms=int(duration_s * 1000),
      track_event_buffer_kb=16384 if include_memory_infra else 8192,
      chrome_trace_config=_chrome_trace_config(
          [c for c in categories if c != "__metadata"]),
      enabled_categories="\n".join(
          f'      enabled_categories: "{c}"' for c in categories),
      producer_filter=producer_filter,
      memory_infra_data_source=memory_infra,
      ftrace_events="\n".join(
          f'      ftrace_events: "{e}"' for e in FTRACE_EVENTS))


def build_command_line_args(extra_args: str) -> str:
  """Returns the comma-separated commandLineArgs for the launch intent."""
  args = [PERFETTO_FEATURE_ARG]
  for arg in extra_args.split(","):
    arg = arg.strip()
    if arg:
      args.append(arg if arg.startswith("--") else f"--{arg}")
  return ",".join(args)


def build_launch_command(package: str, extra_args: str) -> str:
  """Returns the `am start` command that cold-starts the app."""
  cmd_args = shlex.quote(build_command_line_args(extra_args))
  return (f"am start -S -a android.intent.action.VIEW -d '{DEFAULT_HOME_URL}' "
          f"--esa commandLineArgs {cmd_args} "
          f"--esa url_params '{URL_PARAMS}' -p {package}")


def _adb_cmd(serial: Optional[str], *args: str) -> List[str]:
  cmd = ["adb"]
  if serial:
    cmd.extend(["-s", serial])
  cmd.extend(args)
  return cmd


def _adb_shell(serial: Optional[str],
               command: str,
               input_text: Optional[str] = None,
               timeout: float = 120) -> subprocess.CompletedProcess:
  return subprocess.run(
      _adb_cmd(serial, "shell", command),
      input=input_text,
      capture_output=True,
      text=True,
      errors="replace",
      check=False,
      timeout=timeout)


def _device_info(serial: Optional[str], package: str) -> Dict[str, str]:
  """Verifies the device and returns the perfetto, app and build versions."""
  res = subprocess.run(
      _adb_cmd(serial, "get-state"),
      capture_output=True,
      text=True,
      errors="replace",
      check=False,
      timeout=10)
  if res.stdout.strip() != "device":
    detail = res.stderr.strip() or res.stdout.strip()
    raise RuntimeError(f"No adb device (serial {serial}): {detail}")
  res = _adb_shell(
      serial, 'echo "perfetto: $(perfetto --version 2>/dev/null | head -1)"; '
      f"dumpsys package {package} | grep -m1 versionName; "
      'echo "fingerprint: $(getprop ro.build.fingerprint)"')
  version_name = re.search(r"versionName=(\S+)", res.stdout)
  if not version_name:
    raise RuntimeError(f"{package} is not installed on the device.")
  perfetto = re.search(r"^perfetto:[ \t]*(.*)$", res.stdout, re.M)
  fingerprint = re.search(r"^fingerprint:[ \t]*(.*)$", res.stdout, re.M)
  return {
      "perfetto": perfetto.group(1).strip() if perfetto else "",
      "app_version_name": version_name.group(1),
      "build_fingerprint": fingerprint.group(1).strip() if fingerprint else "",
  }


def _restore_golden(serial: Optional[str], package: str, golden_tar: str):
  """Restores app data from a device tar and drops stale .pma files."""
  restore = (f"cd /data/data/{package} && rm -rf ./cache ./app_content_shell"
             " ./files ./shared_prefs ./code_cache ./app_textures ./databases"
             f" ./no_backup && tar xf {shlex.quote(golden_tar)} -C . && "
             "rm -f app_content_shell/BrowserStabilityMetrics/*.pma")
  res = _adb_shell(
      serial,
      f"run-as {package} sh -c {shlex.quote(restore)} && sync",
      timeout=300)
  if res.returncode != 0:
    raise RuntimeError(f"Golden restore failed: {res.stderr.strip()}")


def _drop_caches(serial: Optional[str]) -> bool:
  res = _adb_shell(serial, "sync && echo 3 > /proc/sys/vm/drop_caches")
  if res.returncode != 0:
    print(
        "WARNING: --drop-caches needs root; tracing with a warm page cache "
        f"({res.stderr.strip()}).",
        file=sys.stderr)
  return res.returncode == 0


def _start_perfetto(serial: Optional[str], config: str, remote: str) -> int:
  # Unlike --background, --background-wait returns only after the data
  # sources have started, so the launch that follows is fully traced.
  res = _adb_shell(
      serial,
      f"perfetto --background-wait --txt -c - -o {shlex.quote(remote)}",
      input_text=config)
  pid = re.search(r"^\s*(\d+)\s*$", res.stdout, re.M)
  if res.returncode != 0 or not pid:
    if pid:  # Do not leave a session whose data sources failed running.
      _adb_shell(serial, f"kill -INT {pid.group(1)}")
    raise RuntimeError(f"perfetto failed to start: {res.stderr.strip()}")
  return int(pid.group(1))


def _wait_for_exit(serial: Optional[str], pid: int, timeout_s: int = 60):
  # Check /proc instead of `kill -0`, which may be denied for the perfetto
  # process (its own SELinux domain). Pulling the trace before perfetto exits
  # loses its final flush and end-of-trace stats.
  for _ in range(timeout_s):
    if _adb_shell(serial, f"test -d /proc/{pid}").returncode != 0:
      return
    time.sleep(1)
  print(
      f"WARNING: perfetto (pid {pid}) is still running after {timeout_s} s.",
      file=sys.stderr)


def _uptime_action(serial: Optional[str],
                   phase: str,
                   name: str,
                   command: str,
                   check: bool = True) -> Dict[str, Any]:
  """Runs `command` on the device between two /proc/uptime reads.

  /proc/uptime is CLOCK_BOOTTIME, the trace clock, so start_s * 1e9 and
  end_s * 1e9 are trace timestamps.
  """
  res = _adb_shell(
      serial, f"cat /proc/uptime; {command}; rc=$?; cat /proc/uptime; exit $rc")
  uptimes = re.findall(r"^(\d+\.\d+) \d+\.\d+\s*$", res.stdout, re.M)
  output = (res.stdout + res.stderr).strip()
  if len(uptimes) < 2 or (check and (res.returncode != 0 or "Error" in output)):
    raise RuntimeError(f"{name} failed (exit code {res.returncode}): {output}")
  return {
      "phase": phase,
      "name": name,
      "start_s": float(uptimes[0]),
      "end_s": float(uptimes[-1]),
  }


def run_cuj(args: argparse.Namespace, events: List[Dict[str, Any]]) -> int:
  """Launches the app and drives the CUJ without polling the device.

  Appends the timed adb actions to `events` and returns the exit code of
  --cuj-cmd (0 without it).
  """
  serial, package = args.serial, args.package
  launch_phase = "manual" if args.cuj == "manual" else "cold_start"
  events.append(
      _uptime_action(serial, launch_phase, "launch",
                     build_launch_command(package, args.extra_args)))
  returncode = 0
  if args.cuj == "cold_start":
    time.sleep(args.duration_s)
  elif args.cuj == "lifecycle":
    time.sleep(30)
    for _ in range(8):
      for key in ("KEYCODE_DPAD_DOWN", "KEYCODE_DPAD_RIGHT"):
        events.append(
            _uptime_action(serial, "scroll", key, f"input keyevent {key}"))
        time.sleep(2)
    events.append(
        _uptime_action(
            serial, "play", "watch", "am start -a android.intent.action.VIEW "
            f"-d '{DEFAULT_VIDEO_URL}' -p {package}"))
    time.sleep(45)
    events.append(
        _uptime_action(serial, "exit", "KEYCODE_HOME",
                       "input keyevent KEYCODE_HOME"))
    time.sleep(15)
  else:
    try:
      if args.cuj_cmd:
        returncode = subprocess.run(
            args.cuj_cmd, shell=True, check=False).returncode
      elif args.duration_s:
        time.sleep(args.duration_s)
      else:
        print("Tracing until Ctrl+C...")
        while True:
          time.sleep(3600)
    except KeyboardInterrupt:
      print("\nStopping the trace...")
  return returncode


def phase_bounds(events: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
  """Groups events into phases; each starts when its first action returned.

  The last event is the trace stop, which ends the last phase.
  """
  phases: List[Dict[str, Any]] = []
  for event in events:
    if phases and phases[-1]["name"] == event["phase"]:
      continue
    if phases:
      phases[-1]["end_s"] = event["end_s"]
    phases.append({"name": event["phase"], "start_s": event["end_s"]})
  return phases[:-1]


def record_trace(args: argparse.Namespace, config: str, trace_path: str,
                 phases_path: str) -> Dict[str, Any]:
  """Records the CUJ into trace_path and the capture info into phases_path.

  Perfetto is stopped, phases_path written, and the trace pulled also when the
  CUJ fails or is interrupted; the exception is then re-raised.
  """
  serial, package = args.serial, args.package
  device = _device_info(serial, package)
  print(" ".join([package] + list(device.values())))
  _adb_shell(serial, f"am force-stop {package}")
  if args.golden_tar:
    _restore_golden(serial, package, args.golden_tar)
  caches_dropped = args.drop_caches and _drop_caches(serial)
  remote = f"{REMOTE_TRACE_DIR}/{os.path.basename(trace_path)}"
  _adb_shell(serial, f"rm -f {shlex.quote(remote)}")
  pid = _start_perfetto(serial, config, remote)
  print(f"Perfetto (pid {pid}) is recording; running the {args.cuj} CUJ...")
  events: List[Dict[str, Any]] = []
  capture = {
      "cuj": args.cuj,
      "cuj_cmd": args.cuj_cmd,
      "cuj_cmd_returncode": 0,
      "error": "",
      "package": package,
      "serial": serial,
      "command_line_args": build_command_line_args(args.extra_args),
      "golden_tar": args.golden_tar,
      "caches_dropped": caches_dropped,
      "device": device,
      "clock": ("CLOCK_BOOTTIME seconds (/proc/uptime read before and after "
                "each adb action); trace ts in ns = seconds * 1e9"),
      "events": events,
  }
  try:
    capture["cuj_cmd_returncode"] = run_cuj(args, events)
  except BaseException as exc:  # Includes Ctrl+C; save the trace, re-raise.
    capture["error"] = f"{type(exc).__name__}: {exc}"
    raise
  finally:
    # Perfetto may have already exited if the safety duration_ms cap elapsed.
    events.append(
        _uptime_action(
            serial, "stop", "stop_trace", f"kill -INT {pid}", check=False))
    _wait_for_exit(serial, pid)
    capture["phases"] = phase_bounds(events)
    with open(phases_path, "w", encoding="utf-8") as f:
      json.dump(capture, f, indent=2)
      f.write("\n")
    res = subprocess.run(
        _adb_cmd(serial, "pull", remote, trace_path),
        capture_output=True,
        text=True,
        errors="replace",
        check=False,
        timeout=600)
    if res.returncode != 0:
      raise RuntimeError(f"adb pull failed: {res.stderr.strip()}")
    if not os.path.isfile(trace_path) or os.path.getsize(trace_path) == 0:
      raise RuntimeError(f"Pulled trace is missing or empty: {trace_path}")
    _adb_shell(serial, f"rm -f {shlex.quote(remote)}")
    print(f"Saved trace to {trace_path}")
  return capture


def load_queries(package: str = DEFAULT_PACKAGE,
                 path: str = QUERIES_SQL) -> List[Tuple[int, str, str]]:
  """Returns (number, title, sql) for each "-- Q<n>: <title>" block."""
  with open(path, encoding="utf-8") as f:
    text = f.read()
  queries = []
  for block in re.split(r"(?m)^(?=-- Q\d+: )", text)[1:]:
    match = re.match(r"-- Q(\d+): (.*)", block)
    if match:
      queries.append((int(match.group(1)), match.group(2).strip(),
                      block.replace(SQL_PACKAGE_LITERAL, f"'{package}'")))
  if not queries:
    raise ValueError(f"No '-- Q<n>: <title>' queries found in {path}")
  return queries


def _run_query(trace_processor: str, trace: str, sql: str,
               tmp_dir: str) -> Tuple[str, str]:
  """Runs one SQL statement and returns (csv_text, error)."""
  sql_path = os.path.join(tmp_dir, "query.sql")
  with open(sql_path, "w", encoding="utf-8") as f:
    f.write(sql)
  try:
    res = subprocess.run([trace_processor, "-q", sql_path, trace],
                         capture_output=True,
                         text=True,
                         errors="replace",
                         check=False,
                         timeout=900)
  except (OSError, subprocess.SubprocessError) as exc:
    return "", f"{type(exc).__name__}: {exc}"
  if res.returncode != 0:
    return "", (" ".join(res.stderr.strip().splitlines()[-3:]) or
                f"trace_processor_shell exited with {res.returncode}")
  return res.stdout, ""


def parse_csv(text: str) -> List[List[str]]:
  """Parses trace_processor CSV output, stripping embedded NUL bytes."""
  return list(csv.reader(io.StringIO(text.replace("\x00", "")), strict=True))


def _parse_query_csv(text: str) -> Tuple[List[List[str]], str]:
  try:
    return parse_csv(text), ""
  except csv.Error as exc:
    return [], f"malformed CSV output: {exc}"


def _parse_nonneg_int(value: Any) -> Optional[int]:
  if isinstance(value, bool):
    return None
  try:
    parsed = int(value)
  except (TypeError, ValueError):
    return None
  return parsed if parsed >= 0 else None


def _parse_finite_float(value: Any) -> Optional[float]:
  if isinstance(value, bool):
    return None
  try:
    parsed = float(value)
  except (TypeError, ValueError):
    return None
  return parsed if math.isfinite(parsed) else None


def _duration_s(metadata: Dict[str, str]) -> float:
  duration_ns = _parse_nonneg_int(metadata.get("duration_ns", 0))
  return round((duration_ns or 0) / 1e9, 3)


_HEALTH_HEADER = ["kind", "name", "idx", "severity", "value"]


def parse_health(
    rows: List[List[str]]) -> Tuple[Dict[str, Any], Dict[str, str]]:
  """Returns (trace_health, metadata) from the HEALTH_SQL result rows."""
  counters = {name: 0 for name in HEALTH_COUNTERS}
  data_loss = []
  metadata: Dict[str, str] = {}
  parse_errors = []
  if rows and rows[0] != _HEALTH_HEADER:
    parse_errors.append(f"unexpected health header: {rows[0]!r}")
    body_rows: List[List[str]] = []
  else:
    body_rows = rows[1:]
  for row in body_rows:
    if len(row) != 5:
      parse_errors.append(f"malformed health row ({len(row)} columns)")
      continue
    kind, name, idx, severity, value = row
    if kind == "meta":
      if value == "[NULL]":
        if name == "duration_ns":
          parse_errors.append("invalid duration_ns: [NULL]")
        continue
      if name in ("duration_ns", "tracing_disabled_ns"):
        if _parse_nonneg_int(value) is None:
          parse_errors.append(f"invalid {name}: {value!r}")
          continue
      metadata[name] = value
    elif kind == "stat":
      parsed_value = _parse_nonneg_int(value)
      if parsed_value is None:
        parse_errors.append(f"invalid stat {name}: {value!r}")
        continue
      if name in counters:
        counters[name] += parsed_value
      if severity == "data_loss":
        suffix = "" if idx == "[NULL]" else f"[{idx}]"
        data_loss.append(f"{name}{suffix}={parsed_value}")
    else:
      parse_errors.append(f"unknown health row kind: {kind!r}")
  if rows and "duration_ns" not in metadata and not parse_errors:
    parse_errors.append("missing duration_ns")
  # traced writes tracing_disabled_ns and the final stats when tracing stops.
  # Without them the trace was cut off and the counters above are incomplete.
  truncated = "tracing_disabled_ns" not in metadata
  health: Dict[str, Any] = {
      "healthy": (not truncated and not data_loss and
                  not any(counters.values()) and not parse_errors),
      "truncated":
          truncated,
      "counters":
          counters,
      "data_loss":
          data_loss,
      "ftrace_setup_errors": [
          line.strip()
          for line in metadata.get("ftrace_setup_errors", "").splitlines()
          if line.strip()
      ],
  }
  if parse_errors:
    health["error"] = "; ".join(parse_errors)
  return health, metadata


def _sha256(path: str) -> str:
  digest = hashlib.sha256()
  with open(path, "rb") as f:
    for chunk in iter(lambda: f.read(1 << 20), b""):
      digest.update(chunk)
  return digest.hexdigest()


def _cell(value: str) -> str:
  if re.fullmatch(r"-?\d+\.\d+", value):
    value = value.rstrip("0").rstrip(".")
  value = value.replace("|", "\\|").replace("\n", " ")
  return value if len(value) <= 60 else value[:57] + "..."


def health_banner(health: Dict[str, Any]) -> str:
  counters = ", ".join(f"`{k}` {v}" for k, v in health["counters"].items())
  if health["healthy"]:
    return f"**HEALTHY** (no data loss): {counters}."
  problems = [f"`{d}`" for d in health["data_loss"]]
  if health["truncated"]:
    problems.insert(0, "truncated: no `tracing_disabled_ns` or final stats")
  if health.get("error"):
    problems.insert(0, "health query failed: " + health["error"])
  details = "; ".join(problems) or "nonzero counters"
  return f"**WARNING: DATA LOSS** ({details}): {counters}."


def _after_home(phases: List[Dict[str, Any]], q5_rows: List[List[str]]) -> str:
  """Returns when each storage engine's last I/O ended after HOME."""
  home = next((_parse_finite_float(p.get("start_s"))
               for p in phases
               if isinstance(p, dict) and p.get("name") == "exit"), None)
  if home is None or len(q5_rows) < 2:
    return ""
  header = q5_rows[0]
  if "engine" not in header or "last_end_boottime_s" not in header:
    return ""
  engine = header.index("engine")
  last_end = header.index("last_end_boottime_s")
  min_len = max(engine, last_end) + 1
  items = []
  for row in q5_rows[1:]:
    if len(row) < min_len:
      continue
    end_s = _parse_finite_float(row[last_end])
    if end_s is not None and end_s > home:
      items.append(f"{row[engine]} +{end_s - home:.2f} s")
  return ("Last storage I/O after HOME (Q5): " + (", ".join(items) or "none") +
          ".")


def _markdown_table(rows: List[List[str]]) -> List[str]:
  """Returns the header and the first REPORT_ROWS rows as a Markdown table."""
  if not rows or not rows[0]:
    return []
  header = rows[0]
  lines = [
      "| " + " | ".join(_cell(col) for col in header) + " |",
      "|" + " --- |" * len(header),
  ]
  for row in rows[1:REPORT_ROWS + 1]:
    if not row:
      continue
    padded = (row + [""] * len(header))[:len(header)]
    lines.append("| " + " | ".join(_cell(value) for value in padded) + " |")
  return lines


def _phase_lines(capture: Dict[str, Any],
                 q5_rows: List[List[str]]) -> List[str]:
  if not isinstance(capture, dict):
    return []
  raw_phases = capture.get("phases")
  if not isinstance(raw_phases, list) or not raw_phases:
    return []
  phases = []
  for phase in raw_phases:
    if not isinstance(phase, dict) or not isinstance(phase.get("name"), str):
      return []
    start = _parse_finite_float(phase.get("start_s"))
    end = _parse_finite_float(phase.get("end_s"))
    if start is None or end is None:
      return []
    phases.append({"name": phase["name"], "start_s": start, "end_s": end})
  cuj = capture.get("cuj") or "unknown"
  t0 = phases[0]["start_s"]
  lines = [
      "",
      f"## Phases (`{cuj}`, CLOCK_BOOTTIME s; trace ts = s * 1e9)",
      "",
      "| Phase | Start s | End s | From launch s | Duration s |",
      "| :--- | ---: | ---: | ---: | ---: |",
  ]
  for phase in phases:
    name, start, end = phase["name"], phase["start_s"], phase["end_s"]
    lines.append(f"| {name} | {start:.2f} | {end:.2f} | {start - t0:.1f} | "
                 f"{end - start:.1f} |")
  after_home = _after_home(phases, q5_rows)
  return lines + (["", after_home] if after_home else [])


def load_capture(phases_path: str) -> Optional[Dict[str, Any]]:
  """Loads and validates a <stem>.phases.json sidecar, warning if malformed."""
  try:
    with open(phases_path, encoding="utf-8") as f:
      data = json.load(f)
  except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
    print(
        f"WARNING: ignoring malformed phases file {phases_path}: {exc}",
        file=sys.stderr)
    return None
  if not isinstance(data, dict):
    print(
        f"WARNING: ignoring malformed phases file {phases_path}: "
        f"expected JSON object, got {type(data).__name__}",
        file=sys.stderr)
    return None
  device = data.get("device")
  if device is not None and not isinstance(device, dict):
    print(
        f"WARNING: ignoring malformed phases file {phases_path}: "
        "'device' must be an object",
        file=sys.stderr)
    return None
  phases = data.get("phases")
  if phases is not None:
    if not isinstance(phases, list) or any(
        not isinstance(p, dict) or not isinstance(p.get("name"), str) or
        _parse_finite_float(p.get("start_s")) is None or
        _parse_finite_float(p.get("end_s")) is None for p in phases):
      print(
          f"WARNING: ignoring malformed phases file {phases_path}: "
          "invalid 'phases' entries",
          file=sys.stderr)
      return None
  return data


def format_markdown(summary: Dict[str, Any], results: List[Dict[str,
                                                                Any]]) -> str:
  trace, health = summary["trace"], summary["trace_health"]
  path, sha256 = trace["path"], trace["sha256"]
  size_mb, duration_s = trace["size_bytes"] / 1e6, trace["duration_s"]
  versions = ", ".join(
      f"{name} `{value}`" for name, value in summary["versions"].items()
      if value)
  lines = [
      f"# Perfetto storage trace `{os.path.basename(path)}`",
      "",
      f"- Trace: `{path}`, {size_mb:.1f} MB, {duration_s:.1f} s, "
      f"sha256 `{sha256}`",
      f"- Versions: {versions}",
      "",
      "## Trace health",
      "",
      health_banner(health),
  ]
  if health["ftrace_setup_errors"]:
    lines.append("Ftrace setup errors: " +
                 "; ".join(health["ftrace_setup_errors"]) + ".")
  q5_rows = next((r["rows"] for r in results if r["number"] == 5), [])
  lines += _phase_lines(summary.get("capture") or {}, q5_rows)
  for result in results:
    number, title = result["number"], result["title"]
    csv_name, row_count = result["csv"], result["row_count"]
    lines += [
        "", f"## Q{number}: {title}", "", f"`{csv_name}`: {row_count} rows"
    ]
    if result["error"]:
      lines[-1] += ", ERROR: " + result["error"]
    if len(result["rows"]) > 1:
      lines += [""] + _markdown_table(result["rows"])
  return "\n".join(lines) + "\n"


def analyze_trace(trace: str, report_dir: str, trace_processor: str,
                  package: str, capture: Optional[Dict[str, Any]]) -> int:
  """Runs the queries on a trace and writes the CSV, JSON and MD reports."""
  stem = os.path.splitext(os.path.basename(trace))[0]
  try:
    version = subprocess.run([trace_processor, "--version"],
                             capture_output=True,
                             text=True,
                             errors="replace",
                             check=False,
                             timeout=60).stdout.strip().splitlines()
  except (OSError, subprocess.SubprocessError) as exc:
    print(f"Error: failed to run {trace_processor}: {exc}", file=sys.stderr)
    return 1
  results = []
  with tempfile.TemporaryDirectory() as tmp_dir:
    health_text, health_error = _run_query(trace_processor, trace, HEALTH_SQL,
                                           tmp_dir)
    for number, title, sql in load_queries(package):
      text, error = _run_query(trace_processor, trace, sql, tmp_dir)
      csv_name = f"{stem}.q{number}.csv"
      with open(os.path.join(report_dir, csv_name), "w", encoding="utf-8") as f:
        f.write(text)
      rows, csv_error = _parse_query_csv(text)
      if not error and not csv_error and not rows:
        csv_error = "empty CSV output from trace_processor_shell"
      error = "; ".join(e for e in (error, csv_error) if e)
      results.append({
          "number": number,
          "title": title,
          "csv": csv_name,
          "row_count": max(len(rows) - 1, 0),
          "error": error,
          "rows": rows,
      })
  health_rows, health_csv_error = _parse_query_csv(health_text)
  if not health_error and not health_csv_error and not health_rows:
    health_csv_error = "empty CSV output from trace_processor_shell"
  health, metadata = parse_health(health_rows)
  health_errors = [
      e for e in (health_error, health_csv_error, health.get("error")) if e
  ]
  if health_errors:
    health["healthy"] = False
    health["error"] = "; ".join(health_errors)
  raw_device = capture.get("device") if isinstance(capture, dict) else None
  device = raw_device if isinstance(raw_device, dict) else {}
  summary = {
      "trace": {
          "path": trace,
          "sha256": _sha256(trace),
          "size_bytes": os.path.getsize(trace),
          "duration_s": _duration_s(metadata),
          "clock_id": metadata.get("trace_time_clock_id", ""),
      },
      "versions": {
          "trace_processor": version[0] if version else "",
          "device_perfetto": str(device.get("perfetto") or ""),
          "app_version_name": str(device.get("app_version_name") or ""),
          "build_fingerprint": metadata.get("android_build_fingerprint", ""),
          "chrome_user_agent": metadata.get("cr-user-agent", ""),
      },
      "trace_health": health,
      "queries": [{
          k: v for k, v in r.items() if k != "rows"
      } for r in results],
      "capture": capture if isinstance(capture, dict) else None,
  }
  summary_path = os.path.join(report_dir, f"{stem}.summary.json")
  with open(summary_path, "w", encoding="utf-8") as f:
    json.dump(summary, f, indent=2)
    f.write("\n")
  md_path = os.path.join(report_dir, f"{stem}.md")
  with open(md_path, "w", encoding="utf-8") as f:
    f.write(format_markdown(summary, results))
  print(f"Trace health: {health_banner(health)}")
  print(f"Reports: {md_path}, {summary_path}, {stem}.q*.csv")
  failed = ["Q" + str(r["number"]) for r in results if r["error"]]
  if failed:
    print("ERROR: queries failed: " + ", ".join(failed), file=sys.stderr)
  return 1 if failed or not health["healthy"] else 0


def parse_args(argv: Optional[List[str]] = None) -> argparse.Namespace:
  parser = argparse.ArgumentParser(
      description=("Record a Perfetto storage trace of Cobalt on Android TV "
                   "during a CUJ and analyze it with trace_processor_shell."))
  parser.add_argument(
      "--serial",
      default=os.environ.get("ANDROID_SERIAL"),
      help="adb device serial (default: $ANDROID_SERIAL).")
  parser.add_argument(
      "--package",
      default=DEFAULT_PACKAGE,
      help=f"Android package (default: {DEFAULT_PACKAGE}).")
  parser.add_argument(
      "--cuj",
      choices=("cold_start", "lifecycle", "manual"),
      default="cold_start",
      help=("cold_start: launch, then trace for --duration-s. lifecycle: "
            "launch, 30 s, DPAD scroll, watch intent 45 s, HOME 15 s. "
            "manual: launch, then trace until Ctrl+C, --duration-s or "
            "--cuj-cmd exits (default: cold_start)."))
  parser.add_argument(
      "--duration-s",
      type=float,
      help=(f"cold_start: seconds to trace after launch (default: "
            f"{COLD_START_DURATION_S:g}). manual: optional time limit."))
  parser.add_argument(
      "--cuj-cmd",
      default="",
      help=("manual only: host shell command to run after the launch; "
            "tracing stops when it exits. It must not restart the app, "
            "which would drop the tracing feature flag."))
  parser.add_argument(
      "--golden-tar",
      default="",
      help=("Device path of an app data tar to restore with run-as before "
            "tracing (also deletes BrowserStabilityMetrics/*.pma)."))
  parser.add_argument(
      "--drop-caches",
      action="store_true",
      help="Drop the page cache before tracing (needs root; else warns).")
  parser.add_argument(
      "--extra-args",
      default="",
      help=("Comma-separated extra commandLineArgs; "
            f"{PERFETTO_FEATURE_ARG} is always added. Cobalt merges repeated "
            "--enable-features, so pass several features as "
            "'--enable-features=A;B'."))
  parser.add_argument(
      "--include-memory-infra",
      action="store_true",
      help="Also record periodic MemoryInfra dumps (16 MiB event buffer).")
  parser.add_argument(
      "--out",
      default="",
      help=("Local trace path (default: "
            "storage_perfetto_<cuj>_<timestamp>.pftrace)."))
  parser.add_argument(
      "--report-dir",
      default="",
      help="Directory for the reports (default: the trace's directory).")
  parser.add_argument(
      "--trace-processor",
      default=DEFAULT_TRACE_PROCESSOR,
      help=f"trace_processor_shell binary (default: {DEFAULT_TRACE_PROCESSOR})."
  )
  parser.add_argument(
      "--analyze-only",
      default="",
      metavar="TRACE",
      help="Analyze an existing trace instead of recording one.")
  parser.add_argument(
      "--print-config",
      action="store_true",
      help="Print the Perfetto config and exit.")
  args = parser.parse_args(argv)
  if not re.fullmatch(r"[\w.]+", args.package):
    parser.error(f"Invalid --package: {args.package}")
  if args.duration_s is not None and (not math.isfinite(args.duration_s) or
                                      args.duration_s <= 0):
    parser.error(f"--duration-s must be positive: {args.duration_s}")
  if args.analyze_only and args.out:
    parser.error("--analyze-only and --out cannot be combined")
  if args.cuj_cmd and args.cuj != "manual":
    parser.error("--cuj-cmd requires --cuj manual")
  if args.cuj_cmd and args.duration_s is not None:
    parser.error("--cuj-cmd and --duration-s cannot be combined")
  if args.duration_s is not None and args.cuj == "lifecycle":
    parser.error("--duration-s does not apply to --cuj lifecycle")
  if args.cuj == "cold_start" and args.duration_s is None:
    args.duration_s = COLD_START_DURATION_S
  return args


def main(argv: Optional[List[str]] = None) -> int:
  args = parse_args(argv)
  config = build_perfetto_config(
      duration_cap_s(args), args.package, args.include_memory_infra)
  if args.print_config:
    sys.stdout.write(config)
    return 0
  trace_processor = os.path.expanduser(args.trace_processor)
  if not os.path.isfile(trace_processor):
    print(
        f"Error: {trace_processor} not found. Download it with 'curl -LO "
        "https://get.perfetto.dev/trace_processor && python3 trace_processor "
        "--version', or pass --trace-processor.",
        file=sys.stderr)
    return 1
  trace = (
      args.analyze_only or args.out or
      f"storage_perfetto_{args.cuj}_{datetime.now():%Y%m%d_%H%M%S}.pftrace")
  if args.analyze_only:
    if not os.path.isfile(trace):
      print(f"Error: {trace} not found.", file=sys.stderr)
      return 1
    if os.path.getsize(trace) == 0:
      print(f"Error: {trace} is empty.", file=sys.stderr)
      return 1
  stem = os.path.splitext(os.path.basename(trace))[0]
  report_dir = args.report_dir or os.path.dirname(os.path.abspath(trace))
  try:
    os.makedirs(report_dir, exist_ok=True)
  except OSError as exc:
    print(f"Error: cannot create {report_dir}: {exc}", file=sys.stderr)
    return 1
  phases_path = os.path.join(report_dir, f"{stem}.phases.json")
  capture = None
  if args.analyze_only:
    if os.path.isfile(phases_path):
      capture = load_capture(phases_path)
  else:
    try:
      os.makedirs(os.path.dirname(os.path.abspath(trace)), exist_ok=True)
      capture = record_trace(args, config, trace, phases_path)
    except (RuntimeError, OSError, subprocess.SubprocessError,
            KeyboardInterrupt) as exc:
      print(f"Error: {type(exc).__name__}: {exc}", file=sys.stderr)
      return 1
  try:
    returncode = analyze_trace(trace, report_dir, trace_processor, args.package,
                               capture)
  except (OSError, ValueError, RuntimeError) as exc:
    print(f"Error: {type(exc).__name__}: {exc}", file=sys.stderr)
    return 1
  cuj_cmd_returncode = (capture if isinstance(capture, dict) else
                        {}).get("cuj_cmd_returncode")
  if cuj_cmd_returncode:
    print(
        f"ERROR: --cuj-cmd exited with {cuj_cmd_returncode}.", file=sys.stderr)
    returncode = 1
  return returncode


if __name__ == "__main__":
  sys.exit(main())
