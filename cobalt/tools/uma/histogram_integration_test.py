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


class BaseHistogramTest(unittest.TestCase):
  """Base class for histogram tests."""

  def setUp(self):
    # This context manager will patch the websocket module for all tests.
    self.patcher = patch('pull_uma_histogram_set_via_cdp.websocket')
    self.mock_websocket = self.patcher.start()
    self.addCleanup(self.patcher.stop)

  def _create_mock_response(self, json_data):
    mock_response = MagicMock()
    mock_response.json.return_value = json_data
    return mock_response


class AndroidHistogramTest(BaseHistogramTest):
  """Tests for the Android implementation."""

  @patch('pull_uma_histogram_set_via_cdp.run_adb_command')
  def test_is_package_running_android(self, mock_run_adb_command):
    mock_run_adb_command.return_value = ('12345', '')
    self.assertTrue(
        pull_uma_histogram_set_via_cdp.is_package_running_android(
            'test.package'))

  @patch('pull_uma_histogram_set_via_cdp.run_adb_command')
  def test_is_package_not_running_android(self, mock_run_adb_command):
    mock_run_adb_command.return_value = ('', '')
    self.assertFalse(
        pull_uma_histogram_set_via_cdp.is_package_running_android(
            'test.package'))

  @patch('pull_uma_histogram_set_via_cdp.device_utils')
  @patch('pull_uma_histogram_set_via_cdp.requests')
  def test_get_websocket_url_android(self, mock_requests, mock_device_utils):
    mock_requests.exceptions.ConnectionError = (
        real_requests.exceptions.ConnectionError)
    mock_device = MagicMock()
    mock_device_utils.DeviceUtils.HealthyDevices.return_value = [mock_device]
    mock_response = self._create_mock_response([{
        'type': 'page',
        'title': 'Mock Title',
        'webSocketDebuggerUrl': 'ws://test_url'
    }])
    mock_requests.get.return_value = mock_response
    self.assertEqual(
        pull_uma_histogram_set_via_cdp.get_websocket_url(
            platform='android', port=9222, quiet=True), 'ws://test_url')


class LinuxHistogramTest(BaseHistogramTest):
  """Tests for the Linux implementation."""

  @patch('pull_uma_histogram_set_via_cdp.subprocess.check_output')
  def test_is_cobalt_running_linux(self, mock_check_output):
    mock_check_output.return_value = b'12345'
    self.assertTrue(pull_uma_histogram_set_via_cdp.is_cobalt_running_linux())

  @patch('pull_uma_histogram_set_via_cdp.subprocess.check_output')
  def test_is_cobalt_not_running_linux(self, mock_check_output):
    mock_check_output.side_effect = FileNotFoundError
    self.assertFalse(pull_uma_histogram_set_via_cdp.is_cobalt_running_linux())

  @patch('pull_uma_histogram_set_via_cdp.requests')
  def test_get_websocket_url_linux(self, mock_requests):
    mock_requests.exceptions.ConnectionError = (
        real_requests.exceptions.ConnectionError)
    mock_response = self._create_mock_response([{
        'type': 'browser',
        'webSocketDebuggerUrl': 'ws://test_url_linux'
    }])
    mock_requests.get.return_value = mock_response
    self.assertEqual(
        pull_uma_histogram_set_via_cdp.get_websocket_url(
            platform='linux', port=9222, quiet=True), 'ws://test_url_linux')


class SharedHistogramTest(BaseHistogramTest):
  """Tests for shared functionality."""

  def test_get_histograms_from_file(self):
    with open('test_histograms.txt', 'w', encoding='utf-8') as f:
      f.write('hist1\n')
      f.write('hist2\n')
      f.write('# commented hist\n')
      f.write('hist3\n')
    self.addCleanup(os.remove, 'test_histograms.txt')
    histograms = (
        pull_uma_histogram_set_via_cdp._get_histograms_from_file(  # pylint: disable=protected-access
            'test_histograms.txt'))
    self.assertEqual(histograms, ['hist1', 'hist2', 'hist3'])

  @patch('sys.argv',
         ['', '--platform', 'linux', '--no-manage-cobalt', '--port', '9223'])
  @patch('pull_uma_histogram_set_via_cdp.is_cobalt_running_linux')
  @patch('pull_uma_histogram_set_via_cdp.loop')
  @patch(
      'pull_uma_histogram_set_via_cdp.get_websocket_url',
      return_value='ws://test_url')
  def test_main_linux(self, mock_get_websocket, mock_loop, mock_is_running):
    pull_uma_histogram_set_via_cdp.main()
    mock_get_websocket.assert_called_with(
        platform='linux', port=9223, quiet=False)
    mock_is_running.assert_not_called()  # Should be called inside loop
    mock_loop.assert_called_once()


if __name__ == '__main__':
  unittest.main()
