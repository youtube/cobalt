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
"""This module collects memory and GBA data."""

import threading
from data_store import MonitoringDataStore
from device_monitor import DeviceMonitor
from time_provider import TimeProvider


class DataCollector:
  """Continuously polls and collects memory and GBA data."""

  def __init__(self, builder):  # pylint: disable=used-before-assignment
    self._package_name = builder.package_name
    self._poll_interval_ms = builder.poll_interval_ms
    self._data_store = builder.data_store
    self._device_monitor = builder.device_monitor
    self._time_provider = builder.time_provider
    self._stop_event = builder.stop_event

  def _get_time_ms(self):
    return self._time_provider.monotonic_ns() / 1000

  def run_polling_loop(self):
    """Runs the data polling loop in a dedicated thread."""
    print('Starting data polling thread '
          '(meminfo and GraphicBufferAllocator)...')
    iteration = 0
    while not self._stop_event.is_set():
      iteration += 1
      current_time_str = self._time_provider.strftime(
          '%Y-%m-%d %H:%M:%S', self._time_provider.localtime())
      header_prefix = f'[{current_time_str}][Poll Iter: {iteration}]'
      current_snapshot_data = {'timestamp': current_time_str}
      app_is_running = self._device_monitor.is_package_running(
          self._package_name)

      meminfo_total_pss_str = 'N/A'
      if app_is_running:
        raw_mem_data = self._device_monitor.get_meminfo_data(self._package_name)
        if raw_mem_data:
          total_pss_val = raw_mem_data.get('TOTAL', {}).get('Pss Total')
          if total_pss_val is not None:
            meminfo_total_pss_str = str(total_pss_val)
          for category, data_map in raw_mem_data.items():
            pss_total = data_map.get('Pss Total')
            if pss_total is not None:
              current_snapshot_data[f'{category} Pss Total'] = pss_total
        elif app_is_running:
          print(
              f'{header_prefix} Warning: No meminfo data parsed for '
              f'{self._package_name} this cycle.',
              end=' | ',
          )

      sf_stats = self._device_monitor.get_surface_flinger_gba_kb()
      current_snapshot_data.update(sf_stats)
      gba_kb_val = sf_stats.get('GraphicBufferAllocator KB')
      gba_kb_str = f'{gba_kb_val:.1f} KB' if gba_kb_val is not None else 'N/A'

      if app_is_running or iteration == 1:
        print(
            f'{header_prefix} Mem(TOTAL PSS): {meminfo_total_pss_str} KB | SF -'
            f' GBA: {gba_kb_str}')
      elif not app_is_running and (iteration % 5 == 0):
        print(f'{header_prefix} App {self._package_name} not running. SF - GBA:'
              f' {gba_kb_str}')

      self._data_store.add_snapshot(current_snapshot_data)

      start_sleep_time = self._get_time_ms()
      while self._get_time_ms() - start_sleep_time < self._poll_interval_ms:
        if self._stop_event.is_set():
          break
        self._time_provider.sleep(1 / self._poll_interval_ms / 2)
      if self._stop_event.is_set():
        break

    print('Data polling thread stopped.')


class DataCollectorBuilder:
  """Builder for DataCollector."""

  def __init__(self):
    self.package_name = None
    self.poll_interval_ms = None
    self.data_store = None
    self.device_monitor = None
    self.time_provider = None
    self.stop_event = None

  def with_package_name(self, package_name: str):
    self.package_name = package_name
    return self

  def with_poll_interval_ms(self, poll_interval_ms: int):
    self.poll_interval_ms = poll_interval_ms
    return self

  def with_data_store(self, data_store: MonitoringDataStore):
    self.data_store = data_store
    return self

  def with_device_monitor(self, device_monitor: DeviceMonitor):
    self.device_monitor = device_monitor
    return self

  def with_time_provider(self, time_provider: TimeProvider):
    self.time_provider = time_provider
    return self

  def with_stop_event(self, stop_event: threading.Event):
    self.stop_event = stop_event
    return self

  def build(self) -> DataCollector:  # pylint: disable=used-before-assignment
    # Add validation here if necessary to ensure all required fields are set
    if not all([
        self.package_name, self.poll_interval_ms, self.data_store,
        self.device_monitor, self.time_provider, self.stop_event
    ]):
      raise ValueError('All DataCollector parameters must be set before '
                       'building.')
    return DataCollector(self)
