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
import shlex
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

  @patch('pull_uma_histogram_set_via_cdp.run_adb_command')
  @patch('pull_uma_histogram_set_via_cdp.requests')
  def test_get_websocket_url_android(self, mock_requests, mock_run_adb_command):
    mock_run_adb_command.return_value = ('', '')
    mock_requests.exceptions.ConnectionError = (
        real_requests.exceptions.ConnectionError)
    mock_response = self._create_mock_response([{
        'type': 'page',
        'title': 'Mock Title',
        'webSocketDebuggerUrl': 'ws://test_url'
    }])
    mock_requests.get.return_value = mock_response
    self.assertEqual(
        pull_uma_histogram_set_via_cdp.get_websocket_url(
            platform='android', port=9222, quiet=True), 'ws://test_url')
    mock_run_adb_command.assert_called_with([
        'adb', 'forward', 'tcp:9222',
        'localabstract:content_shell_devtools_remote'
    ])

  @patch('pull_uma_histogram_set_via_cdp.time')
  @patch('pull_uma_histogram_set_via_cdp.run_adb_command')
  @patch('pull_uma_histogram_set_via_cdp.requests')
  def test_get_websocket_url_android_no_target(self, mock_requests,
                                               mock_run_adb_command, mock_time):  # pylint: disable=unused-argument
    mock_run_adb_command.return_value = ('', '')
    mock_requests.exceptions.ConnectionError = (
        real_requests.exceptions.ConnectionError)
    # No page target and /json/new returns no WebSocket URL on every attempt.
    mock_requests.get.side_effect = lambda url, timeout: (
        self._create_mock_response({} if url.endswith('/json/new') else []))
    self.assertIsNone(
        pull_uma_histogram_set_via_cdp.get_websocket_url(
            platform='android', port=9222, quiet=True))

  @patch('pull_uma_histogram_set_via_cdp.time')
  @patch('pull_uma_histogram_set_via_cdp.run_adb_command')
  @patch('pull_uma_histogram_set_via_cdp.requests')
  def test_get_websocket_url_android_connection_error(self, mock_requests,
                                                      mock_run_adb_command,
                                                      mock_time):
    mock_run_adb_command.return_value = ('', '')
    mock_requests.exceptions.ConnectionError = (
        real_requests.exceptions.ConnectionError)
    mock_requests.get.side_effect = real_requests.exceptions.ConnectionError
    self.assertIsNone(
        pull_uma_histogram_set_via_cdp.get_websocket_url(
            platform='android', port=9222, quiet=True))
    self.assertEqual(mock_time.sleep.call_count,
                     pull_uma_histogram_set_via_cdp.MAX_WEBSOCKET_RETRIES)

  @patch('pull_uma_histogram_set_via_cdp.run_adb_command')
  @patch('pull_uma_histogram_set_via_cdp.requests')
  def test_get_websocket_url_android_new_tab(self, mock_requests,
                                             mock_run_adb_command):
    mock_run_adb_command.return_value = ('', '')
    mock_requests.exceptions.ConnectionError = (
        real_requests.exceptions.ConnectionError)

    # No existing page, then a new tab is created
    mock_get_responses = [
        self._create_mock_response([]),
        self._create_mock_response({
            'webSocketDebuggerUrl': 'ws://new_tab_url',
            'title': 'New Tab'
        })
    ]
    mock_requests.get.side_effect = mock_get_responses

    self.assertEqual(
        pull_uma_histogram_set_via_cdp.get_websocket_url(
            platform='android', port=9222, quiet=True), 'ws://new_tab_url')
    self.assertEqual(mock_requests.get.call_count, 2)

  @patch('pull_uma_histogram_set_via_cdp.run_adb_command')
  def test_stop_package(self, mock_run_adb_command):
    mock_run_adb_command.return_value = ('', '')
    self.assertTrue(
        pull_uma_histogram_set_via_cdp.stop_package('test.package', quiet=True))

  @patch('pull_uma_histogram_set_via_cdp.run_adb_command')
  def test_launch_cobalt(self, mock_run_adb_command):
    mock_run_adb_command.return_value = ('', '')
    self.assertTrue(
        pull_uma_histogram_set_via_cdp.launch_cobalt(
            'test.package', 'test.activity', 'http://test.url', quiet=True))
    command = mock_run_adb_command.call_args[0][0]
    self.assertEqual(command[:2], ['adb', 'shell'])
    self.assertIn('-n test.package/test.activity', command[2])

  @patch('pull_uma_histogram_set_via_cdp.run_adb_command')
  def test_launch_cobalt_view_intent_without_activity(self,
                                                      mock_run_adb_command):
    mock_run_adb_command.return_value = ('Starting: Intent { act=VIEW }', '')
    url = "http://test.url/?a=1&b='$x'"
    self.assertTrue(
        pull_uma_histogram_set_via_cdp.launch_cobalt(
            'test.package', '', url, quiet=True))
    command = mock_run_adb_command.call_args[0][0]
    self.assertEqual(command[:2], ['adb', 'shell'])
    # The device shell must see the URL as a single, unexpanded argument.
    device_args = shlex.split(command[2])
    self.assertIn('android.intent.action.VIEW', device_args)
    self.assertEqual(device_args[device_args.index('-d') + 1], url)
    self.assertIn('-p', device_args)
    self.assertNotIn('-n', device_args)

  @patch('pull_uma_histogram_set_via_cdp.run_adb_command')
  def test_launch_cobalt_reports_am_error_on_stdout(self, mock_run_adb_command):
    mock_run_adb_command.return_value = (
        'Starting: Intent { cmp=test.package/test.activity }\n'
        'Error type 3\nError: Activity class {test.package/test.activity} '
        'does not exist.', '')
    self.assertFalse(
        pull_uma_histogram_set_via_cdp.launch_cobalt(
            'test.package', 'test.activity', 'http://test.url', quiet=True))

  @patch('sys.argv', [
      '', '--platform', 'android', '--package-name', 'test.package',
      '--duration-s', '0.01', '--quiet'
  ])
  @patch('pull_uma_histogram_set_via_cdp.stop_package')
  @patch('pull_uma_histogram_set_via_cdp.launch_cobalt')
  @patch('pull_uma_histogram_set_via_cdp.is_package_running_android')
  @patch(
      'pull_uma_histogram_set_via_cdp.get_websocket_url',
      new=MagicMock(return_value='ws://test_url'))
  @patch('pull_uma_histogram_set_via_cdp.loop')
  def test_main_android_view_intent_and_duration(self, mock_loop,
                                                 mock_is_running, mock_launch,
                                                 mock_stop):
    mock_is_running.side_effect = [False, True]
    stop_seen = []

    def fake_loop(websocket_url, stop_event, *unused_args):  # pylint: disable=unused-argument
      stop_seen.append(stop_event.wait(5))

    mock_loop.side_effect = fake_loop
    pull_uma_histogram_set_via_cdp.main()
    # A non-default package without --activity-name uses the VIEW intent.
    self.assertEqual(mock_launch.call_args.kwargs['activity_name'], '')
    # --duration-s stops the polling thread instead of running forever.
    self.assertEqual(stop_seen, [True])
    mock_stop.assert_called_once()


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

  @patch('pull_uma_histogram_set_via_cdp.requests')
  def test_get_websocket_url_linux_fallback(self, mock_requests):
    mock_requests.exceptions.ConnectionError = (
        real_requests.exceptions.ConnectionError)
    mock_response = self._create_mock_response([{
        'type': 'page',
        'webSocketDebuggerUrl': 'ws://test_url_page'
    }])
    mock_requests.get.return_value = mock_response
    self.assertEqual(
        pull_uma_histogram_set_via_cdp.get_websocket_url(
            platform='linux', port=9222, quiet=True), 'ws://test_url_page')

  @patch('pull_uma_histogram_set_via_cdp.requests')
  def test_get_websocket_url_linux_no_target(self, mock_requests):
    mock_requests.exceptions.ConnectionError = (
        real_requests.exceptions.ConnectionError)
    mock_response = self._create_mock_response([])
    mock_requests.get.return_value = mock_response
    self.assertIsNone(
        pull_uma_histogram_set_via_cdp.get_websocket_url(
            platform='linux', port=9222, quiet=True))


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


class CoreLogicTest(BaseHistogramTest):
  """Tests for the core CDP interaction and polling loop."""

  @patch('pull_uma_histogram_set_via_cdp.json.loads')
  @patch('pull_uma_histogram_set_via_cdp.time')
  def test_interact_via_cdp(self, mock_time, mock_json_loads):  # pylint: disable=unused-argument
    mock_ws_connection = MagicMock()
    self.mock_websocket.create_connection.return_value = mock_ws_connection
    # A non-object message is skipped instead of aborting the poll.
    mock_json_loads.side_effect = [[], {'id': 1}, {'id': 2}, {'id': 3}]

    pull_uma_histogram_set_via_cdp._interact_via_cdp(  # pylint: disable=protected-access
        'ws://test_url', ['hist1'], None, 'http://test.url', True)

    self.mock_websocket.create_connection.assert_called_once_with(
        'ws://test_url', timeout=pull_uma_histogram_set_via_cdp.CDP_TIMEOUT_S)
    self.assertEqual(mock_ws_connection.send.call_count, 3)
    self.assertEqual(mock_ws_connection.recv.call_count, 4)
    mock_ws_connection.close.assert_called_once()

  @patch('pull_uma_histogram_set_via_cdp.get_websocket_url')
  @patch('pull_uma_histogram_set_via_cdp.is_package_running_android')
  @patch('pull_uma_histogram_set_via_cdp._interact_via_cdp')
  def test_loop_android(self, mock_interact, mock_is_running,
                        mock_get_websocket):
    mock_get_websocket.return_value = 'ws://current_page'
    stop_event = MagicMock()
    stop_event.is_set.side_effect = [False, True]  # Run loop once
    args = MagicMock(platform='android', package_name='test.package')

    pull_uma_histogram_set_via_cdp.loop('ws://test_url', stop_event, [], args,
                                        None)

    mock_is_running.assert_called_with('test.package')
    # The page target is looked up again before each poll.
    mock_interact.assert_called_once()
    self.assertEqual(mock_interact.call_args[0][0], 'ws://current_page')

  @patch('pull_uma_histogram_set_via_cdp.get_websocket_url')
  @patch('pull_uma_histogram_set_via_cdp.is_package_running_android')
  @patch('pull_uma_histogram_set_via_cdp._interact_via_cdp')
  def test_loop_android_skips_poll_without_page_target(self, mock_interact,
                                                       mock_is_running,
                                                       mock_get_websocket):
    mock_is_running.return_value = True
    mock_get_websocket.return_value = None
    stop_event = MagicMock()
    stop_event.is_set.side_effect = [False, True]  # Run loop once
    args = MagicMock(platform='android', package_name='test.package')

    pull_uma_histogram_set_via_cdp.loop('ws://stale_page', stop_event, [], args,
                                        None)

    # The stale startup target is not reused.
    mock_interact.assert_not_called()


if __name__ == '__main__':
  unittest.main()
