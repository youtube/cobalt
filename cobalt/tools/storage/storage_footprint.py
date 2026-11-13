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
"""Storage footprint by bucket and folder: timeline, findings, and reports.

Every file and directory is attributed to one storage bucket with the table
that the browser's footprint sampler also uses
(cobalt/browser/metrics/cobalt_storage_buckets.inc), so lab numbers match the
Cobalt.Storage.Footprint.* histograms. storage_monitor.py collects the
samples; this module writes them as JSONL and turns them into reports:

  storage_footprint.py report run.jsonl [--html out.html] [--markdown out.md]
  storage_footprint.py compare a.jsonl b.jsonl [--output diff.md]
"""

# Python 3.12+ tokenizes f-strings, so pylint reports the quotes inside
# f-string expressions as inconsistent.
# pylint: disable=inconsistent-quotes

import argparse
from dataclasses import dataclass, field
from datetime import datetime
import html
import json
import math
import os
import re
import sys
from typing import Any, Dict, Iterable, List, Optional, Sequence, Tuple

JSONL_VERSION = 1

BUCKETS_INC_PATH = os.path.normpath(
    os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "..", "..", "browser",
        "metrics", "cobalt_storage_buckets.inc"))

# Lab-only root and bucket: Android's device-encrypted app directory
# (/data/user_de/0/<package>), which holds the OS's GL and Skia shader caches.
# The browser has no path key for it, so it has no UMA histogram.
USER_DE = "UserDe"

# Display order of the roots. AppData contains UserData and DirCache; its
# group holds only the buckets outside them.
ROOT_ORDER = ("UserData", "DirCache", "AppData", USER_DE)
ROOT_TITLES = {
    "UserData": "UserData: persistent Chromium storage",
    "DirCache": "DirCache: cache directory (the OS may clear it)",
    "AppData": "AppData: the Java app and other app files",
    USER_DE: "UserDe: device-encrypted dir, OS shader caches (lab only)",
}

# Budgets that findings check: the 1 MB USER_DATA partner quota and
# kSbMaxSystemPathCacheDirectorySize (24 MiB) on Android.
BUDGETS_BYTES = {"UserData": 1 << 20, "DirCache": 24 << 20}

# A sample keeps the largest files, plus every SQLite companion file above
# this size, for the "largest files" and "SQLite journals" findings.
NOTABLE_FILE_COUNT = 25
NOTABLE_COMPANION_BYTES = 32 << 10

_SQLITE_COMPANIONS = ("-journal", "-wal", "-shm")
_BUCKET_RE = re.compile(r"COBALT_STORAGE_BUCKET\(\s*(\w+)\s*\)")
_PATH_RE = re.compile(
    r'COBALT_STORAGE_PATH\(\s*(\w+)\s*,\s*"([^"]*)"\s*,\s*(\w+)\s*\)')
_S_IFMT = 0o170000
_S_IFDIR = 0o040000
_S_IFREG = 0o100000


# ---------------------------------------------------------------------------
# Bucket table
# ---------------------------------------------------------------------------
@dataclass(frozen=True)
class BucketRule:
  root: str
  path: str
  bucket: str


def _matches_rule(rel_path: str, rule_path: str) -> bool:
  """Same matching as ClassifyStoragePath() in cobalt_storage_footprint.cc."""
  if not rule_path:
    return True
  if not rel_path.startswith(rule_path):
    return False
  rest = rel_path[len(rule_path):]
  return not rest or rest.startswith("/") or rest in _SQLITE_COMPANIONS


@dataclass(frozen=True)
class BucketTable:
  """The buckets and path rules of cobalt_storage_buckets.inc."""
  buckets: Tuple[str, ...]
  rules: Tuple[BucketRule, ...]

  def classify(self, root: str, rel_path: str) -> str:
    """Returns the bucket of |rel_path| ('/'-separated) under |root|."""
    if root == USER_DE:
      return USER_DE
    for rule in self.rules:
      if rule.root == root and _matches_rule(rel_path, rule.path):
        return rule.bucket
    raise ValueError(f"No bucket rule matches {root}:{rel_path}")

  def all_buckets(self) -> Tuple[str, ...]:
    """The buckets of the table plus the lab-only UserDe bucket."""
    return self.buckets + (USER_DE,)


def parse_bucket_table(text: str) -> BucketTable:
  """Parses the X-macro table; macros may span lines."""
  code = re.sub(r"//[^\n]*", "", text)
  buckets = tuple(_BUCKET_RE.findall(code))
  rules = tuple(BucketRule(*match) for match in _PATH_RE.findall(code))
  if not buckets or not rules:
    raise ValueError("No COBALT_STORAGE_BUCKET or COBALT_STORAGE_PATH found")
  if len(set(buckets)) != len(buckets):
    raise ValueError("Duplicate bucket names")
  for rule in rules:
    if rule.bucket not in buckets:
      raise ValueError(f"Rule for undeclared bucket {rule.bucket}")
  for root in {rule.root for rule in rules}:
    if not any(rule.root == root and not rule.path for rule in rules):
      raise ValueError(f"Root {root} has no catch-all rule")
  return BucketTable(buckets=buckets, rules=rules)


def load_bucket_table(path: str = BUCKETS_INC_PATH) -> BucketTable:
  with open(path, encoding="utf-8") as f:
    return parse_bucket_table(f.read())


# ---------------------------------------------------------------------------
# Collection and aggregation
# ---------------------------------------------------------------------------
@dataclass(frozen=True)
class StatEntry:
  """One file or directory from `stat -c '%f %s %b %B %Y %n'`."""
  path: str
  is_dir: bool
  is_file: bool
  size: int
  allocated: int
  mtime: int


def find_stat_command(dirs: Sequence[str]) -> str:
  """Lists every file and directory under |dirs| (the dirs included).

  Symbolic links are skipped, as in the browser's walk.
  """
  return (f"find {' '.join(dirs)} \\( -type f -o -type d \\) "
          "-exec stat -c '%f %s %b %B %Y %n' {} + 2>/dev/null")


def parse_stat_entries(output: str) -> List[StatEntry]:
  """Parses the output of find_stat_command()."""
  entries: List[StatEntry] = []
  for line in output.splitlines():
    parts = line.rstrip("\r").split(" ", 5)
    if len(parts) < 6:
      continue
    raw_mode, size, blocks, block_size, mtime, path = parts
    try:
      mode = int(raw_mode, 16)
      entries.append(
          StatEntry(
              path=path,
              is_dir=(mode & _S_IFMT) == _S_IFDIR,
              is_file=(mode & _S_IFMT) == _S_IFREG,
              size=int(size),
              allocated=int(blocks) * int(block_size),
              mtime=int(mtime)))
    except ValueError:
      continue
  return entries


@dataclass(frozen=True)
class RootSpec:
  """A measured root: its device path and its key in the folder tree."""
  name: str
  device_path: str
  rel: str


def android_roots(package: str) -> List[RootSpec]:
  """The roots of an Android app, with folder keys relative to AppData."""
  app_data = f"/data/data/{package}"
  return [
      RootSpec("AppData", app_data, ""),
      RootSpec("UserData", f"{app_data}/app_content_shell/content_shell",
               "app_content_shell/content_shell"),
      RootSpec("DirCache", f"{app_data}/cache", "cache"),
      RootSpec(USER_DE, f"/data/user_de/0/{package}", "user_de"),
  ]


def _is_under(rel: str, root_rel: str) -> bool:
  return (not root_rel and not rel.startswith("user_de")) or rel == root_rel \
      or rel.startswith(root_rel + "/")


def _folder_key(entry_path: str, roots: Sequence[RootSpec]) -> Optional[str]:
  """Returns the path of an entry in the folder tree, or None if outside."""
  for root in roots:
    if entry_path == root.device_path:
      return root.rel
    if entry_path.startswith(root.device_path + "/"):
      rest = entry_path[len(root.device_path) + 1:]
      if root.name == USER_DE:
        return f"user_de/{rest}"
      return f"{root.rel}/{rest}" if root.rel else rest
  return None


def _new_totals() -> List[int]:
  # allocated_bytes, logical_bytes, files, dirs
  return [0, 0, 0, 0]


def aggregate_footprint(entries: Iterable[StatEntry], roots: Sequence[RootSpec],
                        table: BucketTable) -> Dict[str, Any]:
  """Attributes entries to roots, buckets and folders.

  Like the browser's walk: an entry counts in every root that contains it
  and in the bucket of the innermost root; directories count with their own
  blocks. Folder totals include everything below the folder.
  """
  roots_by_depth = sorted((r for r in roots if r.name != "AppData"),
                          key=lambda r: -len(r.rel))
  app_data = next((r for r in roots if r.name == "AppData"), None)
  root_totals = {r.name: _new_totals() for r in roots}
  bucket_totals = {b: _new_totals() for b in table.all_buckets()}
  bucket_roots: Dict[str, Dict[str, int]] = {}
  folders: Dict[str, List[int]] = {}
  files: List[Tuple[str, int, int]] = []

  for entry in entries:
    key = _folder_key(entry.path, roots)
    if key is None:
      continue
    innermost = None
    containing = []
    for root in roots_by_depth:
      if (root.name == USER_DE and
          (key == "user_de" or key.startswith("user_de/"))) or (
              root.name != USER_DE and _is_under(key, root.rel)):
        containing.append(root)
        if innermost is None:
          innermost = root
    if not key.startswith("user_de") and app_data is not None:
      containing.append(app_data)
      if innermost is None:
        innermost = app_data
    if innermost is None:
      continue

    values = (entry.allocated, entry.size if entry.is_file else 0,
              1 if entry.is_file else 0, 1 if entry.is_dir else 0)
    for root in containing:
      _add(root_totals[root.name], values)
    if innermost.name == USER_DE:
      rel_in_root = key[len("user_de/"):] if key != "user_de" else ""
    elif innermost.rel:
      rel_in_root = key[len(innermost.rel) + 1:] if key != innermost.rel \
          else ""
    else:
      rel_in_root = key
    bucket = table.classify(innermost.name, rel_in_root)
    _add(bucket_totals[bucket], values)
    by_root = bucket_roots.setdefault(bucket, {})
    by_root[innermost.name] = by_root.get(innermost.name, 0) + entry.allocated

    folder_values = values[:3]
    parts = key.split("/") if key else []
    ancestors = [] if key.startswith("user_de") else [""]
    for i in range(1, len(parts) + (1 if entry.is_dir else 0)):
      ancestors.append("/".join(parts[:i]))
    for folder in ancestors:
      _add(folders.setdefault(folder, [0, 0, 0]), folder_values)
    if entry.is_file:
      files.append((key, entry.allocated, entry.size))

  files.sort(key=lambda f: (-f[1], f[0]))
  notable = files[:NOTABLE_FILE_COUNT]
  notable += [
      f for f in files[NOTABLE_FILE_COUNT:]
      if f[1] >= NOTABLE_COMPANION_BYTES and f[0].endswith(_SQLITE_COMPANIONS)
  ]
  # The main database of each notable journal or WAL, for comparison.
  by_path = {f[0]: f for f in files}
  notable_paths = {f[0] for f in notable}
  for path in list(notable_paths):
    if path.endswith(_SQLITE_COMPANIONS):
      main_db = re.sub(r"-(journal|wal|shm)$", "", path)
      if main_db in by_path and main_db not in notable_paths:
        notable.append(by_path[main_db])
        notable_paths.add(main_db)
  return {
      "roots": {
          name: _totals_dict(t) for name, t in root_totals.items()
      },
      "buckets": {
          name:
              dict(
                  _totals_dict(t),
                  root=max(bucket_roots[name], key=bucket_roots[name].get)
                  if name in bucket_roots else None)
          for name, t in bucket_totals.items()
      },
      "folders": folders,
      "files": [list(f) for f in notable],
  }


def _add(totals: List[int], values: Sequence[int]) -> None:
  for i, value in enumerate(values):
    totals[i] += value


def _totals_dict(totals: Sequence[int]) -> Dict[str, int]:
  return {
      "allocated_bytes": totals[0],
      "logical_bytes": totals[1],
      "files": totals[2],
      "dirs": totals[3],
  }


# ---------------------------------------------------------------------------
# JSONL timeline
# ---------------------------------------------------------------------------
class FootprintTimeline:
  """Writes the footprint timeline as JSONL, one record per line.

  Lifetime/Ownership: Owned by storage_monitor.main() for one run; the file
  is flushed after every record so an interrupted run keeps its data.
  Threading Model: Single-threaded.
  """

  def __init__(self, path: str, *, package: str, platform: str, cuj: str,
               interval_sec: float, extra_args: str, roots: Sequence[RootSpec],
               table: BucketTable):
    self.path = path
    self._roots = list(roots)
    self._table = table
    self._last_pid: Optional[int] = None
    self.sample_count = 0
    self.last_total_bytes = 0
    # Kept open for the run and closed by close().
    self._file = open(path, "w", encoding="utf-8")  # pylint: disable=consider-using-with
    self._write({
        "type": "header",
        "version": JSONL_VERSION,
        "package": package,
        "platform": platform,
        "cuj": cuj,
        "interval_sec": interval_sec,
        "extra_args": extra_args,
        "start_time": datetime.now().isoformat(timespec="seconds"),
        "roots": {
            r.name: r.device_path for r in self._roots
        },
        "buckets": list(table.all_buckets()),
        "budgets_bytes": BUDGETS_BYTES,
        "folder_fields": ["allocated_bytes", "logical_bytes", "files"],
        "file_fields": ["path", "allocated_bytes", "logical_bytes"],
    })

  def event(self, t: float, name: str, detail: str = "") -> None:
    self._write({
        "type": "event",
        "t": round(t, 2),
        "event": name,
        "detail": detail
    })

  def sample(self,
             t: float,
             entries: Sequence[StatEntry],
             pid: Optional[int] = None,
             io: Optional[Tuple[int, int]] = None) -> Dict[str, Any]:
    """Records one poll; |io| is (read_bytes, write_bytes) of |pid|."""
    if pid != self._last_pid:
      if self._last_pid is not None:
        self.event(t, "process_exit", f"pid {self._last_pid}")
      if pid is not None:
        self.event(t, "process_start", f"pid {pid}")
      self._last_pid = pid
    footprint = aggregate_footprint(entries, self._roots, self._table)
    record = {"type": "sample", "t": round(t, 2), "pid": pid}
    record["io"] = ({
        "read_bytes": io[0],
        "write_bytes": io[1]
    } if io is not None else None)
    record.update(footprint)
    self._write(record)
    self.sample_count += 1
    self.last_total_bytes = total_bytes(record)
    return record

  def close(self) -> None:
    self._file.close()

  def _write(self, record: Dict[str, Any]) -> None:
    self._file.write(json.dumps(record, separators=(",", ":")) + "\n")
    self._file.flush()


def total_bytes(sample: Dict[str, Any]) -> int:
  """Everything the app owns: AppData plus the lab-only UserDe."""
  return sum(sample["roots"].get(name, {}).get("allocated_bytes", 0)
             for name in ("AppData", USER_DE))


@dataclass
class Timeline:
  header: Dict[str, Any]
  events: List[Dict[str, Any]]
  samples: List[Dict[str, Any]]
  path: str = ""


def load_timeline(path: str) -> Timeline:
  header: Dict[str, Any] = {}
  events: List[Dict[str, Any]] = []
  samples: List[Dict[str, Any]] = []
  with open(path, encoding="utf-8") as f:
    for line in f:
      line = line.strip()
      if not line:
        continue
      try:
        record = json.loads(line)
      except json.JSONDecodeError:
        continue  # A truncated last line of an interrupted run.
      kind = record.get("type")
      if kind == "header":
        header = record
      elif kind == "event":
        events.append(record)
      elif kind == "sample":
        samples.append(record)
  if not samples:
    raise ValueError(f"{path} has no samples")
  return Timeline(header=header, events=events, samples=samples, path=path)


# ---------------------------------------------------------------------------
# Analysis: key moments, rows, findings
# ---------------------------------------------------------------------------
@dataclass
class Moment:
  label: str
  index: int


@dataclass
class Row:
  """A bucket, or a root group subtotal, across the samples."""
  name: str
  group: str
  series: List[int]
  files_end: int
  logical_end: int
  is_group: bool = False

  @property
  def peak(self) -> int:
    return max(self.series)

  @property
  def peak_index(self) -> int:
    return self.series.index(self.peak)

  @property
  def delta(self) -> int:
    return self.series[-1] - self.series[0]


@dataclass
class Finding:
  severity: str  # "high", "medium" or "info".
  title: str
  details: List[str] = field(default_factory=list)


@dataclass
class Analysis:
  timeline: Timeline
  times: List[float]
  moments: List[Moment]
  groups: List[Tuple[Row, List[Row]]]
  total: Row
  empty_buckets: List[str]
  findings: List[Finding]


def _bucket_group(timeline: Timeline, bucket: str) -> str:
  counts: Dict[str, int] = {}
  for sample in timeline.samples:
    root = sample["buckets"].get(bucket, {}).get("root")
    if root:
      counts[root] = counts.get(root, 0) + 1
  if counts:
    return max(counts, key=counts.get)
  return USER_DE if bucket == USER_DE else ""


def _phases(timeline: Timeline) -> List[Dict[str, Any]]:
  """The CUJs of a multi-CUJ run (--cuj all), else the phases of the CUJ."""
  cujs = [e for e in timeline.events if e["event"] == "cuj"]
  if len(cujs) >= 2:
    return cujs
  return [e for e in timeline.events if e["event"] == "cuj_phase"]


def _key_moments(timeline: Timeline, times: List[float]) -> List[Moment]:
  """Start, +10 s/+30 s after the first launch, phase ends, and the end."""

  def first_at_or_after(t: float) -> Optional[int]:
    for i, sample_t in enumerate(times):
      if sample_t >= t:
        return i
    return None

  def last_before(t: float) -> Optional[int]:
    index = None
    for i, sample_t in enumerate(times):
      if sample_t < t:
        index = i
    return index

  moments = [Moment("Start", 0)]
  starts = [e for e in timeline.events if e["event"] == "process_start"]
  # A process that is already running when monitoring starts is not a launch.
  launches = [e for e in starts if e["t"] > times[0]]
  if launches:
    t0 = launches[0]["t"]
    for offset, label in ((10, "+10 s"), (30, "+30 s")):
      index = first_at_or_after(t0 + offset)
      if index is not None and index != len(times) - 1:
        moments.append(Moment(f"{label} after launch", index))
  phases = _phases(timeline)
  for i, phase in enumerate(phases[:-1]):
    index = last_before(phases[i + 1]["t"])
    if index is not None and index > 0:
      moments.append(Moment(f"end of {phase['detail']}", index))
  moments.append(Moment("End", len(times) - 1))
  # Drop moments that point at the same sample as an earlier one.
  seen = set()
  unique = []
  for moment in sorted(moments, key=lambda m: m.index):
    if moment.index not in seen or moment.label == "End":
      if moment.label == "End" and moment.index in seen:
        unique = [m for m in unique if m.index != moment.index]
      unique.append(moment)
      seen.add(moment.index)
  return unique


def analyze(timeline: Timeline) -> Analysis:
  samples = timeline.samples
  times = [s["t"] for s in samples]
  buckets = timeline.header.get("buckets") or list(samples[0]["buckets"])
  rows_by_group: Dict[str, List[Row]] = {}
  empty = []
  for bucket in buckets:
    series = [
        s["buckets"].get(bucket, {}).get("allocated_bytes", 0) for s in samples
    ]
    if not any(series):
      empty.append(bucket)
      continue
    end = samples[-1]["buckets"].get(bucket, {})
    group = _bucket_group(timeline, bucket)
    rows_by_group.setdefault(group, []).append(
        Row(bucket, group, series, end.get("files", 0),
            end.get("logical_bytes", 0)))

  groups = []
  for group in ROOT_ORDER:
    rows = rows_by_group.get(group)
    if not rows:
      continue
    rows.sort(key=lambda r: (-r.peak, r.name))
    series = [sum(r.series[i] for r in rows) for i in range(len(samples))]
    subtotal = Row(
        group,
        group,
        series,
        sum(r.files_end for r in rows),
        sum(r.logical_end for r in rows),
        is_group=True)
    groups.append((subtotal, rows))
  total = Row(
      "Total",
      "", [total_bytes(s) for s in samples],
      sum(g.files_end for g, _ in groups),
      sum(g.logical_end for g, _ in groups),
      is_group=True)
  analysis = Analysis(
      timeline=timeline,
      times=times,
      moments=_key_moments(timeline, times),
      groups=groups,
      total=total,
      empty_buckets=empty,
      findings=[])
  analysis.findings = find_issues(analysis)
  return analysis


def _folder_bytes(sample: Dict[str, Any], folder: str) -> int:
  values = sample.get("folders", {}).get(folder)
  return values[0] if values else 0


def _top_children(sample: Dict[str, Any], folder: str,
                  limit: int) -> List[Tuple[str, int]]:
  prefix = folder + "/" if folder else ""
  children = []
  for path, values in sample.get("folders", {}).items():
    if path == folder or not path.startswith(prefix) or path == "user_de":
      continue
    if "/" not in path[len(prefix):]:
      children.append((path, values[0]))
  for path, allocated, _ in sample.get("files", []):
    if path.startswith(prefix) and "/" not in path[len(prefix):]:
      children.append((path, allocated))
  children.sort(key=lambda c: -c[1])
  return children[:limit]


def _drop_events(series: Sequence[int]) -> List[Tuple[int, int, int]]:
  """(bytes, index before, index after) of each drop of 256 KB and 20%.

  A drop is measured from the highest of the previous 5 samples, because a
  deletion can span polls (e.g. rename, then delete). Drops do not overlap.
  """
  events: List[Tuple[int, int, int]] = []
  start = 0
  for i in range(1, len(series)):
    window = range(max(start, i - 5), i)
    if not window:
      continue
    before = max(window, key=lambda j: series[j])
    drop = series[before] - series[i]
    if drop >= 256 << 10 and drop >= 0.2 * series[before]:
      if events and before == events[-1][2]:
        # Still falling from the previous drop: extend it.
        first = events[-1][1]
        events[-1] = (series[first] - series[i], first, i)
      else:
        events.append((drop, before, i))
      start = i
  return events


def find_issues(analysis: Analysis) -> List[Finding]:
  """Flags what usually needs attention in a storage footprint."""
  findings: List[Finding] = []
  samples = analysis.timeline.samples
  end = samples[-1]
  budgets = analysis.timeline.header.get("budgets_bytes", BUDGETS_BYTES)
  rows = {
      row.name: row for _, group_rows in analysis.groups for row in group_rows
  }

  # 1. Over budget.
  for root, budget in budgets.items():
    value = end["roots"].get(root, {}).get("allocated_bytes", 0)
    if value > budget:
      group_rows = next((r for g, r in analysis.groups if g.name == root), [])
      by_end = sorted(group_rows, key=lambda r: -r.series[-1])
      top = ", ".join(f"{r.name} {fmt_size(r.series[-1])}" for r in by_end[:3]
                      if r.series[-1])
      findings.append(
          Finding(
              "high", f"{root} is {fmt_size(value)} at the end, "
              f"{value / budget:.1f}x its {fmt_size(budget)} budget",
              [f"Largest buckets: {top}."] if top else []))

  # 2. Space that kCobaltStorageCleanup would reclaim.
  cleanup = rows.get("CleanupTargets")
  if cleanup and cleanup.series[-1] > 0:
    paths = _top_children(end, "app_content_shell/content_shell", 50)
    targets = [
        f"{p.rsplit('/', 1)[-1]} {fmt_size(b)}" for p, b in paths
        if _bucket_of_path(analysis, p) == "CleanupTargets" and b
    ][:6]
    findings.append(
        Finding(
            "medium", f"CleanupTargets holds {fmt_size(cleanup.series[-1])}"
            " that kCobaltStorageCleanup deletes", [
                f"Largest: {', '.join(targets)}." if targets else
                "Paths under UserData listed in cobalt_storage_buckets.inc.",
                "With the cleanup disabled (the default), some of these are "
                "in use, e.g. the GPU program cache in UserData/ShaderCache."
            ]))

  # 3. Large SQLite journals and write-ahead logs.
  companions: Dict[str, Tuple[int, float]] = {}
  for sample in samples:
    for path, allocated, _ in sample.get("files", []):
      if (path.endswith(_SQLITE_COMPANIONS) and
          allocated > companions.get(path, (0, 0.0))[0]):
        companions[path] = (allocated, sample["t"])
  end_files = {f[0]: f[1] for f in end.get("files", [])}
  for path, (peak, t) in sorted(companions.items(), key=lambda c: -c[1][0]):
    if peak < 256 << 10:
      continue
    name = path.rsplit("/", 1)[-1]
    at_end = end_files.get(path, 0)
    db = re.sub(r"-(journal|wal|shm)$", "", path)
    db_size = end_files.get(db)
    vs_db = (f", {at_end / db_size:.1f}x its database ({fmt_size(db_size)})"
             if db_size and at_end else "")
    if peak > 1.2 * at_end:
      title = (f"{name} peaked at {fmt_size(peak)} (t={t:.0f}s); "
               f"{fmt_size(at_end)} at the end{vs_db}")
    else:
      title = f"{name} is {fmt_size(at_end)}{vs_db}"
    findings.append(
        Finding("medium", title, [
            f"{path}. A WAL this large has not been checkpointed into "
            "the database; a large journal means a large transaction."
        ]))

  # 4. Growth over the run.
  duration_min = max((analysis.times[-1] - analysis.times[0]) / 60.0, 1e-9)
  growing = [
      r for r in rows.values()
      if r.delta >= 256 << 10 and r.delta >= 0.2 * max(r.series[0], 1)
  ]
  for row in sorted(growing, key=lambda r: -r.delta)[:5]:
    # When did 10% and 90% of the growth happen? A burst is described by
    # its window; steady growth by its rate.
    t_lo = next(analysis.times[i]
                for i, v in enumerate(row.series)
                if v >= row.series[0] + 0.1 * row.delta)
    t_hi = next(analysis.times[i]
                for i, v in enumerate(row.series)
                if v >= row.series[0] + 0.9 * row.delta)
    when = (f"mostly between t={t_lo:.0f}s and t={t_hi:.0f}s" if t_hi -
            t_lo < 30 *
            duration_min else f"{fmt_size(row.delta / duration_min)}/min")
    findings.append(
        Finding(
            "info", f"{row.name} grew by {fmt_size(row.delta)} "
            f"({fmt_size(row.series[0])} to {fmt_size(row.series[-1])}, "
            f"{when})"))

  # 5. Transient spikes (buckets that drop repeatedly are reported as churn
  # in 8).
  drop_events = {row.name: _drop_events(row.series) for row in rows.values()}
  spiked = set()
  for row in rows.values():
    spike = row.peak - max(row.series[0], row.series[-1])
    if spike >= 512 << 10 and len(drop_events[row.name]) < 2:
      spiked.add(row.name)
      findings.append(
          Finding(
              "info", f"{row.name} peaked {fmt_size(spike)} above its "
              f"start and end (t={analysis.times[row.peak_index]:.0f}s)", [
                  "Temporary files, e.g. LevelDB compaction or a SQLite "
                  "journal, that were deleted before the end."
              ]))

  # 6. Space no bucket names.
  for row in rows.values():
    if row.name.startswith("Other") and row.series[-1] >= 256 << 10:
      folder = {
          "OtherUserData": "app_content_shell/content_shell",
          "OtherCache": "cache",
          "OtherAppData": ""
      }.get(row.name)
      top = []
      if folder is not None:
        for path, size in _top_children(end, folder, 20):
          if _bucket_of_path(analysis, path) == row.name and size:
            top.append(f"{path} {fmt_size(size)}")
      findings.append(
          Finding(
              "info", f"{row.name}: {fmt_size(row.series[-1])} not "
              "attributed to a named bucket", [
                  f"Largest: {', '.join(top[:5])}." if top else "",
                  "Add a rule to cobalt_storage_buckets.inc if it matters."
              ]))

  # 7. Block and inode overhead of small files.
  for row in rows.values():
    allocated = row.series[-1]
    if (allocated - row.logical_end >= 512 << 10 and
        allocated >= 1.5 * max(row.logical_end, 1)):
      findings.append(
          Finding(
              "info", f"{row.name}: {fmt_size(allocated)} allocated for "
              f"{fmt_size(row.logical_end)} of data in {row.files_end} "
              "files", [
                  "Small files each take at least one 4 KB block, plus an "
                  "inode block on f2fs."
              ]))

  # 8. Large drops of buckets: one drop, or repeated drops (churn).
  launches = [
      e["t"]
      for e in analysis.timeline.events
      if e["event"] == "process_start" and e["t"] > analysis.times[0]
  ]
  reported = []
  for row in rows.values():
    events = drop_events[row.name]
    if (len(events) == 1 and row.name in spiked and
        abs(events[0][1] - row.peak_index) <= 5):
      continue  # The end of the transient spike reported above.
    if events:
      reported.append((max(e[0] for e in events), row, events))
  for _, row, events in sorted(reported, key=lambda r: -r[0])[:3]:
    drop, before, i = max(events)
    t = analysis.times[i]
    if (row.name == "CleanupTargets" and
        any(28 <= t - start <= 45 for start in launches)):
      cause = ("About 30 s after a launch: the delayed storage cleanup "
               "(kCobaltStorageCleanup).")
    elif row.group == "DirCache":
      cause = ("Files deleted from the cache: an eviction, or the OS "
               "trimming it.")
    else:
      cause = "Files deleted, e.g. by the storage cleanup or a data clear."
    if len(events) == 1:
      title = (f"{row.name} dropped by {fmt_size(drop)} at t={t:.0f}s "
               f"({fmt_size(row.series[before])} to "
               f"{fmt_size(row.series[i])})")
      details = [cause]
    else:
      times = ", ".join(f"{analysis.times[e[2]]:.0f}s" for e in events[:6])
      title = (f"{row.name} dropped {len(events)} times, by up to "
               f"{fmt_size(drop)} (t={times})")
      details = [
          "Data deleted and written again (churn), which costs flash "
          "writes on every cycle.", cause
      ]
    findings.append(Finding("info", title, details))

  for finding in findings:
    finding.details = [d for d in finding.details if d]
  order = {"high": 0, "medium": 1, "info": 2}
  findings.sort(key=lambda f: order[f.severity])
  return findings


def _bucket_of_path(analysis: Analysis, folder_path: str) -> str:
  """The bucket of a folder or file path (relative to AppData)."""
  table = _table_for(analysis)
  if folder_path == "user_de" or folder_path.startswith("user_de/"):
    return USER_DE
  for root_name, rel in (("UserData", "app_content_shell/content_shell"),
                         ("DirCache", "cache")):
    if folder_path == rel:
      return table.classify(root_name, "")
    if folder_path.startswith(rel + "/"):
      return table.classify(root_name, folder_path[len(rel) + 1:])
  return table.classify("AppData", folder_path)


_TABLE_CACHE: Dict[str, BucketTable] = {}


def _table_for(analysis: Analysis) -> BucketTable:
  del analysis  # The table of this checkout is the reference.
  if "default" not in _TABLE_CACHE:
    _TABLE_CACHE["default"] = load_bucket_table()
  return _TABLE_CACHE["default"]


# ---------------------------------------------------------------------------
# Formatting helpers
# ---------------------------------------------------------------------------
def fmt_size(num_bytes: float) -> str:
  """Human-readable size; KB and MB are 1024-based, as in UMA."""
  if num_bytes == 0:
    return "0"
  sign = "-" if num_bytes < 0 else ""
  value = abs(num_bytes)
  if value < 1024:
    return f"{sign}{value:.0f} B"
  if value < 1 << 20:
    return f"{sign}{value / 1024:.0f} KB"
  if value < 10 << 20:
    return f"{sign}{value / (1 << 20):.2f} MB"
  return f"{sign}{value / (1 << 20):.1f} MB"


def fmt_delta(num_bytes: float) -> str:
  if num_bytes == 0:
    return "0"
  return ("+" if num_bytes > 0 else "") + fmt_size(num_bytes)


def _count(number: int, noun: str) -> str:
  return f"{number} {noun}" + ("" if number == 1 else "s")


# ---------------------------------------------------------------------------
# Markdown
# ---------------------------------------------------------------------------
def format_markdown(analysis: Analysis, html_name: str = "") -> str:
  """The footprint section of the Markdown report."""
  lines = [
      "## Storage Footprint by Bucket",
      "",
      ("Allocated disk space (`st_blocks * 512`; KB and MB are 1024-based) "
       "per storage bucket, shared with the "
       "`Cobalt.Storage.Footprint.*` UMA histograms "
       "(`cobalt/browser/metrics/cobalt_storage_buckets.inc`)." +
       (f" Charts and the folder tree: `{html_name}`." if html_name else "")),
      "",
      "### Findings",
      "",
  ]
  if analysis.findings:
    for finding in analysis.findings:
      lines.append(f"- **[{finding.severity.upper()}]** {finding.title}.")
      for detail in finding.details:
        lines.append(f"  - {detail}")
  else:
    lines.append("- No issues found.")

  moments = analysis.moments
  header = ["Bucket"] + [
      f"{m.label} (t={analysis.times[m.index]:.0f}s)" for m in moments
  ] + ["Peak", "Change", "Files"]
  lines += [
      "",
      "### Footprint at key moments",
      "",
      "| " + " | ".join(header) + " |",
      "| :-- | " + " | ".join(["--:"] * (len(header) - 1)) + " |",
  ]
  for subtotal, rows in analysis.groups:
    lines.append(_markdown_row(analysis, subtotal, bold=True))
    for row in rows:
      lines.append(_markdown_row(analysis, row))
  lines.append(_markdown_row(analysis, analysis.total, bold=True))
  if analysis.empty_buckets:
    lines += [
        "", "Empty buckets: " +
        ", ".join(f"`{b}`" for b in analysis.empty_buckets) + "."
    ]

  lines += [
      "",
      "### Largest folders at the end (by files directly inside)",
      "",
      "| Folder | Own files | Whole folder | Change | Files | Bucket |",
      "| :-- | --: | --: | --: | --: | :-- |",
  ]
  for folder in largest_folders(analysis, 12):
    lines.append(
        f"| `{folder['path'] or '.'}` | {fmt_size(folder['own'])} | "
        f"{fmt_size(folder['total'])} | {fmt_delta(folder['delta'])} | "
        f"{folder['files']} | {folder['bucket']} |")
  lines += [
      "",
      "### Largest files at the end",
      "",
      "| File | Allocated | Change | Bucket |",
      "| :-- | --: | --: | :-- |",
  ]
  for path, allocated, change in largest_files(analysis, 10):
    lines.append(f"| `{path}` | {fmt_size(allocated)} | {change} | "
                 f"`{_bucket_of_path(analysis, path)}` |")
  lines.append("")
  return "\n".join(lines)


def _markdown_row(analysis: Analysis, row: Row, bold: bool = False) -> str:
  name = row.name
  if row.is_group and row.name in ROOT_TITLES:
    budget = BUDGETS_BYTES.get(row.name)
    name = ROOT_TITLES[row.name] + (f" (budget {fmt_size(budget)})"
                                    if budget else "")
  cells = [fmt_size(row.series[m.index]) for m in analysis.moments]
  peak, peak_t = _peak_text(analysis, row)
  peak = f"{peak} ({peak_t})" if peak_t else peak
  values = cells + [peak, fmt_delta(row.delta), str(row.files_end)]
  if bold:
    name = f"**{name}**"
    values = [f"**{v}**" for v in values]
  else:
    name = f"`{name}`"
  return "| " + " | ".join([name] + values) + " |"


_ROOT_FOLDERS = ("", "app_content_shell/content_shell", "cache", "user_de")


def largest_folders(analysis: Analysis, limit: int) -> List[Dict[str, Any]]:
  """Folders ranked by the size of the files directly inside them."""
  end = analysis.timeline.samples[-1]
  start = analysis.timeline.samples[0]
  folders = end.get("folders", {})
  result = []
  for path, values in folders.items():
    children = sum(v[0] for p, v in folders.items() if _is_child(p, path))
    result.append({
        "path":
            path,
        "own":
            values[0] - children,
        "total":
            values[0],
        "delta":
            values[0] - _folder_bytes(start, path),
        "files":
            values[2],
        # Files at the top of a root belong to several buckets.
        "bucket": ("several" if path in _ROOT_FOLDERS else
                   f"`{_bucket_of_path(analysis, path)}`"),
    })
  result.sort(key=lambda f: (-f["own"], f["path"]))
  return result[:limit]


def largest_files(analysis: Analysis, limit: int) -> List[Tuple[str, int, str]]:
  """(path, allocated bytes, change since the start) of the largest files."""
  start_files = {
      f[0]: f[1] for f in analysis.timeline.samples[0].get("files", [])
  }
  result = []
  for path, allocated, _ in analysis.timeline.samples[-1].get("files",
                                                              [])[:limit]:
    change = (
        fmt_delta(allocated -
                  start_files[path]) if path in start_files else "new")
    result.append((path, allocated, change))
  return result


def _is_child(path: str, parent: str) -> bool:
  if parent == "":
    return path != "" and "/" not in path and path != "user_de"
  return path.startswith(parent + "/") and "/" not in path[len(parent) + 1:]


# ---------------------------------------------------------------------------
# HTML
# ---------------------------------------------------------------------------
_PALETTES = {
    "UserData": ("#1a73e8", "#7baaf7", "#a142f4", "#5c6bc0", "#c58af9",
                 "#0d47a1", "#4285f4", "#9fa8da", "#3949ab", "#d7aefb"),
    "DirCache": ("#e8710a", "#fcad70", "#d93025", "#b06000", "#f6aea9",
                 "#f9ab00", "#ee675c", "#fdd663"),
    "AppData":
        ("#188038", "#81c995", "#129eaf", "#34a853", "#78d9ec", "#0b8043"),
    USER_DE: ("#9aa0a6",),
}
_EVENT_COLORS = {
    "launch": "#2ca02c",
    "home": "#ff7f0e",
    "process_start": "#1f77b4",
    "process_exit": "#d62728",
    "cuj_phase": "#555555",
}

_CSS = """
body{font-family:Roboto,Arial,sans-serif;margin:24px;color:#202124;max-width:1500px}
h1{font-size:22px;margin-bottom:4px}h2{font-size:18px;margin-top:28px;
border-bottom:1px solid #dadce0;padding-bottom:4px}
.meta{color:#5f6368;font-size:13px}
.cards{display:flex;gap:12px;flex-wrap:wrap;margin:14px 0}
.card{border:1px solid #dadce0;border-radius:8px;padding:10px 14px;min-width:150px}
.card .v{font-size:20px;font-weight:500}.card .l{color:#5f6368;font-size:12px}
.card.over{border-color:#d93025;background:#fce8e6}
ul.findings{list-style:none;padding:0}ul.findings li{margin:6px 0;padding:8px 10px;
border-radius:6px;background:#f8f9fa}
.sev{display:inline-block;font-size:11px;font-weight:700;padding:1px 6px;
border-radius:4px;margin-right:8px;color:#fff}
.sev.high{background:#d93025}.sev.medium{background:#e37400}.sev.info{background:#5f6368}
ul.findings li ul{margin:4px 0 0 0;color:#3c4043;font-size:13px}
table{border-collapse:collapse;font-size:13px}
th,td{padding:4px 8px;border-bottom:1px solid #eee;text-align:right;white-space:nowrap}
th{background:#f1f3f4;position:sticky;top:0}
td.name,th.name{text-align:left}
th.m{white-space:normal;min-width:62px;max-width:92px}
tr.group td{background:#e8f0fe;font-weight:600}tr.total td{background:#f1f3f4;font-weight:700}
td.up{color:#c5221f}td.down{color:#188038}
.swatch{display:inline-block;width:10px;height:10px;margin-right:6px;border-radius:2px}
.bar{display:inline-block;height:8px;background:#8ab4f8;vertical-align:middle;margin-right:6px}
.barbox{display:inline-block;width:126px}
details{margin-left:18px}details>summary{cursor:pointer;list-style:none}
details>summary::before{content:'\\25B8 ';color:#5f6368}details[open]>summary::before{content:'\\25BE '}
.leaf{margin-left:30px}.tree span.size{display:inline-block;width:80px;text-align:right}
.tree span.delta{display:inline-block;width:80px;text-align:right}
.tree span.tag{color:#5f6368;font-size:12px;margin-left:8px}
.tree .row{font-size:13px;line-height:20px;white-space:nowrap}
.note{color:#5f6368;font-size:12px}
.tree .peak{color:#b06000}.tree .gone{color:#80868b}
.scroll{overflow-x:auto}
"""


def format_html(analysis: Analysis) -> str:
  """A self-contained HTML report (no scripts or external resources)."""
  header = analysis.timeline.header
  samples = analysis.timeline.samples
  title = (f"Storage footprint: {header.get('package', '')} "
           f"({header.get('cuj', '')})")
  parts = [
      "<!DOCTYPE html><html><head><meta charset='utf-8'>",
      f"<title>{html.escape(title)}</title><style>{_CSS}</style></head><body>",
      f"<h1>{html.escape(title)}</h1>",
      "<div class='meta'>" + html.escape(
          f"Started {header.get('start_time', '?')}, "
          f"{analysis.times[-1] - analysis.times[0]:.0f} s, "
          f"{len(samples)} samples every {header.get('interval_sec', '?')} s" +
          (f", extra args: {header['extra_args']}" if header.get("extra_args")
           else "") + f". Source: {os.path.basename(analysis.timeline.path)}") +
      "</div>",
      _html_cards(analysis),
      ("<div class='meta'><a href='#findings'>Findings</a> &middot; "
       "<a href='#moments'>Key moments</a> &middot; "
       "<a href='#chart'>Over time</a> &middot; "
       "<a href='#tree'>Folders</a> &middot; "
       "<a href='#files'>Files</a></div>"),
      "<h2 id='findings'>Findings</h2>",
      _html_findings(analysis),
      "<h2 id='moments'>Footprint at key moments</h2>",
      "<div class='scroll'>" + _html_moments_table(analysis) + "</div>",
      ("<p class='note'>Allocated disk space (st_blocks &times; 512, what "
       "<code>du</code> and Android count); KB/MB are 1024-based. Rows are "
       "the buckets of <code>cobalt_storage_buckets.inc</code>, the same "
       "as <code>Cobalt.Storage.Footprint.{Phase}.Bucket.{Bucket}.SizeKB"
       "</code> (UMA KB = bytes // 1024). Group rows hold only the buckets "
       "classified under that root; the total is everything the app owns."
       "</p>"),
      "<h2 id='chart'>Footprint over time</h2>",
      _html_area_chart(analysis),
      _html_io_chart(analysis),
      "<h2 id='tree'>Folders at the end</h2>",
      ("<p class='note'>Whole-folder size (including subfolders), change "
       "since the start, file count and bucket. <span style='color:#b06000'>"
       "Peak</span>: the largest size during the run, when well above the "
       "end. <s>Struck out</s>: deleted during the run. Folders under 0.5% "
       "of their parent are folded.</p>"),
      "<div class='tree'>" + _html_tree(analysis) + "</div>",
      "<h2 id='files'>Largest files at the end</h2>",
      _html_files_table(analysis),
      "</body></html>",
  ]
  return "\n".join(parts)


def _html_cards(analysis: Analysis) -> str:
  end = analysis.timeline.samples[-1]
  cards = [("Total at end", fmt_size(analysis.total.series[-1]),
            f"{fmt_delta(analysis.total.delta)} since start", False),
           ("Peak", fmt_size(analysis.total.peak),
            f"t={analysis.times[analysis.total.peak_index]:.0f}s", False)]
  for root in ("UserData", "DirCache"):
    value = end["roots"].get(root, {}).get("allocated_bytes", 0)
    budget = BUDGETS_BYTES[root]
    cards.append(
        (f"{root} at end", fmt_size(value),
         f"{100 * value / budget:.0f}% of {fmt_size(budget)} budget", value
         > budget))
  launches = sum(
      1 for e in analysis.timeline.events if e["event"] == "process_start")
  cards.append(("Processes", str(launches), "process starts seen", False))
  out = ["<div class='cards'>"]
  for label, value, sub, over in cards:
    out.append(f"<div class='card{' over' if over else ''}'>"
               f"<div class='l'>{html.escape(label)}</div>"
               f"<div class='v'>{html.escape(value)}</div>"
               f"<div class='l'>{html.escape(sub)}</div></div>")
  out.append("</div>")
  return "".join(out)


def _html_findings(analysis: Analysis) -> str:
  if not analysis.findings:
    return "<p>No issues found.</p>"
  out = ["<ul class='findings'>"]
  for finding in analysis.findings:
    out.append(f"<li><span class='sev {finding.severity}'>"
               f"{finding.severity.upper()}</span>"
               f"{html.escape(finding.title)}")
    if finding.details:
      out.append("<ul>" + "".join(
          f"<li>{html.escape(d)}</li>" for d in finding.details) + "</ul>")
    out.append("</li>")
  out.append("</ul>")
  return "".join(out)


def _sparkline(series: Sequence[int],
               width: int = 120,
               height: int = 22) -> str:
  if min(series) == max(series):
    return (f"<svg width='{width}' height='{height}'><line x1='1' x2='"
            f"{width - 1}' y1='{height / 2}' y2='{height / 2}' "
            "stroke='#bdc1c6' stroke-width='1.3'/></svg>")
  peak = max(series)
  n = len(series)
  points = " ".join(f"{(i / max(n - 1, 1)) * (width - 2) + 1:.1f},"
                    f"{height - 2 - (v / peak) * (height - 4):.1f}"
                    for i, v in enumerate(series))
  return (f"<svg width='{width}' height='{height}'>"
          f"<polyline fill='none' stroke='#1a73e8' stroke-width='1.3' "
          f"points='{points}'/></svg>")


def _colors(analysis: Analysis) -> Dict[str, str]:
  colors = {}
  for subtotal, rows in analysis.groups:
    palette = _PALETTES.get(subtotal.name, _PALETTES[USER_DE])
    for i, row in enumerate(rows):
      colors[row.name] = palette[i % len(palette)]
  return colors


def _peak_text(analysis: Analysis, row: Row) -> Tuple[str, str]:
  """The peak, and its time if it is above both the start and the end."""
  if row.peak > max(row.series[0], row.series[-1]):
    return fmt_size(row.peak), f"t={analysis.times[row.peak_index]:.0f}s"
  return fmt_size(row.peak), ""


def _html_moments_table(analysis: Analysis) -> str:
  colors = _colors(analysis)
  total_end = max(analysis.total.series[-1], 1)
  head = ["<th class='name'>Bucket</th>"] + [
      f"<th class='m'>{html.escape(m.label)}<br><span class='note'>"
      f"t={analysis.times[m.index]:.0f}s</span></th>" for m in analysis.moments
  ] + [
      "<th>Peak</th><th>Change</th><th>Trend</th>"
      "<th class='name'>Share at end</th><th>Files</th>"
  ]
  out = ["<table><tr>" + "".join(head) + "</tr>"]

  def row_html(row: Row, css: str, label: str) -> str:
    cells = "".join(
        f"<td>{fmt_size(row.series[m.index])}</td>" for m in analysis.moments)
    delta_css = ("up" if row.delta >= 256 << 10 else
                 "down" if row.delta <= -(256 << 10) else "")
    share = row.series[-1] / total_end
    peak, peak_t = _peak_text(analysis, row)
    return (f"<tr class='{css}'><td class='name'>{label}</td>{cells}"
            f"<td>{peak} <span class='note'>{peak_t}</span></td>"
            f"<td class='{delta_css}'>{fmt_delta(row.delta)}</td>"
            f"<td>{_sparkline(row.series)}</td>"
            f"<td class='name'><span class='bar' style='width:"
            f"{max(1, int(share * 60))}px'></span>{100 * share:.1f}%</td>"
            f"<td>{row.files_end}</td></tr>")

  for subtotal, rows in analysis.groups:
    budget = BUDGETS_BYTES.get(subtotal.name)
    title = ROOT_TITLES.get(subtotal.name, subtotal.name)
    description = title.split(": ", 1)[-1] if ": " in title else ""
    if budget:
      description += f", budget {fmt_size(budget)}"
    label = (f"{html.escape(subtotal.name)}<br><span class='note'>"
             f"{html.escape(description)}</span>")
    out.append(row_html(subtotal, "group", label))
    for row in rows:
      swatch = (f"<span class='swatch' style='background:"
                f"{colors[row.name]}'></span>")
      out.append(row_html(row, "", swatch + html.escape(row.name)))
  out.append(row_html(analysis.total, "total", "Total"))
  out.append("</table>")
  if analysis.empty_buckets:
    out.append("<p class='note'>Empty buckets (0 bytes throughout): " +
               html.escape(", ".join(analysis.empty_buckets)) + ".</p>")
  return "".join(out)


def _nice_step(span: float, target: int) -> float:
  raw = span / max(target, 1)
  magnitude = 10**math.floor(math.log10(raw)) if raw > 0 else 1
  for factor in (1, 2, 5, 10):
    if raw <= factor * magnitude:
      return factor * magnitude
  return 10 * magnitude


def _html_area_chart(analysis: Analysis) -> str:
  width, height = 1200, 380
  left, right, top, bottom = 60, 250, 40, 30
  plot_w, plot_h = width - left - right, height - top - bottom
  times = analysis.times
  t0, t1 = times[0], max(times[-1], times[0] + 1e-6)
  colors = _colors(analysis)
  max_total = max(analysis.total.peak, 1)
  # Stacked by root group, UserData at the bottom; within a group the largest
  # bucket is lowest. Buckets under 1% of the peak total share one band.
  layers: List[Tuple[str, List[int], str, str]] = []
  small: List[Row] = []
  for subtotal, rows in analysis.groups:
    for row in rows:
      if row.peak < 0.01 * max_total:
        small.append(row)
      else:
        layers.append((row.name, row.series, colors[row.name], subtotal.name))
  if small:
    layers.append((f"{len(small)} small buckets",
                   [sum(r.series[i] for r in small) for i in range(len(times))
                   ], "#dadce0", ""))
  y_max_mb = max_total / (1 << 20) * 1.05

  def x(t: float) -> float:
    return left + (t - t0) / (t1 - t0) * plot_w

  def y(num_bytes: float) -> float:
    return top + plot_h - (num_bytes / (1 << 20)) / y_max_mb * plot_h

  out = [
      f"<svg width='{width}' height='{height}' "
      "style='font-size:11px;font-family:Arial'>"
  ]
  # Phase bands (CUJs in a multi-CUJ run), labeled when the label fits.
  phases = _phases(analysis.timeline)
  for i, phase in enumerate(phases):
    start = max(phase["t"], t0)
    end = phases[i + 1]["t"] if i + 1 < len(phases) else t1
    band_w = max(x(end) - x(start), 0)
    out.append(f"<rect x='{x(start):.1f}' y='{top}' width='{band_w:.1f}' "
               f"height='{plot_h}' fill='{'#f1f3f4' if i % 2 else '#fff'}'>"
               f"<title>{html.escape(phase['detail'])}: t={start:.0f}s to "
               f"{end:.0f}s</title></rect>")
    if len(phases) > 1 and band_w >= 6 * len(phase["detail"]) + 6:
      out.append(f"<text x='{x(start) + 3:.1f}' y='{top + 12}' fill='#3c4043' "
                 "stroke='#fff' stroke-width='3' paint-order='stroke'>"
                 f"{html.escape(phase['detail'])}</text>")
  # Stacked areas, largest at the bottom.
  baseline = [0] * len(times)
  for name, series, color, _ in layers:
    upper = [baseline[i] + series[i] for i in range(len(times))]
    points = [f"{x(times[i]):.1f},{y(upper[i]):.1f}" for i in range(len(times))]
    points += [
        f"{x(times[i]):.1f},{y(baseline[i]):.1f}"
        for i in reversed(range(len(times)))
    ]
    out.append(f"<polygon points='{' '.join(points)}' fill='{color}' "
               f"fill-opacity='0.9' stroke='none'><title>"
               f"{html.escape(name)}: {fmt_size(series[-1])} at the end, peak "
               f"{fmt_size(max(series))}</title></polygon>")
    baseline = upper
  # Axes.
  step = _nice_step(y_max_mb, 6)
  value = 0.0
  while value <= y_max_mb + 1e-9:
    yy = top + plot_h - value / y_max_mb * plot_h
    out.append(f"<line x1='{left}' x2='{left + plot_w}' y1='{yy:.1f}' "
               f"y2='{yy:.1f}' stroke='#e0e0e0'/>"
               f"<text x='{left - 6}' y='{yy + 4:.1f}' text-anchor='end'>"
               f"{value:g} MB</text>")
    value += step
  t_step = _nice_step(t1 - t0, 10)
  tick = math.ceil(t0 / t_step) * t_step
  while tick <= t1 + 1e-9:
    out.append(f"<text x='{x(tick):.1f}' y='{top + plot_h + 16}' "
               f"text-anchor='middle'>{tick:g}s</text>")
    tick += t_step
  # Events: every event is a dashed line with a tooltip. Launches, HOME
  # presses, and process starts/exits that no launch explains (a crash, or
  # the OS killing the app) get a label, in the first row without overlap.
  launch_times = [
      e["t"] for e in analysis.timeline.events if e["event"] == "launch"
  ]
  row_ends = [-1e9, -1e9, -1e9]
  for event in analysis.timeline.events:
    if event["event"] in ("cuj_phase", "cuj"):
      continue
    color = _EVENT_COLORS.get(event["event"], "#999")
    xx = x(min(max(event["t"], t0), t1))
    out.append(f"<line x1='{xx:.1f}' x2='{xx:.1f}' y1='{top}' "
               f"y2='{top + plot_h}' stroke='{color}' stroke-dasharray='4,3'>"
               f"<title>{html.escape(event['event'])} "
               f"{html.escape(event.get('detail', ''))} "
               f"t={event['t']:.1f}s</title></line>")
    from_launch = any(0 <= event["t"] - t <= 15 for t in launch_times)
    if event["event"] not in ("launch", "home") and from_launch:
      continue
    label_w = 6 * len(event["event"])
    # Labels that would run past the plot go to the left of the marker.
    label_x = xx - 2 - label_w if xx + label_w > left + plot_w else xx + 2
    row = next((r for r, end in enumerate(row_ends) if label_x > end + 4), None)
    if row is None:
      continue
    row_ends[row] = label_x + label_w
    out.append(f"<text x='{label_x:.1f}' y='{top - 26 + 11 * row}' "
               f"fill='{color}'>{html.escape(event['event'])}</text>")
  # Legend, top of the stack first, with a header per root group.
  ly = top - 4
  group = None
  for name, series, color, layer_group in reversed(layers):
    if layer_group != group:
      group = layer_group
      if group:
        out.append(f"<text x='{left + plot_w + 16}' y='{ly + 9}' "
                   f"font-weight='bold' fill='#3c4043'>{group}</text>")
        ly += 14
    out.append(f"<rect x='{left + plot_w + 16}' y='{ly}' width='10' "
               f"height='10' fill='{color}'/><text x='{left + plot_w + 30}' "
               f"y='{ly + 9}'>{html.escape(name)} ({fmt_size(series[-1])})"
               "</text>")
    ly += 14
  out.append("</svg>")
  return "".join(out)


def _io_rates(
    samples: List[Dict[str, Any]]) -> List[Tuple[float, float, float]]:
  """(t, read KB/s, write KB/s) from /proc/<pid>/io deltas."""
  rates = []
  previous = None
  for sample in samples:
    io = sample.get("io")
    if io is None:
      previous = None
      continue
    if previous is not None:
      dt = sample["t"] - previous["t"]
      same = (
          sample["pid"] == previous["pid"] and
          io["read_bytes"] >= previous["io"]["read_bytes"] and
          io["write_bytes"] >= previous["io"]["write_bytes"])
      base_r = previous["io"]["read_bytes"] if same else 0
      base_w = previous["io"]["write_bytes"] if same else 0
      if dt > 0:
        rates.append((sample["t"], (io["read_bytes"] - base_r) / 1024 / dt,
                      (io["write_bytes"] - base_w) / 1024 / dt))
    previous = sample
  return rates


def _html_io_chart(analysis: Analysis) -> str:
  rates = _io_rates(analysis.timeline.samples)
  if not rates:
    return ("<p class='note'>No block I/O lane: /proc/&lt;pid&gt;/io was not "
            "readable or the app was not running.</p>")
  # Same x axis as _html_area_chart(); writes and reads have their own scale,
  # because startup reads (the library and APK) dwarf storage writes.
  width, lane_h, gap = 1200, 56, 22
  left, right, top = 60, 250, 18
  plot_w = width - left - right
  height = top + 2 * (lane_h + gap)
  t0, t1 = analysis.times[0], max(analysis.times[-1], analysis.times[0] + 1e-6)

  def x(t: float) -> float:
    return left + (t - t0) / (t1 - t0) * plot_w

  out = [
      f"<svg width='{width}' height='{height}' "
      "style='font-size:11px;font-family:Arial'>",
      f"<text x='{left}' y='10' fill='#555'>Block I/O of the app process "
      "(/proc/&lt;pid&gt;/io), each lane scaled to its own peak</text>"
  ]
  for lane, (index, color, label) in enumerate(
      ((2, "#d93025", "write"), (1, "#1a73e8", "read"))):
    lane_top = top + gap + lane * (lane_h + gap)
    base = lane_top + lane_h
    peak_rate = max(rates, key=lambda r, i=index: r[i])
    peak = peak_rate[index] or 1.0
    points = " ".join(
        f"{x(r[0]):.1f},{base - r[index] / peak * lane_h:.1f}" for r in rates)
    out.append(f"<polyline fill='none' stroke='{color}' stroke-width='1.2' "
               f"points='{points}'/>"
               f"<line x1='{left}' x2='{left + plot_w}' y1='{base}' "
               f"y2='{base}' stroke='#999'/>"
               f"<text x='{left - 6}' y='{base}' text-anchor='end' "
               f"fill='{color}'>{label}</text>"
               f"<text x='{left + plot_w + 16}' y='{lane_top + 10}' "
               f"fill='{color}'>{label}: peak {peak_rate[index]:.0f} KB/s "
               f"at t={peak_rate[0]:.0f}s</text>")
  out.append("</svg>")
  return "".join(out)


_ROOT_LABELS = {
    "": "AppData root",
    "app_content_shell/content_shell": "UserData root",
    "cache": "DirCache root",
    "user_de": "UserDe root (lab only)",
}


def _folder_label(analysis: Analysis, folders: Dict[str, List[int]],
                  path: str) -> str:
  """The bucket of a folder, its root name, or 'several buckets'."""
  if path in _ROOT_LABELS:
    return _ROOT_LABELS[path]
  bucket = _bucket_of_path(analysis, path)
  for child in folders:
    if _is_child(child, path) and (child in _ROOT_LABELS or
                                   _bucket_of_path(analysis, child) != bucket):
      return "several buckets"
  return bucket


@dataclass
class _FolderHistory:
  peak: int = -1
  peak_t: float = 0.0
  last: int = 0  # The size in the last sample that had the folder.
  gone_t: Optional[float] = None  # The first sample without it after that.


def _folder_histories(
    samples: List[Dict[str, Any]]) -> Dict[str, _FolderHistory]:
  """The peak of every folder during the run, and when it disappeared."""
  histories: Dict[str, _FolderHistory] = {}
  for i, sample in enumerate(samples):
    for path, values in sample.get("folders", {}).items():
      history = histories.setdefault(path, _FolderHistory())
      if values[0] > history.peak:
        history.peak, history.peak_t = values[0], sample["t"]
      history.last = values[0]
      history.gone_t = (samples[i + 1]["t"] if i + 1 < len(samples) else None)
  return histories


def _html_tree(analysis: Analysis) -> str:
  end = analysis.timeline.samples[-1]
  start = analysis.timeline.samples[0]
  folders = end.get("folders", {})
  histories = _folder_histories(analysis.timeline.samples)
  files_by_parent: Dict[str, List[Tuple[str, int]]] = {}
  for path, allocated, _ in end.get("files", []):
    parent = path.rsplit("/", 1)[0] if "/" in path else ""
    files_by_parent.setdefault(parent, []).append((path, allocated))
  total = max(analysis.total.series[-1], 1)
  roots = analysis.timeline.header.get("roots", {})

  def bar(size: int) -> str:
    return (f"<span class='barbox'><span class='bar' style='width:"
            f"{max(1, int(120 * size / total))}px'></span></span>")

  def delta_span(delta: int) -> str:
    css = ("color:#c5221f" if delta >= 256 << 10 else
           "color:#188038" if delta <= -(256 << 10) else "")
    return f"<span class='delta' style='{css}'>{fmt_delta(delta)}</span>"

  def peak_note(path: str, size: int) -> str:
    history = histories.get(path, _FolderHistory())
    if history.peak - size < 256 << 10 or history.peak < 1.2 * size:
      return ""
    return (f" &middot; <span class='peak'>peak {fmt_size(history.peak)} at "
            f"t={history.peak_t:.0f}s</span>")

  def node(path: str, depth: int) -> str:
    values = folders.get(path, [0, 0, 0])
    size = values[0]
    name = (
        roots.get("AppData", "AppData")
        if path == "" else roots.get(USER_DE, "user_de")
        if path == "user_de" else path.rsplit("/", 1)[-1])
    line = (f"{bar(size)}<span class='size'>{fmt_size(size)}</span>"
            f"{delta_span(size - _folder_bytes(start, path))}"
            f" &nbsp;<b>{html.escape(name)}</b>"
            f"<span class='tag'>{_count(values[2], 'file')} &middot; "
            f"{html.escape(_folder_label(analysis, folders, path))}"
            f"{peak_note(path, size)}</span>")
    children = sorted(
        ((p, v[0]) for p, v in folders.items() if _is_child(p, path)),
        key=lambda c: -c[1])
    # Small folders are folded, unless they had a notable peak.
    big = [
        c for c in children
        if c[1] >= 0.005 * max(size, 1) or peak_note(c[0], c[1])
    ]
    small = [c for c in children if c not in big]
    inner = [node(p, depth + 1) for p, _ in big]
    for file_path, allocated in sorted(
        files_by_parent.get(path, []), key=lambda f: -f[1])[:5]:
      inner.append(f"<div class='row leaf'>{bar(allocated)}"
                   f"<span class='size'>{fmt_size(allocated)}</span>"
                   f"<span class='delta'></span>"
                   f" &nbsp;{html.escape(file_path.rsplit('/', 1)[-1])}"
                   f"<span class='tag'>file &middot; "
                   f"{html.escape(_bucket_of_path(analysis, file_path))}"
                   "</span></div>")
    if small:
      inner.append(f"<div class='row leaf note'>"
                   f"{_count(len(small), 'smaller folder')}, "
                   f"{fmt_size(sum(s for _, s in small))}</div>")
    # Folders deleted during the run, e.g. by the storage cleanup.
    gone = sorted(
        ((p, h) for p, h in histories.items() if p not in folders and
         _is_child(p, path) and h.peak >= 64 << 10 and h.gone_t is not None),
        key=lambda g: -g[1].peak)
    for gone_path, history in gone[:5]:
      inner.append(f"<div class='row leaf gone'>{bar(0)}"
                   f"<span class='size'>0</span>"
                   f"{delta_span(-_folder_bytes(start, gone_path))}"
                   f" &nbsp;<s>{html.escape(gone_path.rsplit('/', 1)[-1])}</s>"
                   f"<span class='tag'>deleted at t={history.gone_t:.0f}s "
                   f"(was {fmt_size(history.last)}) &middot; "
                   f"{html.escape(_bucket_of_path(analysis, gone_path))}"
                   "</span></div>")
    if not inner:
      return f"<div class='row leaf'>{line}</div>"
    is_open = depth < 2 or path in _ROOT_LABELS or size >= 0.1 * total
    return (f"<details{' open' if is_open else ''}><summary class='row'>"
            f"{line}</summary>{''.join(inner)}</details>")

  out = [node("", 0)]
  if "user_de" in folders:
    out.append(node("user_de", 0))
  return "".join(out)


def _html_files_table(analysis: Analysis) -> str:
  end = analysis.timeline.samples[-1]
  start_files = {
      f[0]: f[1] for f in analysis.timeline.samples[0].get("files", [])
  }
  out = [
      "<table><tr><th class='name'>File</th><th>Allocated</th>"
      "<th>Logical</th><th>Change</th><th class='name'>Bucket</th></tr>"
  ]
  for path, allocated, logical in end.get("files", [])[:20]:
    change = (
        fmt_delta(allocated -
                  start_files[path]) if path in start_files else "new")
    out.append(f"<tr><td class='name'>{html.escape(path)}</td>"
               f"<td>{fmt_size(allocated)}</td><td>{fmt_size(logical)}</td>"
               f"<td>{change}</td><td class='name'>"
               f"{html.escape(_bucket_of_path(analysis, path))}</td></tr>")
  out.append("</table>")
  return "".join(out)


# ---------------------------------------------------------------------------
# Compare
# ---------------------------------------------------------------------------
def format_comparison(a: Analysis, b: Analysis) -> str:
  """Markdown table of two runs, per root group and bucket."""

  def describe(analysis: Analysis) -> str:
    header = analysis.timeline.header
    return (f"`{os.path.basename(analysis.timeline.path)}`: "
            f"{header.get('package', '?')}, CUJ `{header.get('cuj', '?')}`, "
            f"{header.get('start_time', '?')}, "
            f"{analysis.times[-1] - analysis.times[0]:.0f} s" +
            (f", extra args `{header['extra_args']}`"
             if header.get("extra_args") else ""))

  def rows_of(analysis: Analysis) -> Dict[str, Row]:
    rows = {g.name: g for g, _ in analysis.groups}
    rows.update({
        r.name: r for _, group_rows in analysis.groups for r in group_rows
    })
    rows["Total"] = analysis.total
    return rows

  rows_a, rows_b = rows_of(a), rows_of(b)
  order: List[Tuple[str, bool]] = []
  for group in ROOT_ORDER:
    members = sorted(
        {r.name for g, rs in a.groups if g.name == group for r in rs}
        | {r.name for g, rs in b.groups if g.name == group for r in rs})
    if group in rows_a or group in rows_b:
      order.append((group, True))
      order += [(m, False) for m in members]
  order.append(("Total", True))

  lines = [
      "# Storage footprint comparison",
      "",
      f"- **A**: {describe(a)}",
      f"- **B**: {describe(b)}",
      "",
      "| Bucket | A end | B end | B - A (end) | A peak | B peak | "
      "B - A (peak) |",
      "| :-- | --: | --: | --: | --: | --: | --: |",
  ]
  for name, is_group in order:
    row_a, row_b = rows_a.get(name), rows_b.get(name)
    end_a = row_a.series[-1] if row_a else 0
    end_b = row_b.series[-1] if row_b else 0
    peak_a = row_a.peak if row_a else 0
    peak_b = row_b.peak if row_b else 0
    cells = [
        fmt_size(end_a),
        fmt_size(end_b),
        fmt_delta(end_b - end_a),
        fmt_size(peak_a),
        fmt_size(peak_b),
        fmt_delta(peak_b - peak_a)
    ]
    label = ROOT_TITLES.get(name, name) if is_group else f"`{name}`"
    if is_group:
      label, cells = f"**{label}**", [f"**{c}**" for c in cells]
    lines.append("| " + " | ".join([label] + cells) + " |")
  for tag, analysis in (("A", a), ("B", b)):
    lines += ["", f"### Findings in {tag}", ""]
    lines += [
        f"- **[{f.severity.upper()}]** {f.title}." for f in analysis.findings
    ] or ["- No issues found."]
  lines.append("")
  return "\n".join(lines)


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------
def write_reports(jsonl_path: str,
                  html_path: Optional[str] = None,
                  markdown_path: Optional[str] = None) -> Analysis:
  analysis = analyze(load_timeline(jsonl_path))
  html_path = html_path or os.path.splitext(jsonl_path)[0] + ".html"
  with open(html_path, "w", encoding="utf-8") as f:
    f.write(format_html(analysis))
  if markdown_path:
    with open(markdown_path, "w", encoding="utf-8") as f:
      f.write(format_markdown(analysis, os.path.basename(html_path)))
  return analysis


def main(argv: Optional[List[str]] = None) -> int:
  parser = argparse.ArgumentParser(
      description=__doc__.split("\n\n", maxsplit=1)[0])
  sub = parser.add_subparsers(dest="command", required=True)
  report = sub.add_parser("report", help="Render HTML/Markdown from JSONL.")
  report.add_argument("jsonl")
  report.add_argument("--html", default=None)
  report.add_argument("--markdown", default=None)
  compare = sub.add_parser("compare", help="Compare two JSONL runs.")
  compare.add_argument("a")
  compare.add_argument("b")
  compare.add_argument("--output", default=None)
  args = parser.parse_args(argv)

  if args.command == "report":
    analysis = write_reports(args.jsonl, args.html, args.markdown)
    print(format_markdown(analysis))
    return 0
  text = format_comparison(
      analyze(load_timeline(args.a)), analyze(load_timeline(args.b)))
  if args.output:
    with open(args.output, "w", encoding="utf-8") as f:
      f.write(text)
  print(text)
  return 0


if __name__ == "__main__":
  sys.exit(main())
