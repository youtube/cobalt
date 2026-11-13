#!/usr/bin/env python3
#
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
"""Interleaved (ABBA) A/B harness for Cobalt UMA verification on Android.

Before every launch the harness restores a golden app-data tar on the device,
then runs experiment/android_cdp_puller.py in Basic YouTube TV Home Cold-Start +
Lifecycle Exit CUJ mode (--mode homefeed or --mode wpr, 0 DPAD scrolling by
default, capturing both in-session S1 and lifecycle exit flush S2 - S1).

Runs are scheduled as ABBA pairs (A B, B A, A B, ...). --noop-pairs adds A/A
pairs (arm A flags in both slots), interleaved with the A/B pairs, so the
noise floor is measured in the same session on the same device state.

Every attempt is appended to <results-dir>/manifest.jsonl.
android_ab_report.py builds the [IMPROVED]/[REGRESSED]/[NOISE] table from the
manifest and the raw dumps.

Session hygiene:
  * --spec <json>: a checked-in experiment spec whose keys are these options
    (unknown keys are rejected); command-line flags override it. The spec,
    its sha256 and --primary-metrics (the pre-registered rows) go into
    <experiment>_session.json.
  * Provenance: the session records the sha256 of every installed APK (checked
    again at the end), the device build, the golden tar sha256, and the git
    commit and file hashes of the harness, puller and histogram list.
  * A host-side lock per adb serial, and preflight checks (--expect-version,
    free space on /data) before the first launch.
"""

import argparse
import collections
import hashlib
import json
import os
import re
import shlex
import subprocess
import sys
import time

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
UMA_DIR = os.path.dirname(SCRIPT_DIR)
if SCRIPT_DIR not in sys.path:
  sys.path.insert(0, SCRIPT_DIR)

import android_uma_dump as uma_dump  # pylint: disable=wrong-import-position

DEFAULT_PULL_SCRIPT = os.path.join(SCRIPT_DIR, 'android_cdp_puller.py')
DEFAULT_HISTOGRAM_FILE = os.path.join(UMA_DIR, 'storage_histograms.txt')
DEFAULT_PACKAGE = 'com.google.android.youtube.tv'
DEFAULT_DEVICE_TAR = '/data/local/tmp/golden_signed_in.tar'
DEFAULT_HOMEFEED_POLL_INTERVAL_S = 35.0
DEFAULT_WPR_POLL_INTERVAL_S = 20.0
DEFAULT_WARMUP_DURATION_S = 35.0
DEFAULT_WPR_HTTP_PORT = uma_dump.DEFAULT_WPR_HTTP_PORT
DEFAULT_WPR_HTTPS_PORT = uma_dump.DEFAULT_WPR_HTTPS_PORT
DEFAULT_PAIRS = 3
# Everything under /data/data/<pkg> that the golden tar captures.
APP_DATA_DIRS = ('./cache', './app_content_shell', './files', './shared_prefs',
                 './code_cache', './app_textures', './databases', './no_backup')
MEDIASHELL_PACKAGE = 'com.google.android.apps.mediashell'
MIN_HISTOGRAMS_PER_DUMP = 20
RESTORE_SETTLE_S = 2.0
RUN_TIMEOUT_MARGIN_S = 240
# Host-side per-serial locks, so two sessions never share one device.
LOCK_DIR = os.path.join(os.path.expanduser('~'), '.cache', 'cobalt_ab_harness')
# Preflight: below this, app-data restores and SQLite/LevelDB writes may fail.
MIN_FREE_DATA_MB = 200
# --spec keys that document the experiment but are not command-line options.
SPEC_ONLY_KEYS = ('description',)
# Commands enforced in DeviceSession / _SessionEnv to prevent sabrina sleep.
KEEP_AWAKE_COMMANDS = (
    'svc power stayon true',
    'settings put global stay_on_while_plugged_in 7',
    'settings put system screen_off_timeout 2147483647',
    'settings put secure sleep_timeout 2147483647',
    'settings put secure screensaver_enabled 0',
    'input keyevent KEYCODE_WAKEUP',
)


class Device:
  """Thin adb wrapper bound to one serial."""

  def __init__(self, serial: str):
    self.serial = serial

  def adb(self, *args, check: bool = True, timeout: float = 180) -> str:
    cmd = ['adb'] + (['-s', self.serial] if self.serial else []) + list(args)
    proc = subprocess.run(
        cmd, capture_output=True, text=True, timeout=timeout, check=False)
    if check and proc.returncode != 0:
      command = ' '.join(cmd)
      output = (proc.stderr or proc.stdout).strip()
      raise RuntimeError(f'`{command}` failed ({proc.returncode}): {output}')
    return proc.stdout.strip()

  def shell(self, command: str, check: bool = True, timeout: float = 180):
    return self.adb('shell', command, check=check, timeout=timeout)


def ensure_awake(device: Device):
  """Wakes the device and stops any screensaver before a launch."""
  device.shell('input keyevent KEYCODE_WAKEUP', check=False)
  device.shell('cmd dreams stop-dreaming', check=False)


class DeviceSession:
  """Keeps the device quiet and awake for the session; restores it on exit."""

  def __init__(self,
               device: Device,
               keep_awake: bool = True,
               disable_mediashell: bool = True):
    self.device = device
    self.keep_awake = keep_awake
    self.disable_mediashell = disable_mediashell
    self.restore_actions = []

  def __enter__(self):
    d = self.device
    if self.disable_mediashell:
      installed = MEDIASHELL_PACKAGE in d.shell('pm list packages', check=False)
      disabled = MEDIASHELL_PACKAGE in d.shell(
          'pm list packages -d', check=False)
      if installed and not disabled:
        d.shell(f'pm disable-user --user 0 {MEDIASHELL_PACKAGE}', check=False)
        self.restore_actions.append(f'pm enable {MEDIASHELL_PACKAGE}')
    if self.keep_awake:
      screensaver = d.shell(
          'settings get secure screensaver_enabled', check=False)
      stay_on = d.shell(
          'settings get global stay_on_while_plugged_in', check=False)
      screen_off_timeout = d.shell(
          'settings get system screen_off_timeout', check=False)
      sleep_timeout = d.shell('settings get secure sleep_timeout', check=False)
      for cmd in KEEP_AWAKE_COMMANDS:
        d.shell(cmd, check=False)
      if screensaver not in ('', 'null'):
        self.restore_actions.append(
            f'settings put secure screensaver_enabled {screensaver}')
      if stay_on not in ('', 'null'):
        self.restore_actions.append(
            f'settings put global stay_on_while_plugged_in {stay_on}')
      if screen_off_timeout not in ('', 'null'):
        self.restore_actions.append(
            f'settings put system screen_off_timeout {screen_off_timeout}')
      if sleep_timeout not in ('', 'null'):
        self.restore_actions.append(
            f'settings put secure sleep_timeout {sleep_timeout}')
    ensure_awake(d)
    return self

  def __exit__(self, *exc_info):
    for action in reversed(self.restore_actions):
      self.device.shell(action, check=False)
      print(f'[device] restored: {action}', flush=True)
    return False


_SessionEnv = DeviceSession


def restore_app_data(device: Device, package: str, device_tar: str):
  """Force-stops the app and replaces its data dirs with the golden tar."""
  device.shell(f'am force-stop {package}')
  dirs = ' '.join(APP_DATA_DIRS)
  device.shell(
      f'run-as {package} sh -c "rm -rf {dirs} && tar xf {device_tar} -C ."')
  device.shell('sync')
  time.sleep(RESTORE_SETTLE_S)


def sha256_of_file(path: str) -> str:
  digest = hashlib.sha256()
  with open(path, 'rb') as f:
    for chunk in iter(lambda: f.read(1 << 20), b''):
      digest.update(chunk)
  return digest.hexdigest()


def prepare_device_tar(device: Device, device_tar: str, host_tar: str) -> str:
  """Returns the on-device tar's sha256, pushing host_tar first if needed."""

  def device_sha():
    out = device.shell(f'sha256sum {device_tar}', check=False)
    return out.split()[0] if out and 'No such file' not in out else ''

  sha = device_sha()
  if host_tar:
    host_sha = sha256_of_file(host_tar)
    if sha != host_sha:
      print(f'[device] pushing {host_tar} -> {device_tar}', flush=True)
      device.adb('push', host_tar, device_tar)
      device.shell(f'chmod 644 {device_tar}')
      sha = device_sha()
      if sha != host_sha:
        raise RuntimeError(f'{device_tar} sha256 {sha} != host {host_sha}')
  if not sha:
    raise RuntimeError(f'{device_tar} not found on the device')
  return sha


def device_provenance(device: Device, package: str) -> dict:
  package_info = device.shell(f'dumpsys package {package}', check=False)

  def find(pattern):
    match = re.search(pattern, package_info)
    return match.group(1) if match else ''

  return {
      'serial':
          device.serial,
      'device':
          device.shell('getprop ro.product.device', check=False),
      'build_fingerprint':
          device.shell('getprop ro.build.fingerprint', check=False),
      'package':
          package,
      'version_name':
          find(r'versionName=(\S+)'),
      'version_code':
          find(r'versionCode=(\d+)'),
      'last_update_time':
          find(r'lastUpdateTime=([\d-]+ [\d:]+)'),
      'apk_sha256':
          apk_sha256s(device, package),
  }


def apk_sha256s(device: Device, package: str) -> dict:
  """Returns {apk file name: sha256} for every APK of the installed package."""
  listing = device.shell(f'pm path {package}', check=False)
  hashes = {}
  for line in listing.splitlines():
    if not line.startswith('package:'):
      continue
    path = line.split(':', 1)[1].strip()
    out = device.shell(
        f'sha256sum {shlex.quote(path)}', check=False, timeout=600).split()
    hashes[os.path.basename(path)] = out[0] if out else ''
  return hashes


def code_provenance(args) -> dict:
  """Identifies the harness code and inputs that produced the results."""
  files = {
      'android_ab_harness.py': os.path.abspath(__file__),
      'pull_script': args.pull_script,
      'android_uma_dump.py': uma_dump.__file__,
      'histogram_file': args.histogram_file,
  }
  info = {
      name: {
          'path': os.path.abspath(path),
          'sha256': sha256_of_file(path)
      } for name, path in files.items() if path and os.path.isfile(path)
  }
  try:
    head = subprocess.run(['git', '-C', SCRIPT_DIR, 'rev-parse', 'HEAD'],
                          capture_output=True,
                          text=True,
                          timeout=60,
                          check=False)
    dirty = subprocess.run(
        ['git', '-C', SCRIPT_DIR, 'status', '--porcelain', '--', '.'],
        capture_output=True,
        text=True,
        timeout=60,
        check=False)
    if head.returncode == 0:
      info['git'] = {
          'commit': head.stdout.strip(),
          'dirty': bool(dirty.stdout.strip())
      }
  except (OSError, subprocess.SubprocessError):
    pass
  return info


def acquire_device_lock(serial: str):
  """Takes an exclusive host-side lock for the device; returns the lock file."""
  import fcntl  # pylint: disable=import-outside-toplevel
  os.makedirs(LOCK_DIR, exist_ok=True)
  name = re.sub(r'[^\w.-]', '_', serial or 'default') + '.lock'
  path = os.path.join(LOCK_DIR, name)
  lock_file = open(path, 'a+', encoding='utf-8')  # pylint: disable=consider-using-with
  try:
    fcntl.flock(lock_file, fcntl.LOCK_EX | fcntl.LOCK_NB)
  except BlockingIOError as e:
    lock_file.close()
    raise RuntimeError(f'device {serial} is in use by another harness '
                       f'session (lock {path})') from e
  lock_file.seek(0)
  lock_file.truncate()
  lock_file.write(f'{os.getpid()}\n')
  lock_file.flush()
  return lock_file


def preflight_problems(device: Device, args, provenance: dict) -> list:
  """Returns reasons not to start the session (empty if it may start)."""
  problems = []
  if args.expect_version and provenance.get('version_name') != (
      args.expect_version):
    ver = provenance.get('version_name') or 'not installed'
    problems.append(
        f'{args.package_name} is {ver}, expected {args.expect_version}')
  if not provenance.get('apk_sha256'):
    problems.append(f'{args.package_name} has no installed APK')
  df_lines = device.shell('df -k /data', check=False).splitlines()
  fields = df_lines[-1].split() if df_lines else []
  if len(fields) >= 4 and fields[3].isdigit():
    free_mb = int(fields[3]) // 1024
    if free_mb < MIN_FREE_DATA_MB:
      problems.append(f'/data has {free_mb} MB free (< {MIN_FREE_DATA_MB} MB)')
  return problems


def load_spec(parser, path: str) -> dict:
  """Loads a JSON experiment spec whose keys are this script's options."""
  try:
    with open(path, encoding='utf-8') as f:
      spec = json.load(f)
  except (OSError, ValueError) as e:
    parser.error(f'--spec {path}: {e}')
  if not isinstance(spec, dict):
    parser.error(f'--spec {path}: expected a JSON object')
  known = {action.dest for action in parser._actions}  # pylint: disable=protected-access
  unknown = sorted(set(spec) - known - set(SPEC_ONLY_KEYS) - {'spec'})
  if unknown:
    parser.error(f'--spec {path}: unknown keys {unknown}')
  return spec


def build_schedule(pairs: int, noop_pairs: int) -> list:
  """Returns [(group, pair_number, (arm, arm))] in execution order."""

  def group_pairs(group, count):
    return [(group, i + 1, ('A', 'B') if i % 2 == 0 else ('B', 'A'))
            for i in range(count)]

  ab = group_pairs('ab', pairs)
  noop = group_pairs('noop', noop_pairs)
  schedule = []
  for i in range(max(pairs, noop_pairs)):
    if i < len(ab):
      schedule.append(ab[i])
    if i < len(noop):
      schedule.append(noop[i])
  return schedule


def expected_url_params(url_params: str) -> list:
  return [p.strip() for p in (url_params or '').split(',') if p.strip()]


def validate_run(mode: str,
                 dump_path: str,
                 exit_code: int,
                 url_params: str,
                 *,
                 landing_gate_s: float = 14.0,
                 network: str = 'live',
                 exit_flush: bool = False) -> dict:
  """Checks one run's output; returns {'valid', 'reason', ...details}."""
  info = {'valid': False, 'reason': '', 'landing': None}
  if not os.path.exists(dump_path):
    info['reason'] = f'no dump (exit code {exit_code})'
    return info
  histograms, annotations = uma_dump.parse_dump(dump_path)
  landing = annotations.get(uma_dump.LANDING_GATE_KEY)
  info['landing'] = landing
  if exit_code == uma_dump.LANDING_GATE_EXIT_CODE or (landing and
                                                      not landing.get('ok')):
    info['reason'] = 'landing gate: ' + (
        landing.get('reason', '') if landing else 'failed')
    return info

  skip_landing_gate = (
      mode == 'wpr' or network == 'replay' or landing_gate_s <= 0)
  if not skip_landing_gate:
    if not landing:
      info['reason'] = f'no landing-gate record (exit code {exit_code})'
      return info
    href = landing.get('href', '')
    if landing.get('text_len', 0) <= 0:
      info['reason'] = 'landing gate: empty page text'
      return info
    missing = [p for p in expected_url_params(url_params) if p not in href]
    if missing:
      info['reason'] = f'landing gate: url_params {missing} not in {href}'
      return info

  if exit_code != 0:
    info['reason'] = f'exit code {exit_code}'
    return info

  if len(histograms) < MIN_HISTOGRAMS_PER_DUMP:
    info['reason'] = f'only {len(histograms)} histograms in the dump'
    return info
  if uma_dump.EXIT_FLUSH_KEY in annotations:
    info['exit_flush'] = annotations.get(uma_dump.EXIT_FLUSH_KEY)
  if exit_flush:
    root, ext = os.path.splitext(dump_path)
    s2_path = f'{root}.s2{ext}'
    ef_info = info.get('exit_flush') or {}
    if not ef_info.get('ok') or not os.path.exists(s2_path):
      err = ef_info.get('error', 'missing S2 snapshot')
      info['reason'] = f'exit flush: {err}'
      return info
  info['valid'] = True
  return info


class Harness:
  """Runs the schedule and records every attempt in the manifest."""

  def __init__(self, args, device: Device):
    self.args = args
    self.device = device
    self.manifest_path = os.path.join(args.results_dir, 'manifest.jsonl')
    self.seq = 0

  def _append(self, record: dict):
    with open(self.manifest_path, 'a', encoding='utf-8') as f:
      f.write(json.dumps(record) + '\n')

  def _warmup_runs_for_arm(self, group: str, arm: str) -> int:
    a = self.args
    if group == 'ab' and arm == 'B' and getattr(a, 'arm_b_warmup_runs',
                                                None) is not None:
      return a.arm_b_warmup_runs
    if getattr(a, 'arm_a_warmup_runs', None) is not None and (arm == 'A' or
                                                              group == 'noop'):
      return a.arm_a_warmup_runs
    return getattr(a, 'warmup_runs', 0) or 0

  def _pull_command(self,
                    flags: str,
                    dump_path: str,
                    mode: str,
                    poll_interval_s: float,
                    *,
                    warmup_runs: int = 0) -> list:
    a = self.args
    eff_flags = flags or ''
    network = getattr(a, 'network', 'live') or 'live'
    if mode == 'wpr' and network == 'live':
      network = 'replay'
    landing_gate_s = 0.0 if (mode == 'wpr' or
                             network == 'replay') else a.landing_gate_s

    cmd = [
        a.python, a.pull_script, f'--package-name={a.package_name}',
        f'--histogram-file={a.histogram_file}', f'--extra-args={eff_flags}',
        f'--url-params={a.url_params}', f'--landing-gate-s={landing_gate_s}',
        f'--output-file={dump_path}'
    ]
    if a.serial:
      cmd.append(f'--device-id={a.serial}')
    if a.app_url:
      cmd.append(f'--app-url={a.app_url}')
    if warmup_runs > 0:
      warmup_dur = getattr(a, 'warmup_duration_s', DEFAULT_WARMUP_DURATION_S)
      cmd += [
          f'--warmup-runs={warmup_runs}',
          f'--warmup-duration-s={warmup_dur}',
      ]
    if mode == 'wpr' or network in ('record', 'replay'):
      http_port = getattr(a, 'wpr_http_port', DEFAULT_WPR_HTTP_PORT)
      https_port = getattr(a, 'wpr_https_port', DEFAULT_WPR_HTTPS_PORT)
      cmd += [
          '--mode=wpr',
          f'--network={network}',
          f'--poll-interval-s={poll_interval_s}',
          '--max-polls=1',
          '--no-dpad-nav',
          '--nav-end-s=0',
          f'--wpr-http-port={http_port}',
          f'--wpr-https-port={https_port}',
      ]
      if getattr(a, 'wpr_archive', ''):
        cmd.append(f'--wpr-archive={a.wpr_archive}')
      if getattr(a, 'wpr_go_bin', ''):
        cmd.append(f'--wpr-go-bin={a.wpr_go_bin}')
      if getattr(a, 'exit_flush', True):
        cmd.append('--exit-flush')
    else:
      cmd += [
          '--mode=homefeed',
          f'--poll-interval-s={poll_interval_s}',
          '--max-polls=1',
      ]
      if getattr(a, 'no_dpad_nav', True):
        cmd += ['--no-dpad-nav', '--nav-end-s=0']
      else:
        cmd.append('--dpad-nav')
        if a.nav_end_s is not None:
          cmd.append(f'--nav-end-s={a.nav_end_s}')
      if getattr(a, 'exit_flush', True):
        cmd.append('--exit-flush')
    return cmd

  def _launch(self, cmd: list, log_path: str, timeout_s: float) -> int:
    with open(log_path, 'w', encoding='utf-8') as log:
      log.write('$ ' + ' '.join(shlex.quote(c) for c in cmd) + '\n')
      log.flush()
      try:
        proc = subprocess.run(
            cmd,
            stdout=log,
            stderr=subprocess.STDOUT,
            timeout=timeout_s,
            check=False)
        return proc.returncode
      except subprocess.TimeoutExpired:
        log.write(f'\nTIMEOUT after {timeout_s}s\n')
        self.device.shell(
            f'am force-stop {self.args.package_name}', check=False)
        return -1

  def prewarm(self):
    """Discarded launches that bring the APK into the OS page cache."""
    a = self.args
    prewarm_mode = 'wpr' if a.mode == 'wpr' else 'homefeed'
    for i in range(1, a.prewarm_runs + 1):
      base = os.path.join(a.results_dir, f'{a.experiment}_prewarm{i}')
      ensure_awake(self.device)
      restore_app_data(self.device, a.package_name, a.device_tar)
      start = time.time()
      code = self._launch(
          self._pull_command(a.arm_a_flags, base + '.txt', prewarm_mode,
                             a.prewarm_duration_s), base + '.log',
          a.prewarm_duration_s + RUN_TIMEOUT_MARGIN_S)
      info = validate_run(
          prewarm_mode,
          base + '.txt',
          code,
          a.url_params,
          landing_gate_s=a.landing_gate_s,
          network=getattr(a, 'network', 'live'))
      self._append({
          'type': 'prewarm',
          'experiment': a.experiment,
          'run_id': os.path.basename(base),
          'exit_code': code,
          'duration_s': round(time.time() - start, 1),
          'landing': info.get('landing'),
          'note': info['reason'],
      })
      note = ' | ' + info['reason'] if info['reason'] else ''
      print(
          f'[prewarm {i}] exit {code}, {time.time() - start:.0f}s{note}',
          flush=True)

  def run_slot(self, group: str, pair: int, slot: int, arm: str) -> dict:
    """Runs one arm of one pair, retrying invalid runs up to --max-attempts."""
    a = self.args
    flags = a.arm_b_flags if (group == 'ab' and arm == 'B') else a.arm_a_flags
    label = a.arm_b_label if (group == 'ab' and arm == 'B') else a.arm_a_label
    warmup_runs = self._warmup_runs_for_arm(group, arm)
    warmup_extra_s = warmup_runs * getattr(a, 'warmup_duration_s',
                                           DEFAULT_WARMUP_DURATION_S)
    run_id = f'{a.experiment}_{group}_p{pair:02d}_{arm}'
    timeout_s = a.poll_interval_s + warmup_extra_s + RUN_TIMEOUT_MARGIN_S
    record = {}
    attempt = failures = 0
    while True:
      attempt += 1
      self.seq += 1
      base = os.path.join(a.results_dir,
                          run_id + (f'_try{attempt}' if attempt > 1 else ''))
      ensure_awake(self.device)
      restore_app_data(self.device, a.package_name, a.device_tar)
      start = time.time()
      started_at = uma_dump.timestamp()
      code = self._launch(
          self._pull_command(
              flags,
              base + '.txt',
              a.mode,
              a.poll_interval_s,
              warmup_runs=warmup_runs), base + '.log', timeout_s)
      info = validate_run(
          a.mode,
          base + '.txt',
          code,
          a.url_params,
          landing_gate_s=a.landing_gate_s,
          network=getattr(a, 'network', 'live'),
          exit_flush=bool(getattr(a, 'exit_flush', False)))
      record = {
          'type': 'run',
          'experiment': a.experiment,
          'mode': a.mode,
          'group': group,
          'pair': pair,
          'slot': slot,
          'arm': arm,
          'label': label,
          'flags': flags,
          'warmup_runs': warmup_runs,
          'run_id': run_id,
          'attempt': attempt,
          'seq': self.seq,
          'started_at': started_at,
          'duration_s': round(time.time() - start, 1),
          'exit_code': code,
          'dump': os.path.basename(base + '.txt'),
          'log': os.path.basename(base + '.log'),
      }
      record.update(info)
      self._append(record)
      landing = info.get('landing') or {}
      detail = info['reason'] if info['reason'] else f'{a.mode} ok'
      verdict = 'OK' if info['valid'] else 'INVALID'
      duration_s = record['duration_s']
      snippet = (landing.get('snippet') or '')[:50]
      print(
          f'[{self.seq:02d}] {run_id} ({label}) try {attempt}: {verdict} '
          f'{duration_s:.0f}s | {detail} | landing {snippet!r}',
          flush=True)
      if info['valid']:
        return record
      failures += 1
      if failures >= a.max_attempts:
        return record

  def run(self):
    a = self.args
    queue = collections.deque(build_schedule(a.pairs, a.noop_pairs))
    planned = 2 * len(queue)
    print(
        f'Schedule: {len(queue)} pairs ({planned} runs), mode {a.mode}: ' +
        ' '.join(f'{g}{p}:{o[0]}{o[1]}' for g, p, o in queue),
        flush=True)
    while queue:
      group, pair, order = queue.popleft()
      for slot, arm in enumerate(order):
        record = self.run_slot(group, pair, slot, arm)
        if not record.get('valid'):
          run_id, reason = record.get('run_id'), record.get('reason')
          launches = record.get('attempt')
          raise RuntimeError(
              f'{run_id} gave up after {launches} launches ({reason}); '
              'if the landing gate keeps failing, re-capture the golden tar.')
      self._append({
          'type': 'pair',
          'experiment': a.experiment,
          'mode': a.mode,
          'group': group,
          'pair': pair,
          'order': ''.join(order),
      })


def parse_args(argv=None):
  parser = argparse.ArgumentParser(
      description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
  parser.add_argument(
      '--spec',
      help='JSON experiment spec: an object whose keys are the dest names of '
      'these options (e.g. "pairs", "arm_b_flags"), plus "description". '
      'Unknown keys are rejected; command-line flags override the spec.')
  parser.add_argument('--results-dir', help='Required (flag or spec).')
  parser.add_argument(
      '--experiment',
      help='Name of this experiment; prefixes run ids in the manifest. '
      'Required (flag or spec).')
  parser.add_argument(
      '--primary-metrics',
      default='',
      help='Pre-registered primary rows, comma-separated "Histogram" or '
      '"Histogram:stat" (stat: count, sum, mean, value, or enum bucket name), '
      'recorded in the session for android_ab_report.py.')
  parser.add_argument(
      '--expect-version',
      default='',
      help='Preflight: abort before the first launch unless the installed '
      'package has this versionName.')
  parser.add_argument(
      '--nav-end-s',
      type=float,
      default=None,
      help='Last time (s after launch) the puller sends a scripted DPAD key '
      'when --dpad-nav is enabled.')
  parser.add_argument(
      '--no-dpad-nav',
      dest='no_dpad_nav',
      action='store_true',
      default=True,
      help='Disable scripted DPAD key navigation (default: True across all '
      'Storage Lab recipes for deterministic request counts).')
  parser.add_argument(
      '--dpad-nav',
      dest='no_dpad_nav',
      action='store_false',
      help='Enable scripted DPAD key navigation in homefeed mode.')
  parser.add_argument(
      '--mode',
      choices=('homefeed', 'wpr'),
      default='homefeed',
      help='Verification recipe mode: "homefeed" (Basic YouTube TV Home '
      'Cold-Start + Exit CUJ) or "wpr" (via Web Page Replay).')
  parser.add_argument(
      '--pairs',
      type=int,
      default=DEFAULT_PAIRS,
      help=f'Number of ABBA A/B pairs (default: {DEFAULT_PAIRS}).')
  parser.add_argument(
      '--noop-pairs',
      type=int,
      default=0,
      help='Number of interleaved No-Op (A/A) pairs.')
  parser.add_argument('--arm-a-flags', default='')
  parser.add_argument('--arm-b-flags', default='')
  parser.add_argument('--arm-a-label', default='Control')
  parser.add_argument('--arm-b-label', default='Treatment')
  parser.add_argument(
      '--warmup-runs',
      type=int,
      default=0,
      help='Number of warmup/cleanup launch cycles per measured slot before '
      'the measured launch (passed to android_cdp_puller.py).')
  parser.add_argument(
      '--arm-a-warmup-runs',
      type=int,
      default=None,
      help='Override --warmup-runs for Arm A (Control) slots.')
  parser.add_argument(
      '--arm-b-warmup-runs',
      type=int,
      default=None,
      help='Override --warmup-runs for Arm B (Treatment) slots.')
  parser.add_argument(
      '--warmup-duration-s',
      type=float,
      default=DEFAULT_WARMUP_DURATION_S,
      help='Duration in seconds for each warmup launch (default: 35.0).')
  parser.add_argument(
      '--serial',
      default=os.environ.get('ANDROID_SERIAL'),
      help='adb serial (default: $ANDROID_SERIAL).')
  parser.add_argument('--package-name', default=DEFAULT_PACKAGE)
  parser.add_argument(
      '--device-tar',
      default=DEFAULT_DEVICE_TAR,
      help='Golden app-data tar on the device, restored before every run.')
  parser.add_argument(
      '--host-tar',
      help='Host copy of the golden tar; pushed if the device copy differs.')
  parser.add_argument(
      '--final-restore-tar',
      help='Device tar to restore when the session ends.')
  parser.add_argument('--histogram-file', default=DEFAULT_HISTOGRAM_FILE)
  parser.add_argument('--url-params', default=uma_dump.DEFAULT_URL_PARAMS)
  parser.add_argument(
      '--app-url', default='', help='Custom startup URL for every launch.')
  parser.add_argument('--landing-gate-s', type=float, default=None)
  parser.add_argument('--poll-interval-s', type=float, default=None)
  parser.add_argument(
      '--network',
      choices=('live', 'record', 'replay'),
      default='live',
      help='Network mode: "live" (default), "record", or "replay" (WPR).')
  parser.add_argument(
      '--wpr-archive',
      default='',
      help='Path to the Web Page Replay (.wprgo) archive for '
      '--mode=wpr or --network=record|replay.')
  parser.add_argument(
      '--wpr-go-bin',
      default='',
      help='Optional path to the wpr binary or wpr.go source file.')
  parser.add_argument(
      '--wpr-http-port',
      type=int,
      default=DEFAULT_WPR_HTTP_PORT,
      help=f'Host/device HTTP port for WPR (default: {DEFAULT_WPR_HTTP_PORT}).')
  parser.add_argument(
      '--wpr-https-port',
      type=int,
      default=DEFAULT_WPR_HTTPS_PORT,
      help='Host/device HTTPS port for WPR (default: '
      f'{DEFAULT_WPR_HTTPS_PORT}).')
  parser.add_argument(
      '--exit-flush',
      dest='exit_flush',
      action='store_true',
      default=True,
      help='Capture in-session snapshot S1 before exit, trigger '
      'h5vcc.system.exit() (or KEYCODE_HOME), and capture lifecycle flush '
      'snapshot S2 (.s2.txt) (default: True).')
  parser.add_argument(
      '--no-exit-flush',
      dest='exit_flush',
      action='store_false',
      help='Disable separate S1/S2 exit-flush capture.')
  parser.add_argument('--prewarm-runs', type=int, default=1)
  parser.add_argument('--prewarm-duration-s', type=float, default=20.0)
  parser.add_argument(
      '--max-attempts',
      type=int,
      default=3,
      help='Per slot, invalid launches before the session aborts (default: 3).')
  parser.add_argument(
      '--no-keep-awake',
      dest='keep_awake',
      action='store_false',
      help='Do not disable the screensaver / enable stay-awake.')
  parser.add_argument(
      '--keep-mediashell',
      dest='disable_mediashell',
      action='store_false',
      help='Do not disable cast_shell during the session.')
  parser.add_argument('--pull-script', default=DEFAULT_PULL_SCRIPT)
  parser.add_argument(
      '--python',
      default=sys.executable,
      help='Interpreter for the puller (default: this one).')
  parser.add_argument(
      '--dry-run', action='store_true', help='Print the schedule and exit.')
  spec = {}
  pre_parser = argparse.ArgumentParser(add_help=False)
  pre_parser.add_argument('--spec')
  spec_path = pre_parser.parse_known_args(argv)[0].spec
  if spec_path:
    spec = load_spec(parser, spec_path)
    parser.set_defaults(**{
        k: v for k, v in spec.items() if k not in SPEC_ONLY_KEYS
    })
  args = parser.parse_args(argv)
  args.spec_doc = spec
  if isinstance(args.primary_metrics, (list, tuple)):
    args.primary_metrics = ','.join(args.primary_metrics)
  for required in ('results_dir', 'experiment'):
    if not getattr(args, required):
      flag_name = required.replace('_', '-')
      parser.error(f'--{flag_name} is required (flag or spec)')

  if args.mode == 'wpr' and args.network == 'live':
    args.network = 'replay'

  if args.poll_interval_s is None:
    if args.mode == 'wpr' or args.network in ('record', 'replay'):
      args.poll_interval_s = DEFAULT_WPR_POLL_INTERVAL_S
    else:
      args.poll_interval_s = DEFAULT_HOMEFEED_POLL_INTERVAL_S

  is_wpr = args.mode == 'wpr' or args.network in ('record', 'replay')

  if args.landing_gate_s is None:
    if args.mode == 'wpr' or args.network == 'replay':
      args.landing_gate_s = 0.0
    else:
      args.landing_gate_s = 14.0

  if is_wpr:
    args.no_dpad_nav = True
    args.nav_end_s = 0.0

  return args


def _write_json(path: str, data: dict):
  with open(path, 'w', encoding='utf-8') as f:
    json.dump(data, f, indent=1)


def main(argv=None) -> int:
  args = parse_args(argv)
  if args.dry_run:
    for group, pair, order in build_schedule(args.pairs, args.noop_pairs):
      flags = [
          args.arm_b_flags if
          (group == 'ab' and arm == 'B') else args.arm_a_flags for arm in order
      ]
      print(f'{group} pair {pair}: {order[0]}={flags[0]!r}, '
            f'{order[1]}={flags[1]!r}')
    return 0
  os.makedirs(args.results_dir, exist_ok=True)
  existing = os.path.join(args.results_dir, f'{args.experiment}_session.json')
  if os.path.exists(existing):
    print(f'{existing} exists: dumps would be overwritten while the manifest '
          'appends. Use a new --experiment or --results-dir.')
    return 2
  device = Device(args.serial)
  device.adb('get-state')
  lock = acquire_device_lock(args.serial)  # pylint: disable=unused-variable
  config = {
      k: v for k, v in vars(args).items() if k not in ('python', 'spec_doc')
  }
  session_path = os.path.join(args.results_dir,
                              f'{args.experiment}_session.json')
  session = {
      'experiment': args.experiment,
      'started_at': uma_dump.timestamp(),
      'config': config,
      'primary_metrics': [
          m.strip() for m in args.primary_metrics.split(',') if m.strip()
      ],
      'spec': args.spec_doc or None,
      'spec_sha256': sha256_of_file(args.spec) if args.spec else None,
      'code': code_provenance(args),
      'device': device_provenance(device, args.package_name),
  }
  problems = preflight_problems(device, args, session['device'])
  if problems:
    session.update({'status': 'preflight failed', 'problems': problems})
    _write_json(session_path, session)
    print('PREFLIGHT FAILED:\n  ' + '\n  '.join(problems), flush=True)
    return 2
  session['device_tar_sha256'] = prepare_device_tar(device, args.device_tar,
                                                    args.host_tar)
  session['status'] = 'running'
  _write_json(session_path, session)
  print(json.dumps(session, indent=1), flush=True)
  harness = Harness(args, device)
  status = 'ok'
  start = time.time()
  try:
    with DeviceSession(device, args.keep_awake, args.disable_mediashell):
      try:
        harness.prewarm()
        harness.run()
      finally:
        if args.final_restore_tar:
          restore_app_data(device, args.package_name, args.final_restore_tar)
          device.shell('input keyevent KEYCODE_HOME', check=False)
          print(f'[device] restored {args.final_restore_tar}', flush=True)
        else:
          device.shell(f'am force-stop {args.package_name}', check=False)
  except (Exception, KeyboardInterrupt) as e:  # pylint: disable=broad-exception-caught
    status = f'aborted: {e!r}'
    print(f'ABORTED: {e!r}', flush=True)
  try:
    apk_end = apk_sha256s(device, args.package_name)
  except (RuntimeError, OSError, subprocess.SubprocessError) as e:
    apk_end = {'error': repr(e)}
  session.update({
      'finished_at': uma_dump.timestamp(),
      'duration_s': round(time.time() - start),
      'status': status,
      'runs': harness.seq,
      'apk_unchanged': apk_end == session['device']['apk_sha256'],
  })
  if not session['apk_unchanged']:
    print(
        f'WARNING: the installed APK changed during the session: {apk_end}',
        flush=True)
  _write_json(session_path, session)
  print(f'Session {status}: {harness.seq} launches in '
        f'{(time.time() - start) / 60:.1f} min; manifest: '
        f'{harness.manifest_path}')
  return 0 if status == 'ok' else 1


if __name__ == '__main__':
  sys.exit(main())
