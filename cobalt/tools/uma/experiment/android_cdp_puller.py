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
"""Single-launch CDP histogram puller for Storage Lab A/B experiments.

Wraps pull_uma_histogram_set_via_cdp.py with Storage Lab CUJ support:
  * Basic YouTube TV Home Cold-Start + Lifecycle Exit CUJ (--mode=homefeed or
    --mode=wpr) with 0 DPAD scrolling by default (--no-dpad-nav).
  * Kabuki url_params injection (--url-params=force_startup_screen=none,...).
  * CDP landing gate at t=14s (verifying window focus and rejecting the
    "Watch as guest" account picker).
  * Separate in-session snapshot S1 and post-exit snapshot S2 (--exit-flush via
    h5vcc.system.exit() or KEYCODE_HOME + 3s settle).
  * Warmup launch backgrounding (KEYCODE_HOME + 3s settle before force-stop so
    SimpleCache writes the-real-index and GrShaderCache flushes shaders).
  * Web Page Replay (WprServer) for deterministic replay (--mode=wpr).
"""

import argparse
import functools
import json
import os
import re
import shutil
import signal
import socket
import subprocess
import sys
import threading
import time


def _resolve_repo_with_deps(repo_root: str) -> str:
  """Resolves the main repo root when running inside a sparse git worktree."""
  if os.path.isdir(os.path.join(repo_root, 'third_party', 'catapult')):
    return repo_root
  git_path = os.path.join(repo_root, '.git')
  if os.path.isfile(git_path):
    try:
      with open(git_path, 'r', encoding='utf-8') as f:
        line = f.read().strip()
      if line.startswith('gitdir:'):
        gitdir = line.split(':', 1)[1].strip()
        main_root = os.path.abspath(os.path.join(gitdir, '..', '..', '..'))
        if os.path.isdir(os.path.join(main_root, 'third_party', 'catapult')):
          return main_root
    except OSError:
      pass
  return repo_root


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
UMA_DIR = os.path.dirname(SCRIPT_DIR)
REPOSITORY_ROOT = _resolve_repo_with_deps(
    os.path.abspath(os.path.join(UMA_DIR, '../../..')))
for _path in (
    SCRIPT_DIR,
    UMA_DIR,
    os.path.join(REPOSITORY_ROOT, 'cobalt', 'tools', 'performance'),
    os.path.join(REPOSITORY_ROOT, 'third_party', 'catapult', 'devil'),
    os.path.join(REPOSITORY_ROOT, 'third_party', 'catapult', 'telemetry',
                 'third_party', 'websocket-client'),
):
  if _path not in sys.path:
    sys.path.insert(0, _path)

from adb_command_runner import run_adb_command  # pylint: disable=wrong-import-position
import pull_uma_histogram_set_via_cdp as base_cdp  # pylint: disable=wrong-import-position
import requests  # pylint: disable=wrong-import-position
import android_uma_dump as uma_dump  # pylint: disable=wrong-import-position
import websocket  # pylint: disable=wrong-import-position

# Re-exported constants and helpers from pull_uma_histogram_set_via_cdp.py.
COBALT_DEBUG_PORT = base_cdp.COBALT_DEBUG_PORT
CDP_HOST = base_cdp.CDP_HOST
TARGET_URL = 'https://www.youtube.com/tv#/watch?v=1La4QzGeaaQ'
DEFAULT_COBALT_PACKAGE_NAME = base_cdp.DEFAULT_COBALT_PACKAGE_NAME
DEFAULT_COBALT_ACTIVITY_NAME = base_cdp.DEFAULT_COBALT_ACTIVITY_NAME
DEFAULT_POLL_INTERVAL_S = 35.0
DEFAULT_WARMUP_DURATION_S = 35.0
DEFAULT_MEMORY_METRICS_FEATURE = (
    'CobaltMetricsInterval:memory-metrics-interval/10')
ANDROID_NAV_START_DELAY_S = 12.0
ANDROID_NAV_END_TIME_S = 32.0
ANDROID_NAV_STEP_INTERVAL_S = 4.0
ANDROID_NAV_KEYCODES = (
    'KEYCODE_DPAD_DOWN',
    'KEYCODE_DPAD_RIGHT',
    'KEYCODE_DPAD_LEFT',
    'KEYCODE_DPAD_RIGHT',
    'KEYCODE_DPAD_LEFT',
    'KEYCODE_DPAD_UP',
)
MAX_WEBSOCKET_RETRIES = base_cdp.MAX_WEBSOCKET_RETRIES
WEBSOCKET_RETRY_DELAY_S = base_cdp.WEBSOCKET_RETRY_DELAY_S
COMPETING_DEVTOOLS_PACKAGES = (
    'dev.cobalt.coat',
    'com.google.android.youtube.tv',
    'org.chromium.content_shell_apk',
)

_print_q = base_cdp._print_q  # pylint: disable=protected-access
is_package_running_android = base_cdp.is_package_running_android
is_cobalt_running_linux = base_cdp.is_cobalt_running_linux
stop_package = base_cdp.stop_package
_get_histograms_from_file = base_cdp._get_histograms_from_file  # pylint: disable=protected-access
_get_chrome_guiding_metrics_for_memory = (
    base_cdp._get_chrome_guiding_metrics_for_memory)  # pylint: disable=protected-access
_get_cobalt_resident_memory_metrics = (
    base_cdp._get_cobalt_resident_memory_metrics)  # pylint: disable=protected-access


def clear_stability_metrics_pma(package_name: str, quiet: bool):
  """Removes stale BrowserStabilityMetrics .pma files before a measured run."""
  _print_q(
      'Clearing stale BrowserStabilityMetrics .pma files for '
      f"'{package_name}'...", quiet)
  run_adb_command([
      'adb', 'shell', 'run-as', package_name, 'rm', '-rf',
      'app_content_shell/BrowserStabilityMetrics'
  ])


def get_websocket_url(platform: str, port: int, quiet: bool):
  """Connects to the Cobalt DevTools endpoint, preferring non-splash pages."""
  if platform != 'android':
    return base_cdp.get_websocket_url(platform=platform, port=port, quiet=quiet)
  for i in range(MAX_WEBSOCKET_RETRIES):
    try:
      run_adb_command([
          'adb', 'forward', f'tcp:{port}',
          'localabstract:content_shell_devtools_remote'
      ])
      response = requests.get(f'http://{CDP_HOST}:{port}/json', timeout=5)
      response.raise_for_status()
      targets = response.json()
      page_targets = [
          t for t in targets
          if t.get('type') == 'page' and t.get('webSocketDebuggerUrl')
      ]
      non_splash = [
          t for t in page_targets if 'splash.html' not in t.get('url', '') and
          'splash.html' not in t.get('title', '')
      ]
      for target in (non_splash or page_targets):
        _print_q(
            'Found existing tab: ' + target.get('title', '') + ' - ' +
            target['webSocketDebuggerUrl'], quiet)
        return target['webSocketDebuggerUrl']
      _print_q('No suitable existing page found, trying to create a new tab...',
               quiet)
      new_tab_response = requests.get(
          f'http://{CDP_HOST}:{port}/json/new', timeout=5)
      new_tab_response.raise_for_status()
      new_tab_data = new_tab_response.json()
      if new_tab_data.get('webSocketDebuggerUrl'):
        _print_q(
            'Created new tab: ' + new_tab_data.get('title', '') + ' - ' +
            new_tab_data['webSocketDebuggerUrl'], quiet)
        return new_tab_data['webSocketDebuggerUrl']
      raise RuntimeError(
          'Could not find or create a suitable WebSocket debugger URL.')
    except requests.exceptions.ConnectionError:
      _print_q(
          f'Attempt {i + 1}/{MAX_WEBSOCKET_RETRIES}: '
          f'Connection error to Cobalt on {CDP_HOST}:{port}. Retrying...',
          quiet)
    except RuntimeError as e:
      _print_q(
          f'Attempt {i + 1}/{MAX_WEBSOCKET_RETRIES}: An error occurred '
          f'while getting WebSocket URL: {e}. Retrying...', quiet)
    time.sleep(WEBSOCKET_RETRY_DELAY_S)
  _print_q('Failed to get WebSocket URL after multiple retries.', quiet)
  return None


DEFAULT_WPR_POLL_INTERVAL_S = 20.0
DEFAULT_URL_PARAMS = uma_dump.DEFAULT_URL_PARAMS
URL_PARAM_PATTERN = re.compile(r'^[a-zA-Z0-9_=]*$')
DEFAULT_LANDING_GATE_S = 14.0
DEFAULT_LANDING_GATE_FAIL_TEXT = 'Watch as guest'
LANDING_GATE_EXIT_CODE = uma_dump.LANDING_GATE_EXIT_CODE
LANDING_PROBE_ATTEMPTS = 5
LANDING_SNIPPET_CHARS = 160
EXIT_FLUSH_SETTLE_S = 3.0
DEFAULT_WPR_HTTP_PORT = uma_dump.DEFAULT_WPR_HTTP_PORT
DEFAULT_WPR_HTTPS_PORT = uma_dump.DEFAULT_WPR_HTTPS_PORT
DEFAULT_WPR_HOST_RESOLVER_RULES = uma_dump.DEFAULT_WPR_HOST_RESOLVER_RULES
DEFAULT_WPR_SPKI_LIST = uma_dump.DEFAULT_WPR_SPKI_LIST
CATAPULT_WPR_DIR = os.path.join(REPOSITORY_ROOT, 'third_party', 'catapult',
                                'web_page_replay_go')
CATAPULT_TELEMETRY_BIN_DIR = os.path.join(REPOSITORY_ROOT, 'third_party',
                                          'catapult', 'telemetry', 'telemetry',
                                          'internal', 'bin')
WPR_STARTUP_TIMEOUT_S = 30.0


def _split_extra_args_tokens(extra_args: str) -> list:
  """Splits extra_args on commas outside of quotes."""
  tokens = []
  current = []
  in_quote = None
  for ch in extra_args or '':
    if ch in ('"', "'"):
      if in_quote == ch:
        in_quote = None
      elif in_quote is None:
        in_quote = ch
      current.append(ch)
    elif ch == ',' and in_quote is None:
      tokens.append(''.join(current))
      current = []
    else:
      current.append(ch)
  if current:
    tokens.append(''.join(current))
  return tokens


def _normalize_extra_args(extra_args: str) -> list:
  """Normalizes comma-separated extra commandLineArgs for Android launch."""
  normalized = []
  if extra_args:
    for raw_token in _split_extra_args_tokens(extra_args):
      cleaned = raw_token.strip()
      if not cleaned:
        continue
      is_continuation = (not cleaned.startswith('--') and
                         ('=' not in cleaned or cleaned.endswith('=')) and
                         bool(normalized))
      if is_continuation:
        if normalized[-1].startswith(
            ('--host-resolver-rules=',
             '--ignore-certificate-errors-spki-list=')):
          normalized[-1] = f'{normalized[-1]},{cleaned}'
        else:
          normalized[-1] = f'{normalized[-1]};{cleaned}'
      else:
        if not cleaned.startswith('--'):
          cleaned = f'--{cleaned}'
        normalized.append(cleaned)

  enable_idx = next(
      (i for i, arg in enumerate(normalized)
       if arg.startswith('--enable-features=')),
      None,
  )
  if enable_idx is None:
    normalized.append(f'--enable-features={DEFAULT_MEMORY_METRICS_FEATURE}')
  elif 'CobaltMetricsInterval' not in normalized[enable_idx]:
    normalized[enable_idx] = (
        f'{normalized[enable_idx]};{DEFAULT_MEMORY_METRICS_FEATURE}')
  return normalized


def build_wpr_extra_args(
    extra_args: str = '',
    spki_list: str = DEFAULT_WPR_SPKI_LIST,
    host_resolver_rules: str = DEFAULT_WPR_HOST_RESOLVER_RULES,
) -> str:
  """Appends WPR host-resolver and SPKI flags to extra_args if not present."""
  parts = [extra_args.strip()] if extra_args and extra_args.strip() else []
  existing = extra_args or ''
  clean_rules = host_resolver_rules.strip('"').strip("'")
  if '--host-resolver-rules=' not in existing:
    parts.append(f'--host-resolver-rules={clean_rules}')
  if '--ignore-certificate-errors-spki-list=' not in existing:
    parts.append(f'--ignore-certificate-errors-spki-list={spki_list}')
  return ','.join(parts)


def locate_wpr_cert_and_key(wpr_dir: str = CATAPULT_WPR_DIR,
                            include_ecdsa: bool = False) -> tuple:
  """Locates WPR certificate and private key files in Catapult."""
  rsa_cert = os.path.join(wpr_dir, 'wpr_cert.pem')
  rsa_key = os.path.join(wpr_dir, 'wpr_key.pem')
  ecdsa_cert = os.path.join(wpr_dir, 'ecdsa_cert.pem')
  ecdsa_key = os.path.join(wpr_dir, 'ecdsa_key.pem')
  if not (os.path.isfile(rsa_cert) and os.path.isfile(rsa_key)):
    raise FileNotFoundError(
        f'WPR cert/key not found under {wpr_dir} ({rsa_cert}, {rsa_key})')
  if include_ecdsa and os.path.isfile(ecdsa_cert) and os.path.isfile(ecdsa_key):
    return f'{rsa_cert},{ecdsa_cert}', f'{rsa_key},{ecdsa_key}'
  return rsa_cert, rsa_key


def locate_wpr_go_command(
    wpr_go_bin: str = '',
    wpr_dir: str = CATAPULT_WPR_DIR,
    telemetry_bin_dir: str = CATAPULT_TELEMETRY_BIN_DIR,
) -> tuple:
  """Locates a prebuilt wpr binary or wpr.go source; returns (cmd, cwd)."""
  if wpr_go_bin:
    if os.path.isfile(wpr_go_bin):
      if wpr_go_bin.endswith('.go'):
        return ['go', 'run', os.path.abspath(wpr_go_bin)], wpr_dir
      return [os.path.abspath(wpr_go_bin)], None
    resolved = shutil.which(wpr_go_bin)
    if resolved:
      return [resolved], None
    raise FileNotFoundError(f'WPR binary not found: {wpr_go_bin}')

  candidates = [
      os.path.join(telemetry_bin_dir, 'linux', 'x86_64', 'wpr'),
      os.path.join(wpr_dir, 'wpr'),
  ]
  for candidate in candidates:
    if os.path.isfile(candidate) and os.access(candidate, os.X_OK):
      return [candidate], None

  wpr_go_src = os.path.join(wpr_dir, 'src', 'wpr.go')
  if os.path.isfile(wpr_go_src):
    return ['go', 'run', wpr_go_src], wpr_dir

  raise FileNotFoundError(
      f'Could not find wpr binary in {telemetry_bin_dir} or {wpr_go_src}')


def setup_wpr_adb_reverse(http_port: int = DEFAULT_WPR_HTTP_PORT,
                          https_port: int = DEFAULT_WPR_HTTPS_PORT,
                          serial: str = None):
  """Sets up adb reverse port forwarding for WPR HTTP and HTTPS ports."""
  adb_prefix = ['adb'] + (['-s', serial] if serial else [])
  for port in (http_port, https_port):
    run_adb_command(adb_prefix + ['reverse', f'tcp:{port}', f'tcp:{port}'])


def teardown_wpr_adb_reverse(http_port: int = DEFAULT_WPR_HTTP_PORT,
                             https_port: int = DEFAULT_WPR_HTTPS_PORT,
                             serial: str = None):
  """Removes adb reverse port forwarding for WPR HTTP and HTTPS ports."""
  adb_prefix = ['adb'] + (['-s', serial] if serial else [])
  for port in (http_port, https_port):
    run_adb_command(adb_prefix + ['reverse', '--remove', f'tcp:{port}'])


class WprServer:
  """Manages Web Page Replay (record or replay) and adb reverse forwarding."""

  def __init__(self,
               mode: str,
               archive_path: str,
               http_port: int = DEFAULT_WPR_HTTP_PORT,
               https_port: int = DEFAULT_WPR_HTTPS_PORT,
               *,
               wpr_go_bin: str = '',
               serial: str = None,
               quiet: bool = False):
    if mode not in ('record', 'replay'):
      raise ValueError(f'Invalid WPR mode: {mode!r}')
    self.mode = mode
    self.archive_path = os.path.abspath(archive_path)
    self.http_port = http_port
    self.https_port = https_port
    self.wpr_go_bin = wpr_go_bin
    self.serial = serial
    self.quiet = quiet
    self.proc = None

  def _build_command(self) -> tuple:
    cmd_prefix, cwd = locate_wpr_go_command(self.wpr_go_bin)
    cert_arg, key_arg = locate_wpr_cert_and_key()
    cmd = cmd_prefix + [
        self.mode,
        f'--http_port={self.http_port}',
        f'--https_port={self.https_port}',
        f'--https_cert_file={cert_arg}',
        f'--https_key_file={key_arg}',
        '--inject_scripts=',
        self.archive_path,
    ]
    return cmd, cwd

  def _wait_until_ready(self, timeout_s: float = WPR_STARTUP_TIMEOUT_S):
    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
      if self.proc is not None and self.proc.poll() is not None:
        raise RuntimeError(
            f'WPR exited prematurely with code {self.proc.returncode}')
      try:
        with socket.create_connection(('127.0.0.1', self.http_port),
                                      timeout=0.5):
          pass
        with socket.create_connection(('127.0.0.1', self.https_port),
                                      timeout=0.5):
          return
      except OSError:
        time.sleep(0.2)
    raise TimeoutError(
        f'WPR did not listen on ports {self.http_port}/{self.https_port} '
        f'within {timeout_s}s')

  def __enter__(self):
    if self.mode == 'replay' and not os.path.isfile(self.archive_path):
      raise FileNotFoundError(
          f'WPR replay archive does not exist: {self.archive_path}')
    if self.mode == 'record':
      os.makedirs(os.path.dirname(self.archive_path) or '.', exist_ok=True)
    cmd, cwd = self._build_command()
    cmd_str = ' '.join(cmd)
    _print_q(f'Starting WPR ({self.mode}): {cmd_str}', self.quiet)
    self.proc = subprocess.Popen(
        cmd, cwd=cwd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
      self._wait_until_ready()
      setup_wpr_adb_reverse(self.http_port, self.https_port, serial=self.serial)
    except Exception:
      self.stop()
      raise
    return self

  def stop(self):
    teardown_wpr_adb_reverse(
        self.http_port, self.https_port, serial=self.serial)
    if self.proc is not None and self.proc.poll() is None:
      self.proc.send_signal(signal.SIGINT)
      try:
        self.proc.wait(timeout=10)
      except subprocess.TimeoutExpired:
        self.proc.kill()
        self.proc.wait(timeout=5)
    self.proc = None

  def __exit__(self, *exc_info):
    self.stop()
    return False


def parse_url_params(url_params: str) -> list:
  """Splits comma-separated Kabuki URL params and validates each one."""
  params = [p.strip() for p in (url_params or '').split(',') if p.strip()]
  invalid = [p for p in params if not URL_PARAM_PATTERN.match(p)]
  if invalid:
    raise ValueError(f'Invalid url_params {invalid}: each comma-separated '
                     f'param must match {URL_PARAM_PATTERN.pattern}')
  return params


def launch_cobalt(package_name: str,
                  activity_name: str,
                  url: str,
                  quiet: bool,
                  extra_args: str = '',
                  *,
                  url_params: str = '',
                  app_url: str = ''):
  """Launches the Cobalt application with url_params and app_url support."""
  cmd_args = ['--remote-allow-origins=*']
  cmd_args.extend(_normalize_extra_args(extra_args))
  has_url_arg = any(arg.startswith('--url=') for arg in cmd_args)
  params = parse_url_params(url_params)
  url_params_extra = ''
  if params:
    url_params_extra = ' --esa url_params \'' + ','.join(params) + '\''
  app_url = app_url.replace(',', '%2C')
  if package_name == DEFAULT_COBALT_PACKAGE_NAME:
    if not has_url_arg:
      cmd_args.append(f'--url={app_url or url}')
    joined_args = ','.join(cmd_args)
    command_str = (
        f'adb shell "am start -n {package_name}/{activity_name} '
        f'--esa commandLineArgs \'{joined_args}\'{url_params_extra}"')
  else:
    if app_url and not has_url_arg:
      cmd_args.append(f'--url={app_url}')
    joined_args = ','.join(cmd_args)
    command_str = (f'adb shell "am start -a android.intent.action.VIEW '
                   f'-d \'{url}\' --esa commandLineArgs \'{joined_args}\''
                   f'{url_params_extra} -p {package_name}"')
  stdout, stderr = run_adb_command(command_str, shell=True)
  if stderr:
    _print_q(f'Error launching Cobalt: {stderr}', quiet)
    return False
  _print_q(
      'Cobalt launch command sent.' +
      (f' Launch stdout: {stdout}' if stdout else ''), quiet)
  return True


class _CdpSession:
  """Minimal CDP client that matches each response to its request id."""

  def __init__(self, websocket_url: str, timeout_s: float = 15):
    self._ws = websocket.create_connection(websocket_url, timeout=timeout_s)
    self._next_id = 0

  def call(self, method: str, params: dict = None) -> dict:
    """Sends one command and returns its full response message."""
    self._next_id += 1
    message_id = self._next_id
    self._ws.send(
        json.dumps({
            'id': message_id,
            'method': method,
            'params': params or {}
        }))
    while True:
      message = json.loads(self._ws.recv())
      if message.get('id') == message_id:
        return message

  def evaluate(self, expression: str):
    """Evaluates a JS expression in the page and returns its value."""
    response = self.call('Runtime.evaluate', {
        'expression': expression,
        'returnByValue': True
    })
    return response.get('result', {}).get('result', {}).get('value')

  def close(self):
    self._ws.close()


def _find_page_websocket_url(port: int):
  """Returns the debugger URL of the first non-splash page target, or None."""
  try:
    response = requests.get(f'http://{CDP_HOST}:{port}/json', timeout=5)
    targets = response.json()
  except (requests.exceptions.RequestException, ValueError):
    return None
  for target in targets:
    if (target.get('type') == 'page' and target.get('webSocketDebuggerUrl') and
        'splash.html' not in target.get('url', '')):
      return target['webSocketDebuggerUrl']
  return None


LANDING_PROBE_EXPRESSION = """(() => {
  return JSON.stringify({
    href: location.href,
    title: document.title,
    text: document.body ? document.body.innerText : '',
  });
})()"""
EMAIL_PATTERN = re.compile(r'[\w.+-]+@[\w-]+\.[\w.]+')


def is_package_in_foreground_android(package_name: str):
  """Returns whether package_name has the window focus (None if unknown)."""
  stdout, _ = run_adb_command(
      'adb shell "dumpsys window | grep -E \'mCurrentFocus|mFocusedApp\'"',
      shell=True)
  if not stdout:
    return None
  return package_name in stdout


def probe_landing_via_cdp(platform: str, port: int, package_name: str,
                          fail_text: str) -> dict:
  """Checks which screen the app landed on via CDP Runtime.evaluate."""
  result = {
      'ok': False,
      'reason': '',
      'time': uma_dump.timestamp(),
      'href': '',
      'title': '',
      'text_len': 0,
      'snippet': '',
      'fail_text_found': False,
      'foreground': None,
  }
  if platform == 'android':
    run_adb_command([
        'adb', 'forward', f'tcp:{port}',
        'localabstract:content_shell_devtools_remote'
    ])
  value = None
  for attempt in range(LANDING_PROBE_ATTEMPTS):
    if attempt:
      time.sleep(1)
    ws_url = _find_page_websocket_url(port)
    if not ws_url:
      continue
    try:
      session = _CdpSession(ws_url, timeout_s=10)
      try:
        raw = session.evaluate(LANDING_PROBE_EXPRESSION)
      finally:
        session.close()
      value = json.loads(raw) if raw else value
    except Exception as e:  # pylint: disable=broad-exception-caught
      result['reason'] = f'CDP probe failed: {e}'
      continue
    if value and (value.get('text') or '').strip():
      break
  if platform == 'android':
    result['foreground'] = is_package_in_foreground_android(package_name)
  if value:
    text = value.get('text') or ''
    result['href'] = value.get('href') or ''
    result['title'] = value.get('title') or ''
    result['text_len'] = len(text.strip())
    snippet = EMAIL_PATTERN.sub('<email>', ' '.join(text.split()))
    result['snippet'] = snippet[:LANDING_SNIPPET_CHARS]
    result['fail_text_found'] = bool(fail_text) and fail_text in text
  if value is None:
    result['reason'] = (
        result['reason'] or 'no CDP page target besides the splash screen')
  elif result['fail_text_found']:
    result['reason'] = f'"{fail_text}" is on screen (account picker)'
  elif result['foreground'] is False:
    result['reason'] = f'{package_name} does not have the window focus'
  else:
    result['ok'] = True
    result['reason'] = ''
  return result


def _run_landing_gate(args, output_file) -> bool:
  """Runs the landing gate, records it in the dump and returns the verdict."""
  result = probe_landing_via_cdp(
      platform=args.platform,
      port=args.port,
      package_name=args.package_name,
      fail_text=args.landing_gate_fail_text)
  uma_dump.write_annotation(output_file, uma_dump.LANDING_GATE_KEY, result)
  href, snippet, reason = result['href'], result['snippet'], result['reason']
  if result['ok']:
    _print_q(f'Landing gate passed: {href} | {snippet[:80]!r}', args.quiet)
  else:
    print(f'LANDING GATE FAILED: {reason} (href={href})')
  return result['ok']


def _interact_via_cdp(websocket_url: str, histograms: list, output_file,
                      url: str, quiet: bool):
  """Queries Performance.getMetrics and Browser.getHistograms once via CDP."""
  base_cdp._interact_via_cdp(  # pylint: disable=protected-access
      websocket_url, histograms, output_file, url, quiet)


def _wait_with_android_navigation(duration_s: float,
                                  quiet: bool,
                                  stop_event=None,
                                  *,
                                  checkpoint_s: float = None,
                                  on_checkpoint=None,
                                  nav_end_s: float = ANDROID_NAV_END_TIME_S,
                                  no_dpad_nav: bool = False) -> bool:
  """Waits duration_s on Android while optionally sending DPAD keys."""
  if not isinstance(duration_s, (int, float)):
    time.sleep(duration_s)
    return True

  events = []
  if (not no_dpad_nav and nav_end_s > 0 and
      duration_s > ANDROID_NAV_START_DELAY_S):
    event_s = ANDROID_NAV_START_DELAY_S
    key_idx = 0
    while event_s < duration_s and event_s <= nav_end_s:
      events.append(
          (event_s, ANDROID_NAV_KEYCODES[key_idx % len(ANDROID_NAV_KEYCODES)]))
      key_idx += 1
      event_s += ANDROID_NAV_STEP_INTERVAL_S
  if on_checkpoint is not None and isinstance(checkpoint_s, (int, float)):
    events.append((max(0.0, min(checkpoint_s, duration_s)), None))
    events.sort(key=lambda event: (event[0], event[1] is not None))

  start = time.monotonic()
  for event_s, keycode in events:
    remaining = event_s - (time.monotonic() - start)
    if remaining > 0:
      time.sleep(remaining)
    if stop_event is not None and stop_event.is_set():
      return True
    if keycode is None:
      if not on_checkpoint():
        return False
      continue
    _print_q(f'Sending Android navigation key {keycode} at t={event_s:.0f}s',
             quiet)
    run_adb_command(['adb', 'shell', 'input', 'keyevent', keycode])
  remaining = duration_s - (time.monotonic() - start)
  if remaining > 0:
    time.sleep(remaining)
  return True


def _build_exit_flush_js() -> str:
  """Builds the JS snippet that triggers h5vcc.system.exit()."""
  return """(() => {
  try {
    if (window.h5vcc && window.h5vcc.system &&
        typeof window.h5vcc.system.exit === 'function') {
      window.h5vcc.system.exit();
      return 'h5vcc.system.exit';
    }
  } catch (e) {}
  return 'none';
})()"""


def sibling_path(output_path: str, tag: str) -> str:
  """Returns output_path with '.<tag>' inserted before its extension."""
  root, ext = os.path.splitext(output_path)
  ext = ext or '.txt'
  return f'{root}.{tag}{ext}'


def _snapshot_histograms(session: _CdpSession, histograms: list):
  """Takes a cumulative snapshot; returns (dump lines, {name: histogram})."""
  ts = uma_dump.timestamp()
  lines = []
  merged = {}
  for query in histograms:
    response = session.call('Browser.getHistograms', {'query': query})
    lines.append(uma_dump.format_line(query, response, ts))
    for h in response.get('result', {}).get('histograms', []):
      merged[h['name']] = h
  return lines, merged


def _write_snapshot(output_path: str, tag: str, lines: list):
  if output_path:
    with open(sibling_path(output_path, tag), 'w', encoding='utf-8') as f:
      f.writelines(lines)


def _format_bucket(bucket: dict) -> str:
  low, high, count = bucket['low'], bucket['high'], bucket['count']
  return f'[{low},{high})x{count}'


def _format_buckets(buckets: list) -> str:
  return ' '.join(_format_bucket(b) for b in buckets)


def _print_delta_summary(delta: dict, quiet: bool):
  for name in sorted(delta):
    h = delta[name]
    count, total = h['count'], h['sum']
    buckets = _format_buckets(h['buckets'])[:90]
    _print_q(f'    {name:56} n={count:<5} sum={total:<8} {buckets}', quiet)


def _capture_session_and_exit_flush(websocket_url: str, histograms: list, args,
                                    output_file):
  """Captures in-session snapshot S1, triggers exit, and captures S2."""
  session = _CdpSession(websocket_url)
  try:
    s1_lines, s1 = _snapshot_histograms(session, histograms)
    if output_file:
      output_file.writelines(s1_lines)
      output_file.flush()
    _write_snapshot(getattr(args, 'output_file', None), 's1', s1_lines)
    exit_js = _build_exit_flush_js()
    exit_method = session.evaluate(exit_js)
    if exit_method != 'h5vcc.system.exit':
      run_adb_command(['adb', 'shell', 'input', 'keyevent', 'KEYCODE_HOME'])
      exit_method = 'KEYCODE_HOME'
    time.sleep(EXIT_FLUSH_SETTLE_S)
    exit_info = {'method': exit_method, 'ok': True}
    try:
      s2_lines, s2 = _snapshot_histograms(session, histograms)
      _write_snapshot(getattr(args, 'output_file', None), 's2', s2_lines)
      _print_q(f'Lifecycle flush via {exit_method}; S2 - S1:', args.quiet)
      _print_delta_summary(uma_dump.snapshot_delta(s1, s2), args.quiet)
    except Exception as e:  # pylint: disable=broad-exception-caught
      exit_info = {'method': exit_method, 'ok': False, 'error': str(e)}
      _print_q(f'Exit flush S2 snapshot failed: {e}', args.quiet)
    uma_dump.write_annotation(output_file, uma_dump.EXIT_FLUSH_KEY, exit_info)
  finally:
    session.close()


def loop(websocket_url: str, stop_event, histograms: list, args, output_file):
  """Runs the data polling loop in a dedicated thread."""
  _print_q('Starting data polling thread', args.quiet)
  iteration = 0
  manage_cobalt = getattr(args, 'no_manage_cobalt', True) is False
  max_polls = getattr(args, 'max_polls', 0)
  has_max_polls = isinstance(max_polls, int) and max_polls > 0
  landing_gate_s = getattr(args, 'landing_gate_s', 0)
  has_landing_gate = (
      isinstance(landing_gate_s, (int, float)) and landing_gate_s > 0)
  while not stop_event.is_set():
    if iteration > 0 or manage_cobalt:
      if args.platform == 'android' and manage_cobalt:
        on_checkpoint = None
        if iteration == 0 and has_landing_gate:
          on_checkpoint = functools.partial(_run_landing_gate, args,
                                            output_file)
        if not _wait_with_android_navigation(
            args.poll_interval_s,
            args.quiet,
            stop_event,
            checkpoint_s=landing_gate_s,
            on_checkpoint=on_checkpoint,
            nav_end_s=getattr(args, 'nav_end_s', ANDROID_NAV_END_TIME_S),
            no_dpad_nav=getattr(args, 'no_dpad_nav', False)):
          args.landing_gate_failed = True
          stop_event.set()
          break
      else:
        time.sleep(args.poll_interval_s)
      if stop_event.is_set():
        break
      refreshed_url = get_websocket_url(
          platform=args.platform,
          port=getattr(args, 'port', COBALT_DEBUG_PORT),
          quiet=args.quiet)
      if refreshed_url:
        websocket_url = refreshed_url
    iteration += 1
    current_time_str = time.strftime('%Y-%m-%d %H:%M:%S', time.localtime())
    header_prefix = f'[{current_time_str}][Poll Iter: {iteration}]'

    app_is_running = False
    if args.platform == 'android':
      app_is_running = is_package_running_android(args.package_name)
    elif args.platform == 'linux':
      app_is_running = is_cobalt_running_linux()

    if app_is_running:
      if getattr(args, 'exit_flush', False) is True:
        _capture_session_and_exit_flush(websocket_url, histograms, args,
                                        output_file)
      else:
        _interact_via_cdp(websocket_url, histograms, output_file, args.url,
                          args.quiet)
    elif iteration % 5 == 0:
      _print_q(f'{header_prefix} App not running.', args.quiet)

    if has_max_polls and iteration >= max_polls:
      stop_event.set()
      break
  _print_q('Data polling thread stopped.', args.quiet)


def _start_and_wait_for_cobalt_android(args) -> bool:
  """Starts Cobalt on Android and waits up to 10 seconds for the process."""
  for competing_pkg in COMPETING_DEVTOOLS_PACKAGES:
    if competing_pkg != args.package_name and is_package_running_android(
        competing_pkg):
      stop_package(competing_pkg, args.quiet)
  launch_cobalt(
      package_name=args.package_name,
      activity_name=DEFAULT_COBALT_ACTIVITY_NAME,
      url=args.url,
      quiet=args.quiet,
      extra_args=getattr(args, 'extra_args', ''),
      url_params=getattr(args, 'url_params', ''),
      app_url=getattr(args, 'app_url', '') or '')
  _print_q(f'Waiting for {args.package_name} to start...', args.quiet)
  for _ in range(10):
    if is_package_running_android(args.package_name):
      _print_q(f'{args.package_name} has started.', args.quiet)
      return True
    time.sleep(1)
  _print_q(
      f'Error: Failed to start {args.package_name} within 10 seconds. '
      '\nExiting...\n', args.quiet)
  return False


def _run_warmup_cycles(args, histograms: list):
  """Executes warmup/migration launch cycles on Android before measurement."""
  warmup_runs = getattr(args, 'warmup_runs', 0)
  if not isinstance(warmup_runs, int) or warmup_runs <= 0:
    return
  warmup_duration_s = getattr(args, 'warmup_duration_s',
                              DEFAULT_WARMUP_DURATION_S)
  warmup_output_path = getattr(args, 'warmup_output_file', None)
  if warmup_output_path:
    with open(warmup_output_path, 'w', encoding='utf-8'):
      pass

  for run_idx in range(1, warmup_runs + 1):
    _print_q(
        f'\n=== Warmup Run {run_idx}/{warmup_runs} '
        f'(duration={warmup_duration_s}s) ===', args.quiet)
    if is_package_running_android(args.package_name):
      stop_package(args.package_name, args.quiet)
    clear_stability_metrics_pma(args.package_name, args.quiet)
    if not _start_and_wait_for_cobalt_android(args):
      sys.exit(1)
    _wait_with_android_navigation(
        warmup_duration_s,
        args.quiet,
        nav_end_s=getattr(args, 'nav_end_s', ANDROID_NAV_END_TIME_S),
        no_dpad_nav=getattr(args, 'no_dpad_nav', False))
    if warmup_output_path:
      ws_url = get_websocket_url(
          platform=args.platform, port=args.port, quiet=args.quiet)
      if ws_url:
        with open(warmup_output_path, 'a', encoding='utf-8') as warmup_file:
          _interact_via_cdp(ws_url, histograms, warmup_file, args.url,
                            args.quiet)
    # Background the activity before force-stopping so SimpleCache writes
    # the-real-index (INDEX_WRITE_REASON_ANDROID_STOPPED) and storage flushes.
    run_adb_command(['adb', 'shell', 'input', 'keyevent', 'KEYCODE_HOME'])
    time.sleep(EXIT_FLUSH_SETTLE_S)
    stop_package(args.package_name, args.quiet)


def _apply_mode_and_network_defaults(args):
  """Configures args for Basic YouTube TV Home CUJs (homefeed/wpr)."""
  mode = getattr(args, 'mode', None)
  network = getattr(args, 'network', 'live') or 'live'
  if mode == 'wpr' and network == 'live':
    network = 'replay'
    args.network = 'replay'

  if mode == 'homefeed':
    if not getattr(args, 'dpad_nav', False):
      args.no_dpad_nav = True
      args.nav_end_s = 0.0

  if network in ('record', 'replay'):
    args.no_dpad_nav = True
    args.nav_end_s = 0.0
    args.extra_args = build_wpr_extra_args(
        getattr(args, 'extra_args', '') or '')
    if network == 'replay':
      args.landing_gate_s = 0.0


def _run_main_inner(args, output_file, histograms: list) -> int:
  """Runs warmup, launches Cobalt, and runs the poll loop."""
  stop_event = threading.Event()
  if args.platform == 'android' and not args.no_manage_cobalt:
    _run_warmup_cycles(args, histograms)
    if is_package_running_android(args.package_name):
      stop_package(args.package_name, args.quiet)
    clear_stability_metrics_pma(args.package_name, args.quiet)
    if not _start_and_wait_for_cobalt_android(args):
      sys.exit(1)
  time.sleep(1)

  websocket_url = get_websocket_url(
      platform=args.platform, port=args.port, quiet=args.quiet)
  if not websocket_url:
    _print_q('Websocket not found. Exiting...', args.quiet)
    if args.platform == 'android' and not args.no_manage_cobalt:
      stop_package(args.package_name, args.quiet)
    return 1

  polling_thread = threading.Thread(
      target=loop,
      args=(websocket_url, stop_event, histograms, args, output_file),
      daemon=True,
  )
  polling_thread.start()

  _print_q(
      f'\nPolling every {args.poll_interval_s}s. '
      'Ctrl+C to stop & save.', args.quiet)

  try:
    while polling_thread.is_alive():
      time.sleep(1)
  except KeyboardInterrupt:
    _print_q('\nCtrl+C received. Signaling polling thread to stop...',
             args.quiet)
  except Exception as e:  # pylint: disable=broad-exception-caught
    _print_q(f'Main loop encountered an error: {e}', args.quiet)
  finally:
    stop_event.set()
    if args.platform == 'android' and not args.no_manage_cobalt:
      stop_package(args.package_name, args.quiet)
  _print_q('\nDONE!!!\n', args.quiet)
  if getattr(args, 'landing_gate_failed', False):
    return LANDING_GATE_EXIT_CODE
  return 0


def _run_main(args, output_file) -> int:
  """The main logic of the script. Returns the process exit code."""
  _apply_mode_and_network_defaults(args)
  if args.histogram_file:
    histograms = _get_histograms_from_file(args.histogram_file)
  else:
    histograms = _get_chrome_guiding_metrics_for_memory()
    histograms.extend(_get_cobalt_resident_memory_metrics())

  _print_q(f'Ensure Cobalt is running with --remote-debugging-port={args.port}',
           args.quiet)
  _print_q('-' * 50, args.quiet)

  network = getattr(args, 'network', 'live') or 'live'
  wpr_archive = getattr(args, 'wpr_archive', '') or ''
  if network in ('record', 'replay') and wpr_archive:
    with WprServer(
        mode=network,
        archive_path=wpr_archive,
        http_port=getattr(args, 'wpr_http_port', DEFAULT_WPR_HTTP_PORT),
        https_port=getattr(args, 'wpr_https_port', DEFAULT_WPR_HTTPS_PORT),
        wpr_go_bin=getattr(args, 'wpr_go_bin', '') or '',
        serial=getattr(args, 'device_id', None),
        quiet=args.quiet):
      return _run_main_inner(args, output_file, histograms)
  return _run_main_inner(args, output_file, histograms)


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--histogram-file',
      help='Path to a text file containing a list of histograms to query.')
  parser.add_argument(
      '--no-manage-cobalt',
      action='store_true',
      help='If set, the script will not start or stop the Cobalt process. '
      'This is the default behavior on Linux.')
  parser.add_argument(
      '--package-name',
      default=DEFAULT_COBALT_PACKAGE_NAME,
      help='The package name of the Cobalt application.')
  parser.add_argument(
      '-s',
      '--device-id',
      '--serial',
      dest='device_id',
      default=None,
      help='ADB device serial or host:port (sets ANDROID_SERIAL).')
  parser.add_argument(
      '--platform',
      choices=['android', 'linux'],
      default='android',
      help='The platform to run on (android or linux).')
  parser.add_argument(
      '--mode',
      choices=['homefeed', 'wpr'],
      default='homefeed',
      help='Storage Lab verification mode: "homefeed" (Basic YouTube TV Home '
      'Cold-Start + Exit CUJ with 0 scrolling) or "wpr" (via Web Page Replay).')
  parser.add_argument(
      '--poll-interval-s',
      type=float,
      default=None,
      help='The polling frequency in seconds (default: 20s for wpr mode, '
      '35s otherwise).')
  parser.add_argument(
      '--max-polls',
      type=int,
      default=1,
      help='Maximum number of poll iterations before exiting (0 = unlimited).')
  parser.add_argument(
      '--nav-end-s',
      type=float,
      default=ANDROID_NAV_END_TIME_S,
      help='Last time (seconds after launch) a scripted DPAD key may be sent '
      'on Android when DPAD navigation is enabled.')
  parser.add_argument(
      '--no-dpad-nav',
      dest='no_dpad_nav',
      action='store_true',
      default=False,
      help='Disable scripted DPAD key events on Android (default when '
      '--mode=homefeed or wpr).')
  parser.add_argument(
      '--dpad-nav',
      dest='dpad_nav',
      action='store_true',
      default=False,
      help='Enable scripted DPAD key events even in --mode=homefeed.')
  parser.add_argument(
      '--extra-args',
      default='',
      help='Optional comma-separated Chromium command-line flags appended to '
      'commandLineArgs on Android launch.')
  parser.add_argument(
      '--warmup-runs',
      type=int,
      default=0,
      help='Number of warmup/migration launch cycles to run on Android before '
      'the measured run (default: 0).')
  parser.add_argument(
      '--warmup-duration-s',
      type=float,
      default=DEFAULT_WARMUP_DURATION_S,
      help='Duration in seconds for each warmup run before stopping the '
      'package (default: 35.0, past the +30s storage cleanup task).')
  parser.add_argument(
      '--warmup-output-file',
      help='Optional file path to record a CDP histogram snapshot at the end '
      'of each warmup run.')
  parser.add_argument(
      '--port',
      type=int,
      default=COBALT_DEBUG_PORT,
      help='CDP port for Cobalt on Linux.')
  parser.add_argument(
      '--output-file', help='Path to a file to direct all output to.')
  parser.add_argument(
      '--url',
      default=TARGET_URL,
      help='The target URL for Cobalt to navigate to on launch.')
  parser.add_argument(
      '--url-params',
      default=DEFAULT_URL_PARAMS,
      help='Comma-separated Kabuki URL params passed with `--esa url_params` '
      'on Android launch and appended to the startup URL by Cobalt.')
  parser.add_argument(
      '--landing-gate-s',
      type=float,
      default=DEFAULT_LANDING_GATE_S,
      help='Seconds into the measured window at which the landing screen is '
      'checked via CDP (0 disables the gate; default: 14.0).')
  parser.add_argument(
      '--landing-gate-fail-text',
      default=DEFAULT_LANDING_GATE_FAIL_TEXT,
      help='Page text that fails the landing gate (default: '
      f'"{DEFAULT_LANDING_GATE_FAIL_TEXT}").')
  parser.add_argument(
      '--network',
      choices=['live', 'record', 'replay'],
      default='live',
      help='Network mode: "live" (default), "record", or "replay" (WPR).')
  parser.add_argument(
      '--wpr-archive',
      default='',
      help='Path to the Web Page Replay (.wprgo) archive for '
      '--network=record|replay.')
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
      '--app-url',
      default='',
      help='Android: startup URL passed as --url= in commandLineArgs.')
  parser.add_argument(
      '--exit-flush',
      dest='exit_flush',
      action='store_true',
      default=False,
      help='Capture in-session snapshot S1 before exit, trigger '
      'h5vcc.system.exit() (or KEYCODE_HOME), and capture lifecycle flush '
      'snapshot S2 (.s2.txt).')
  parser.add_argument(
      '--no-exit-flush',
      dest='exit_flush',
      action='store_false',
      help='Disable separate S1/S2 exit-flush capture.')
  parser.add_argument(
      '-q',
      '--quiet',
      action='store_true',
      help='If set, suppresses all non-essential print output.')
  args = parser.parse_args()

  if args.poll_interval_s is None:
    if args.mode == 'wpr' or args.network in ('record', 'replay'):
      args.poll_interval_s = DEFAULT_WPR_POLL_INTERVAL_S
    else:
      args.poll_interval_s = DEFAULT_POLL_INTERVAL_S

  _apply_mode_and_network_defaults(args)

  try:
    parse_url_params(args.url_params)
  except ValueError as e:
    parser.error(str(e))

  if args.device_id:
    os.environ['ANDROID_SERIAL'] = args.device_id

  if args.platform == 'linux':
    args.no_manage_cobalt = True

  if args.output_file:
    with open(args.output_file, 'w', encoding='utf-8') as f:
      exit_code = _run_main(args, f)
  else:
    exit_code = _run_main(args, None)
  if exit_code:
    sys.exit(exit_code)


if __name__ == '__main__':
  main()
