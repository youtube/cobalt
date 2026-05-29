#!/usr/bin/env python3
# Copyright 2026 The Cobalt Authors. All Rights Reserved.
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
"""Unit tests for run_device_tests.py."""

import json
import os
import pathlib
import sys
import unittest
from unittest.mock import MagicMock, mock_open, patch

# Mock missing dependencies before importing run_device_tests
mock_grpc = MagicMock()
sys.modules['grpc'] = mock_grpc
sys.modules['on_device_tests_gateway_pb2'] = MagicMock()
sys.modules['on_device_tests_gateway_pb2_grpc'] = MagicMock()

import run_device_tests


class TestRunDeviceTests(unittest.TestCase):

  @patch('subprocess.run')
  def test_download_results_from_gcs_success(self, mock_run):
    mock_run.return_value.returncode = 0
    res = run_device_tests.download_results_from_gcs('gs://bucket/path',
                                                     '/local/dir')
    self.assertTrue(res)
    mock_run.assert_called_once()

  @patch('subprocess.run')
  def test_download_results_from_gcs_fail(self, mock_run):
    mock_run.return_value.returncode = 1
    res = run_device_tests.download_results_from_gcs('gs://bucket/path',
                                                     '/local/dir')
    self.assertFalse(res)

  @patch('os.path.exists')
  @patch('junit_mini_parser.find_failing_tests')
  def test_check_failures_in_xml_true(self, mock_find, mock_exists):
    mock_exists.return_value = True
    mock_find.return_value = {'file.xml': [('test1', 'msg')]}
    res = run_device_tests.check_failures_in_xml(['file.xml'])
    self.assertTrue(res)

  @patch('os.path.exists')
  @patch('junit_mini_parser.find_failing_tests')
  def test_check_failures_in_xml_false(self, mock_find, mock_exists):
    mock_exists.return_value = True
    mock_find.return_value = {}
    res = run_device_tests.check_failures_in_xml(['file.xml'])
    self.assertFalse(res)

  @patch('pathlib.Path.rglob')
  def test_get_failed_targets_no_xml(self, mock_rglob):
    # Simulate no XML files found
    mock_rglob.side_effect = [[], []]  # For XML and then for Log
    targets = ['cobalt/dom:dom_test']
    res = run_device_tests.get_failed_targets('/dummy/dir', targets)
    self.assertEqual(res, targets)

  @patch('pathlib.Path.rglob')
  @patch('run_device_tests.check_failures_in_xml')
  def test_get_failed_targets_with_failures(self, mock_check, mock_rglob):
    mock_file = MagicMock()
    mock_file.name = 'dom_test.xml'
    mock_rglob.return_value = [mock_file]
    mock_check.return_value = True

    targets = ['cobalt/dom:dom_test']
    res = run_device_tests.get_failed_targets('/dummy/dir', targets)
    self.assertEqual(res, targets)

  @patch('run_device_tests.download_results_from_gcs')
  @patch('run_device_tests.get_failed_targets')
  @patch('on_device_tests_gateway_client.process_test_requests')
  @patch('on_device_tests_gateway_client.OnDeviceTestsGatewayClient')
  def test_main_success(self, mock_client, mock_process, mock_get_failed,
                        mock_download):
    mock_download.return_value = True
    mock_get_failed.return_value = []
    mock_process.return_value = []

    test_args = [
        '--token', 'dummy_token', '--test_type', 'unit_test', '--device_family',
        'android', '--targets', '["cobalt/dom:dom_test"]', '--filter_json_dir',
        '/dummy/filters', '--dimensions', '{}', '--gcs_archive_path',
        'gs://archive', '--gcs_results_path', 'gs://results', '--event_name',
        'pull_request'
    ]

    with patch('sys.argv', ['run_device_tests.py'] + test_args):
      rc = run_device_tests.main()
      self.assertEqual(rc, 0)


if __name__ == '__main__':
  unittest.main()
