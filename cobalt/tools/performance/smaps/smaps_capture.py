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
# connected. Example: R58M1293QYV. Set to None if only one device is
# connected.
DEVICE_SERIAL = None
ADB_PATH = 'adb'

# Timing Configuration
INTERVAL_MINUTES = 2
CAPTURE_DURATION_SECONDS = 3600 * 3  # 3 hours
OUTPUT_DIR = 'cobalt_smaps_logs'


class SmapsCapturer:
  """A class to capture smaps data from an Android or Linux device."""

  def __init__(  # pylint: disable=too-many-positional-arguments
      self,
      config,
      subprocess_module=subprocess,
      time_module=time,
      os_module=os,
      datetime_module=datetime):
    self.process_name = config.process_name
    self.platform = config.platform
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

  def _get_main_pid(self, pids):
    """Returns the main pid from a list of pids."""
    if not pids:
      return None
    if len(pids) == 1:
      return pids[0]
    try:
      # Get pid and elapsed time for each pid
      command = ['ps', '-o', 'pid,etimes', '-p', ','.join(pids)]
      result = self.subprocess.run(
          command, capture_output=True, text=True, check=False, timeout=10)
      if result.returncode == 0:
        lines = result.stdout.strip().split('\n')[1:]
        if not lines:
          return None
        pid_to_etime = {}
        for line in lines:
          parts = line.strip().split()
          if len(parts) == 2:
            pid, etime = parts
            if pid.isdigit() and etime.isdigit():
              pid_to_etime[pid] = int(etime)
        if not pid_to_etime:
          return None
        return max(pid_to_etime, key=pid_to_etime.get)
      return None
    except Exception as e:  # pylint: disable=broad-exception-caught
      print(f'Error during main PID retrieval: {e}')
      return None

  def get_pids_linux(self):
    """Executes 'pidof' on Linux and returns a list of PIDs."""
    try:
      command = ['pidof', self.process_name]
      result = self.subprocess.run(
          command, capture_output=True, text=True, check=False, timeout=10)
      if result.returncode == 0:
        pids = result.stdout.strip().split()
        main_pid = self._get_main_pid(pids)
        return [main_pid] if main_pid else []
      return []
    except Exception as e:  # pylint: disable=broad-exception-caught
      print(f'Error during Linux PID retrieval: {e}')
      return []

  def get_pids_android(self):
    """Executes 'adb shell pidof' and returns a list containing the PID."""
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
        return [pid] if pid.isdigit() else []
      return []

    except Exception as e:  # pylint: disable=broad-exception-caught
      print(f'Error during Android PID retrieval: {e}')
      return []

  def get_pids(self):
    """Returns a list of PIDs for the configured platform."""
    if self.platform == 'linux':
      return self.get_pids_linux()
    if self.platform == 'android':
      return self.get_pids_android()
    raise ValueError(f'Unsupported platform: {self.platform}')

  def capture_smaps_linux(self, pid):
    """Reads /proc/pid/smaps content on Linux."""
    timestamp = self.datetime.datetime.now().strftime('%Y%m%d_%H%M%S')
    output_filename = self.os.path.join(self.output_dir,
                                        f'smaps_{timestamp}_{pid}.txt')
    smaps_path = f'/proc/{pid}/smaps'

    print(f'[{timestamp}] Capturing {smaps_path}...')

    try:
      with self.open(smaps_path, 'r', encoding='utf-8') as smaps_file, \
           self.open(output_filename, 'w', encoding='utf-8') as output_file:
        for line in smaps_file:
          output_file.write(line)
      file_size = self.os.path.getsize(output_filename) / 1024
      print(f'[{timestamp}] Capture successful. '
            f'File size: {file_size:.2f} KB')
    except FileNotFoundError:
      print(f'[{timestamp}] ERROR: Could not find {smaps_path}. '
            'The process may have terminated.')
    except Exception as e:  # pylint: disable=broad-exception-caught
      print(f'[{timestamp}] CRITICAL ERROR during Linux capture: {e}')

  def capture_smaps_android(self, pid):
    """Efficiently streams /proc/pid/smaps content
       from Android to a local file."""
    timestamp = self.datetime.datetime.now().strftime('%Y%m%d_%H%M%S')
    output_filename = self.os.path.join(self.output_dir,
                                        f'smaps_{timestamp}_{pid}.txt')

    print(f'[{timestamp}] Capturing /proc/{pid}/smaps from Android...')

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
                f'(RC {process.returncode}). '
                f'Command: {command}')
          print(f'   Error output: {process.stderr.strip()}')
          if self.os.path.exists(output_filename):
            self.os.remove(output_filename)

    except Exception as e:  # pylint: disable=broad-exception-caught
      print(f'[{timestamp}] CRITICAL ERROR during Android capture: {e}')

  def capture_smaps(self, pid):
    """Captures smaps for the configured platform."""
    if self.platform == 'linux':
      self.capture_smaps_linux(pid)
    elif self.platform == 'android':
      self.capture_smaps_android(pid)
    else:
      raise ValueError(f'Unsupported platform: {self.platform}')

  def run_capture_cycles(self):
    """Main function containing the periodic loop logic."""
    self.os.makedirs(self.output_dir, exist_ok=True)
    print(f'Logs will be saved in: {self.output_dir}\n')
    print(f'Starting periodic SMAPS capture for '
          f"'{self.process_name}' on platform '{self.platform}' "
          f'with a duration of {self.capture_duration_seconds} seconds.')

    if self.interval_seconds <= 0:
      print('Interval must be positive.')
      return

    try:
      start_time = self.time.time()
      while (self.time.time() - start_time) < self.capture_duration_seconds:
        cycle_start_time = self.datetime.datetime.now()
        current_time_str = cycle_start_time.strftime('%H:%M:%S')

        print('-' * 50)
        print(f'[{current_time_str}] STARTING NEW '
              f'{self.interval_seconds}s CYCLE.')

        pids = self.get_pids()

        if pids:
          for pid in pids:
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
      '--platform',
      type=str,
      choices=['android', 'linux'],
      default='android',
      help='The platform to capture from (default: android)')
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

  # Android-specific arguments
  android_group = parser.add_argument_group('Android Specific Arguments')
  android_group.add_argument(
      '-s',
      '--device_serial',
      type=str,
      default=DEVICE_SERIAL,
      help='The device serial number (optional, for Android only)')
  android_group.add_argument(
      '--adb_path',
      type=str,
      default=ADB_PATH,
      help=('The path to the adb executable (for Android only, ' +
            f'default: {ADB_PATH})'))

  args = parser.parse_args(argv)

  capturer = SmapsCapturer(args)
  capturer.run_capture_cycles()


def main():
  """Main entry point."""
  run_smaps_capture_tool()


if __name__ == '__main__':
  main()
