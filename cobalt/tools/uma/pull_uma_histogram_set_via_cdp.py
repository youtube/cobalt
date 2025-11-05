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
import sys
import time
import threading
import requests
import websocket  # Need websocket-client

REPOSITORY_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), '../../../'))
sys.path.append(os.path.join(REPOSITORY_ROOT, 'cobalt', 'tools', 'performance'))
sys.path.append(
    os.path.join(REPOSITORY_ROOT, 'third_party', 'catapult', 'devil'))

from adb_command_runner import run_adb_command  # pylint: disable=wrong-import-position
from devil.android import device_utils  # pylint: disable=wrong-import-position

# Configuration
COBALT_DEBUG_PORT = 9222
CDP_HOST = '127.0.0.1'
TARGET_URL = 'https://www.youtube.com/tv#/watch?v=AB-4pS2Og1g'
DEFAULT_COBALT_PACKAGE_NAME = 'dev.cobalt.coat'
DEFAULT_COBALT_ACTIVITY_NAME = 'dev.cobalt.app.MainActivity'
DEFAULT_POLL_INTERVAL_S = 30.0


def is_package_running(package_name: str) -> bool:
  """Checks if a given Android package is currently running."""
  stdout, _ = run_adb_command(['adb', 'shell', 'pidof', package_name])
  return bool(stdout and stdout.strip().isdigit())


def stop_package(package_name: str) -> bool:
  """Attempts to force-stop an Android package."""
  print(f'Attempting to stop package \'{package_name}\'...')
  _, stderr = run_adb_command(
      ['adb', 'shell', 'am', 'force-stop', package_name])
  if stderr:
    print(f'Error stopping package \'{package_name}\': {stderr}')
    return False
  print(f'Package \'{package_name}\' stop command issued.')
  time.sleep(2)  # Give time for the stop to propagate.
  return True


def launch_cobalt(package_name: str, activity_name: str):
  """Launches the Cobalt application with a specified URL.
       Returns True on success and False upon failure."""
  command_str = (f'adb shell am start -n {package_name}/{activity_name} '
                 f'--esa commandLineArgs \'--remote-allow-origins=*,'
                 f'--url=\"{TARGET_URL}\"\'')
  stdout, stderr = run_adb_command(command_str, shell=True)
  if stderr:
    print(f'Error launching Cobalt: {stderr}')
    return False
  else:
    print('Cobalt launch command sent.' +
          (f' Launch stdout: {stdout}' if stdout else ''))
    return True


def get_websocket_url():
  """
    Connects to the Cobalt DevTools Protocol endpoint to get the WebSocket URL
    for the first available tab (or a new tab if one isn\'t open).
    """
  try:
    devs = list(device_utils.DeviceUtils.HealthyDevices())
    dev = devs[0]
    dev.adb.Forward(
        'tcp:9222',
        'localabstract:content_shell_devtools_remote',
        allow_rebind=True)
    response = requests.get(
        f'http://{CDP_HOST}:{COBALT_DEBUG_PORT}/json', timeout=5)
    response.raise_for_status()  # Raise an exception for bad status codes
    targets = response.json()

    # Find a suitable target (e.g., page type)
    for target in targets:
      if target.get('type') == 'page' and target.get('webSocketDebuggerUrl'):
        print('Found existing tab: ' + target['title'] + ' - ' +
              target['webSocketDebuggerUrl'])
        return target['webSocketDebuggerUrl']

    # If no suitable page target is found, create a new one
    print('No suitable existing page found, trying to create a new tab...')
    new_tab_url = f'http://{CDP_HOST}:{COBALT_DEBUG_PORT}/json/new'
    new_tab_response = requests.get(new_tab_url, timeout=5)
    new_tab_response.raise_for_status()
    new_tab_data = new_tab_response.json()
    if new_tab_data.get('webSocketDebuggerUrl'):
      print('Created new tab: ' + new_tab_data['title'] + ' - ' +
            new_tab_data['webSocketDebuggerUrl'])
      return new_tab_data['webSocketDebuggerUrl']

    raise RuntimeError(
        'Could not find or create a suitable WebSocket debugger URL.')

  except requests.exceptions.ConnectionError:
    print(
        f'Error: Could not connect to Cobalt on {CDP_HOST}:{COBALT_DEBUG_PORT}.'
    )
    print('Please ensure Cobalt is running with --remote-debugging-port=9222.')
    return None
  except Exception as e:  # pylint: disable=broad-exception-caught
    print(f'An error occurred while getting WebSocket URL: {e}')
    return None


def _get_histograms_from_file(file_path: str) -> list:
  """Reads a list of histograms from a text file."""
  with open(file_path, 'r', encoding='utf-8') as f:
    return [
        line.strip() for line in f if line.strip() and not line.startswith('#')
    ]


def _get_chrome_guiding_metrics() -> list:
  metrics = []
  metrics.append('Memory.PartitionAlloc.MemoryReclaim')
  metrics.append('Memory.PartitionAlloc.PartitionRoot.ExtrasSize')
  metrics.append('Memory.PartitionAlloc.PeriodicPurge')
  return metrics


def _print_cobalt_histogram_names(ws, message_id: int, histograms: list,
                                  output_file):
  """Prints histogram data to the console and writes it to a file."""
  print('\n' + '-' * 50 + '\n')
  for metric in histograms:
    command = {
        'id': message_id,
        'method': 'Browser.getHistograms',
        'params': {
            'query': metric
        }
    }
    ws.send(json.dumps(command))
    response = json.loads(ws.recv())
    message_id += 1
    print(f'\nMetric - {metric}:\n{response}\n')
    if output_file:
      timestamp = time.strftime('%Y-%m-%d %H:%M:%S', time.localtime())
      output_file.write(f'{timestamp},{metric},{json.dumps(response)}\n')
      output_file.flush()
  print('\n' + '-' * 50 + '\n')


def _interact_via_cdp(websocket_url: str, histograms: list, output_file):
  ws = None
  try:
    ws = websocket.create_connection(websocket_url)
    print(f'Connected to WebSocket: {websocket_url}')

    message_id = 1
    command = {
        'id': message_id,
        'method': 'Performance.enable',
        'params': {
            'timeDomain': 'threadTicks'
        }
    }
    ws.send(json.dumps(command))
    response = json.loads(ws.recv())
    print('Enabled Performance domain (ID: ' + str(response['id']) + ')')
    message_id += 1

    command = {
        'id': message_id,
        'method': 'Performance.getMetrics',
        'params': {}
    }
    ws.send(json.dumps(command))
    response = json.loads(ws.recv())
    message_id += 1

    if 'result' in response and 'metrics' in response['result']:
      print(f'\nPerformance Metrics for {TARGET_URL}:')
      for metric in response['result']['metrics']:
        print('  ' + metric['name'] + ': ' + metric['value'])
    else:
      print(f'Could not retrieve metrics: {response}')

    _print_cobalt_histogram_names(ws, message_id, histograms, output_file)
  except Exception as e:  # pylint: disable=broad-exception-caught
    print(f'An error occurred: {e}')
  finally:
    if ws:
      ws.close()
      print('WebSocket connection closed.')


def loop(websocket_url: str, stop_event, histograms: list, args, output_file):
  """Runs the data polling loop in a dedicated thread."""
  print('Starting data polling thread')
  iteration = 0
  while not stop_event.is_set():
    iteration += 1
    current_time_str = time.strftime('%Y-%m-%d %H:%M:%S', time.localtime())
    header_prefix = f'[{current_time_str}][Poll Iter: {iteration}]'
    app_is_running = is_package_running(args.package_name)

    if app_is_running:
      _interact_via_cdp(websocket_url, histograms, output_file)
    elif not app_is_running and (iteration % 5 == 0):
      print(f'{header_prefix} App {args.package_name} not running.')

    time.sleep(args.poll_interval_s)
  print('Data polling thread stopped.')


def _run_main(args, output_file):
  """The main logic of the script."""
  if args.histogram_file:
    histograms = _get_histograms_from_file(args.histogram_file)
  else:
    histograms = _get_chrome_guiding_metrics()

  print('Ensure Cobalt is running with --remote-debugging-port=9222')
  print('-' * 50)
  stop_event = threading.Event()

  if not is_package_running(args.package_name):
    launch_cobalt(
        package_name=args.package_name,
        activity_name=DEFAULT_COBALT_ACTIVITY_NAME)
    if not is_package_running(args.package_name):
      print(f'Error: Failed to start {args.package_name}/'
            f'{DEFAULT_COBALT_ACTIVITY_NAME}. \nExiting...\n')
      sys.exit(1)
  time.sleep(1)

  websocket_url = get_websocket_url()
  if not websocket_url:
    print('Websocket not found. Exiting...')
    if not args.no_manage_cobalt:
      stop_package(args.package_name)
    return

  polling_thread = threading.Thread(
      target=loop,
      args=(websocket_url, stop_event, histograms, args, output_file),
      daemon=True,
  )
  polling_thread.start()

  print(f'\nPolling every {args.poll_interval_s}s. '
        'Ctrl+C to stop & save.')

  try:
    while polling_thread.is_alive():
      time.sleep(1)
  except KeyboardInterrupt:
    print('\nCtrl+C received. Signaling polling thread to stop...')
  except Exception as e:  # pylint: disable=broad-exception-caught
    print(f'Main loop encountered an error: {e}')
  finally:
    stop_event.set()
    if not args.no_manage_cobalt:
      stop_package(args.package_name)
  print('\nDONE!!!\n')


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--histogram-file',
      help='Path to a text file containing a list of histograms to query.')
  parser.add_argument(
      '--no-manage-cobalt',
      action='store_true',
      help='If set, the script will not start or stop the Cobalt process.')
  parser.add_argument(
      '--package-name',
      default=DEFAULT_COBALT_PACKAGE_NAME,
      help='The package name of the Cobalt application.')
  parser.add_argument(
      '--poll-interval-s',
      type=float,
      default=DEFAULT_POLL_INTERVAL_S,
      help='The polling frequency in seconds.')
  parser.add_argument(
      '--output-file', help='Path to a file to direct all output to.')
  args = parser.parse_args()

  if args.output_file:
    with open(args.output_file, 'a', encoding='utf-8') as f:
      _run_main(args, f)
  else:
    _run_main(args, None)


if __name__ == '__main__':
  main()
