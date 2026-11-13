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
"""Host runner and instrumentation script for SbPlayer endurance tests.

Runs SbPlayerEnduranceTest.DISABLED_CreateDestroy on an Android device via
the generated gtest runner wrapper (for example,
out/android-arm_qa/bin/run_player_benchmarks) while collecting a self-contained
diagnostic bundle:

  <bundle-dir>/
    results.csv          Per-iteration timings and statuses pulled from device.
    logcat.txt           Continuous `adb logcat -v threadtime` for the run.
    runner.log           Combined stdout/stderr of the host gtest runner.
    display.txt          Pre-run `dumpsys display` + `dumpsys window displays`.
    meminfo_start.txt    Pre-run `dumpsys meminfo`.
    meminfo_end.txt      Post-run `dumpsys meminfo`.
    manifest.json        Run metadata, device properties, and summary counts.
    dumpsys/
      iter_00050_*.txt   Periodic codec, resource_manager, and package meminfo.
      anomaly_iter_*.txt Immediate codec and resource_manager on error/outlier.
"""

import argparse
import csv
import datetime
import fcntl
import json
import math
import os
import re
import shlex
import shutil
import signal
import subprocess
import sys
import tempfile
import threading
import time
from typing import Any, Callable, Dict, List, Optional, Sequence, Set, Tuple

_TEST_FILTER = 'SbPlayerEnduranceTest.DISABLED_CreateDestroy'
_TEST_PACKAGE = 'org.chromium.native_test'
_REMOTE_DOWNLOAD_DIR = '/sdcard/Download'
_ANOMALY_MIN_INTERVAL_SEC = 5.0
_ANOMALY_MAX_SNAPSHOTS = 20
_POLL_INTERVAL_SEC = 0.5
_BOOT_TIMEOUT_SEC = 300.0
_BOOT_SETTLE_SEC = 3.0
_REBOOT_WAIT_POLL_SEC = 5.0
_UNAUTHORIZED_MAX_SEC = 10.0
_UNAUTHORIZED_MIN_POLLS = 3
_SPAWN_STAGGER_SEC = 2.1
_INPROC_SPAWN_LOCK = threading.Lock()
_LATENCY_COLUMNS = (
    ('create_to_presenting_ms', 'presenting_slow'),
    ('destroy_ms', 'destroy_slow'),
    ('seek_max_ms', 'seek_slow'),
)


def _iso_utc_now() -> str:
  return datetime.datetime.now(
      datetime.timezone.utc).isoformat(timespec='milliseconds')


def _sanitize_token(text: str) -> str:
  cleaned = re.sub(r'[^a-zA-Z0-9_]+', '_', text.strip()).strip('_').lower()
  return cleaned or 'unknown'


def _parse_stop_packages(raw_values: Sequence[str]) -> List[str]:
  """Flattens repeatable/comma-separated --stop-package values."""
  packages: List[str] = []
  seen = set()
  for raw in raw_values:
    for entry in raw.split(','):
      pkg = entry.strip()
      if pkg and pkg not in seen:
        seen.add(pkg)
        packages.append(pkg)
  return packages


def _resolve_runner_path(runner_arg: str) -> Tuple[str, str]:
  """Resolves the gtest runner path and working directory."""
  candidate = os.path.abspath(runner_arg)
  if os.path.isfile(candidate):
    return candidate, os.getcwd()

  repo_root = os.path.abspath(
      os.path.join(os.path.dirname(__file__), '..', '..', '..'))
  repo_candidate = os.path.abspath(os.path.join(repo_root, runner_arg))
  if os.path.isfile(repo_candidate):
    return repo_candidate, repo_root

  raise FileNotFoundError(f'Runner binary not found: {runner_arg}')


class AdbClient:
  """Thin helper around `adb -s <serial>` subprocess calls."""

  def __init__(self, adb_bin: str, serial: str):
    self.adb_bin = adb_bin
    self.serial = serial

  def base_cmd(self) -> List[str]:
    return [self.adb_bin, '-s', self.serial]

  def run(
      self,
      args: Sequence[str],
      timeout: float = 30.0,
      check: bool = False,
  ) -> subprocess.CompletedProcess:
    """Runs an adb subcommand and captures raw bytes."""
    return subprocess.run(
        self.base_cmd() + list(args),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        timeout=timeout,
        check=check,
    )

  def shell_text(self, command: str, timeout: float = 30.0) -> Tuple[int, str]:
    """Runs `adb shell` and returns `(returncode, stripped_stdout)`."""
    proc = self.run(['shell', command], timeout=timeout, check=False)
    return proc.returncode, proc.stdout.decode(
        'utf-8', errors='replace').strip()

  def get_prop(self, name: str) -> str:
    rc, out = self.shell_text(f'getprop {shlex.quote(name)}', timeout=10.0)
    return out if rc == 0 else ''

  def get_boot_id(self) -> str:
    rc, out = self.shell_text(
        'cat /proc/sys/kernel/random/boot_id', timeout=10.0)
    return out if rc == 0 else ''

  def get_device_state(self, timeout: float = 10.0) -> str:
    """Returns this serial's state from `adb devices` (or `'missing'`)."""
    proc = subprocess.run(
        [self.adb_bin, 'devices'],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        timeout=timeout,
        check=False,
    )
    if proc.returncode != 0:
      err = proc.stderr.decode('utf-8', errors='replace').strip()
      raise RuntimeError(f'`adb devices` failed (rc={proc.returncode}): {err}')
    stdout = proc.stdout.decode('utf-8', errors='replace')
    for raw_line in stdout.splitlines():
      line = raw_line.strip()
      if not line or line.startswith('List of devices') or line.startswith('*'):
        continue
      parts = line.split()
      if len(parts) >= 2 and parts[0] == self.serial:
        return parts[1]
    return 'missing'

  def dump_to_file(
      self,
      dest_path: str,
      commands: Sequence[Sequence[str]],
      timeout: float = 30.0,
  ) -> None:
    """Writes one or more `adb exec-out` command outputs into `dest_path`."""
    chunks: List[bytes] = []
    for index, cmd in enumerate(commands):
      if index > 0:
        chunks.append(b'\n' + (b'=' * 72) + b'\n')
      try:
        proc = self.run(['exec-out'] + list(cmd), timeout=timeout, check=False)
        chunks.append(proc.stdout)
        if proc.returncode != 0 and proc.stderr:
          chunks.append(b'\n[stderr]\n' + proc.stderr)
      except (OSError, subprocess.SubprocessError) as exc:
        chunks.append(f'[capture failed: {exc}]\n'.encode('utf-8'))
    with open(dest_path, 'wb') as out_file:
      for chunk in chunks:
        out_file.write(chunk)


def _format_adb_state_error(serial: str, state: str) -> str:
  """Formats an actionable error message for a non-`'device'` adb state."""
  if state == 'unauthorized':
    return (f"Device {serial} has adb state='unauthorized'. Accept the "
            "'Allow USB debugging?' prompt on the device (with "
            "'Always allow from this computer' checked), or run without "
            '--reboot-before if authorization does not persist across reboots.')
  if state == 'missing':
    return (
        f"Device {serial} not found in `adb devices` (adb state='missing'). "
        'Check USB/network connection and serial.')
  return (f'Device {serial} is not ready in `adb devices` '
          f"(adb state={state!r}; expected 'device').")


def _reboot_and_wait(
    adb: AdbClient,
    abort_check: Optional[Callable[[], bool]] = None,
) -> Tuple[str, str]:
  """Reboots the device and blocks until Android services and storage boot."""
  pre_boot_id = adb.get_boot_id()
  print(f'[endurance] Rebooting device {adb.serial} (boot_id={pre_boot_id})...')
  adb.run(['reboot'], timeout=30.0, check=True)
  # Brief pause so adbd tears down before polling state.
  time.sleep(2.0)

  deadline = time.monotonic() + _BOOT_TIMEOUT_SEC
  post_boot_id = ''
  unauthorized_since: Optional[float] = None
  unauthorized_polls = 0
  while time.monotonic() < deadline:
    if abort_check is not None and abort_check():
      return pre_boot_id, ''
    remaining = max(1.0, deadline - time.monotonic())
    try:
      state = adb.get_device_state(timeout=min(10.0, remaining))
      if state == 'unauthorized':
        now_mono = time.monotonic()
        if unauthorized_since is None:
          unauthorized_since = now_mono
        unauthorized_polls += 1
        if ((now_mono - unauthorized_since) >= _UNAUTHORIZED_MAX_SEC and
            unauthorized_polls >= _UNAUTHORIZED_MIN_POLLS):
          raise RuntimeError(_format_adb_state_error(adb.serial, state))
        time.sleep(1.0)
        continue

      unauthorized_since = None
      unauthorized_polls = 0
      if state != 'device':
        adb.run(
            ['wait-for-device'],
            timeout=min(_REBOOT_WAIT_POLL_SEC, remaining),
            check=False,
        )
        if adb.get_device_state(timeout=10.0) != 'device':
          continue

      cur_boot_id = adb.get_boot_id()
      if pre_boot_id and cur_boot_id == pre_boot_id:
        time.sleep(1.0)
        continue
      boot_done = (
          adb.get_prop('sys.boot_completed') == '1' or
          adb.get_prop('dev.bootcomplete') == '1')
      if not boot_done:
        time.sleep(1.0)
        continue
      pm_rc, pm_out = adb.shell_text('pm path android', timeout=15.0)
      if pm_rc != 0 or 'package:' not in pm_out:
        time.sleep(1.0)
        continue
      dl_rc, _ = adb.shell_text(
          f'mkdir -p {_REMOTE_DOWNLOAD_DIR} && test -d {_REMOTE_DOWNLOAD_DIR}',
          timeout=15.0,
      )
      if dl_rc != 0:
        time.sleep(1.0)
        continue
      post_boot_id = cur_boot_id
      break
    except (OSError, subprocess.SubprocessError):
      time.sleep(1.0)
  else:
    raise TimeoutError(f'Device {adb.serial} did not finish booting within '
                       f'{_BOOT_TIMEOUT_SEC}s')

  time.sleep(_BOOT_SETTLE_SEC)
  print(f'[endurance] Device {adb.serial} booted (boot_id={post_boot_id}).')
  return pre_boot_id, post_boot_id


def _prepare_external_storage(
    adb: AdbClient,
    sdk_int: int,
    temp_dir: str,
) -> Optional[str]:
  """Ensures /sdcard/Download access and returns an adb shim path if needed.

  On some Android 11+ (API 30+) builds, $EXTERNAL_STORAGE in `adb shell` is set
  to `/mnt/sdcard`. Chromium's `devil` runner reads $EXTERNAL_STORAGE and passes
  `/mnt/sdcard/Download/...` to the test process, which SELinux blocks for
  `untrusted_app` even when `/sdcard/Download` is accessible. When that mismatch
  is detected, a lightweight `--adb-path` wrapper normalizes `devil`'s storage
  probe to `/sdcard` while forwarding all other adb arguments unchanged.
  """
  dl_rc, dl_out = adb.shell_text(
      f'mkdir -p {_REMOTE_DOWNLOAD_DIR} && test -d {_REMOTE_DOWNLOAD_DIR}',
      timeout=15.0,
  )
  if dl_rc != 0:
    raise RuntimeError(
        f'Failed to prepare {_REMOTE_DOWNLOAD_DIR} on {adb.serial}: {dl_out}')
  if sdk_int >= 30:
    pm_rc, pm_out = adb.shell_text(f'pm path {_TEST_PACKAGE}', timeout=10.0)
    if pm_rc == 0 and 'package:' in pm_out:
      adb.shell_text(
          f'appops set {_TEST_PACKAGE} MANAGE_EXTERNAL_STORAGE allow',
          timeout=10.0,
      )

  _, ext_storage = adb.shell_text('echo $EXTERNAL_STORAGE', timeout=10.0)
  sdcard_rc, _ = adb.shell_text('test -d /sdcard', timeout=10.0)
  if sdk_int >= 30 and ext_storage != '/sdcard' and sdcard_rc == 0:
    adb.shell_text('rm -f /data/local/tmp/cache_token', timeout=10.0)
    shim_path = os.path.join(temp_dir, 'adb_sdcard_shim.py')
    shim_source = (
        '#!/usr/bin/env python3\n'
        'import os\n'
        'import sys\n'
        f'REAL_ADB = {adb.adb_bin!r}\n'
        'PROBE = \'echo $EXTERNAL_STORAGE;\'\n'
        'args = [REAL_ADB] + [\n'
        '    a.replace(PROBE, \'echo /sdcard;\') if PROBE in a else a\n'
        '    for a in sys.argv[1:]\n'
        ']\n'
        'os.execv(REAL_ADB, args)\n')
    with open(shim_path, 'w', encoding='utf-8') as shim_file:
      shim_file.write(shim_source)
    os.chmod(shim_path, 0o755)
    return shim_path
  return None


class CsvMonitor:
  """Polls the growing on-device CSV and captures periodic/anomaly dumpsys."""

  def __init__(
      self,
      *,
      adb: AdbClient,
      remote_csv_path: str,
      dumpsys_dir: str,
      snapshot_every: int,
      anomaly_ms: float,
  ):
    self._adb = adb
    self._remote_csv_path = remote_csv_path
    self._dumpsys_dir = dumpsys_dir
    self._snapshot_every = snapshot_every
    self._anomaly_ms = anomaly_ms

    self._byte_offset = 0
    self._fieldnames: Optional[List[str]] = None
    self._captured_periodic: Set[int] = set()
    self._anomaly_iterations: Set[int] = set()
    self._captured_anomalies: Set[Tuple[int, str]] = set()
    self.periodic_iterations: List[int] = []
    self.anomaly_row_count = 0
    self.anomaly_snapshots: List[Dict[str, Any]] = []
    self._last_anomaly_mono: Optional[float] = None

    self._stop_event = threading.Event()
    self._lock = threading.Lock()
    self._thread = threading.Thread(target=self._run_loop, daemon=True)

  def start(self) -> None:
    self._thread.start()

  def stop(self) -> None:
    self._stop_event.set()
    self._thread.join(timeout=30.0)

  def _run_loop(self) -> None:
    while not self._stop_event.is_set():
      self.poll_once()
      self._stop_event.wait(_POLL_INTERVAL_SEC)

  def poll_once(self) -> None:
    """Reads newly completed remote CSV lines and triggers snapshots."""
    with self._lock:
      start_byte = self._byte_offset + 1
      cmd = (f'tail -c +{start_byte} {shlex.quote(self._remote_csv_path)}'
             ' 2>/dev/null')
      try:
        proc = self._adb.run(['exec-out', cmd], timeout=15.0, check=False)
      except (OSError, subprocess.SubprocessError):
        return
      if proc.returncode != 0 or not proc.stdout:
        return

      last_newline = proc.stdout.rfind(b'\n')
      if last_newline < 0:
        return
      complete = proc.stdout[:last_newline + 1]
      self._byte_offset += len(complete)

      for raw_line in complete.decode('utf-8', errors='replace').splitlines():
        line = raw_line.strip()
        if not line or line.startswith('#'):
          continue
        if self._fieldnames is None:
          self._fieldnames = next(csv.reader([line]))
          continue
        parsed = next(csv.DictReader([line], fieldnames=self._fieldnames))
        self._handle_row(parsed)

  def _detect_anomaly(self, row: Dict[str, str]) -> Optional[str]:
    """Returns a short reason tag if `row` is an error or latency outlier."""
    status = (row.get('status') or '').strip()
    if status and status != 'ok':
      return _sanitize_token(status)
    if self._anomaly_ms > 0:
      for column, label in _LATENCY_COLUMNS:
        raw_val = (row.get(column) or '').strip()
        if not raw_val:
          continue
        try:
          if float(raw_val) > self._anomaly_ms:
            return label
        except ValueError:
          continue
    return None

  def _handle_row(self, row: Dict[str, str]) -> None:
    """Processes one parsed CSV row for anomaly and periodic triggers."""
    try:
      iter_idx = int((row.get('iteration') or '').strip())
    except ValueError:
      return
    status = (row.get('status') or '').strip()

    reason = self._detect_anomaly(row)
    if reason is not None:
      if iter_idx not in self._anomaly_iterations:
        self._anomaly_iterations.add(iter_idx)
        self.anomaly_row_count += 1
      anomaly_key = (iter_idx, reason)
      now_mono = time.monotonic()
      allow_snapshot = (
          anomaly_key not in self._captured_anomalies and
          (len(self.anomaly_snapshots) == 0 or
           (len(self.anomaly_snapshots) < _ANOMALY_MAX_SNAPSHOTS and
            self._last_anomaly_mono is not None and
            (now_mono - self._last_anomaly_mono) >= _ANOMALY_MIN_INTERVAL_SEC)))
      if allow_snapshot:
        self._captured_anomalies.add(anomaly_key)
        self._last_anomaly_mono = now_mono
        prefix = f'anomaly_iter_{iter_idx:05d}_{reason}'
        codec_name = f'{prefix}_codec.txt'
        rm_name = f'{prefix}_resource_manager.txt'
        self._adb.dump_to_file(
            os.path.join(self._dumpsys_dir, codec_name),
            [['dumpsys', 'media.codec']],
            timeout=15.0,
        )
        self._adb.dump_to_file(
            os.path.join(self._dumpsys_dir, rm_name),
            [['dumpsys', 'media.resource_manager']],
            timeout=15.0,
        )
        self.anomaly_snapshots.append({
            'iteration': iter_idx,
            'status': status,
            'reason': reason,
            'files': [codec_name, rm_name],
        })

    completed_count = iter_idx + 1
    is_interim_hang = status.endswith('_hung')
    if (self._snapshot_every > 0 and not is_interim_hang and
        completed_count % self._snapshot_every == 0 and
        completed_count not in self._captured_periodic):
      self._captured_periodic.add(completed_count)
      self.periodic_iterations.append(completed_count)
      prefix = f'iter_{completed_count:05d}'
      self._adb.dump_to_file(
          os.path.join(self._dumpsys_dir, f'{prefix}_codec.txt'),
          [['dumpsys', 'media.codec']],
          timeout=15.0,
      )
      self._adb.dump_to_file(
          os.path.join(self._dumpsys_dir, f'{prefix}_resource_manager.txt'),
          [['dumpsys', 'media.resource_manager']],
          timeout=15.0,
      )
      self._adb.dump_to_file(
          os.path.join(self._dumpsys_dir, f'{prefix}_meminfo_pkg.txt'),
          [['dumpsys', 'meminfo', _TEST_PACKAGE]],
          timeout=15.0,
      )


def _summarize_csv(csv_path: str) -> Dict[str, Any]:
  """Summarizes authoritative per-iteration rows from the pulled results.csv."""
  summary: Dict[str, Any] = {
      'csv_present': False,
      'raw_csv_row_count': 0,
      'row_count': 0,
      'ok_count': 0,
      'failed_count': 0,
      'status_counts': {},
  }
  if not os.path.isfile(csv_path):
    return summary

  summary['csv_present'] = True
  data_lines: List[str] = []
  with open(csv_path, 'r', encoding='utf-8', errors='replace') as csv_file:
    for raw_line in csv_file:
      stripped = raw_line.strip()
      if stripped and not stripped.startswith('#'):
        data_lines.append(stripped)

  if not data_lines:
    return summary

  reader = csv.DictReader(data_lines)
  by_iteration: Dict[int, Dict[str, str]] = {}
  raw_count = 0
  for row in reader:
    raw_count += 1
    try:
      iter_idx = int((row.get('iteration') or '').strip())
    except ValueError:
      continue
    # Watchdog rows share an iteration index with the final row; the last row
    # written for an iteration is authoritative.
    by_iteration[iter_idx] = row

  status_counts: Dict[str, int] = {}
  ok_count = 0
  failed_count = 0
  for _, row in sorted(by_iteration.items()):
    status = (row.get('status') or 'unknown').strip() or 'unknown'
    status_counts[status] = status_counts.get(status, 0) + 1
    if status == 'ok':
      ok_count += 1
    else:
      failed_count += 1

  summary['raw_csv_row_count'] = raw_count
  summary['row_count'] = len(by_iteration)
  summary['ok_count'] = ok_count
  summary['failed_count'] = failed_count
  summary['status_counts'] = status_counts
  return summary


def _terminate_process_group(
    proc: Optional[subprocess.Popen],
    grace_sec: float = 5.0,
) -> Optional[int]:
  """Terminates a `start_new_session=True` process group cleanly."""
  if proc is None:
    return None
  existing_rc = proc.poll()
  if existing_rc is not None:
    return existing_rc
  try:
    os.killpg(proc.pid, signal.SIGTERM)
  except OSError:
    pass
  try:
    return proc.wait(timeout=grace_sec)
  except (OSError, subprocess.SubprocessError):
    try:
      os.killpg(proc.pid, signal.SIGKILL)
    except OSError:
      try:
        proc.kill()
      except OSError:
        pass
    try:
      return proc.wait(timeout=5.0)
    except (OSError, subprocess.SubprocessError):
      return proc.poll()


def _stop_logcat_process(proc: Optional[subprocess.Popen]) -> None:
  """Terminates the background `adb logcat` process cleanly."""
  _terminate_process_group(proc, grace_sec=5.0)


def _compute_runner_timeout(args: argparse.Namespace) -> int:
  """Computes the host shard timeout in seconds when --runner-timeout is 0."""
  if args.runner_timeout > 0:
    return args.runner_timeout
  per_iter_sec = 5.0 + (args.gap_ms / 1000.0) + (args.seeks * 1.5)
  return max(1800, int(args.iterations * per_iter_sec) + 300)


def _clean_stale_bundle_files(bundle_dir: str, dumpsys_dir: str) -> None:
  """Removes stale bundle artifacts from a prior run in the same directory."""
  for filename in (
      'results.csv',
      'logcat.txt',
      'runner.log',
      'display.txt',
      'meminfo_start.txt',
      'meminfo_end.txt',
      'manifest.json',
  ):
    path = os.path.join(bundle_dir, filename)
    if os.path.isfile(path):
      os.remove(path)
  if os.path.isdir(dumpsys_dir):
    for entry in os.listdir(dumpsys_dir):
      if entry.endswith('.txt') and (entry.startswith('iter_') or
                                     entry.startswith('anomaly_iter_')):
        os.remove(os.path.join(dumpsys_dir, entry))


def _parse_args(argv: Sequence[str]) -> argparse.Namespace:
  """Parses and validates command-line flags."""
  parser = argparse.ArgumentParser(
      description=(
          'Run SbPlayerEnduranceTest.DISABLED_CreateDestroy on an Android '
          'device and collect a diagnostic bundle.'))
  parser.add_argument(
      '-d',
      '--serial',
      '--device',
      dest='serial',
      required=True,
      help='Target adb device serial.')
  parser.add_argument(
      '--runner',
      default='out/android-arm_qa/bin/run_player_benchmarks',
      help='Path to the generated run_player_benchmarks wrapper script.')
  parser.add_argument(
      '--bundle-dir',
      required=True,
      help='Host output directory for the run bundle.')
  parser.add_argument(
      '--iterations',
      type=int,
      default=1000,
      help='Number of endurance iterations to run (default: 1000).')
  parser.add_argument(
      '--gap-ms',
      type=int,
      default=0,
      help='Sleep duration in ms between destroy and next create (default: 0).')
  parser.add_argument(
      '--video',
      default='vertical_1080p_24_fps_399_av1.dmp',
      help='Video .dmp filename, or comma-separated list of .dmp clips.')
  parser.add_argument(
      '--audio',
      default='beneath_the_canopy_aac_stereo.dmp',
      help='Audio .dmp filename, or empty string "" for video-only playback.')
  parser.add_argument(
      '--output-mode',
      choices=('punch_out', 'decode_to_texture'),
      default='punch_out',
      help='SbPlayer output mode (default: punch_out).')
  parser.add_argument(
      '--experimental-features',
      default='',
      help='Comma-separated Key=<int> Starboard experimental feature overrides.'
  )
  parser.add_argument(
      '--seeks',
      type=int,
      default=0,
      help='Number of seeks per iteration before teardown (default: 0).')
  parser.add_argument(
      '--concurrent-players',
      type=int,
      default=0,
      help='Background decode-to-texture players held open (default: 0).')
  parser.add_argument(
      '--hang-timeout-ms',
      type=int,
      default=30000,
      help='Watchdog timeout in ms per blocking player step (default: 30000).')
  parser.add_argument(
      '--runner-timeout',
      type=int,
      default=0,
      help='Shard timeout in seconds; 0 auto-scales from iterations.')
  parser.add_argument(
      '--snapshot-every',
      type=int,
      default=50,
      help='Capture periodic dumpsys every N iterations; 0 disables.')
  parser.add_argument(
      '--anomaly-ms',
      type=float,
      default=1500.0,
      help='Latency threshold in ms for anomaly snapshots; 0 disables.')
  parser.add_argument(
      '--reboot-before',
      action='store_true',
      help='Reboot the device and wait for boot completion before starting.')
  parser.add_argument(
      '--stop-package',
      action='append',
      default=[],
      help='Package name(s) (repeatable or comma-separated) to force-stop.')
  parser.add_argument('--adb', default='adb', help='Path to the adb binary.')

  args = parser.parse_args(argv)
  if args.iterations < 1:
    parser.error('--iterations must be >= 1')
  if args.gap_ms < 0:
    parser.error('--gap-ms must be >= 0')
  if not args.video.strip():
    parser.error('--video must not be empty')
  if args.seeks < 0:
    parser.error('--seeks must be >= 0')
  if args.concurrent_players < 0:
    parser.error('--concurrent-players must be >= 0')
  if args.hang_timeout_ms < 1:
    parser.error('--hang-timeout-ms must be >= 1')
  if args.runner_timeout < 0:
    parser.error('--runner-timeout must be >= 0')
  if args.snapshot_every < 0:
    parser.error('--snapshot-every must be >= 0')
  if not math.isfinite(args.anomaly_ms) or args.anomaly_ms < 0:
    parser.error('--anomaly-ms must be finite and >= 0')
  return args


def _build_runner_command(
    *,
    runner_path: str,
    args: argparse.Namespace,
    shard_timeout_sec: int,
    remote_csv_path: str,
    adb_path: str,
    json_results_path: Optional[str] = None,
) -> List[str]:
  """Builds the host `run_player_benchmarks` command line."""
  cmd = [
      runner_path,
      '-d',
      args.serial,
      '-v',
      '-t',
      str(shard_timeout_sec),
      '--num-retries',
      '0',
      '--adb-path',
      adb_path,
      '--gtest_also_run_disabled_tests',
      f'--gtest_filter={_TEST_FILTER}',
  ]
  # Note: do not pass --isolated-script-test-output here; Starboard gtest
  # binaries use plain GoogleTest rather than Chromium's TestSuite JSON writer,
  # so local_device_gtest_run.py would fail trying to pull a nonexistent JSON
  # file from the device. Pairing --json-results-file with SWARMING_SERVER=''
  # in _build_runner_env() keeps stdout gtest parsing intact while disabling
  # LocalOutputManager's 1-second TEST_RESULTS_* directory creation.
  if json_results_path:
    cmd.extend(['--json-results-file', json_results_path])

  test_flags = [
      f'--endurance_iterations={args.iterations}',
      f'--endurance_gap_ms={args.gap_ms}',
      f'--endurance_video={args.video}',
      f'--endurance_audio={args.audio}',
      f'--endurance_output_mode={args.output_mode}',
      f'--endurance_seeks={args.seeks}',
      f'--endurance_concurrent_players={args.concurrent_players}',
      f'--endurance_hang_timeout_ms={args.hang_timeout_ms}',
      f'--endurance_csv={remote_csv_path}',
  ]
  if args.experimental_features:
    test_flags.append(
        f'--endurance_experimental_features={args.experimental_features}')
  return cmd + ['--'] + test_flags


def _build_runner_env(
    base_env: Optional[Dict[str, str]] = None,) -> Dict[str, str]:
  """Builds the host runner environment avoiding LocalOutputManager races.

  Setting SWARMING_SERVER='' makes Chromium's local_utils.IsOnSwarming() return
  True without configuring LogDog or a GCS bucket, so output_manager_factory
  uses NoopArchivedFile instead of creating shared out/.../TEST_RESULTS_<1s_ts>
  directories and test_runner.py sets save_detailed_results=False.
  """
  env = dict(base_env if base_env is not None else os.environ)
  env.setdefault('SWARMING_SERVER', '')
  return env


def _default_spawn_lock_path() -> str:
  return os.path.join(
      tempfile.gettempdir(),
      f'cobalt_android_endurance_spawn_{os.getuid()}.lock',
  )


def _spawn_runner_staggered(
    *,
    runner_cmd: Sequence[str],
    runner_cwd: str,
    runner_log_file: Any,
    runner_env: Dict[str, str],
    lock_path: Optional[str] = None,
) -> subprocess.Popen:
  """Spawns the host runner under an inter-process lock with >=2s stagger."""
  resolved_lock = lock_path or _default_spawn_lock_path()
  with _INPROC_SPAWN_LOCK:
    fd = os.open(resolved_lock, os.O_CREAT | os.O_RDWR, 0o600)
    with os.fdopen(fd, 'r+b') as lock_file:
      fcntl.flock(lock_file.fileno(), fcntl.LOCK_EX)
      try:
        lock_file.seek(0)
        raw_ts = lock_file.read(64).decode('utf-8', errors='replace').strip()
        if raw_ts:
          try:
            last_mono = float(raw_ts)
            now_mono = time.monotonic()
            if 0.0 <= (now_mono - last_mono) < _SPAWN_STAGGER_SEC:
              time.sleep(_SPAWN_STAGGER_SEC - (now_mono - last_mono))
          except ValueError:
            pass

        # pylint: disable=consider-using-with
        proc = subprocess.Popen(
            list(runner_cmd),
            cwd=runner_cwd,
            env=runner_env,
            stdout=runner_log_file,
            stderr=subprocess.STDOUT,
            start_new_session=True,
        )
        spawn_mono = time.monotonic()
        lock_file.seek(0)
        lock_file.truncate()
        lock_file.write(f'{spawn_mono:.6f}\n'.encode('utf-8'))
        lock_file.flush()
        return proc
      finally:
        fcntl.flock(lock_file.fileno(), fcntl.LOCK_UN)


def _collect_device_info(adb: AdbClient) -> Dict[str, Any]:
  """Queries standard Android build and SoC properties for manifest.json."""
  sdk_str = adb.get_prop('ro.build.version.sdk')
  try:
    sdk_int = int(sdk_str)
  except ValueError:
    sdk_int = 0
  return {
      'serial': adb.serial,
      'model': adb.get_prop('ro.product.model'),
      'manufacturer': adb.get_prop('ro.product.manufacturer'),
      'device': adb.get_prop('ro.product.device'),
      'board_platform': adb.get_prop('ro.board.platform'),
      'soc_model': adb.get_prop('ro.soc.model'),
      'hardware': adb.get_prop('ro.hardware'),
      'cpu_abi': adb.get_prop('ro.product.cpu.abi'),
      'android_release': adb.get_prop('ro.build.version.release'),
      'sdk_int': sdk_int,
      'build_id': adb.get_prop('ro.build.id'),
      'build_fingerprint': adb.get_prop('ro.build.fingerprint'),
      'boot_id': adb.get_boot_id(),
  }


def main(argv: Optional[Sequence[str]] = None) -> int:
  args = _parse_args(argv if argv is not None else sys.argv[1:])

  bundle_dir = os.path.abspath(args.bundle_dir)
  dumpsys_dir = os.path.join(bundle_dir, 'dumpsys')
  os.makedirs(dumpsys_dir, exist_ok=True)
  _clean_stale_bundle_files(bundle_dir, dumpsys_dir)

  start_utc = _iso_utc_now()
  start_mono = time.monotonic()
  stop_packages = _parse_stop_packages(args.stop_package)
  shard_timeout_sec = _compute_runner_timeout(args)
  remote_csv_name = f'endurance_{os.getpid()}_{int(time.time() * 1000)}.csv'
  remote_csv_path = f'{_REMOTE_DOWNLOAD_DIR}/{remote_csv_name}'

  device_info: Dict[str, Any] = {'serial': args.serial}
  runner_cmd: List[str] = []
  logcat_proc: Optional[subprocess.Popen] = None
  runner_proc: Optional[subprocess.Popen] = None
  monitor: Optional[CsvMonitor] = None
  runner_exit_code: Optional[int] = None
  runner_timed_out = False
  runner_started = False
  adb_shim_path: Optional[str] = None
  setup_error: Optional[str] = None
  received_signal: Optional[int] = None
  adb: Optional[AdbClient] = None

  def _forward_signal(signum: int, unused_frame: Any) -> None:
    nonlocal received_signal
    received_signal = signum
    if runner_proc is not None and runner_proc.poll() is None:
      try:
        os.killpg(runner_proc.pid, signum)
      except OSError:
        pass

  prev_sigint = signal.signal(signal.SIGINT, _forward_signal)
  prev_sigterm = signal.signal(signal.SIGTERM, _forward_signal)

  try:
    try:
      adb_bin = shutil.which(args.adb) or args.adb
      if not os.path.isfile(adb_bin):
        raise FileNotFoundError(f'adb binary not found: {args.adb}')
      runner_path, runner_cwd = _resolve_runner_path(args.runner)
      adb = AdbClient(adb_bin=os.path.abspath(adb_bin), serial=args.serial)

      initial_state = adb.get_device_state(timeout=10.0)
      if initial_state != 'device':
        raise RuntimeError(_format_adb_state_error(adb.serial, initial_state))

      pre_boot_id = ''
      post_boot_id = ''
      if args.reboot_before and received_signal is None:
        pre_boot_id, post_boot_id = _reboot_and_wait(
            adb, abort_check=lambda: received_signal is not None)

      if received_signal is None:
        for pkg in stop_packages:
          adb.shell_text(f'am force-stop {shlex.quote(pkg)}', timeout=15.0)

        device_info = _collect_device_info(adb)
        if pre_boot_id:
          device_info['pre_reboot_boot_id'] = pre_boot_id
          device_info['post_reboot_boot_id'] = post_boot_id

        logcat_path = os.path.join(bundle_dir, 'logcat.txt')
        runner_log_path = os.path.join(bundle_dir, 'runner.log')
        with tempfile.TemporaryDirectory() as temp_dir, \
            open(logcat_path, 'wb') as logcat_file, \
            open(runner_log_path, 'wb') as runner_log_file:
          try:
            adb_shim_path = _prepare_external_storage(
                adb=adb,
                sdk_int=device_info['sdk_int'],
                temp_dir=temp_dir,
            )
            adb.shell_text(
                f'rm -f {shlex.quote(remote_csv_path)}', timeout=15.0)

            # Best-effort ring buffer resize + clear before streaming logcat.
            adb.run(['logcat', '-G', '16M'], timeout=10.0, check=False)
            adb.run(['logcat', '-c'], timeout=10.0, check=False)
            # pylint: disable=consider-using-with
            logcat_proc = subprocess.Popen(
                adb.base_cmd() + ['logcat', '-v', 'threadtime'],
                stdout=logcat_file,
                stderr=subprocess.STDOUT,
                start_new_session=True,
            )

            adb.dump_to_file(
                os.path.join(bundle_dir, 'display.txt'),
                [['dumpsys', 'display'], ['dumpsys', 'window', 'displays']],
                timeout=30.0,
            )
            adb.dump_to_file(
                os.path.join(bundle_dir, 'meminfo_start.txt'),
                [['dumpsys', 'meminfo']],
                timeout=30.0,
            )

            if received_signal is None:
              monitor = CsvMonitor(
                  adb=adb,
                  remote_csv_path=remote_csv_path,
                  dumpsys_dir=dumpsys_dir,
                  snapshot_every=args.snapshot_every,
                  anomaly_ms=args.anomaly_ms,
              )
              monitor.start()

              json_results_path = os.path.join(temp_dir, 'gtest_results.json')
              runner_cmd = _build_runner_command(
                  runner_path=runner_path,
                  args=args,
                  shard_timeout_sec=shard_timeout_sec,
                  remote_csv_path=remote_csv_path,
                  adb_path=adb_shim_path or adb.adb_bin,
                  json_results_path=json_results_path,
              )
              runner_env = _build_runner_env()
              print(f'[endurance] Starting run in {bundle_dir}...')
              header_line = f'Command: {shlex.join(runner_cmd)}\n\n'.encode(
                  'utf-8')
              runner_log_file.write(header_line)
              runner_log_file.flush()

              runner_started = True
              runner_proc = _spawn_runner_staggered(
                  runner_cmd=runner_cmd,
                  runner_cwd=runner_cwd,
                  runner_log_file=runner_log_file,
                  runner_env=runner_env,
              )
              if received_signal is not None and runner_proc.poll() is None:
                try:
                  os.killpg(runner_proc.pid, received_signal)
                except OSError:
                  pass
              try:
                runner_exit_code = runner_proc.wait(timeout=shard_timeout_sec +
                                                    120)
              except subprocess.TimeoutExpired:
                runner_timed_out = True
                runner_exit_code = _terminate_process_group(
                    runner_proc, grace_sec=10.0)
                try:
                  adb.shell_text(f'am force-stop {_TEST_PACKAGE}', timeout=10.0)
                except (OSError, subprocess.SubprocessError):
                  pass
          finally:
            try:
              if monitor is not None:
                monitor.stop()
                # Final synchronous sweep so a multiple landing right before
                # runner exit (for example iteration 99 -> iter_00100_*) is
                # never missed.
                monitor.poll_once()

              if runner_started and adb is not None:
                local_csv_path = os.path.join(bundle_dir, 'results.csv')
                try:
                  pull_proc = adb.run(['pull', remote_csv_path, local_csv_path],
                                      timeout=60.0,
                                      check=False)
                  if pull_proc.returncode != 0 and os.path.exists(
                      local_csv_path):
                    os.remove(local_csv_path)
                except (OSError, subprocess.SubprocessError):
                  if os.path.exists(local_csv_path):
                    os.remove(local_csv_path)

                try:
                  adb.shell_text(
                      f'rm -f {shlex.quote(remote_csv_path)}', timeout=15.0)
                except (OSError, subprocess.SubprocessError):
                  pass

                adb.dump_to_file(
                    os.path.join(bundle_dir, 'meminfo_end.txt'),
                    [['dumpsys', 'meminfo']],
                    timeout=30.0,
                )
            finally:
              _stop_logcat_process(logcat_proc)
              logcat_proc = None
    except (OSError, subprocess.SubprocessError, RuntimeError,
            TimeoutError) as exc:
      setup_error = str(exc)
      print(f'[endurance] Error: {setup_error}', file=sys.stderr)
  finally:
    _stop_logcat_process(logcat_proc)
    signal.signal(signal.SIGINT, prev_sigint)
    signal.signal(signal.SIGTERM, prev_sigterm)

  end_utc = _iso_utc_now()
  wall_duration_sec = round(time.monotonic() - start_mono, 3)
  csv_summary = _summarize_csv(os.path.join(bundle_dir, 'results.csv'))
  row_count = csv_summary['row_count']
  ok_count = csv_summary['ok_count']
  failed_count = csv_summary['failed_count']
  sec_per_iter = (
      round(wall_duration_sec / row_count, 4) if row_count > 0 else None)
  periodic_count = len(monitor.periodic_iterations) if monitor else 0
  anomaly_count = len(monitor.anomaly_snapshots) if monitor else 0

  if received_signal is not None:
    host_exit_code = 128 + received_signal
  elif setup_error is not None:
    host_exit_code = 2
  elif runner_timed_out:
    host_exit_code = 124
  elif runner_exit_code is None:
    host_exit_code = 1
  elif runner_exit_code != 0:
    host_exit_code = runner_exit_code
  elif (not csv_summary['csv_present'] or row_count != args.iterations or
        csv_summary['failed_count'] > 0):
    host_exit_code = 1
  else:
    host_exit_code = 0

  manifest = {
      'bundle_dir': bundle_dir,
      'start_time_utc': start_utc,
      'end_time_utc': end_utc,
      'wall_duration_sec': wall_duration_sec,
      'seconds_per_iteration_wall': sec_per_iter,
      'host_exit_code': host_exit_code,
      'setup_error': setup_error,
      'runner_command': runner_cmd,
      'runner_exit_code': runner_exit_code,
      'runner_timed_out': runner_timed_out,
      'shard_timeout_sec': shard_timeout_sec,
      'adb_storage_shim_used': bool(adb_shim_path),
      'args': {
          'serial': args.serial,
          'runner': args.runner,
          'iterations': args.iterations,
          'gap_ms': args.gap_ms,
          'video': args.video,
          'audio': args.audio,
          'output_mode': args.output_mode,
          'experimental_features': args.experimental_features,
          'seeks': args.seeks,
          'concurrent_players': args.concurrent_players,
          'hang_timeout_ms': args.hang_timeout_ms,
          'runner_timeout': args.runner_timeout,
          'snapshot_every': args.snapshot_every,
          'anomaly_ms': args.anomaly_ms,
          'reboot_before': args.reboot_before,
          'stop_packages': stop_packages,
      },
      'device': device_info,
      'csv': csv_summary,
      'snapshots': {
          'periodic_snapshot_count': periodic_count,
          'periodic_iterations':
              (sorted(monitor.periodic_iterations) if monitor else []),
          'anomaly_row_count': monitor.anomaly_row_count if monitor else 0,
          'anomaly_snapshot_count': anomaly_count,
          'anomaly_snapshots': monitor.anomaly_snapshots if monitor else [],
      },
  }
  manifest_path = os.path.join(bundle_dir, 'manifest.json')
  with open(manifest_path, 'w', encoding='utf-8') as manifest_file:
    json.dump(
        manifest, manifest_file, indent=2, sort_keys=False, allow_nan=False)
    manifest_file.write('\n')

  print(f'[endurance] Done in {wall_duration_sec:.1f}s: '
        f'host_rc={host_exit_code}, runner_rc={runner_exit_code}, '
        f'rows={row_count}/{args.iterations}, ok={ok_count}, '
        f'failed={failed_count}, periodic_snaps={periodic_count}, '
        f'anomaly_snaps={anomaly_count}')
  return host_exit_code


if __name__ == '__main__':
  sys.exit(main())
