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

import json
import os
import sys
import time
import requests  # To get the WebSocket URL
import threading
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
DEFAULT_POLL_INTERVAL_MS = 100


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
                 f'--url="{TARGET_URL}"\'')
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
    for the first available tab (or a new tab if one isn't open).
    """
  try:
    devs = list(device_utils.DeviceUtils.HealthyDevices())
    dev = devs[0]
    dev.adb.Forward(
        'tcp:9222',
        'localabstract:content_shell_devtools_remote',
        allow_rebind=True)
    response = requests.get(f'http://{CDP_HOST}:{COBALT_DEBUG_PORT}/json')
    response.raise_for_status()  # Raise an exception for bad status codes
    targets = response.json()

    # Find a suitable target (e.g., page type)
    # You might want to filter by 'type': 'page' or 'type': 'iframe' etc.
    # For simplicity, we'll take the first 'page' type target.
    for target in targets:
      if target.get('type') == 'page' and target.get('webSocketDebuggerUrl'):
        print(f'Found existing tab: {target["title"]} - '
              f'{target["webSocketDebuggerUrl"]}')
        return target['webSocketDebuggerUrl']

    # If no suitable page target is found, create a new one (optional,
    # but useful)
    print('No suitable existing page found, trying to create a new tab...')
    new_tab_url = f'http://{CDP_HOST}:{COBALT_DEBUG_PORT}/json/new'
    new_tab_response = requests.get(new_tab_url)
    new_tab_response.raise_for_status()
    new_tab_data = new_tab_response.json()
    if new_tab_data.get('webSocketDebuggerUrl'):
      print(f'Created new tab: {new_tab_data["title"]} - '
            f'{new_tab_data["webSocketDebuggerUrl"]}')
      return new_tab_data['webSocketDebuggerUrl']

    raise Exception(  # pylint: disable=broad-exception-raised
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


def _get_chrome_guiding_metrics() -> list:
  metrics = []
  metrics.append('User.ClientCrashProportion.BrowserCrash3')
  metrics.append('User.ClientCrashProportion.RendererCrash3')
  metrics.append('User.ClientRetention')
  metrics.append('WebVitals.FirstContentfulPaint4')
  metrics.append('WebVitals.LargestContentfulPaint3')
  metrics.append('Browser.MainThreadsCongestion')
  metrics.append('EventLatency.GestureScrollUpdate.Touchscreen.TotalLatency')
  metrics.append('Event.ScrollJank.DelayedFramesPercentage.FixedWindow')
  metrics.append('PageLoad.InteractiveTiming.InputDelay3')
  metrics.append('WebVitals.InteractionToNextPaint2')
  metrics.append('Startup.Android.Cold.TimeToFirstVisibleContent4')
  metrics.append('WebVitals.CumulativeLayoutShift6')
  metrics.append(
      'PageLoad.Clients.Ads.AdPaintTiming.NavigationToFirstContentfulPaint3')
  metrics.append('Memory.Browser.PrivateMemoryFootprint')
  metrics.append('Memory.Renderer.PrivateMemoryFootprint')
  metrics.append('Memory.Total.PrivateMemoryFootprint')
  metrics.append('Power.ForegroundBatteryDrain.30SecondsAvg2')
  metrics.append('Omnibox.MediatedSearchQueryVolume')
  return metrics


def _print_similar_histogram_names(ws):
  print('\n', '-' * 50, '\n')
  metrics = []
  metrics.append('Crash')
  metrics.append('Retention')
  metrics.append('Congestion')
  metrics.append('Startup')
  metrics.append('Memory')
  metrics.append('Graphics')
  metrics.append('Paint')
  metrics.append('Latency')
  for metric in metrics:
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
    print('\n', f'Metric - {metric}:', '\n', response, '\n')
  print('\n', '-' * 50, '\n')


def _interact_via_cdp(websocket_url: str):
  ws = None  # Initialize WebSocket client
  try:
    ws = websocket.create_connection(websocket_url)
    print(f'Connected to WebSocket: {websocket_url}')

    message_id = 1

    # 1. Enable Performance domain
    command = {
        'id': message_id,
        'method': 'Performance.enable',
        'params': {
            'timeDomain': 'threadTicks'
        }
    }
    ws.send(json.dumps(command))
    response = json.loads(ws.recv())
    print(f'Enabled Performance domain (ID: {response["id"]})')
    message_id += 1

    # 2. Get Performance Metrics
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
        print(f'  {metric["name"]}: {metric["value"]}')
    else:
      print(f'Could not retrieve metrics: {response}')

  # 3. Get Chrome Histograms
    print('\n', '-' * 50, '\n')
    metrics = _get_chrome_guiding_metrics()
    for metric in metrics:
      command = {
          'id': message_id,
          'method': 'Browser.getHistogram',
          'params': {
              'name': metric
          }
      }
      ws.send(json.dumps(command))
      response = json.loads(ws.recv())
      message_id += 1
      print('\n', f'Metric - {metric}:', '\n', response, '\n')
    print('\n', '-' * 50, '\n')
  except Exception as e:  # pylint: disable=broad-exception-caught
    print(f'An error occurred: {e}')
  finally:
    if ws:
      ws.close()
      print('WebSocket connection closed.')


def _get_time_ms():
  return time.monotonic_ns() / 1000


def loop(websocket_url: str, stop_event):
  """Runs the data polling loop in a dedicated thread."""
  print('Starting data polling thread')
  iteration = 0
  while not stop_event.is_set():
    iteration += 1
    current_time_str = time.strftime('%Y-%m-%d %H:%M:%S', time.localtime())
    header_prefix = f'[{current_time_str}][Poll Iter: {iteration}]'
    app_is_running = is_package_running(DEFAULT_COBALT_PACKAGE_NAME)

    if app_is_running:
      _interact_via_cdp(websocket_url)
    elif not app_is_running and (iteration % 5 == 0):
      print(f'{header_prefix} App {DEFAULT_COBALT_PACKAGE_NAME} not running.')

    start_sleep_time = _get_time_ms()
    while _get_time_ms() - start_sleep_time < DEFAULT_POLL_INTERVAL_MS:
      if stop_event.is_set():
        break
      time.sleep(1 / DEFAULT_POLL_INTERVAL_MS / 2)
    if stop_event.is_set():
      break
  print('Data polling thread stopped.')


def main():
  print('Ensure Cobalt is running with --remote-debugging-port=9222')
  print('-' * 50)
  stop_event = threading.Event()

  if not is_package_running(DEFAULT_COBALT_PACKAGE_NAME):
    launch_cobalt(
        package_name=DEFAULT_COBALT_PACKAGE_NAME,
        activity_name=DEFAULT_COBALT_ACTIVITY_NAME)
    if not is_package_running(DEFAULT_COBALT_PACKAGE_NAME):
      print(f'Error: Failed to start {DEFAULT_COBALT_PACKAGE_NAME}/'
            f'{DEFAULT_COBALT_ACTIVITY_NAME}. \nExiting...\n')
      stop_package(DEFAULT_COBALT_PACKAGE_NAME)
      sys.exit(1)
  time.sleep(1)

  websocket_url = get_websocket_url()
  if not websocket_url:
    print('Websocket not found. Exiting...')
    stop_package(DEFAULT_COBALT_PACKAGE_NAME)
    return

  polling_thread = threading.Thread(
      target=loop,
      args=(websocket_url, stop_event),
      daemon=True,
  )
  polling_thread.start()

  print(f'\nPolling every {DEFAULT_POLL_INTERVAL_MS}ms. '
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
    stop_package(DEFAULT_COBALT_PACKAGE_NAME)
  print('\nDONE!!!\n')


if __name__ == '__main__':
  main()
