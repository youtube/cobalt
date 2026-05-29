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
"""Unit tests for run_host_tests.py."""

import json
import os
import unittest
from unittest.mock import MagicMock, mock_open, patch

import run_host_tests


class TestRunHostTests(unittest.TestCase):

  @patch('os.path.exists')
  @patch(
      'builtins.open',
      new_callable=mock_open,
      read_data='{"failing_tests": ["Test1", "Test2"]}')
  def test_get_initial_filter_valid(self, mock_file, mock_exists):
    mock_exists.return_value = True
    res = run_host_tests.get_initial_filter('my_test', '/dummy/dir')
    self.assertEqual(res, '-Test1:Test2')

  @patch('os.path.exists')
  def test_get_initial_filter_missing(self, mock_exists):
    mock_exists.return_value = False
    res = run_host_tests.get_initial_filter('my_test', '/dummy/dir')
    self.assertEqual(res, '*')

  @patch('os.path.exists')
  @patch('builtins.open', new_callable=mock_open, read_data='invalid json')
  def test_get_initial_filter_invalid_json(self, mock_file, mock_exists):
    mock_exists.return_value = True
    res = run_host_tests.get_initial_filter('my_test', '/dummy/dir')
    self.assertEqual(res, '*')

  @patch('subprocess.run')
  @patch('builtins.open', new_callable=mock_open)
  def test_run_test_binary(self, mock_file, mock_run):
    mock_run.return_value.returncode = 0
    rc = run_host_tests.run_test_binary('my_test', 0, 1, 'out.xml', 'out.txt')
    self.assertEqual(rc, 0)
    mock_run.assert_called_once()

  @patch('subprocess.run')
  @patch('builtins.open', new_callable=mock_open)
  def test_run_test_binary_fail(self, mock_file, mock_run):
    mock_run.return_value.returncode = 1
    rc = run_host_tests.run_test_binary('my_test', 0, 1, 'out.xml', 'out.txt')
    self.assertEqual(rc, 1)

  @patch('run_host_tests.run_test_binary')
  @patch('os.path.exists')
  def test_handle_run_success(self, mock_exists, mock_run_binary):
    mock_run_binary.return_value = 0
    mock_exists.return_value = True
    args = MagicMock()
    args.event_name = 'pull_request'
    args.results_dir = '/dummy/results'
    args.shard = 0
    args.num_gtest_shards = 1
    args.xvfb_run_path = 'xvfb-run'
    args.filter_json_dir = None

    failed, created = run_host_tests.handle_run(['/path/to/test1'], 1, args, {},
                                                {})
    self.assertEqual(failed, set())
    self.assertIn('test1', created)

  @patch('run_host_tests.run_test_binary')
  @patch('os.path.exists')
  @patch('run_host_tests.generate_crash_report')
  def test_handle_run_fail_crash(self, mock_crash, mock_exists,
                                 mock_run_binary):
    mock_run_binary.return_value = 1
    mock_exists.return_value = False  # XML missing
    args = MagicMock()
    args.event_name = 'pull_request'
    args.results_dir = '/dummy/results'
    args.shard = 0
    args.num_gtest_shards = 1
    args.xvfb_run_path = 'xvfb-run'
    args.filter_json_dir = None

    failed, created = run_host_tests.handle_run(['/path/to/test1'], 1, args, {},
                                                {})
    self.assertEqual(failed, {'/path/to/test1'})
    mock_crash.extract_crash.assert_called_once()


if __name__ == '__main__':
  unittest.main()
