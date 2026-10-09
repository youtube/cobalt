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
"""Monitors and inspects Cobalt storage usage across Critical User Journeys."""

import argparse
from dataclasses import dataclass, field
from datetime import datetime
import json
import re
import subprocess
import sys
import time
from typing import Any, Callable, Dict, List, Optional, Tuple

DEFAULT_PACKAGE = "dev.cobalt.coat"
DEFAULT_ACTIVITY = "dev.cobalt.app.MainActivity"
DEFAULT_HOME_URL = "https://www.youtube.com/tv"
DEFAULT_VIDEO_URL = "https://www.youtube.com/tv#/watch?v=1La4QzGeaaQ"

SUPPORTED_PLATFORMS = ("android",)
SUPPORTED_CUJS = (
    "manual",
    "cold_start",
    "home_scroll",
    "video_playback",
    "pause_flush",
    "warm_relaunch",
    "all",
)

PERSISTENT_LOCATIONS = (
    "USER_DATA",
    "APP_DATA",
    "FILES_DIR",
    "SHARED_PREFS",
    "DATABASES",
    "NO_BACKUP",
    "APP_TEXTURES",
    "DE_DATA",
)


@dataclass
class ProcIoSample:
  """Tracks point-in-time /proc/<pid>/io and optional system I/O counters.

  Lifetime/Ownership: Created per polling tick and owned by StorageTracker.
  Threading Model: Thread-affine; accessed only on the StorageTracker thread.
  """
  pid: int
  rchar: int = 0
  wchar: int = 0
  syscr: int = 0
  syscw: int = 0
  read_bytes: int = 0
  write_bytes: int = 0
  cancelled_write_bytes: int = 0
  disk_reads_completed: Optional[int] = None
  disk_writes_completed: Optional[int] = None
  disk_io_time_ms: Optional[int] = None
  psi_some_total_us: Optional[int] = None
  psi_full_total_us: Optional[int] = None
  psi_some_avg10: Optional[float] = None
  psi_full_avg10: Optional[float] = None


@dataclass
class FileRecord:
  """Tracks metrics for an individual file across a CUJ run.

  Lifetime/Ownership: Created and owned by StorageTracker for the duration of
  the monitoring session.
  Threading Model: Thread-affine; accessed and mutated only on the thread
  running the StorageTracker.
  """
  relative_path: str
  location: str
  storage_type: str
  first_seen_sec: float
  last_seen_sec: float
  existed_at_end: bool = False
  start_bytes: int = 0
  peak_bytes: int = 0
  final_bytes: int = 0
  mtime_changes: int = 0
  last_size: int = 0
  last_mtime: int = 0


@dataclass
class StorageTypeStats:
  """Tracks aggregate metrics for a storage type across a CUJ run.

  Lifetime/Ownership: Created and owned by StorageTracker for the duration of
  the monitoring session.
  Threading Model: Thread-affine; accessed and mutated only on the thread
  running the StorageTracker.
  """
  storage_type: str
  location: str
  start_bytes: Optional[int] = None
  peak_bytes: int = 0
  peak_at_sec: float = 0.0
  final_bytes: int = 0
  write_ticks: int = 0
  files_seen: Dict[str, Tuple[int, int]] = field(default_factory=dict)


_ADB_PREFIX_CACHE: Dict[str, List[str]] = {}


def _format_adb_command(prefix: List[str], cmd_str: str) -> str:
  """Formats an ADB shell command with the given elevation prefix."""
  if not prefix:
    return cmd_str
  if prefix == ["su 0 -c"]:
    escaped_cmd = cmd_str.replace("\"", "\\\"")
    return f"su 0 -c \"{escaped_cmd}\""
  return " ".join(prefix + [cmd_str])


def _get_adb_storage_prefix(package: str) -> List[str]:
  """Determines and caches the working ADB prefix for app data access."""
  if package in _ADB_PREFIX_CACHE:
    return _ADB_PREFIX_CACHE[package]

  test_dir = f"/data/data/{package}"
  candidates = [
      [f"run-as {package}"],
      ["su 0"],
      ["su 0 -c"],
      [],
  ]
  for prefix in candidates:
    cmd_str = _format_adb_command(prefix, f"ls {test_dir}")
    try:
      res = subprocess.run(["adb", "shell", cmd_str],
                           stdout=subprocess.PIPE,
                           stderr=subprocess.PIPE,
                           text=True,
                           check=False,
                           timeout=10)
      if res.returncode == 0 and "Permission denied" not in res.stderr:
        _ADB_PREFIX_CACHE[package] = prefix
        return prefix
    except (subprocess.TimeoutExpired, FileNotFoundError):
      pass

  return [f"run-as {package}"]


def run_adb_shell(cmd_str: str,
                  package: Optional[str] = DEFAULT_PACKAGE,
                  timeout: int = 30) -> Tuple[int, str]:
  """Executes an ADB shell command, using cached elevation for package."""
  prefix = _get_adb_storage_prefix(package) if package else []
  full_cmd = _format_adb_command(prefix, cmd_str)

  try:
    res = subprocess.run(["adb", "shell", full_cmd],
                         stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE,
                         text=True,
                         check=False,
                         timeout=timeout)
    return res.returncode, res.stdout
  except subprocess.TimeoutExpired:
    return -1, ""
  except FileNotFoundError:
    return -1, ""


def normalize_android_rel_path(full_path: str,
                               package: str = DEFAULT_PACKAGE) -> str:
  """Normalizes an Android app path relative to the package data root."""
  de_prefix = f"/data/user_de/0/{package}/"
  if full_path.startswith(de_prefix):
    return f"user_de/{full_path[len(de_prefix):]}"

  ce_prefixes = (
      f"/data/user/0/{package}/",
      f"/data/data/{package}/",
  )
  for prefix in ce_prefixes:
    if full_path.startswith(prefix):
      return full_path[len(prefix):]
  return full_path.lstrip("/")


def is_simple_cache_entry_blob(rel_path: str) -> bool:
  """Returns True if rel_path is an individual SimpleCache hash entry blob."""
  if "/Cache_Data/" in rel_path or "/Code Cache/" in rel_path:
    filename = rel_path.rsplit("/", 1)[-1]
    if "_" in filename and not filename.startswith("index"):
      return True
  return False


_LOCATION_ROOTS: Tuple[Tuple[str, str], ...] = (
    ("app_content_shell/content_shell/", "USER_DATA"),
    ("app_content_shell/", "APP_DATA"),
    ("files/", "FILES_DIR"),
    ("shared_prefs/", "SHARED_PREFS"),
    ("databases/", "DATABASES"),
    ("no_backup/", "NO_BACKUP"),
    ("app_textures/", "APP_TEXTURES"),
    ("cache/", "DIR_CACHE"),
    ("code_cache/", "CODE_CACHE"),
    ("user_de/cache/", "DE_CACHE"),
    ("user_de/code_cache/", "DE_CODE_CACHE"),
    ("user_de/", "DE_DATA"),
)
_SQLITE_SUFFIX_RE = re.compile(r"-(?:journal|wal|shm)$")
_CONTAINER_DIRS = frozenset({"Network", "WebStorage"})


def classify_storage_path(rel_path: str,
                          package: str = DEFAULT_PACKAGE) -> Tuple[str, str]:
  """Derives (location, storage_type) directly from the relative path."""
  norm = normalize_android_rel_path(rel_path, package)

  for prefix, location in _LOCATION_ROOTS:
    if norm.startswith(prefix):
      sub = norm[len(prefix):]
      parts = sub.split("/")
      if len(parts) >= 2 and parts[0] in _CONTAINER_DIRS:
        sub_name = _SQLITE_SUFFIX_RE.sub("", parts[1])
        comp = f"{parts[0]}/{sub_name}"
      else:
        comp = _SQLITE_SUFFIX_RE.sub("", parts[0])
      if comp.startswith(".org.chromium.Chromium."):
        comp = ".org.chromium.Chromium.*"
      if comp in ("Cache", "Code Cache"):
        return location, f"{location}:{comp}"
      return location, comp

  return "OTHER", norm.split("/", 1)[0]


def parse_stat_output(output: str,
                      package: str = DEFAULT_PACKAGE
                     ) -> Dict[str, Tuple[int, int]]:
  """Parses 'stat -c \"%s %Y %n\"' into {rel_path: (size_bytes, mtime)}."""
  snapshot: Dict[str, Tuple[int, int]] = {}
  for line in output.splitlines():
    line = line.strip()
    if not line or "No such file" in line or "Permission denied" in line:
      continue
    parts = line.split(None, 2)
    if len(parts) < 3 or not parts[0].isdigit() or not parts[1].isdigit():
      continue
    size_bytes = int(parts[0])
    mtime = int(parts[1])
    rel_path = normalize_android_rel_path(parts[2], package)
    snapshot[rel_path] = (size_bytes, mtime)
  return snapshot


def collect_android_snapshot(
    package: str = DEFAULT_PACKAGE) -> Dict[str, Tuple[int, int]]:
  """Collects a file snapshot across all CE and DE storage dirs."""
  ce_dir = f"/data/data/{package}"
  de_dir = f"/data/user_de/0/{package}"
  cmd = (f"find {ce_dir} {de_dir} -type f "
         f"-exec stat -c '%s %Y %n' {{}} + 2>/dev/null")
  _, out = run_adb_shell(cmd, package=package)
  return parse_stat_output(out, package=package)


_WHOLE_DISK_RE = re.compile(r"^(?:mmcblk\d+|sd[a-z]+|nvme\d+n\d+|vd[a-z]+)$")


def _parse_diskstats_section(
    lines: List[str]) -> Optional[Tuple[int, int, int]]:
  """Parses /proc/diskstats into (reads_completed, writes_completed, io_ms)."""
  reads_total = 0
  writes_total = 0
  io_ms_total = 0
  matched = False
  for line in lines:
    parts = line.split()
    if len(parts) < 13 or not _WHOLE_DISK_RE.match(parts[2]):
      continue
    if (parts[3].isdigit() and parts[7].isdigit() and parts[12].isdigit()):
      reads_total += int(parts[3])
      writes_total += int(parts[7])
      io_ms_total += int(parts[12])
      matched = True
  if not matched:
    return None
  return reads_total, writes_total, io_ms_total


def _parse_psi_io_section(
    lines: List[str]
) -> Tuple[Optional[int], Optional[int], Optional[float], Optional[float]]:
  """Parses /proc/pressure/io into (some_us, full_us, some_avg, full_avg)."""
  some_total_us: Optional[int] = None
  full_total_us: Optional[int] = None
  some_avg10: Optional[float] = None
  full_avg10: Optional[float] = None

  for line in lines:
    parts = line.split()
    if not parts or parts[0] not in ("some", "full"):
      continue
    kind = parts[0]
    for token in parts[1:]:
      if "=" not in token:
        continue
      k, v = token.split("=", 1)
      if k == "total" and v.isdigit():
        if kind == "some":
          some_total_us = int(v)
        else:
          full_total_us = int(v)
      elif k == "avg10":
        try:
          val = float(v)
          if kind == "some":
            some_avg10 = val
          else:
            full_avg10 = val
        except ValueError:
          pass
  return some_total_us, full_total_us, some_avg10, full_avg10


def parse_proc_io_output(output: str) -> Optional[ProcIoSample]:
  """Parses /proc/<pid>/io, /proc/diskstats, and /proc/pressure/io output."""
  values: Dict[str, int] = {}
  diskstats_lines: List[str] = []
  psi_lines: List[str] = []
  section = "proc_io"

  for raw_line in output.splitlines():
    line = raw_line.strip()
    if not line:
      continue
    if line == "---DISKSTATS---":
      section = "diskstats"
      continue
    if line == "---PSI_IO---":
      section = "psi_io"
      continue

    if section == "proc_io":
      if ":" not in line:
        continue
      key, val_str = line.split(":", 1)
      key = key.strip()
      val_str = val_str.strip()
      if val_str.isdigit():
        values[key] = int(val_str)
    elif section == "diskstats":
      diskstats_lines.append(line)
    elif section == "psi_io":
      psi_lines.append(line)

  disk_tuple = _parse_diskstats_section(diskstats_lines)
  some_us, full_us, some_avg10, full_avg10 = _parse_psi_io_section(psi_lines)

  if "pid" not in values and disk_tuple is None and some_us is None:
    return None

  return ProcIoSample(
      pid=values.get("pid", 0),
      rchar=values.get("rchar", 0),
      wchar=values.get("wchar", 0),
      syscr=values.get("syscr", 0),
      syscw=values.get("syscw", 0),
      read_bytes=values.get("read_bytes", 0),
      write_bytes=values.get("write_bytes", 0),
      cancelled_write_bytes=values.get("cancelled_write_bytes", 0),
      disk_reads_completed=disk_tuple[0] if disk_tuple else None,
      disk_writes_completed=disk_tuple[1] if disk_tuple else None,
      disk_io_time_ms=disk_tuple[2] if disk_tuple else None,
      psi_some_total_us=some_us,
      psi_full_total_us=full_us,
      psi_some_avg10=some_avg10,
      psi_full_avg10=full_avg10,
  )


def collect_android_proc_io(
    package: str = DEFAULT_PACKAGE) -> Optional[ProcIoSample]:
  """Collects /proc/<pid>/io, /proc/diskstats, and /proc/pressure/io."""
  cmd = (f"sh -c 'pid=$(pidof {package}); pid=${{pid%% *}}; "
         "[ -n \"$pid\" ] && [ -r \"/proc/$pid/io\" ] && "
         "echo \"pid: $pid\" && cat \"/proc/$pid/io\" 2>/dev/null; "
         "[ -r /proc/diskstats ] && echo \"---DISKSTATS---\" && "
         "cat /proc/diskstats 2>/dev/null; "
         "[ -r /proc/pressure/io ] && echo \"---PSI_IO---\" && "
         "cat /proc/pressure/io 2>/dev/null'")
  _, out = run_adb_shell(cmd, package=package)
  return parse_proc_io_output(out)


class StorageTracker:
  """Aggregates periodic storage snapshots into Section A and Section B.

  Lifetime/Ownership: Created and owned by the main execution function for the
  duration of a monitoring run.
  Threading Model: Thread-affine; expected to be called and mutated only on
  the main execution thread.
  """

  def __init__(self, package: str = DEFAULT_PACKAGE):
    self.package = package
    self.sample_count = 0
    self.type_stats: Dict[str, StorageTypeStats] = {}
    self.file_records: Dict[str, FileRecord] = {}
    self.start_persistent_bytes: Optional[int] = None
    self.peak_persistent_bytes: int = 0
    self.peak_persistent_at_sec: float = 0.0
    self.final_persistent_bytes: int = 0
    self._last_snapshot: Dict[str, Tuple[int, int]] = {}
    self._proc_io_baselines: Dict[int, ProcIoSample] = {}
    self._proc_io_latest: Dict[int, ProcIoSample] = {}
    self._proc_io_prev: Dict[int, Tuple[float, ProcIoSample]] = {}
    self._last_proc_io_sec: Optional[float] = None
    self.peak_read_kbps: float = 0.0
    self.peak_read_at_sec: float = 0.0
    self.peak_write_kbps: float = 0.0
    self.peak_write_at_sec: float = 0.0
    self._disk_baseline: Optional[Tuple[int, int, int]] = None
    self._disk_latest: Optional[Tuple[int, int, int]] = None
    self._disk_prev: Optional[Tuple[float, int, int]] = None
    self.peak_read_iops: float = 0.0
    self.peak_read_iops_at_sec: float = 0.0
    self.peak_write_iops: float = 0.0
    self.peak_write_iops_at_sec: float = 0.0
    self._psi_baseline: Optional[Tuple[int, int]] = None
    self._psi_latest: Optional[Tuple[int, int]] = None
    self.peak_psi_some_avg10: float = 0.0
    self.peak_psi_full_avg10: float = 0.0

  def record_proc_io(self, elapsed_sec: float,
                     sample: Optional[ProcIoSample]) -> None:
    """Ingests a point-in-time /proc/<pid>/io & system I/O sample."""
    if sample is None:
      return

    if sample.pid > 0:
      if sample.pid not in self._proc_io_baselines:
        if elapsed_sec == 0.0:
          self._proc_io_baselines[sample.pid] = sample
        else:
          self._proc_io_baselines[sample.pid] = ProcIoSample(pid=sample.pid)
          prev_t = (
              self._last_proc_io_sec
              if self._last_proc_io_sec is not None else 0.0)
          dt = elapsed_sec - prev_t
          if dt > 0:
            read_kbps = (sample.read_bytes / 1024.0) / dt
            write_kbps = (sample.write_bytes / 1024.0) / dt
            if read_kbps > self.peak_read_kbps:
              self.peak_read_kbps = read_kbps
              self.peak_read_at_sec = elapsed_sec
            if write_kbps > self.peak_write_kbps:
              self.peak_write_kbps = write_kbps
              self.peak_write_at_sec = elapsed_sec
      elif sample.pid in self._proc_io_prev:
        prev_sec, prev_sample = self._proc_io_prev[sample.pid]
        dt = elapsed_sec - prev_sec
        if dt > 0:
          delta_r_kb = max(0,
                           sample.read_bytes - prev_sample.read_bytes) / 1024.0
          delta_w_kb = max(
              0, sample.write_bytes - prev_sample.write_bytes) / 1024.0
          read_kbps = delta_r_kb / dt
          write_kbps = delta_w_kb / dt
          if read_kbps > self.peak_read_kbps:
            self.peak_read_kbps = read_kbps
            self.peak_read_at_sec = elapsed_sec
          if write_kbps > self.peak_write_kbps:
            self.peak_write_kbps = write_kbps
            self.peak_write_at_sec = elapsed_sec

      self._proc_io_latest[sample.pid] = sample
      self._proc_io_prev[sample.pid] = (elapsed_sec, sample)

    if (sample.disk_reads_completed is not None and
        sample.disk_writes_completed is not None):
      cur_reads = sample.disk_reads_completed
      cur_writes = sample.disk_writes_completed
      cur_io_ms = sample.disk_io_time_ms or 0
      if self._disk_baseline is None:
        self._disk_baseline = (cur_reads, cur_writes, cur_io_ms)
      elif self._disk_prev is not None:
        prev_sec, prev_reads, prev_writes = self._disk_prev
        dt = elapsed_sec - prev_sec
        if dt > 0:
          r_iops = max(0, cur_reads - prev_reads) / dt
          w_iops = max(0, cur_writes - prev_writes) / dt
          if r_iops > self.peak_read_iops:
            self.peak_read_iops = r_iops
            self.peak_read_iops_at_sec = elapsed_sec
          if w_iops > self.peak_write_iops:
            self.peak_write_iops = w_iops
            self.peak_write_iops_at_sec = elapsed_sec
      self._disk_latest = (cur_reads, cur_writes, cur_io_ms)
      self._disk_prev = (elapsed_sec, cur_reads, cur_writes)

    if sample.psi_some_total_us is not None:
      some_us = sample.psi_some_total_us
      full_us = sample.psi_full_total_us or 0
      if self._psi_baseline is None:
        self._psi_baseline = (some_us, full_us)
      self._psi_latest = (some_us, full_us)
      if sample.psi_some_avg10 is not None:
        self.peak_psi_some_avg10 = max(self.peak_psi_some_avg10,
                                       sample.psi_some_avg10)
      if sample.psi_full_avg10 is not None:
        self.peak_psi_full_avg10 = max(self.peak_psi_full_avg10,
                                       sample.psi_full_avg10)

    self._last_proc_io_sec = elapsed_sec

  def record_snapshot(self, elapsed_sec: float,
                      snapshot: Dict[str, Tuple[int, int]]) -> None:
    """Ingests a point-in-time file snapshot at elapsed_sec."""
    if not snapshot:
      return
    self.sample_count += 1
    current_type_bytes: Dict[str, int] = {}
    current_type_files: Dict[str, Dict[str, Tuple[int, int]]] = {}

    for rel_path, (size_bytes, mtime) in snapshot.items():
      location, storage_type = classify_storage_path(rel_path, self.package)

      if storage_type not in self.type_stats:
        self.type_stats[storage_type] = StorageTypeStats(
            storage_type=storage_type,
            location=location,
            start_bytes=0 if self.sample_count > 1 else None,
        )

      current_type_bytes[storage_type] = (
          current_type_bytes.get(storage_type, 0) + size_bytes)
      current_type_files.setdefault(storage_type,
                                    {})[rel_path] = (size_bytes, mtime)

      if rel_path not in self.file_records:
        self.file_records[rel_path] = FileRecord(
            relative_path=rel_path,
            location=location,
            storage_type=storage_type,
            first_seen_sec=elapsed_sec,
            last_seen_sec=elapsed_sec,
            start_bytes=size_bytes if self.sample_count == 1 else 0,
            peak_bytes=size_bytes,
            final_bytes=size_bytes,
            mtime_changes=1 if self.sample_count > 1 else 0,
            last_size=size_bytes,
            last_mtime=mtime,
        )
      else:
        rec = self.file_records[rel_path]
        was_deleted = rel_path not in self._last_snapshot
        if (was_deleted or size_bytes != rec.last_size or
            mtime != rec.last_mtime):
          rec.mtime_changes += 1
        rec.last_seen_sec = elapsed_sec
        rec.peak_bytes = max(rec.peak_bytes, size_bytes)
        rec.final_bytes = size_bytes
        rec.last_size = size_bytes
        rec.last_mtime = mtime

    # Mark files missing from the current snapshot
    for rel_path, rec in self.file_records.items():
      if rel_path in snapshot:
        rec.existed_at_end = True
      else:
        rec.existed_at_end = False
        rec.final_bytes = 0

    # Update per-storage-type stats
    persistent_total = 0
    for storage_type, stats in self.type_stats.items():
      total_b = current_type_bytes.get(storage_type, 0)
      files_now = current_type_files.get(storage_type, {})

      if stats.start_bytes is None:
        stats.start_bytes = total_b
      elif self.sample_count > 1 and files_now != stats.files_seen:
        stats.write_ticks += 1

      if total_b > stats.peak_bytes:
        stats.peak_bytes = total_b
        stats.peak_at_sec = elapsed_sec

      stats.final_bytes = total_b
      stats.files_seen = files_now

      if stats.location in PERSISTENT_LOCATIONS:
        persistent_total += total_b

    if self.start_persistent_bytes is None:
      self.start_persistent_bytes = persistent_total
    if persistent_total > self.peak_persistent_bytes:
      self.peak_persistent_bytes = persistent_total
      self.peak_persistent_at_sec = elapsed_sec
    self.final_persistent_bytes = persistent_total
    self._last_snapshot = dict(snapshot)

  def build_summary_data(self, platform: str, cuj: str, duration_sec: float,
                         interval_sec: float) -> Dict[str, Any]:
    """Produces the structured report dictionary (Section A + Section B)."""
    section_a: List[Dict[str, Any]] = []
    # Sort persistent storage types first (by peak_bytes desc), then cache
    sorted_types = sorted(
        self.type_stats.values(),
        key=lambda s: (0 if s.location in PERSISTENT_LOCATIONS else 1, -s.
                       peak_bytes, s.storage_type),
    )

    for s in sorted_types:
      start_kb = (s.start_bytes or 0) / 1024.0
      peak_kb = s.peak_bytes / 1024.0
      final_kb = s.final_bytes / 1024.0
      section_a.append({
          "storage_type": s.storage_type,
          "location": s.location,
          "start_kb": round(start_kb, 2),
          "peak_kb": round(peak_kb, 2),
          "peak_at_sec": round(s.peak_at_sec, 1),
          "final_kb": round(final_kb, 2),
          "write_ticks": s.write_ticks,
      })

    # Sort files: persistent before cache, then transient/high-churn/peak size
    sorted_files = sorted(
        self.file_records.values(),
        key=lambda f: (0 if f.location in PERSISTENT_LOCATIONS else 1, -f.
                       mtime_changes, -f.peak_bytes, f.relative_path),
    )
    section_b: List[Dict[str, Any]] = []
    for f in sorted_files:
      if is_simple_cache_entry_blob(f.relative_path):
        continue
      # Proposal 1 filter: include only transient (deleted), truncated/spiked,
      # or actively written files (exclude untouched static baseline files).
      is_transient = not f.existed_at_end
      is_truncated = f.peak_bytes > f.final_bytes
      is_active = f.mtime_changes >= 2 or f.final_bytes != f.start_bytes
      if not (is_transient or is_truncated or is_active):
        continue

      if is_transient:
        status = "**Transient (Deleted)**"
      elif is_truncated:
        status = "**Truncated**"
      else:
        status = "Active"

      start_file_kb = f.start_bytes / 1024.0
      peak_file_kb = f.peak_bytes / 1024.0
      final_file_kb = f.final_bytes / 1024.0
      delta_file_kb = (f.final_bytes - f.start_bytes) / 1024.0

      section_b.append({
          "relative_path": f.relative_path,
          "location": f.location,
          "storage_type": f.storage_type,
          "status": status,
          "existed_at_end": f.existed_at_end,
          "first_seen_sec": round(f.first_seen_sec, 1),
          "last_seen_sec": round(f.last_seen_sec, 1),
          "start_kb": round(start_file_kb, 2),
          "peak_kb": round(peak_file_kb, 2),
          "final_kb": round(final_file_kb, 2),
          "delta_kb": round(delta_file_kb, 2),
          "mtime_changes": f.mtime_changes,
      })

    total_rchar = 0
    total_wchar = 0
    total_syscr = 0
    total_syscw = 0
    total_read_bytes = 0
    total_write_bytes = 0
    for pid, latest in self._proc_io_latest.items():
      base = self._proc_io_baselines.get(pid, ProcIoSample(pid=pid))
      total_rchar += max(0, latest.rchar - base.rchar)
      total_wchar += max(0, latest.wchar - base.wchar)
      total_syscr += max(0, latest.syscr - base.syscr)
      total_syscw += max(0, latest.syscw - base.syscw)
      total_read_bytes += max(0, latest.read_bytes - base.read_bytes)
      total_write_bytes += max(0, latest.write_bytes - base.write_bytes)

    write_amp_factor = (
        round(total_write_bytes / total_wchar, 2) if total_wchar > 0 else 0.0)

    start_pers_kb = (self.start_persistent_bytes or 0) / 1024.0
    peak_pers_kb = self.peak_persistent_bytes / 1024.0
    final_pers_kb = self.final_persistent_bytes / 1024.0

    summary: Dict[str, Any] = {
        "metadata": {
            "platform": platform,
            "package": self.package,
            "cuj": cuj,
            "duration_sec": round(duration_sec, 1),
            "interval_sec": interval_sec,
            "sample_count": self.sample_count,
            "start_persistent_kb": round(start_pers_kb, 2),
            "peak_persistent_kb": round(peak_pers_kb, 2),
            "peak_persistent_at_sec": round(self.peak_persistent_at_sec, 1),
            "final_persistent_kb": round(final_pers_kb, 2),
        },
        "process_io_summary": {
            "pids_tracked": len(self._proc_io_latest),
            "rchar_kb": round(total_rchar / 1024.0, 2),
            "wchar_kb": round(total_wchar / 1024.0, 2),
            "read_kb": round(total_read_bytes / 1024.0, 2),
            "write_kb": round(total_write_bytes / 1024.0, 2),
            "syscr": total_syscr,
            "syscw": total_syscw,
            "peak_read_kbps": round(self.peak_read_kbps, 2),
            "peak_read_at_sec": round(self.peak_read_at_sec, 1),
            "peak_write_kbps": round(self.peak_write_kbps, 2),
            "peak_write_at_sec": round(self.peak_write_at_sec, 1),
            "write_amp_factor": write_amp_factor,
        },
        "storage_type_summary": section_a,
        "files_inventory": section_b,
    }

    if self._disk_baseline is not None and self._disk_latest is not None:
      b_r, b_w, b_ms = self._disk_baseline
      l_r, l_w, l_ms = self._disk_latest
      reads_completed = max(0, l_r - b_r)
      writes_completed = max(0, l_w - b_w)
      io_busy_ms = max(0, l_ms - b_ms)
      avg_r_iops = reads_completed / duration_sec if duration_sec > 0 else 0.0
      avg_w_iops = writes_completed / duration_sec if duration_sec > 0 else 0.0
      summary["disk_io_summary"] = {
          "reads_completed": reads_completed,
          "writes_completed": writes_completed,
          "avg_read_iops": round(avg_r_iops, 2),
          "avg_write_iops": round(avg_w_iops, 2),
          "peak_read_iops": round(self.peak_read_iops, 2),
          "peak_read_iops_at_sec": round(self.peak_read_iops_at_sec, 1),
          "peak_write_iops": round(self.peak_write_iops, 2),
          "peak_write_iops_at_sec": round(self.peak_write_iops_at_sec, 1),
          "io_busy_ms": io_busy_ms,
      }

    if self._psi_baseline is not None and self._psi_latest is not None:
      b_some, b_full = self._psi_baseline
      l_some, l_full = self._psi_latest
      some_stall_ms = max(0, l_some - b_some) / 1000.0
      full_stall_ms = max(0, l_full - b_full) / 1000.0
      summary["psi_io_summary"] = {
          "some_stall_ms": round(some_stall_ms, 2),
          "full_stall_ms": round(full_stall_ms, 2),
          "peak_some_avg10": round(self.peak_psi_some_avg10, 2),
          "peak_full_avg10": round(self.peak_psi_full_avg10, 2),
      }

    return summary


def format_markdown_report(report: Dict[str, Any]) -> str:
  """Formats the report dictionary into a Markdown document."""
  meta = report["metadata"]
  platform = meta["platform"]
  package = meta["package"]
  cuj = meta["cuj"]
  duration_sec = meta["duration_sec"]
  sample_count = meta["sample_count"]
  interval_sec = meta["interval_sec"]
  start_pers_kb = meta["start_persistent_kb"]
  peak_pers_kb = meta["peak_persistent_kb"]
  peak_pers_at = meta["peak_persistent_at_sec"]
  final_pers_kb = meta["final_persistent_kb"]

  lines = [
      "# Cobalt Storage Inspection Report",
      f"- **Platform**: `{platform}` (`{package}`)",
      (f"- **CUJ Executed**: `{cuj}` "
       f"(Duration: `{duration_sec}s`, "
       f"Samples: `{sample_count}`, "
       f"Interval: `{interval_sec}s`)"),
      (f"- **Persistent Footprint**: "
       f"Start: `{start_pers_kb:.2f} KB` | "
       f"Peak: `{peak_pers_kb:.2f} KB` (`t={peak_pers_at}s`) | "
       f"Final: `{final_pers_kb:.2f} KB`"),
  ]

  proc_io = report.get("process_io_summary")
  if proc_io and proc_io.get("pids_tracked", 0) > 0:
    rchar_kb = proc_io["rchar_kb"]
    wchar_kb = proc_io["wchar_kb"]
    read_kb = proc_io["read_kb"]
    write_kb = proc_io["write_kb"]
    syscr = proc_io["syscr"]
    syscw = proc_io["syscw"]
    peak_r_kbps = proc_io.get("peak_read_kbps", 0.0)
    peak_r_at = proc_io.get("peak_read_at_sec", 0.0)
    peak_w_kbps = proc_io.get("peak_write_kbps", 0.0)
    peak_w_at = proc_io.get("peak_write_at_sec", 0.0)
    waf = proc_io.get("write_amp_factor", 0.0)
    lines.append("- **Process I/O (`/proc/<pid>/io`)**: "
                 f"VFS Read (`rchar`): `{rchar_kb:.2f} KB` | "
                 f"VFS Write (`wchar`): `{wchar_kb:.2f} KB` | "
                 f"Block Read (`read_bytes`): `{read_kb:.2f} KB` "
                 f"(Peak: `{peak_r_kbps:.2f} KB/s` at `t={peak_r_at}s`) | "
                 f"Block Write (`write_bytes`): `{write_kb:.2f} KB` "
                 f"(Peak: `{peak_w_kbps:.2f} KB/s` at `t={peak_w_at}s`) | "
                 f"Write Amp (`write_bytes/wchar`): `{waf:.2f}x` "
                 f"(`syscr`: `{syscr}`, `syscw`: `{syscw}`)")

  disk_io = report.get("disk_io_summary")
  if disk_io is not None:
    avg_r = disk_io["avg_read_iops"]
    peak_r = disk_io["peak_read_iops"]
    peak_r_t = disk_io["peak_read_iops_at_sec"]
    tot_r = disk_io["reads_completed"]
    avg_w = disk_io["avg_write_iops"]
    peak_w = disk_io["peak_write_iops"]
    peak_w_t = disk_io["peak_write_iops_at_sec"]
    tot_w = disk_io["writes_completed"]
    busy_ms = disk_io["io_busy_ms"]
    lines.append("- **Block Device IOPS (`/proc/diskstats`)**: "
                 f"Read IOPS: avg `{avg_r:.2f}`, peak `{peak_r:.2f}` "
                 f"(`t={peak_r_t}s`, total `{tot_r}`) | "
                 f"Write IOPS: avg `{avg_w:.2f}`, peak `{peak_w:.2f}` "
                 f"(`t={peak_w_t}s`, total `{tot_w}`) | "
                 f"Device Busy: `{busy_ms} ms`")

  psi_io = report.get("psi_io_summary")
  if psi_io is not None:
    some_ms = psi_io["some_stall_ms"]
    some_avg = psi_io["peak_some_avg10"]
    full_ms = psi_io["full_stall_ms"]
    full_avg = psi_io["peak_full_avg10"]
    lines.append(
        "- **I/O Pressure Stall (`/proc/pressure/io`)**: "
        f"Some Stall: `{some_ms:.2f} ms` (peak avg10: `{some_avg:.2f}%`) | "
        f"Full Stall: `{full_ms:.2f} ms` (peak avg10: `{full_avg:.2f}%`)")

  lines.extend([
      "",
      "---",
      "",
      "## Section A: Storage Type Summary (Full Footprint Accounting)",
      "",
      ("| Storage Type | Location | Start KB | Peak KB (Time) | Final KB | "
       "Write Ticks |"),
      "| :--- | :--- | ---: | ---: | ---: | ---: |",
  ])

  for row in report["storage_type_summary"]:
    stype = row["storage_type"]
    loc = row["location"]
    start_kb = row["start_kb"]
    peak_kb = row["peak_kb"]
    peak_at = row["peak_at_sec"]
    final_kb = row["final_kb"]
    ticks = row["write_ticks"]
    lines.append(f"| `{stype}` | `{loc}` | "
                 f"{start_kb:.2f} | "
                 f"{peak_kb:.2f} (`t={peak_at}s`) | "
                 f"{final_kb:.2f} | "
                 f"{ticks} |")

  lines.extend([
      "",
      "---",
      "",
      "## Section B: Transient & High-Churn File Hotspots (Filtered)",
      "",
      ("| Relative File Path | Storage Type | Status | Lifespan | "
       "Start KB | Peak KB | Final KB | Delta KB | Writes (`mtime_changes`) |"),
      "| :--- | :--- | :---: | :---: | ---: | ---: | ---: | ---: | ---: |",
  ])

  for f in report["files_inventory"]:
    rel_path = f["relative_path"]
    stype = f["storage_type"]
    status_str = f.get(
        "status",
        "Active" if f["existed_at_end"] else "**Transient (Deleted)**")
    first_seen = f["first_seen_sec"]
    last_seen = f["last_seen_sec"]
    lifespan_str = f"`t={first_seen}s–{last_seen}s`"
    start_kb = f.get("start_kb", 0.0)
    peak_kb = f["peak_kb"]
    final_kb = f["final_kb"]
    delta_kb = f.get("delta_kb", final_kb - start_kb)
    writes = f["mtime_changes"]
    lines.append(f"| `{rel_path}` | `{stype}` | {status_str} | "
                 f"{lifespan_str} | "
                 f"{start_kb:.2f} | {peak_kb:.2f} | {final_kb:.2f} | "
                 f"{delta_kb:+.2f} | {writes} |")

  lines.append("")
  return "\n".join(lines)


def _poll_for_duration(tracker: StorageTracker,
                       duration_sec: float,
                       interval_sec: float,
                       run_start_time: float,
                       snapshot_fn: Callable[[], Dict[str, Tuple[int, int]]],
                       *,
                       proc_io_fn: Optional[Callable[
                           [], Optional[ProcIoSample]]] = None,
                       action_cb: Optional[Callable[[float], None]] = None,
                       phase_label: str = "") -> None:
  """Polls storage snapshots every interval_sec for duration_sec."""
  phase_start = time.time()
  while True:
    now = time.time()
    phase_elapsed = now - phase_start
    total_elapsed = now - run_start_time

    if action_cb:
      action_cb(phase_elapsed)

    snap = snapshot_fn()
    tracker.record_snapshot(total_elapsed, snap)
    if proc_io_fn:
      tracker.record_proc_io(total_elapsed, proc_io_fn())

    pers_kb = tracker.final_persistent_bytes / 1024.0
    peak_kb = tracker.peak_persistent_bytes / 1024.0
    print(f"[{total_elapsed:6.1f}s] {phase_label:<15} | "
          f"Persistent: {pers_kb:7.2f} KB (Peak: {peak_kb:7.2f} KB) | "
          f"Tracked Files: {len(tracker.file_records)}")

    if phase_elapsed >= duration_sec:
      break

    sleep_for = max(min(0.1, interval_sec), interval_sec - (time.time() - now))
    time.sleep(sleep_for)


def _launch_cobalt_android(url: str,
                           package: str = DEFAULT_PACKAGE,
                           stop_first: bool = True) -> None:
  """Launches Cobalt on Android TV with the specified URL."""
  if stop_first:
    run_adb_shell(f"am force-stop {package}", package=None)
    time.sleep(1.0)
  if package == DEFAULT_PACKAGE:
    cmd = (f"am start -n {package}/{DEFAULT_ACTIVITY} "
           f"--esa commandLineArgs '--url={url}'")
  else:
    cmd = f"am start -a android.intent.action.VIEW -d '{url}' {package}"
  run_adb_shell(cmd, package=None)


def verify_android_device_connected() -> None:
  """Verifies that an Android device is connected and reachable via ADB."""
  try:
    res = subprocess.run(["adb", "get-state"],
                         stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE,
                         text=True,
                         check=False,
                         timeout=5)
  except (subprocess.TimeoutExpired, FileNotFoundError) as exc:
    raise RuntimeError(f"Failed to run 'adb get-state': {exc}") from exc

  if res.returncode != 0 or res.stdout.strip() != "device":
    err = res.stderr.strip() or res.stdout.strip() or "no device connected"
    raise RuntimeError(f"No active Android TV device found via ADB ({err}). "
                       "Verify 'adb devices' (or run 'gcert' if using Pontis).")


def _make_scroll_action() -> Callable[[float], None]:
  """Creates the stateful DPAD scroll callback for home_scroll."""
  last_step = [-1]

  def _scroll_action(phase_elapsed: float) -> None:
    step = int(phase_elapsed // 2)
    if phase_elapsed >= 10.0 and step > last_step[0]:
      last_step[0] = step
      key = "KEYCODE_DPAD_DOWN" if step % 6 == 0 else "KEYCODE_DPAD_RIGHT"
      run_adb_shell(f"input keyevent {key}", package=None)

  return _scroll_action


def _make_playback_action() -> Callable[[float], None]:
  """Creates the stateful in-player shelf navigation callback."""
  last_scrub = [-1]
  shelf_opened = [False]

  def _playback_action(phase_elapsed: float) -> None:
    scrub_step = int(phase_elapsed // 5)
    if phase_elapsed >= 60.0 and scrub_step > last_scrub[0]:
      last_scrub[0] = scrub_step
      if not shelf_opened[0]:
        shelf_opened[0] = True
        run_adb_shell(
            "input keyevent KEYCODE_DPAD_DOWN KEYCODE_DPAD_DOWN", package=None)
      else:
        run_adb_shell("input keyevent KEYCODE_DPAD_RIGHT", package=None)

  return _playback_action


def _make_pause_action() -> Callable[[float], None]:
  """Creates the stateful KEYCODE_HOME backgrounding callback."""
  paused_sent = [False]

  def _pause_action(phase_elapsed: float) -> None:
    if phase_elapsed >= 5.0 and not paused_sent[0]:
      paused_sent[0] = True
      run_adb_shell("input keyevent KEYCODE_HOME", package=None)

  return _pause_action


def run_cuj_android(
    cuj: str,
    interval_sec: float,
    tracker: StorageTracker,
    package: str = DEFAULT_PACKAGE,
    snapshot_fn: Optional[Callable[[], Dict[str, Tuple[int, int]]]] = None,
    *,
    proc_io_fn: Optional[Callable[[], Optional[ProcIoSample]]] = None) -> float:
  """Executes the specified CUJ on Android TV while polling storage."""
  if snapshot_fn is None:
    verify_android_device_connected()
  snap_callable = snapshot_fn or (lambda: collect_android_snapshot(package))
  io_callable = proc_io_fn
  if io_callable is None and snapshot_fn is None:

    def _default_proc_io() -> Optional[ProcIoSample]:
      return collect_android_proc_io(package)

    io_callable = _default_proc_io
  run_start = time.time()

  # Take initial baseline snapshot at t=0s before launching CUJ actions
  tracker.record_snapshot(0.0, snap_callable())
  if io_callable:
    tracker.record_proc_io(0.0, io_callable())

  cujs_to_run = ([
      "cold_start", "home_scroll", "video_playback", "pause_flush",
      "warm_relaunch"
  ] if cuj == "all" else [cuj])

  for active_cuj in cujs_to_run:
    print(f"\n=== Running CUJ: {active_cuj} ===")
    if active_cuj == "manual":
      print("Passive monitoring active (no app restart or synthetic keys). "
            "Interact with the app and press Ctrl+C to stop and generate "
            "report...")
      _poll_for_duration(
          tracker,
          duration_sec=float("inf"),
          interval_sec=interval_sec,
          run_start_time=run_start,
          snapshot_fn=snap_callable,
          proc_io_fn=io_callable,
          phase_label="manual")

    elif active_cuj == "cold_start":
      _launch_cobalt_android(DEFAULT_HOME_URL, package=package, stop_first=True)
      _poll_for_duration(
          tracker,
          duration_sec=60.0,
          interval_sec=interval_sec,
          run_start_time=run_start,
          snapshot_fn=snap_callable,
          proc_io_fn=io_callable,
          phase_label="cold_start")

    elif active_cuj == "home_scroll":
      _launch_cobalt_android(DEFAULT_HOME_URL, package=package, stop_first=True)
      _poll_for_duration(
          tracker,
          duration_sec=90.0,
          interval_sec=interval_sec,
          run_start_time=run_start,
          snapshot_fn=snap_callable,
          proc_io_fn=io_callable,
          action_cb=_make_scroll_action(),
          phase_label="home_scroll")

    elif active_cuj == "video_playback":
      _launch_cobalt_android(
          DEFAULT_VIDEO_URL, package=package, stop_first=True)
      _poll_for_duration(
          tracker,
          duration_sec=180.0,
          interval_sec=interval_sec,
          run_start_time=run_start,
          snapshot_fn=snap_callable,
          proc_io_fn=io_callable,
          action_cb=_make_playback_action(),
          phase_label="video_playback")

    elif active_cuj == "pause_flush":
      _launch_cobalt_android(DEFAULT_HOME_URL, package=package, stop_first=True)
      _poll_for_duration(
          tracker,
          duration_sec=10.0,
          interval_sec=interval_sec,
          run_start_time=run_start,
          snapshot_fn=snap_callable,
          proc_io_fn=io_callable,
          phase_label="pre_pause")
      _poll_for_duration(
          tracker,
          duration_sec=30.0,
          interval_sec=interval_sec,
          run_start_time=run_start,
          snapshot_fn=snap_callable,
          proc_io_fn=io_callable,
          action_cb=_make_pause_action(),
          phase_label="pause_flush")

    elif active_cuj == "warm_relaunch":
      for cycle in range(1, 4):
        _launch_cobalt_android(
            DEFAULT_HOME_URL, package=package, stop_first=True)
        _poll_for_duration(
            tracker,
            duration_sec=20.0,
            interval_sec=interval_sec,
            run_start_time=run_start,
            snapshot_fn=snap_callable,
            proc_io_fn=io_callable,
            phase_label=f"warm_relaunch_{cycle}")

  return time.time() - run_start


def parse_args(argv: Optional[List[str]] = None) -> argparse.Namespace:
  """Parses CLI arguments."""
  parser = argparse.ArgumentParser(
      description="Monitor Cobalt persistent storage across a CUJ.")
  parser.add_argument(
      "--platform",
      choices=SUPPORTED_PLATFORMS,
      default="android",
      help="Target platform (currently supports 'android').")
  parser.add_argument(
      "--package",
      type=str,
      default=DEFAULT_PACKAGE,
      help=("Android package name to monitor "
            f"(default: '{DEFAULT_PACKAGE}', or "
            "'com.google.android.youtube.tv' for Kimono)."))
  parser.add_argument(
      "--cuj",
      default="manual",
      choices=SUPPORTED_CUJS,
      help=("Critical User Journey to run and monitor "
            "(default: 'manual' passive monitoring until Ctrl+C)."))
  parser.add_argument(
      "--interval",
      type=float,
      default=1.0,
      help="Polling interval in seconds (default: 1.0).")
  parser.add_argument(
      "--output",
      type=str,
      default="",
      help="Output report file path (.md or .json).")
  return parser.parse_args(argv)


def main(argv: Optional[List[str]] = None) -> int:
  args = parse_args(argv)

  timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
  output_path = args.output or f"storage_report_{args.cuj}_{timestamp}.md"

  tracker = StorageTracker(package=args.package)
  start_time = time.time()
  try:
    duration_sec = run_cuj_android(
        cuj=args.cuj,
        interval_sec=args.interval,
        tracker=tracker,
        package=args.package)
  except KeyboardInterrupt:
    duration_sec = time.time() - start_time
    print("\nInterrupted by user; generating report from collected samples...")
  except RuntimeError as exc:
    print(f"Error: {exc}", file=sys.stderr)
    return 1

  report_data = tracker.build_summary_data(
      platform=args.platform,
      cuj=args.cuj,
      duration_sec=duration_sec,
      interval_sec=args.interval)

  if output_path.endswith(".json"):
    content = json.dumps(report_data, indent=2) + "\n"
  else:
    content = format_markdown_report(report_data)

  with open(output_path, "w", encoding="utf-8") as f:
    f.write(content)

  print(f"\nReport written to: {output_path}")
  return 0


if __name__ == "__main__":
  sys.exit(main())
