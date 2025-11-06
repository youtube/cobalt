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
"""Tests for pull_uma_histogram_set_via_cdp.py."""

import os
import unittest
from unittest.mock import MagicMock, patch
import requests as real_requests
import pull_uma_histogram_set_via_cdp


class PullUmaHistogramSetViaCdpTest(unittest.TestCase):
  """Tests for pull_uma_histogram_set_via_cdp.py."""

  def test_get_histograms_from_file(self):
    """Tests that histograms are correctly read from a file."""
    with open('test_histograms.txt', 'w', encoding='utf-8') as f:
      f.write('hist1\n')
      f.write('hist2\n')
      f.write('# commented hist\n')
      f.write('hist3\n')
    self.addCleanup(os.remove, 'test_histograms.txt')

    histograms = pull_uma_histogram_set_via_cdp._get_histograms_from_file(  # pylint: disable=protected-access
        'test_histograms.txt')
    self.assertEqual(histograms, ['hist1', 'hist2', 'hist3'])

  @patch('pull_uma_histogram_set_via_cdp.run_adb_command')
  def test_is_package_running(self, mock_run_adb_command):
    """Tests that is_package_running returns True when a PID is found."""
    mock_run_adb_command.return_value = ('12345', '')
    self.assertTrue(
        pull_uma_histogram_set_via_cdp.is_package_running('test.package'))

  @patch('pull_uma_histogram_set_via_cdp.run_adb_command')
  def test_is_package_not_running(self, mock_run_adb_command):
    """Tests that is_package_running returns False when no PID is found."""
    mock_run_adb_command.return_value = ('', '')
    self.assertFalse(
        pull_uma_histogram_set_via_cdp.is_package_running('test.package'))

  @patch('pull_uma_histogram_set_via_cdp.run_adb_command')
  def test_stop_package(self, mock_run_adb_command):
    """Tests that stop_package returns True on success."""
    mock_run_adb_command.return_value = ('', '')
    self.assertTrue(
        pull_uma_histogram_set_via_cdp.stop_package('test.package', quiet=True))

  @patch('pull_uma_histogram_set_via_cdp.run_adb_command')
  def test_stop_package_failure(self, mock_run_adb_command):
    """Tests that stop_package returns False on failure."""
    mock_run_adb_command.return_value = ('', 'Error')
    self.assertFalse(
        pull_uma_histogram_set_via_cdp.stop_package('test.package', quiet=True))

  @patch('pull_uma_histogram_set_via_cdp.run_adb_command')
  def test_launch_cobalt(self, mock_run_adb_command):
    """Tests that launch_cobalt returns True on success."""
    mock_run_adb_command.return_value = ('', '')
    self.assertTrue(
        pull_uma_histogram_set_via_cdp.launch_cobalt(
            'test.package', 'test.activity', 'http://test.url', quiet=True))

  @patch('pull_uma_histogram_set_via_cdp.run_adb_command')
  def test_launch_cobalt_failure(self, mock_run_adb_command):
    """Tests that launch_cobalt returns False on failure."""
    mock_run_adb_command.return_value = ('', 'Error')
    self.assertFalse(
        pull_uma_histogram_set_via_cdp.launch_cobalt(
            'test.package', 'test.activity', 'http://test.url', quiet=True))

  @patch('pull_uma_histogram_set_via_cdp.device_utils')
  @patch('pull_uma_histogram_set_via_cdp.requests')
  def test_get_websocket_url(self, mock_requests, mock_device_utils):
    """Tests that get_websocket_url returns the correct URL."""
    mock_requests.exceptions.ConnectionError = \
        real_requests.exceptions.ConnectionError

    mock_device = MagicMock()
    mock_device_utils.DeviceUtils.HealthyDevices.return_value = [mock_device]
    mock_response = MagicMock()
    mock_response.json.return_value = [{
        'type': 'page',
        'title': 'Mock Title',
        'webSocketDebuggerUrl': 'ws://test_url'
    }]
    mock_requests.get.return_value = mock_response
    self.assertEqual(
        pull_uma_histogram_set_via_cdp.get_websocket_url(quiet=True),
        'ws://test_url')

  @patch('sys.argv', [
      '', '--no-manage-cobalt', '--package-name', 'test.package', '--url',
      'http://test.url'
  ])
  @patch(
      'pull_uma_histogram_set_via_cdp.is_package_running', return_value=False)
  @patch('pull_uma_histogram_set_via_cdp.loop')
  @patch(
      'pull_uma_histogram_set_via_cdp.get_websocket_url',
      return_value='ws://test_url')
  @patch('pull_uma_histogram_set_via_cdp.stop_package')
  @patch('pull_uma_histogram_set_via_cdp.launch_cobalt')
  def test_main_no_manage_cobalt(self, mock_launch_cobalt, mock_stop_package,
                                 unused_get_websocket_url, mock_loop,
                                 unused_is_package_running):
    """Tests that Cobalt is not managed when --no-manage-cobalt is passed."""
    pull_uma_histogram_set_via_cdp.main()
    mock_launch_cobalt.assert_not_called()
    mock_stop_package.assert_not_called()
    mock_loop.assert_called_once()


if __name__ == '__main__':
  unittest.main()
