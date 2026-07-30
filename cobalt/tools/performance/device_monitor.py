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
"""This module serves as an interface to interact with devices via ADB."""

import re
from typing import Dict, Union, List, Optional, Tuple, Callable

# Redefine the Callable type using these aliases
AdbRunnerType = Callable[[Union[List[str], str], bool, int],
                         Tuple[str, Optional[str]]]
TimeProviderType = Callable[[float], None]


class DeviceMonitor:
  """Handles interactions with the Android device via ADB."""

  def __init__(self, adb_runner: AdbRunnerType,
               time_provider: TimeProviderType):
    self._run_adb_command = adb_runner
    self._time_sleep = time_provider

  def is_package_running(self, package_name: str) -> bool:
    """Checks if a given Android package is currently running."""
    stdout, _ = self._run_adb_command(['adb', 'shell', 'pidof', package_name])
    return bool(stdout and stdout.strip().isdigit())

  def stop_package(self, package_name: str) -> bool:
    """Attempts to force-stop an Android package."""
    print(f'Attempting to stop package \'{package_name}\'...')
    _, stderr = self._run_adb_command(
        ['adb', 'shell', 'am', 'force-stop', package_name])
    if stderr:
      print(f'Error stopping package \'{package_name}\': {stderr}')
      return False
    print(f'Package \'{package_name}\' stop command issued.')
    self._time_sleep(2)  # Give time for the stop to propagate.
    return True

  def launch_cobalt(self, package_name: str, activity_name: str, flags: str):
    """Launches the Cobalt application with a specified URL.
       Returns True on success and False upon failure."""
    command_str = (f'adb shell am start -n {package_name}/{activity_name} '
                   f'--esa commandLineArgs \'{flags}\'')
    stdout, stderr = self._run_adb_command(command_str, shell=True)
    if stderr:
      print(f'Error launching Cobalt: {stderr}')
      return False
    else:
      print('Cobalt launch command sent.' +
            (f' Launch stdout: {stdout}' if stdout else ''))
      return True

  def get_meminfo_data(
      self,
      package_name: str) -> Optional[Dict[str, Dict[str, Union[int, str]]]]:
    """Fetches and parses dumpsys meminfo for a given package."""
    command = ['adb', 'shell', 'dumpsys', 'meminfo', package_name]
    output, error_msg = self._run_adb_command(command)
    if error_msg or not output:
      return None

    return self._parse_meminfo_output(output, package_name)

  def _parse_meminfo_output(
      self, output: str,
      package_name: str) -> Optional[Dict[str, Dict[str, Union[int, str]]]]:
    """Helper to parse raw dumpsys meminfo output."""
    mem_data = {}
    lines = output.splitlines()
    in_app_summary_section = False
    in_main_table = False
    column_headers = []

    for i, line in enumerate(lines):
      line_stripped = line.strip()

      if (not in_app_summary_section and '** MEMINFO in pid' in line and
          package_name in line):
        in_app_summary_section = True
        continue
      if not in_app_summary_section:
        continue

      if (not in_main_table and line_stripped.startswith('------') and
          '------' in line_stripped[7:]):
        in_main_table = True
        column_headers = self._parse_meminfo_headers(lines, i)
        continue

      if not in_main_table or not column_headers:
        continue

      if (line_stripped.startswith(
          ('Objects', 'SQL', 'DATABASES', 'AssetAllocations')) or
          not line_stripped):
        in_main_table = False
        break

      parts = line_stripped.split()
      if not parts:
        continue

      value_start_idx = -1
      for k_idx, part in enumerate(parts):
        try:
          int(part)
          value_start_idx = k_idx
          break
        except ValueError:
          continue

      if value_start_idx > 0:
        category_name = ' '.join(parts[:value_start_idx]).strip(':')
        if not category_name:
          continue

        value_strings = parts[value_start_idx:]
        category_values = {}
        for k_h, header_name in enumerate(column_headers):
          if k_h < len(value_strings):
            try:
              category_values[header_name] = int(value_strings[k_h])
            except ValueError:
              category_values[header_name] = value_strings[k_h]
          else:
            category_values[header_name] = 'N/A'
        if category_values:
          mem_data[category_name] = category_values
    return mem_data if mem_data else None

  def _parse_meminfo_headers(self, lines: List[str],
                             separator_idx: int) -> List[str]:
    """Parses column headers from dumpsys meminfo output."""
    default_headers = [
        'Pss Total', 'Private Dirty', 'Private Clean', 'SwapPss Dirty',
        'Rss Total', 'Heap Size', 'Heap Alloc', 'Heap Free'
    ]
    if separator_idx < 2:
      return default_headers

    try:
      h1_parts = lines[separator_idx - 2].strip().split()
      h2_parts = lines[separator_idx - 1].strip().split()
      if len(h1_parts) >= 8 and len(h2_parts) >= 8:
        return [f'{h1_parts[k_idx]} {h2_parts[k_idx]}' for k_idx in range(8)]
    except IndexError:
      pass  # Fallback to default headers
    return default_headers

  def get_surface_flinger_gba_kb(self) -> Dict[str, Optional[float]]:
    """Fetches GraphicBufferAllocator (GBA) usage from SurfaceFlinger
       dumpsys."""
    stats = {'GraphicBufferAllocator KB': None}
    stdout, stderr_msg = self._run_adb_command(
        ['adb', 'shell', 'dumpsys', 'SurfaceFlinger'])
    if stdout is None:
      return stats
    if not stdout and stderr_msg:
      return stats

    gba_kb_match = re.search(
        (r'Total allocated by GraphicBufferAllocator \(estimate\):\s*'
         r'(\d+\.?\d*)\s*KB'),
        stdout,
        re.I,
    )
    if gba_kb_match:
      try:
        stats['GraphicBufferAllocator KB'] = float(gba_kb_match.group(1))
      except ValueError:
        print(f'Warning: Could not parse GBA KB value: {gba_kb_match.group(1)}')
    return stats

  def check_adb_connection(self) -> bool:
    """Checks if ADB is connected and devices are authorized."""
    print('Checking ADB connection...')
    stdout_dev, stderr_dev = self._run_adb_command(['adb', 'devices'])
    if (stderr_dev or not stdout_dev or
        'List of devices attached' not in stdout_dev):
      print('ADB not working. Exiting.')
      return False
    if len(stdout_dev.strip().splitlines()) <= 1:
      print('No devices/emulators authorized. Exiting.')
      return False
    return True
