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
"""Captures smaps data from an Android device."""

import argparse
import datetime
import os
import subprocess
import time

# --- CONFIGURATION ---
PROCESS_NAME = 'com.google.android.youtube.tv'
# Use your actual device serial here if you have multiple devices/emulators
# connected. Example: R58M1293QYV. Set to None if only one device is connected.
DEVICE_SERIAL = 'localhost:45299'
ADB_PATH = 'adb'

# Timing Configuration
INTERVAL_MINUTES = 2
CAPTURE_DURATION_SECONDS = 3600 * 3  # 3 hours
OUTPUT_DIR = 'cobalt_smaps_logs'


class SmapsCapturer:
  """A class to capture smaps data from an Android device."""

  def __init__( # pylint: disable=too-many-positional-arguments
    self,
    config,
    subprocess_module=subprocess,
    time_module=time,
    os_module=os,
    datetime_module=datetime):
    self.process_name = config.process_name
    self.device_serial = config.device_serial
    self.adb_path = config.adb_path
    self.interval_seconds = config.interval_minutes * 60
    self.capture_duration_seconds = config.capture_duration_seconds
    self.output_dir = config.output_dir
    self.subprocess = subprocess_module
    self.time = time_module
    self.os = os_module
    self.datetime = datetime_module
    self.open = open

  def build_adb_command(self, *args):
    """Constructs the ADB command list, including the serial if set."""
    command = [self.adb_path]
    if self.device_serial:
      command.extend(['-s', self.device_serial])
    command.extend(args)
    return command

  def get_pid(self):
    """Executes 'adb shell pidof' and returns the PID."""
    try:
      command = self.build_adb_command('shell', 'pidof', self.process_name)
      result = self.subprocess.run(
          command,
          capture_output=True,
          text=True,
          check=False,  # Do not raise exception if pidof fails (RC 1)
          timeout=10)

      if result.returncode == 0:
        pid = result.stdout.strip().replace('\r', '')
        return pid if pid.isdigit() else None
      return None

    except Exception as e:  # pylint: disable=broad-exception-caught
      print(f'Error during PID retrieval: {e}')
      return None

  def capture_smaps(self, pid):
    """Efficiently streams /proc/pid/smaps content to a local file."""
    timestamp = self.datetime.datetime.now().strftime('%Y%m%d_%H%M%S')
    output_filename = self.os.path.join(self.output_dir,
                                        f'smaps_{timestamp}_{pid}.txt')

    print(f'[{timestamp}] Capturing /proc/{pid}/smaps...')

    try:
      command = self.build_adb_command('shell', 'cat', f'/proc/{pid}/smaps')

      with self.open(output_filename, 'w', encoding='utf-8') as f:
        process = self.subprocess.run(
            command, capture_output=True, text=True, timeout=30, check=False)

        if process.returncode == 0:
          f.write(process.stdout)
          file_size = self.os.path.getsize(output_filename) / 1024
          print(f'[{timestamp}] Capture successful. '
                f'File size: {file_size:.2f} KB')
        else:
          print(f'[{timestamp}] WARNING: adb shell cat failed '
                f'(RC {process.returncode}). Command: {command}')
          print(f'   Error output: {process.stderr.strip()}')
          if self.os.path.exists(output_filename):
            self.os.remove(output_filename)

    except Exception as e:  # pylint: disable=broad-exception-caught
      print(f'[{timestamp}] CRITICAL ERROR during capture: {e}')

  def run_capture_cycles(self):
    """Main function containing the periodic loop logic."""
    self.os.makedirs(self.output_dir, exist_ok=True)
    print(f'Logs will be saved in: {self.output_dir}\n')
    print('Starting periodic SMAPS capture for '
          f"'{self.process_name}' with a duration of "
          f'{self.capture_duration_seconds} seconds per cycle.')

    if self.interval_seconds <= 0:
      print('Interval must be positive.')
      return

    try:
      start_time = self.time.time()
      while (self.time.time() -
             start_time) < self.capture_duration_seconds:
        cycle_start_time = self.datetime.datetime.now()
        current_time_str = cycle_start_time.strftime('%H:%M:%S')

        print('-' * 50)
        print(f'[{current_time_str}] STARTING NEW '
              f'{self.capture_duration_seconds}s CYCLE.')

        pid = self.get_pid()

        if pid:
          self.capture_smaps(pid)
        else:
          print(f"[{current_time_str}] ERROR: Process '{self.process_name}' "
                'not found. Cannot capture.')

        self.time.sleep(self.interval_seconds)

    except KeyboardInterrupt:
      print('\nCapture process stopped by user (Ctrl+C).')
    except Exception as e:  # pylint: disable=broad-exception-caught
      print(f'\nAn unexpected error occurred: {e}')


def run_smaps_capture_tool(argv=None):
  """Parses arguments and runs the SmapsCapturer."""
  parser = argparse.ArgumentParser(
      description='Periodically capture smaps data for a given process.')
  parser.add_argument(
      '-p',
      '--process_name',
      type=str,
      default=PROCESS_NAME,
      help=f'The name of the process to capture (default: {PROCESS_NAME})')
  parser.add_argument(
      '-i',
      '--interval_minutes',
      type=int,
      default=INTERVAL_MINUTES,
      help='The interval in minutes between captures '
      f'(default: {INTERVAL_MINUTES})')
  parser.add_argument(
      '-d',
      '--capture_duration_seconds',
      type=int,
      default=CAPTURE_DURATION_SECONDS,
      help='The total duration in seconds for the capture session '
      f'(default: {CAPTURE_DURATION_SECONDS})')
  parser.add_argument(
      '-o',
      '--output_dir',
      type=str,
      default=OUTPUT_DIR,
      help=f'The directory to save smaps logs (default: {OUTPUT_DIR})')
  parser.add_argument(
      '-s',
      '--device_serial',
      type=str,
      default=DEVICE_SERIAL,
      help='The device serial number (optional, default: {DEVICE_SERIAL})')
  parser.add_argument(
      '--adb_path',
      type=str,
      default=ADB_PATH,
      help=f'The path to the adb executable (default: {ADB_PATH})')

  args = parser.parse_args(argv)

  capturer = SmapsCapturer(args)
  capturer.run_capture_cycles()


def main():
  """Main entry point."""
  run_smaps_capture_tool()


if __name__ == '__main__':
  main()
