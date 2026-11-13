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
import base64
from dataclasses import dataclass, field
from datetime import datetime
import json
import os
import re
import socket
import struct
import subprocess
import sys
import time
from typing import Any, Callable, Dict, List, Optional, Set, Tuple
import urllib.error
import urllib.parse
import urllib.request

DEFAULT_PACKAGE = "dev.cobalt.coat"
DEFAULT_ACTIVITY = "dev.cobalt.app.MainActivity"
DEFAULT_HOME_URL = "https://www.youtube.com/tv"
DEFAULT_VIDEO_URL = "https://www.youtube.com/tv#/watch?v=1La4QzGeaaQ"
DEFAULT_CDP_PORT = 9222

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

_STORAGE_API_PROBE_JS = """(async () => {
  const k = '__cobalt_storage_monitor_probe__';
  const checkStorage = (s) => {
    try {
      s.setItem(k, '1');
      const v = s.getItem(k);
      s.removeItem(k);
      return v === '1' && s.getItem(k) === null;
    } catch (e) {
      return false;
    }
  };
  const checkCookie = () => {
    try {
      document.cookie = k + '=1; Max-Age=60; path=/';
      const ok = document.cookie.includes(k + '=1');
      document.cookie = k + '=; Max-Age=0; path=/';
      return ok;
    } catch (e) {
      return false;
    }
  };
  let mediaOk = false;
  let mediaSmooth = false;
  try {
    const info = await navigator.mediaCapabilities.decodingInfo({
      type: 'media-source',
      video: {
        contentType: 'video/mp4; codecs="avc1.4d401e"',
        width: 1920,
        height: 1080,
        bitrate: 5000000,
        framerate: 30,
      },
    });
    mediaSmooth = Boolean(info && info.smooth);
    mediaOk = Boolean(info && typeof info.smooth === 'boolean' && info.smooth);
  } catch (e) {
    mediaOk = false;
  }
  return {
    session_storage_ok: checkStorage(window.sessionStorage),
    local_storage_ok: checkStorage(window.localStorage),
    cookie_ok: checkCookie(),
    media_capabilities_ok: mediaOk,
    media_capabilities_smooth: mediaSmooth,
  };
})()"""


@dataclass
class ProcIoSample:
  """Point-in-time block I/O, FD, and VMA counters from /proc/<pid>.

  Lifetime/Ownership: Created per polling tick and owned by StorageTracker.
  Threading Model: Thread-affine; accessed only on the StorageTracker thread.
  """
  pid: int
  read_bytes: int = 0
  write_bytes: int = 0
  total_fds: int = 0
  total_vmas: int = 0
  storage_fd_paths: Tuple[str, ...] = ()
  storage_vma_paths: Tuple[str, ...] = ()


@dataclass
class IoCounter:
  """Accumulates one block I/O counter across samples and its peak rate.

  Lifetime/Ownership: Owned by StorageTracker for the monitoring session.
  Threading Model: Thread-affine; accessed only on the StorageTracker thread.
  """
  total_bytes: int = 0
  peak_kbps: float = 0.0
  peak_at_sec: float = 0.0

  def add(self, delta_bytes: int, dt_sec: float, at_sec: float) -> None:
    self.total_bytes += delta_bytes
    if dt_sec > 0:
      kbps = delta_bytes / 1024.0 / dt_sec
      if kbps > self.peak_kbps:
        self.peak_kbps = kbps
        self.peak_at_sec = at_sec


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
  peak_open_fds: int = 0
  peak_vmas: int = 0
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


def _strip_deleted_suffix(path: str) -> str:
  """Strips trailing ' (deleted)' added by Linux procfs to unlinked paths."""
  if path.endswith(" (deleted)"):
    return path[:-10]
  return path


def parse_proc_io_output(output: str) -> Optional[ProcIoSample]:
  """Parses /proc/<pid>/io, /proc/<pid>/fd, and /proc/<pid>/maps output."""
  values: Dict[str, int] = {}
  fd_paths: List[str] = []
  vma_paths: List[str] = []
  for line in output.splitlines():
    key, sep, val = line.partition(":")
    if not sep:
      continue
    key = key.strip()
    val = val.strip()
    if key == "fd_path" and val:
      fd_paths.append(_strip_deleted_suffix(val))
    elif key == "vma_path" and val:
      vma_paths.append(_strip_deleted_suffix(val))
    elif val.isdigit():
      values[key] = int(val)
  if not {"pid", "read_bytes", "write_bytes"} <= values.keys():
    return None
  return ProcIoSample(
      pid=values["pid"],
      read_bytes=values["read_bytes"],
      write_bytes=values["write_bytes"],
      total_fds=values.get("total_fds", 0),
      total_vmas=values.get("total_vmas", 0),
      storage_fd_paths=tuple(fd_paths),
      storage_vma_paths=tuple(vma_paths),
  )


def collect_android_proc_io(
    package: str = DEFAULT_PACKAGE) -> Optional[ProcIoSample]:
  """Reads /proc/<pid>/{io,fd,maps} of the package's main process."""
  cmd = (f"sh -c 'pid=$(pidof {package}); pid=${{pid%% *}}; "
         "[ -z \"$pid\" ] && exit 0; "
         "echo \"pid: $pid\"; "
         "cat \"/proc/$pid/io\" 2>/dev/null; "
         "echo \"total_fds: $(ls -1 \"/proc/$pid/fd\" 2>/dev/null | wc -l)\"; "
         "echo \"total_vmas: $(wc -l < \"/proc/$pid/maps\" 2>/dev/null)\"; "
         f"ls -l \"/proc/$pid/fd\" 2>/dev/null | grep \"/{package}/\" | "
         "sed \"s/.* -> /fd_path: /\" 2>/dev/null; "
         "sed -n \"s|^[^ ]* [^ ]* [^ ]* [^ ]* [^ ]* *\\(/data/.*"
         f"{package}/.*\\)|vma_path: \\1|p\" \"/proc/$pid/maps\" 2>/dev/null'")
  _, out = run_adb_shell(cmd, package=package)
  return parse_proc_io_output(out)


def _recv_exact(sock: socket.socket, num_bytes: int) -> bytes:
  """Reads exactly num_bytes from sock or raises EOFError."""
  chunks: List[bytes] = []
  remaining = num_bytes
  while remaining > 0:
    chunk = sock.recv(remaining)
    if not chunk:
      raise EOFError("Socket closed before reading expected bytes.")
    chunks.append(chunk)
    remaining -= len(chunk)
  return b"".join(chunks)


def _read_ws_frame(sock: socket.socket,
                   initial_buf: bytes = b"") -> Tuple[int, bytes, bytes]:
  """Reads one RFC 6455 WebSocket frame, returning (opcode, payload, extra)."""
  buf = initial_buf
  while len(buf) < 2:
    chunk = sock.recv(4096)
    if not chunk:
      raise EOFError("WebSocket closed before frame header.")
    buf += chunk

  b0, b1 = buf[0], buf[1]
  opcode = b0 & 0x0F
  masked = bool(b1 & 0x80)
  length = b1 & 0x7F
  idx = 2

  if length == 126:
    while len(buf) < idx + 2:
      buf += _recv_exact(sock, idx + 2 - len(buf))
    length = struct.unpack("!H", buf[idx:idx + 2])[0]
    idx += 2
  elif length == 127:
    while len(buf) < idx + 8:
      buf += _recv_exact(sock, idx + 8 - len(buf))
    length = struct.unpack("!Q", buf[idx:idx + 8])[0]
    idx += 8

  mask_key = b""
  if masked:
    while len(buf) < idx + 4:
      buf += _recv_exact(sock, idx + 4 - len(buf))
    mask_key = buf[idx:idx + 4]
    idx += 4

  while len(buf) < idx + length:
    buf += _recv_exact(sock, idx + length - len(buf))

  payload = buf[idx:idx + length]
  leftover = buf[idx + length:]
  if masked:
    payload = bytes(b ^ mask_key[i % 4] for i, b in enumerate(payload))
  return opcode, payload, leftover


def _send_ws_text_frame(sock: socket.socket, text: str) -> None:
  """Sends a client-masked RFC 6455 text frame over sock."""
  data = text.encode("utf-8")
  mask_key = os.urandom(4)
  header = bytearray([0x81])
  length = len(data)
  if length < 126:
    header.append(0x80 | length)
  elif length < 65536:
    header.append(0x80 | 126)
    header.extend(struct.pack("!H", length))
  else:
    header.append(0x80 | 127)
    header.extend(struct.pack("!Q", length))
  header.extend(mask_key)
  masked_data = bytes(b ^ mask_key[i % 4] for i, b in enumerate(data))
  sock.sendall(bytes(header) + masked_data)


def _cdp_ws_evaluate(ws_url: str,
                     expression: str,
                     timeout_sec: float = 10.0) -> Optional[Dict[str, Any]]:
  """Evaluates a JS expression (with awaitPromise) over a CDP WebSocket."""
  parsed = urllib.parse.urlparse(ws_url)
  host = parsed.hostname or "127.0.0.1"
  port = parsed.port or DEFAULT_CDP_PORT
  path = parsed.path or "/"
  if parsed.query:
    path = f"{path}?{parsed.query}"

  with socket.create_connection((host, port), timeout=timeout_sec) as sock:
    key = base64.b64encode(os.urandom(16)).decode("ascii")
    handshake = (f"GET {path} HTTP/1.1\r\n"
                 f"Host: {host}:{port}\r\n"
                 "Upgrade: websocket\r\n"
                 "Connection: Upgrade\r\n"
                 f"Sec-WebSocket-Key: {key}\r\n"
                 "Sec-WebSocket-Version: 13\r\n\r\n")
    sock.sendall(handshake.encode("ascii"))

    resp_buf = b""
    while b"\r\n\r\n" not in resp_buf:
      chunk = sock.recv(4096)
      if not chunk:
        return None
      resp_buf += chunk
    header_bytes, leftover = resp_buf.split(b"\r\n\r\n", 1)
    lines = header_bytes.splitlines()
    if not lines or b" 101 " not in lines[0]:
      return None

    req = json.dumps({
        "id": 1,
        "method": "Runtime.evaluate",
        "params": {
            "expression": expression,
            "awaitPromise": True,
            "returnByValue": True,
        },
    })
    _send_ws_text_frame(sock, req)

    while True:
      opcode, payload, leftover = _read_ws_frame(sock, leftover)
      if opcode == 0x8:  # Connection close
        return None
      if opcode == 0x9:  # Ping -> Pong
        continue
      if opcode == 0x1:  # Text frame
        msg = json.loads(payload.decode("utf-8"))
        if msg.get("id") == 1:
          return (msg.get("result", {}).get("result", {}).get("value"))


def verify_storage_apis_via_cdp(
    port: int = DEFAULT_CDP_PORT,
    timeout_sec: float = 10.0) -> Optional[Dict[str, Any]]:
  """Forwards the Android CDP socket and verifies Web Storage & Media APIs."""
  try:
    subprocess.run([
        "adb",
        "forward",
        f"tcp:{port}",
        "localabstract:content_shell_devtools_remote",
    ],
                   stdout=subprocess.PIPE,
                   stderr=subprocess.PIPE,
                   check=False,
                   timeout=5)
    with urllib.request.urlopen(
        f"http://127.0.0.1:{port}/json", timeout=timeout_sec) as resp:
      targets = json.loads(resp.read().decode("utf-8"))
  except (OSError, urllib.error.URLError, ValueError,
          subprocess.TimeoutExpired):
    return None

  ws_url = None
  for target in targets:
    if target.get("type") == "page" and target.get("webSocketDebuggerUrl"):
      ws_url = target["webSocketDebuggerUrl"]
      break
  if not ws_url:
    return None

  try:
    val = _cdp_ws_evaluate(ws_url, _STORAGE_API_PROBE_JS, timeout_sec)
  except (OSError, EOFError, ValueError, struct.error):
    return None

  if not isinstance(val, dict):
    return None

  session_ok = bool(val.get("session_storage_ok"))
  local_ok = bool(val.get("local_storage_ok"))
  cookie_ok = bool(val.get("cookie_ok"))
  media_ok = bool(val.get("media_capabilities_ok"))
  media_smooth = bool(val.get("media_capabilities_smooth"))
  return {
      "passed": session_ok and local_ok and cookie_ok and media_ok,
      "session_storage_ok": session_ok,
      "local_storage_ok": local_ok,
      "cookie_ok": cookie_ok,
      "media_capabilities_ok": media_ok,
      "media_capabilities_smooth": media_smooth,
  }


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
    self.pids_seen: Set[int] = set()
    self.block_read = IoCounter()
    self.block_write = IoCounter()
    self.peak_total_fds: int = 0
    self.final_total_fds: int = 0
    self.peak_storage_fds: int = 0
    self.final_storage_fds: int = 0
    self.peak_total_vmas: int = 0
    self.final_total_vmas: int = 0
    self.peak_storage_vmas: int = 0
    self.final_storage_vmas: int = 0
    self.storage_api_verification: Optional[Dict[str, Any]] = None
    self.storage_api_verification_attempted: bool = False
    self._last_io: Optional[ProcIoSample] = None
    self._last_io_poll_sec: Optional[float] = None

  def _ensure_type_stats(self, location: str,
                         storage_type: str) -> StorageTypeStats:
    if storage_type not in self.type_stats:
      self.type_stats[storage_type] = StorageTypeStats(
          storage_type=storage_type,
          location=location,
          start_bytes=0 if self.sample_count > 1 else None,
      )
    return self.type_stats[storage_type]

  def record_proc_io(self, elapsed_sec: float,
                     sample: Optional[ProcIoSample]) -> None:
    """Adds the block I/O, FDs, and VMAs from the /proc/<pid> poll.

    |sample| is None when the process is not running.
    """
    prev_poll_sec = self._last_io_poll_sec
    self._last_io_poll_sec = elapsed_sec
    if sample is None:
      return
    prev = self._last_io
    self._last_io = sample
    self.pids_seen.add(sample.pid)

    # Track open FDs and VMAs (excluding 'OTHER' code/lib paths).
    fd_counts: Dict[str, int] = {}
    storage_fds = 0
    for fd_path in sample.storage_fd_paths:
      loc, stype = classify_storage_path(fd_path, self.package)
      if loc == "OTHER":
        continue
      storage_fds += 1
      fd_counts[stype] = fd_counts.get(stype, 0) + 1
      self._ensure_type_stats(loc, stype)

    vma_counts: Dict[str, int] = {}
    storage_vmas = 0
    for vma_path in sample.storage_vma_paths:
      loc, stype = classify_storage_path(vma_path, self.package)
      if loc == "OTHER":
        continue
      storage_vmas += 1
      vma_counts[stype] = vma_counts.get(stype, 0) + 1
      self._ensure_type_stats(loc, stype)

    for stype, count in fd_counts.items():
      stats = self.type_stats[stype]
      stats.peak_open_fds = max(stats.peak_open_fds, count)
    for stype, count in vma_counts.items():
      stats = self.type_stats[stype]
      stats.peak_vmas = max(stats.peak_vmas, count)

    self.final_total_fds = sample.total_fds
    self.peak_total_fds = max(self.peak_total_fds, sample.total_fds)
    self.final_storage_fds = storage_fds
    self.peak_storage_fds = max(self.peak_storage_fds, storage_fds)

    self.final_total_vmas = sample.total_vmas
    self.peak_total_vmas = max(self.peak_total_vmas, sample.total_vmas)
    self.final_storage_vmas = storage_vmas
    self.peak_storage_vmas = max(self.peak_storage_vmas, storage_vmas)

    if prev_poll_sec is None:
      # First poll: a process that is already running has counters that
      # include I/O from before monitoring started, so they are the baseline.
      return
    # A new PID, or counters that went backwards (a recycled PID), means the
    # process (re)started after the previous poll with counters at zero.
    restarted = (
        prev is None or sample.pid != prev.pid or
        sample.read_bytes < prev.read_bytes or
        sample.write_bytes < prev.write_bytes)
    base_read = 0 if restarted else prev.read_bytes
    base_write = 0 if restarted else prev.write_bytes
    dt_sec = elapsed_sec - prev_poll_sec
    self.block_read.add(sample.read_bytes - base_read, dt_sec, elapsed_sec)
    self.block_write.add(sample.write_bytes - base_write, dt_sec, elapsed_sec)

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
      self._ensure_type_stats(location, storage_type)

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
          "peak_open_fds": s.peak_open_fds,
          "peak_vmas": s.peak_vmas,
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

      section_b.append({
          "relative_path": f.relative_path,
          "location": f.location,
          "storage_type": f.storage_type,
          "status": status,
          "existed_at_end": f.existed_at_end,
          "first_seen_sec": round(f.first_seen_sec, 1),
          "last_seen_sec": round(f.last_seen_sec, 1),
          "start_kb": round(f.start_bytes / 1024.0, 2),
          "peak_kb": round(f.peak_bytes / 1024.0, 2),
          "final_kb": round(f.final_bytes / 1024.0, 2),
          "delta_kb": round((f.final_bytes - f.start_bytes) / 1024.0, 2),
          "mtime_changes": f.mtime_changes,
      })

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
            "pids_tracked": len(self.pids_seen),
            "read_kb": round(self.block_read.total_bytes / 1024.0, 2),
            "write_kb": round(self.block_write.total_bytes / 1024.0, 2),
            "peak_read_kbps": round(self.block_read.peak_kbps, 2),
            "peak_read_at_sec": round(self.block_read.peak_at_sec, 1),
            "peak_write_kbps": round(self.block_write.peak_kbps, 2),
            "peak_write_at_sec": round(self.block_write.peak_at_sec, 1),
            "peak_total_fds": self.peak_total_fds,
            "final_total_fds": self.final_total_fds,
            "peak_storage_fds": self.peak_storage_fds,
            "final_storage_fds": self.final_storage_fds,
            "peak_total_vmas": self.peak_total_vmas,
            "final_total_vmas": self.final_total_vmas,
            "peak_storage_vmas": self.peak_storage_vmas,
            "final_storage_vmas": self.final_storage_vmas,
        },
        "storage_type_summary": section_a,
        "files_inventory": section_b,
    }
    if self.storage_api_verification is not None:
      summary["storage_api_verification"] = self.storage_api_verification
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
    read_kb = proc_io["read_kb"]
    peak_read_kbps = proc_io["peak_read_kbps"]
    peak_read_at = proc_io["peak_read_at_sec"]
    write_kb = proc_io["write_kb"]
    peak_write_kbps = proc_io["peak_write_kbps"]
    peak_write_at = proc_io["peak_write_at_sec"]
    pids_tracked = proc_io["pids_tracked"]
    lines.append("- **Block I/O (`/proc/<pid>/io`)**: "
                 f"Read: `{read_kb:.2f} KB` "
                 f"(peak `{peak_read_kbps:.2f} KB/s` at "
                 f"`t={peak_read_at}s`) | "
                 f"Write: `{write_kb:.2f} KB` "
                 f"(peak `{peak_write_kbps:.2f} KB/s` at "
                 f"`t={peak_write_at}s`) | "
                 f"Processes: `{pids_tracked}`")
    peak_fds = proc_io.get("peak_total_fds", 0)
    peak_vmas = proc_io.get("peak_total_vmas", 0)
    if peak_fds > 0 or peak_vmas > 0:
      peak_sfds = proc_io.get("peak_storage_fds", 0)
      final_sfds = proc_io.get("final_storage_fds", 0)
      peak_svmas = proc_io.get("peak_storage_vmas", 0)
      final_svmas = proc_io.get("final_storage_vmas", 0)
      lines.append(
          "- **Open FDs & VMAs (`/proc/<pid>/fd`, `maps`)**: "
          f"Peak FDs: `{peak_fds}` "
          f"(Storage: `{peak_sfds}` peak / `{final_sfds}` final) | "
          f"Peak VMAs: `{peak_vmas}` "
          f"(Storage `mmap`: `{peak_svmas}` peak / `{final_svmas}` final)")

  api_check = report.get("storage_api_verification")
  if api_check is not None:
    overall = "PASS" if api_check.get("passed") else "FAIL"
    ss = "OK" if api_check.get("session_storage_ok") else "FAIL"
    ls = "OK" if api_check.get("local_storage_ok") else "FAIL"
    ck = "OK" if api_check.get("cookie_ok") else "FAIL"
    mc = "OK" if api_check.get("media_capabilities_ok") else "FAIL"
    smooth = api_check.get("media_capabilities_smooth", False)
    lines.append("- **Web API Parity (CDP)**: "
                 f"`{overall}` (`sessionStorage`: `{ss}`, "
                 f"`localStorage`: `{ls}`, `cookie`: `{ck}`, "
                 f"`mediaCapabilities`: `{mc}` [`smooth={smooth}`])")

  lines.extend([
      "",
      "---",
      "",
      "## Section A: Storage Type Summary (Full Footprint Accounting)",
      "",
      ("| Storage Type | Location | Start KB | Peak KB (Time) | Final KB | "
       "Write Ticks | Peak FDs | Peak VMAs |"),
      "| :--- | :--- | ---: | ---: | ---: | ---: | ---: | ---: |",
  ])

  for row in report["storage_type_summary"]:
    stype = row["storage_type"]
    loc = row["location"]
    start_kb = row["start_kb"]
    peak_kb = row["peak_kb"]
    peak_at = row["peak_at_sec"]
    final_kb = row["final_kb"]
    ticks = row["write_ticks"]
    peak_fds = row.get("peak_open_fds", 0)
    peak_vmas = row.get("peak_vmas", 0)
    lines.append(f"| `{stype}` | `{loc}` | "
                 f"{start_kb:.2f} | "
                 f"{peak_kb:.2f} (`t={peak_at}s`) | "
                 f"{final_kb:.2f} | "
                 f"{ticks} | "
                 f"{peak_fds} | "
                 f"{peak_vmas} |")

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


def _poll_for_duration(
    tracker: StorageTracker,
    duration_sec: float,
    interval_sec: float,
    run_start_time: float,
    snapshot_fn: Callable[[], Dict[str, Tuple[int, int]]],
    *,
    proc_io_fn: Optional[Callable[[], Optional[ProcIoSample]]] = None,
    verify_apis_fn: Optional[Callable[[], Optional[Dict[str, Any]]]] = None,
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
    if (verify_apis_fn and not tracker.storage_api_verification_attempted and
        (phase_label == "manual" or phase_elapsed >= 15.0)):
      tracker.storage_api_verification_attempted = True
      tracker.storage_api_verification = verify_apis_fn()

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
                           stop_first: bool = True,
                           enable_cdp: bool = False,
                           extra_args: str = "") -> None:
  """Launches Cobalt on Android TV with the specified URL."""
  if stop_first:
    run_adb_shell(f"am force-stop {package}", package=None)
    time.sleep(1.0)
  cmd_args: List[str] = []
  if enable_cdp:
    cmd_args.append("--remote-allow-origins=*")
  if extra_args:
    for arg in extra_args.split(","):
      cleaned = arg.strip()
      if cleaned:
        if not cleaned.startswith("--"):
          cleaned = f"--{cleaned}"
        cmd_args.append(cleaned)
  if package == DEFAULT_PACKAGE:
    cmd_args.append(f"--url={url}")
    joined_args = ",".join(cmd_args)
    cmd = (f"am start -n {package}/{DEFAULT_ACTIVITY} "
           f"--esa commandLineArgs '{joined_args}'")
  else:
    joined_args = ",".join(cmd_args)
    esa_part = f" --esa commandLineArgs '{joined_args}'" if joined_args else ""
    cmd = (f"am start -a android.intent.action.VIEW "
           f"-d '{url}'{esa_part} -p {package}")
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
    proc_io_fn: Optional[Callable[[], Optional[ProcIoSample]]] = None,
    verify_storage_apis: bool = False,
    cdp_port: int = DEFAULT_CDP_PORT,
    verify_apis_fn: Optional[Callable[[], Optional[Dict[str, Any]]]] = None,
    extra_args: str = "") -> float:
  """Executes the specified CUJ on Android TV while polling storage."""
  if snapshot_fn is None:
    verify_android_device_connected()
  snap_callable = snapshot_fn or (lambda: collect_android_snapshot(package))
  io_callable = proc_io_fn
  if io_callable is None and snapshot_fn is None:

    def _default_proc_io() -> Optional[ProcIoSample]:
      return collect_android_proc_io(package)

    io_callable = _default_proc_io

  apis_callable = verify_apis_fn
  if apis_callable is None and verify_storage_apis:

    def _default_verify_apis() -> Optional[Dict[str, Any]]:
      return verify_storage_apis_via_cdp(port=cdp_port)

    apis_callable = _default_verify_apis

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
          verify_apis_fn=apis_callable,
          phase_label="manual")

    elif active_cuj == "cold_start":
      _launch_cobalt_android(
          DEFAULT_HOME_URL,
          package=package,
          stop_first=True,
          enable_cdp=verify_storage_apis,
          extra_args=extra_args)
      _poll_for_duration(
          tracker,
          duration_sec=60.0,
          interval_sec=interval_sec,
          run_start_time=run_start,
          snapshot_fn=snap_callable,
          proc_io_fn=io_callable,
          verify_apis_fn=apis_callable,
          phase_label="cold_start")

    elif active_cuj == "home_scroll":
      _launch_cobalt_android(
          DEFAULT_HOME_URL,
          package=package,
          stop_first=True,
          enable_cdp=verify_storage_apis,
          extra_args=extra_args)
      _poll_for_duration(
          tracker,
          duration_sec=90.0,
          interval_sec=interval_sec,
          run_start_time=run_start,
          snapshot_fn=snap_callable,
          proc_io_fn=io_callable,
          verify_apis_fn=apis_callable,
          action_cb=_make_scroll_action(),
          phase_label="home_scroll")

    elif active_cuj == "video_playback":
      _launch_cobalt_android(
          DEFAULT_VIDEO_URL,
          package=package,
          stop_first=True,
          enable_cdp=verify_storage_apis,
          extra_args=extra_args)
      _poll_for_duration(
          tracker,
          duration_sec=180.0,
          interval_sec=interval_sec,
          run_start_time=run_start,
          snapshot_fn=snap_callable,
          proc_io_fn=io_callable,
          verify_apis_fn=apis_callable,
          action_cb=_make_playback_action(),
          phase_label="video_playback")

    elif active_cuj == "pause_flush":
      _launch_cobalt_android(
          DEFAULT_HOME_URL,
          package=package,
          stop_first=True,
          enable_cdp=verify_storage_apis,
          extra_args=extra_args)
      _poll_for_duration(
          tracker,
          duration_sec=10.0,
          interval_sec=interval_sec,
          run_start_time=run_start,
          snapshot_fn=snap_callable,
          proc_io_fn=io_callable,
          verify_apis_fn=apis_callable,
          phase_label="pre_pause")
      _poll_for_duration(
          tracker,
          duration_sec=30.0,
          interval_sec=interval_sec,
          run_start_time=run_start,
          snapshot_fn=snap_callable,
          proc_io_fn=io_callable,
          verify_apis_fn=apis_callable,
          action_cb=_make_pause_action(),
          phase_label="pause_flush")

    elif active_cuj == "warm_relaunch":
      for cycle in range(1, 4):
        _launch_cobalt_android(
            DEFAULT_HOME_URL,
            package=package,
            stop_first=True,
            enable_cdp=verify_storage_apis,
            extra_args=extra_args)
        _poll_for_duration(
            tracker,
            duration_sec=20.0,
            interval_sec=interval_sec,
            run_start_time=run_start,
            snapshot_fn=snap_callable,
            proc_io_fn=io_callable,
            verify_apis_fn=apis_callable,
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
      "--verify-storage-apis",
      action="store_true",
      help=("Verify Web Storage (sessionStorage, localStorage, "
            "document.cookie) and MediaCapabilities JS APIs via CDP."))
  parser.add_argument(
      "--cdp-port",
      type=int,
      default=DEFAULT_CDP_PORT,
      help=("Local TCP port for ADB CDP forwarding "
            f"(default: {DEFAULT_CDP_PORT})."))
  parser.add_argument(
      "--extra-args",
      type=str,
      default="",
      help=("Comma-separated extra commandLineArgs passed to Cobalt on launch "
            "(e.g. '--enable-features=CobaltStorageCleanup')."))
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
        package=args.package,
        verify_storage_apis=args.verify_storage_apis,
        cdp_port=args.cdp_port,
        extra_args=args.extra_args)
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
