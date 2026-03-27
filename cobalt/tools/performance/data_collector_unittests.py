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
"""Tests for DataCollector."""

import threading
import unittest
from unittest.mock import MagicMock, patch
from data_collector import DataCollectorBuilder
from time_provider import TimeProvider


class TestDataCollector(unittest.TestCase):
  """Unit tests for the DataCollector class."""

  def setUp(self):
    self.mock_time_provider = MagicMock(spec=TimeProvider)
    self.mock_device_monitor = MagicMock()
    self.mock_data_store = MagicMock()
    self.stop_event = threading.Event()

    self.builder = DataCollectorBuilder()\
      .with_package_name('test.package')\
      .with_poll_interval_ms(100)\
      .with_data_store(self.mock_data_store)\
      .with_device_monitor(self.mock_device_monitor)\
      .with_time_provider(self.mock_time_provider)\
      .with_stop_event(self.stop_event)

  def test_get_time_ms_correctness(self):
    """Verifies that _get_time_ms converts nanoseconds to milliseconds."""
    # 1,000,000,000 ns = 1,000 ms
    self.mock_time_provider.monotonic_ns.return_value = 1000000000
    collector = self.builder.build()
    # Use getattr to bypass pylint W0212
    get_time_ms = getattr(collector, '_get_time_ms')
    self.assertEqual(get_time_ms(), 1000.0)

  def test_run_polling_loop_respects_interval(self):
    """Verifies that the polling loop waits for the correct interval."""
    # This is a bit tricky to test without actual sleep, but we can mock it.
    collector = self.builder.build()

    # We want it to run for exactly 1 iteration then stop.
    self.mock_device_monitor.is_package_running.return_value = False
    self.mock_device_monitor.get_surface_flinger_gba_kb.return_value = {}

    # Mock monotonic_ns to simulate time passing.
    # 1st call in _get_time_ms (start_sleep_time)
    # 2nd call in while loop condition
    # 3rd call in remaining_ms calculation
    # 4th call in while loop condition (to exit)
    self.mock_time_provider.monotonic_ns.side_effect = [
        0,  # start_sleep_time
        50 * 1000000,  # while loop check (50ms passed)
        50 * 1000000,  # remaining_ms calculation
        110 * 1000000  # while loop check (110ms passed, > 100ms interval)
    ]

    # We need to allow at least one iteration of the sleep loop.
    # is_set() is called:
    # 1. Outer while loop: False
    # 2. Inner while loop check: False
    # 3. Outer while loop (next iteration): True
    with patch.object(
        self.stop_event, 'is_set', side_effect=[False, False, True, True,
                                                True]):
      collector.run_polling_loop()

    # Verify sleep was called with 50ms (100 - 50)
    # The first sleep call should be (100 - 50) / 1000 = 0.05 seconds
    self.mock_time_provider.sleep.assert_called_with(0.05)


if __name__ == '__main__':
  unittest.main()
