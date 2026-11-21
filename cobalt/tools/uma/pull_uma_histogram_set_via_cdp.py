#!/usr/bin/env python3
#
# Copyright 2025 The Cobalt Authors. All Rights Reserved.
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
"""Prototype to interact with Cobalt histograms via CDP."""

import argparse
import json
import os
import shlex
import sys
import time
import threading
import requests

REPOSITORY_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), '../../../'))
sys.path.append(os.path.join(REPOSITORY_ROOT, 'cobalt', 'tools', 'performance'))
sys.path.append(
    os.path.join(REPOSITORY_ROOT, 'third_party', 'catapult', 'devil'))
sys.path.append(
    os.path.join(REPOSITORY_ROOT, 'third_party', 'catapult', 'telemetry',
                 'third_party', 'websocket-client'))

import websocket  # pylint: disable=wrong-import-position
import subprocess  # pylint: disable=wrong-import-position

from adb_command_runner import run_adb_command  # pylint: disable=wrong-import-position

# Configuration
COBALT_DEBUG_PORT = 9222
CDP_HOST = '127.0.0.1'
TARGET_URL = 'https://www.youtube.com/tv#/watch?v=AB-4pS2Og1g'
DEFAULT_COBALT_PACKAGE_NAME = 'dev.cobalt.coat'
DEFAULT_COBALT_ACTIVITY_NAME = 'dev.cobalt.app.MainActivity'
DEFAULT_POLL_INTERVAL_S = 30.0
MAX_WEBSOCKET_RETRIES = 10
WEBSOCKET_RETRY_DELAY_S = 2
# Bounds every CDP socket operation, so a stalled target cannot block the
# polling thread forever.
CDP_TIMEOUT_S = 10


def _print_q(message: str, quiet: bool):
  """Prints a message if the quiet flag is not set."""
  if not quiet:
    print(message)


def is_package_running_android(package_name: str) -> bool:
  """Checks if a given Android package is currently running."""
  stdout, _ = run_adb_command(['adb', 'shell', 'pidof', package_name])
  return bool(stdout and stdout.strip().isdigit())


def is_cobalt_running_linux(process_name: str = 'cobalt') -> bool:
  """Checks if a process with a given name is running on Linux."""
  try:
    # Use pgrep to check if the process is running.
    subprocess.check_output(['pgrep', '-f', process_name])
    return True
  except (subprocess.CalledProcessError, FileNotFoundError):
    return False


def stop_package(package_name: str, quiet: bool) -> bool:
  """Attempts to force-stop an Android package."""
  _print_q(f'Attempting to stop package \'{package_name}\'...', quiet)
  _, stderr = run_adb_command(
      ['adb', 'shell', 'am', 'force-stop', package_name])
  if stderr:
    _print_q(f'Error stopping package \'{package_name}\': {stderr}', quiet)
    return False
  _print_q(f'Package \'{package_name}\' stop command issued.', quiet)
  time.sleep(2)  # Give time for the stop to propagate.
  return True


def launch_cobalt(package_name: str, activity_name: str, url: str, quiet: bool):
  """Launches the Cobalt application with a specified URL.

  With an activity name, starts that component and passes the URL as a
  `--url` switch. Without one, sends a VIEW intent for the URL to the package,
  which works for apps whose launch activity is not dev.cobalt.app.MainActivity
  (for example YouTube on Android TV).

  Returns True on success and False upon failure.
  """
  if activity_name:
    am_args = [
        'am', 'start', '-n', f'{package_name}/{activity_name}', '--esa',
        'commandLineArgs', f'--remote-allow-origins=*,--url={url}'
    ]
  else:
    am_args = [
        'am', 'start', '-a', 'android.intent.action.VIEW', '-d', url, '--esa',
        'commandLineArgs', '--remote-allow-origins=*', '-p', package_name
    ]
  # No host shell is involved. adb runs its arguments as one device shell
  # command line, so each argument is quoted for the device shell.
  stdout, stderr = run_adb_command(['adb', 'shell', shlex.join(am_args)])
  # `am start` reports a missing activity or package on stdout.
  if stderr or (stdout and 'Error' in stdout):
    _print_q(f'Error launching Cobalt: {stderr or stdout}', quiet)
    return False
  else:
    _print_q(
        'Cobalt launch command sent.' +
        (f' Launch stdout: {stdout}' if stdout else ''), quiet)
    return True


def _get_devtools_json(port: int, path: str):
  """Returns the parsed reply of a DevTools HTTP endpoint such as /json."""
  response = requests.get(
      f'http://{CDP_HOST}:{port}{path}', timeout=CDP_TIMEOUT_S)
  response.raise_for_status()
  return response.json()


def get_websocket_url(platform: str,
                      port: int,
                      quiet: bool,
                      retries: int = MAX_WEBSOCKET_RETRIES):
  """Returns the WebSocket URL of the DevTools browser target (/json/version).

  Unlike a page, the browser target answers while the page is busy or in the
  background. On Android, the port is first forwarded to the app.
  """
  for i in range(retries):
    if i:
      time.sleep(WEBSOCKET_RETRY_DELAY_S)
    try:
      if platform == 'android':
        run_adb_command([
            'adb', 'forward', f'tcp:{port}',
            'localabstract:content_shell_devtools_remote'
        ])
      version = _get_devtools_json(port, '/json/version')
      if isinstance(version, dict) and version.get('webSocketDebuggerUrl'):
        return version['webSocketDebuggerUrl']
      error = 'no webSocketDebuggerUrl in /json/version'
    except Exception as e:  # pylint: disable=broad-exception-caught
      error = e
    _print_q(
        f'Attempt {i + 1}/{retries}: DevTools on {CDP_HOST}:{port} is not '
        f'ready: {error}', quiet)
  return None


def get_page_target(port: int):
  """Returns (WebSocket URL, page URL) of the first page target, or Nones."""
  targets = _get_devtools_json(port, '/json')
  for target in targets if isinstance(targets, list) else []:
    if (isinstance(target, dict) and target.get('type') == 'page' and
        target.get('webSocketDebuggerUrl')):
      return target['webSocketDebuggerUrl'], target.get('url', '')
  return None, None


def _get_histograms_from_file(file_path: str) -> list:
  """Reads a list of histograms from a text file."""
  with open(file_path, 'r', encoding='utf-8') as f:
    return [
        line.strip() for line in f if line.strip() and not line.startswith('#')
    ]


def _get_chrome_guiding_metrics_for_memory() -> list:
  # TODO(482357006): Re-add process-specific private memory metrics (Browser,
  # Renderer, GPU) when moving to multi-process architecture.
  return [
      'Memory.Total.PrivateMemoryFootprint',
  ]


def _get_cobalt_resident_memory_metrics() -> list:
  # TODO(482357006): Re-add process-specific resident memory metrics (Browser,
  # Renderer, GPU) when moving to multi-process architecture.
  return [
      'Memory.Total.Resident',
  ]


def _receive_response(ws, message_id: int) -> dict:
  """Returns the reply to `message_id`, skipping CDP event messages."""
  while True:
    response = json.loads(ws.recv())
    if isinstance(response, dict) and response.get('id') == message_id:
      return response


def _call(ws, message_id: int, method: str, params: dict = None) -> dict:
  """Sends one CDP command and returns its result. Raises on a CDP error."""
  command = {'id': message_id, 'method': method, 'params': params or {}}
  ws.send(json.dumps(command))
  response = _receive_response(ws, message_id)
  if 'error' in response:
    raise RuntimeError(f'{method} failed: ' + str(response['error']))
  return response.get('result', {})


def fetch_histograms(websocket_url: str) -> list:
  """Returns all histograms, as one snapshot, from one Browser.getHistograms."""
  ws = websocket.create_connection(websocket_url, timeout=CDP_TIMEOUT_S)
  try:
    histograms = _call(ws, 1, 'Browser.getHistograms').get('histograms', [])
  finally:
    ws.close()
  return [h for h in histograms if isinstance(h, dict)]


def matches_query(name: str, query: str) -> bool:
  """A query ending in '*' matches a name prefix; others match exactly."""
  if query.endswith('*'):
    return name.startswith(query[:-1])
  return name == query


def filter_histograms(histograms: list, queries: list) -> list:
  """Returns (query, histograms) pairs. Each histogram matches one query."""
  reported = set()
  result = []
  for query in queries:
    matched = []
    for histogram in histograms:
      name = histogram.get('name', '')
      if name not in reported and matches_query(name, query):
        reported.add(name)
        matched.append(histogram)
    result.append((query, matched))
  return result


def _print_performance_metrics(port: int):
  """Prints the page's Performance.getMetrics. Failures only print a warning.
  """
  try:
    websocket_url, page_url = get_page_target(port)
    if not websocket_url:
      print('No page target. Skipping performance metrics.')
      return
    ws = websocket.create_connection(websocket_url, timeout=CDP_TIMEOUT_S)
    try:
      _call(ws, 1, 'Performance.enable', {'timeDomain': 'threadTicks'})
      metrics = _call(ws, 2, 'Performance.getMetrics').get('metrics', [])
    finally:
      ws.close()
  except Exception as e:  # pylint: disable=broad-exception-caught
    print(f'Performance metrics unavailable: {e}')
    return
  print(f'\nPerformance Metrics for {page_url}:')
  for metric in metrics:
    print('  ' + str(metric.get('name')) + ': ' + str(metric.get('value')))


def poll_once(args, queries: list, poll_index: int):
  """Takes one histogram snapshot. Returns (ok, text for the output file).

  A failed poll is returned as a FAILED line, so it is never dropped silently.
  """
  timestamp = time.strftime('%Y-%m-%d %H:%M:%S', time.localtime())
  try:
    if args.platform == 'android':
      app_is_running = is_package_running_android(args.package_name)
    else:
      app_is_running = is_cobalt_running_linux()
    if not app_is_running:
      raise RuntimeError('app not running')
    # The target changes when the app restarts, so look it up for every poll.
    websocket_url = get_websocket_url(
        args.platform, args.port, args.quiet, retries=1)
    if not websocket_url:
      raise RuntimeError('DevTools browser target not found')
    histograms = fetch_histograms(websocket_url)
  except Exception as e:  # pylint: disable=broad-exception-caught
    reason = ' '.join(str(e).split()) or type(e).__name__
    return False, f'# poll {poll_index} {timestamp} FAILED: {reason}\n'
  lines = [
      f'# poll {poll_index} {timestamp} OK: {len(histograms)} histograms '
      'from the browser target\n'
  ]
  for query, matched in filter_histograms(histograms, queries):
    result = json.dumps({'result': {'histograms': matched}})
    lines.append(f'{timestamp},{query},{result}\n')
  return True, ''.join(lines)


def loop(stop_event, queries: list, args, output_file, results: list):
  """Polls until `stop_event` is set, appending each poll's status to results.
  """
  _print_q('Starting data polling thread', args.quiet)
  next_poll = time.monotonic()
  while not stop_event.is_set():
    ok, text = poll_once(args, queries, len(results) + 1)
    results.append(ok)
    if output_file:
      # One write per poll, so stopping the script cannot split a poll.
      output_file.write(text)
      output_file.flush()
    if not ok:
      print(text.strip(), file=sys.stderr)
    elif not args.quiet:
      print(text)
      if not stop_event.is_set():
        _print_performance_metrics(args.port)
    # Polls start every --poll-interval-s on the monotonic clock, so the time
    # that a poll takes does not delay the later polls.
    next_poll = max(next_poll + args.poll_interval_s, time.monotonic())
    stop_event.wait(next_poll - time.monotonic())
  _print_q('Data polling thread stopped.', args.quiet)


def _run_main(args, histograms: list, output_file) -> bool:
  """The main logic of the script. Returns False if any poll failed."""
  _print_q(f'Ensure Cobalt is running with --remote-debugging-port={args.port}',
           args.quiet)
  _print_q('-' * 50, args.quiet)
  stop_event = threading.Event()

  if args.platform == 'android' and not args.no_manage_cobalt:
    if not is_package_running_android(args.package_name):
      activity_name = args.activity_name
      if activity_name is None:
        # Only the default package is known to have the default activity.
        activity_name = (
            DEFAULT_COBALT_ACTIVITY_NAME
            if args.package_name == DEFAULT_COBALT_PACKAGE_NAME else '')
      launch_cobalt(
          package_name=args.package_name,
          activity_name=activity_name,
          url=args.url,
          quiet=args.quiet)
      _print_q(f'Waiting for {args.package_name} to start...', args.quiet)
      started = False
      for _ in range(5):  # Poll for 5 seconds
        if is_package_running_android(args.package_name):
          started = True
          _print_q(f'{args.package_name} has started.', args.quiet)
          break
        time.sleep(1)

      if not started:
        _print_q(
            f'Error: Failed to start {args.package_name} within 5 seconds. '
            '\nExiting...\n', args.quiet)
        sys.exit(1)
  time.sleep(1)

  # Waits until DevTools is up. Each poll looks the target up again.
  if not get_websocket_url(
      platform=args.platform, port=args.port, quiet=args.quiet):
    print('DevTools browser target not found. Exiting...', file=sys.stderr)
    if args.platform == 'android' and not args.no_manage_cobalt:
      stop_package(args.package_name, args.quiet)
    return False

  results = []
  polling_thread = threading.Thread(
      target=loop,
      args=(stop_event, histograms, args, output_file, results),
      daemon=True,
  )
  polling_thread.start()

  _print_q(
      f'\nPolling every {args.poll_interval_s}s. '
      'Ctrl+C to stop & save.', args.quiet)

  deadline = time.time() + args.duration_s if args.duration_s > 0 else None
  try:
    while polling_thread.is_alive():
      if deadline is not None and time.time() >= deadline:
        _print_q(f'\n--duration-s {args.duration_s} reached. Stopping...',
                 args.quiet)
        break
      time.sleep(1)
  except KeyboardInterrupt:
    _print_q('\nCtrl+C received. Signaling polling thread to stop...',
             args.quiet)
  except Exception as e:  # pylint: disable=broad-exception-caught
    _print_q(f'Main loop encountered an error: {e}', args.quiet)
  finally:
    stop_event.set()
    # Lets an in-flight poll finish writing before the output file closes.
    polling_thread.join(timeout=60)
    if args.platform == 'android' and not args.no_manage_cobalt:
      stop_package(args.package_name, args.quiet)
  failed = results.count(False)
  _print_q(f'\nDONE: {len(results)} polls, {failed} failed.\n', args.quiet)
  return not failed


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--histogram-file',
      help='Path to a text file with one histogram name per line. A line '
      'ending in "*" matches every histogram with that prefix.')
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
      '--activity-name',
      default=None,
      help='(Android) Activity to start. Defaults to '
      f'{DEFAULT_COBALT_ACTIVITY_NAME} for {DEFAULT_COBALT_PACKAGE_NAME}; for '
      'other packages, the URL is sent as a VIEW intent to the package. Pass '
      'an empty string to force the VIEW intent.')
  parser.add_argument(
      '--serial',
      help='(Android) Serial of the device to use. Sets ANDROID_SERIAL for '
      'every adb command.')
  parser.add_argument(
      '--platform',
      choices=['android', 'linux'],
      default='android',
      help='The platform to run on (android or linux).')
  parser.add_argument(
      '--poll-interval-s',
      type=float,
      default=DEFAULT_POLL_INTERVAL_S,
      help='The polling frequency in seconds.')
  parser.add_argument(
      '--duration-s',
      type=float,
      default=0,
      help='Stop polling after this many seconds (default: 0, poll until '
      'Ctrl+C).')
  parser.add_argument(
      '--port',
      type=int,
      default=COBALT_DEBUG_PORT,
      help='Local DevTools port. On Android, this host port is forwarded to '
      'the app with adb forward.')
  parser.add_argument(
      '--output-file',
      help='Path to a file to write the histogram data to. An existing file '
      'is overwritten.')
  parser.add_argument(
      '--url',
      default=TARGET_URL,
      help='The target URL for Cobalt to navigate to on launch.')
  parser.add_argument(
      '-q',
      '--quiet',
      action='store_true',
      help='If set, suppresses all non-essential print output.')
  args = parser.parse_args()

  # On Linux, managing the Cobalt process is not supported by this script.
  if args.platform == 'linux':
    args.no_manage_cobalt = True
  if args.serial:
    os.environ['ANDROID_SERIAL'] = args.serial

  if args.histogram_file:
    histograms = _get_histograms_from_file(args.histogram_file)
  else:
    histograms = _get_chrome_guiding_metrics_for_memory()
    histograms.extend(_get_cobalt_resident_memory_metrics())

  if args.output_file:
    with open(args.output_file, 'w', encoding='utf-8', newline='') as f:
      ok = _run_main(args, histograms, f)
  else:
    ok = _run_main(args, histograms, None)
  if not ok:
    sys.exit(1)


if __name__ == '__main__':
  main()
