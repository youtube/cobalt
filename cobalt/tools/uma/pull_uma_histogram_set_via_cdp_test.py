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

import json
import os
import shlex
import sys
import unittest
from unittest.mock import MagicMock, patch
import requests as real_requests

try:
  import websocket  # pylint: disable=unused-import
except ImportError:
  # Every test patches the module; this only lets plain python3 import it.
  sys.modules['websocket'] = MagicMock()
import pull_uma_histogram_set_via_cdp  # pylint: disable=wrong-import-position


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
    mock_response = self._create_mock_response(
        {'webSocketDebuggerUrl': 'ws://test_url'})
    mock_requests.get.return_value = mock_response
    self.assertEqual(
        pull_uma_histogram_set_via_cdp.get_websocket_url(
            platform='android', port=9222, quiet=True), 'ws://test_url')
    self.assertTrue(
        mock_requests.get.call_args[0][0].endswith(':9222/json/version'))
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
    # /json/version has no WebSocket URL on every attempt.
    mock_requests.get.return_value = self._create_mock_response({})
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
    # Sleeps between attempts.
    self.assertEqual(mock_time.sleep.call_count,
                     pull_uma_histogram_set_via_cdp.MAX_WEBSOCKET_RETRIES - 1)

  @patch('pull_uma_histogram_set_via_cdp.requests')
  def test_get_page_target(self, mock_requests):
    page = {
        'type': 'page',
        'url': 'https://x/tv',
        'webSocketDebuggerUrl': 'ws://p'
    }
    mock_requests.get.return_value = self._create_mock_response(
        ['not a target', {
            'type': 'worker'
        }, page])
    self.assertEqual(
        pull_uma_histogram_set_via_cdp.get_page_target(9222),
        ('ws://p', 'https://x/tv'))
    # No new tab is created when looking up the page.
    self.assertEqual(mock_requests.get.call_count, 1)

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

    def fake_loop(stop_event, *unused_args):  # pylint: disable=unused-argument
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

  @patch('pull_uma_histogram_set_via_cdp.run_adb_command')
  @patch('pull_uma_histogram_set_via_cdp.requests')
  def test_get_websocket_url_linux(self, mock_requests, mock_run_adb_command):
    mock_requests.exceptions.ConnectionError = (
        real_requests.exceptions.ConnectionError)
    mock_response = self._create_mock_response(
        {'webSocketDebuggerUrl': 'ws://test_url_linux'})
    mock_requests.get.return_value = mock_response
    self.assertEqual(
        pull_uma_histogram_set_via_cdp.get_websocket_url(
            platform='linux', port=9222, quiet=True), 'ws://test_url_linux')
    mock_run_adb_command.assert_not_called()

  @patch('pull_uma_histogram_set_via_cdp.requests')
  def test_get_websocket_url_linux_no_target(self, mock_requests):
    mock_requests.exceptions.ConnectionError = (
        real_requests.exceptions.ConnectionError)
    mock_response = self._create_mock_response([])
    mock_requests.get.return_value = mock_response
    self.assertIsNone(
        pull_uma_histogram_set_via_cdp.get_websocket_url(
            platform='linux', port=9222, quiet=True, retries=1))


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

  def test_fetch_histograms(self):
    ws = self.mock_websocket.create_connection.return_value
    # A non-object message and an event are skipped.
    ws.recv.side_effect = [
        '[]', '{"method": "E"}', '{"id": 1, "result": {"histograms": [{}]}}'
    ]
    self.assertEqual(
        pull_uma_histogram_set_via_cdp.fetch_histograms('ws://browser'), [{}])
    self.mock_websocket.create_connection.assert_called_once_with(
        'ws://browser', timeout=pull_uma_histogram_set_via_cdp.CDP_TIMEOUT_S)
    # One call without a query returns every histogram.
    command = json.loads(ws.send.call_args[0][0])
    self.assertEqual((command['method'], command['params']),
                     ('Browser.getHistograms', {}))
    ws.close.assert_called_once()

  def test_filter_histograms(self):
    histograms = [{'name': n} for n in ('A', 'A.B', 'A.BC', 'AB', 'C')]
    result = pull_uma_histogram_set_via_cdp.filter_histograms(
        histograms, ['A', 'A.B*', 'A*', 'Z'])
    # Exact names, prefixes for '*', and each histogram only once.
    self.assertEqual([(q, [h['name'] for h in m]) for q, m in result],
                     [('A', ['A']), ('A.B*', ['A.B', 'A.BC']), ('A*', ['AB']),
                      ('Z', [])])

  @patch('pull_uma_histogram_set_via_cdp.fetch_histograms')
  @patch('pull_uma_histogram_set_via_cdp.get_websocket_url', new=MagicMock())
  @patch('pull_uma_histogram_set_via_cdp.is_package_running_android')
  def test_poll_once(self, mock_is_running, mock_fetch):
    mock_is_running.return_value = True
    mock_fetch.return_value = [{'name': 'A'}, {'name': 'B'}]
    args = MagicMock(platform='android')
    ok, text = pull_uma_histogram_set_via_cdp.poll_once(args, ['A', 'Z'], 3)
    self.assertTrue(ok)
    lines = text.splitlines()
    self.assertRegex(lines[0], r'^# poll 3 \S+ \S+ OK: 2 histograms')
    self.assertEqual([line.split(',', 2)[1:] for line in lines[1:]],
                     [['A', '{"result": {"histograms": [{"name": "A"}]}}'],
                      ['Z', '{"result": {"histograms": []}}']])

  @patch('pull_uma_histogram_set_via_cdp.get_websocket_url', new=MagicMock())
  @patch('pull_uma_histogram_set_via_cdp.is_package_running_android')
  def test_loop_writes_failed_poll(self, mock_is_running):
    mock_is_running.return_value = True
    self.mock_websocket.create_connection.side_effect = TimeoutError(
        'timed out')
    stop_event = MagicMock()
    stop_event.is_set.side_effect = [False, True]  # Run loop once
    output_file = MagicMock()
    results = []
    pull_uma_histogram_set_via_cdp.loop(
        stop_event, ['A'], MagicMock(platform='android', poll_interval_s=30),
        output_file, results)
    self.assertEqual(results, [False])
    output_file.write.assert_called_once()
    self.assertRegex(output_file.write.call_args[0][0],
                     r'^# poll 1 \S+ \S+ FAILED: timed out\n$')


if __name__ == '__main__':
  unittest.main()
