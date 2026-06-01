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
"""Unit tests for prepare_results_for_ci.py."""

import os
import tempfile
import unittest
from unittest.mock import MagicMock, patch

import prepare_results_for_ci


class TestPrepareResultsForCi(unittest.TestCase):

  def test_get_attempt_from_path(self):
    self.assertEqual(
        prepare_results_for_ci._get_attempt_from_path('test_attempt_2.xml'), 2)
    self.assertEqual(
        prepare_results_for_ci._get_attempt_from_path('test.xml'), 1)
    self.assertEqual(
        prepare_results_for_ci._get_attempt_from_path('test_attempt_abc.xml'),
        1)

  def test_group_files_by_attempt(self):
    files = [
        '/path/test_attempt_1.xml', '/path/test_attempt_2.xml',
        '/path/other_attempt_1.xml'
    ]
    grouped = prepare_results_for_ci._group_files_by_attempt(files)
    self.assertIn(1, grouped)
    self.assertIn(2, grouped)
    self.assertEqual(len(grouped[1]), 2)
    self.assertEqual(len(grouped[2]), 1)

  @patch('prepare_results_for_ci._handle_pull_request')
  @patch('glob.glob')
  def test_main_pull_request(self, mock_glob, mock_handle_pr):
    mock_glob.return_value = ['file1.xml']
    mock_handle_pr.return_value = 0
    test_args = [
        '--glob', '*.xml', '--event_name', 'pull_request', '--output_dir',
        '/dummy/out'
    ]
    with patch('sys.argv', ['prepare_results_for_ci.py'] + test_args):
      rc = prepare_results_for_ci.main()
      self.assertEqual(rc, 0)
      mock_handle_pr.assert_called_once()

  @patch('prepare_results_for_ci._handle_push')
  @patch('glob.glob')
  def test_main_push(self, mock_glob, mock_handle_push):
    mock_glob.return_value = ['file1.xml']
    mock_handle_push.return_value = 0
    test_args = [
        '--glob', '*.xml', '--event_name', 'push', '--output_dir', '/dummy/out'
    ]
    with patch('sys.argv', ['prepare_results_for_ci.py'] + test_args):
      rc = prepare_results_for_ci.main()
      self.assertEqual(rc, 0)
      mock_handle_push.assert_called_once()

  @patch('glob.glob')
  def test_main_unsupported_event(self, mock_glob):
    mock_glob.return_value = ['file1.xml']
    test_args = [
        '--glob', '*.xml', '--event_name', 'schedule', '--output_dir',
        '/dummy/out'
    ]
    with patch('sys.argv', ['prepare_results_for_ci.py'] + test_args):
      rc = prepare_results_for_ci.main()
      self.assertEqual(rc, 0)  # Should return 0 and log warning

  @patch('prepare_results_for_ci._handle_pull_request')
  @patch('glob.glob')
  def test_main_with_shards_match(self, mock_glob, mock_handle_pr):
    mock_glob.return_value = ['file1.xml']
    mock_handle_pr.return_value = 0
    test_args = [
        '--glob', '*.xml', '--event_name', 'pull_request', '--output_dir',
        '/dummy/out', '--num_shards', '1'
    ]
    with patch('sys.argv', ['prepare_results_for_ci.py'] + test_args):
      rc = prepare_results_for_ci.main()
      self.assertEqual(rc, 0)
      mock_handle_pr.assert_called_once()

  @patch('glob.glob')
  def test_main_with_shards_mismatch(self, mock_glob):
    mock_glob.return_value = ['file1.xml']
    test_args = [
        '--glob', '*.xml', '--event_name', 'pull_request', '--output_dir',
        '/dummy/out', '--num_shards', '2'
    ]
    with patch('sys.argv', ['prepare_results_for_ci.py'] + test_args):
      rc = prepare_results_for_ci.main()
      self.assertEqual(rc, 1)


if __name__ == '__main__':
  unittest.main()
