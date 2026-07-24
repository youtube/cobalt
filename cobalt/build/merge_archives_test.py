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
"""Tests the merge_archives module."""

import os
import sys
import unittest
from unittest.mock import mock_open, patch

from cobalt.build import merge_archives


class MergeArchivesTest(unittest.TestCase):

  @patch('subprocess.run')
  @patch(
      'builtins.open',
      new_callable=mock_open,
      read_data='lib1.rlib\nlib2.rlib\nlib1.rlib\n')
  def test_success_dedup(self, mock_file, mock_run):
    mock_run.return_value.returncode = 0

    test_args = [
        'merge_archives.py', '--rust-libs-file', 'dummy_rust_libs.txt',
        '--cpp-lib', 'libcpp.a', '--output', 'libout.a'
    ]
    with patch.object(sys, 'argv', test_args):
      ret = merge_archives.main()

    self.assertEqual(ret, 0)
    mock_file.assert_called_once_with(
        'dummy_rust_libs.txt', 'r', encoding='utf-8')

    abs_out = os.path.abspath('libout.a')
    abs_cpp = os.path.abspath('libcpp.a')
    abs_lib1 = os.path.abspath('lib1.rlib')
    abs_lib2 = os.path.abspath('lib2.rlib')

    expected_cmd = [
        'xcrun', 'libtool', '-static', '-o', abs_out, abs_cpp, abs_lib1,
        abs_lib2
    ]
    mock_run.assert_called_once_with(
        expected_cmd, capture_output=True, text=True, check=False)

  @patch('subprocess.run')
  @patch('builtins.open', new_callable=mock_open, read_data='')
  def test_empty_rust_libs(self, mock_file, mock_run):
    mock_run.return_value.returncode = 0

    test_args = [
        'merge_archives.py', '--rust-libs-file', 'dummy_rust_libs.txt',
        '--cpp-lib', 'libcpp.a', '--output', 'libout.a'
    ]
    with patch.object(sys, 'argv', test_args):
      ret = merge_archives.main()

    self.assertEqual(ret, 0)
    mock_file.assert_called_once_with(
        'dummy_rust_libs.txt', 'r', encoding='utf-8')

    abs_out = os.path.abspath('libout.a')
    abs_cpp = os.path.abspath('libcpp.a')

    expected_cmd = ['xcrun', 'libtool', '-static', '-o', abs_out, abs_cpp]
    mock_run.assert_called_once_with(
        expected_cmd, capture_output=True, text=True, check=False)

  @patch('builtins.open', side_effect=IOError('Read error'))
  def test_io_error(self, mock_file):
    test_args = [
        'merge_archives.py', '--rust-libs-file', 'dummy_rust_libs.txt',
        '--cpp-lib', 'libcpp.a', '--output', 'libout.a'
    ]
    with patch.object(sys, 'argv', test_args):
      with patch('sys.stderr'):
        ret = merge_archives.main()

    self.assertEqual(ret, 1)
    mock_file.assert_called_once_with(
        'dummy_rust_libs.txt', 'r', encoding='utf-8')

  @patch('subprocess.run')
  @patch('builtins.open', new_callable=mock_open, read_data='lib1.rlib\n')
  def test_libtool_failure(self, mock_file, mock_run):
    mock_run.return_value.returncode = 42
    mock_run.return_value.stdout = 'some stdout'
    mock_run.return_value.stderr = 'some error'

    test_args = [
        'merge_archives.py', '--rust-libs-file', 'dummy_rust_libs.txt',
        '--cpp-lib', 'libcpp.a', '--output', 'libout.a'
    ]
    with patch.object(sys, 'argv', test_args):
      with patch('sys.stderr'):
        ret = merge_archives.main()

    self.assertEqual(ret, 42)
    mock_file.assert_called_once_with(
        'dummy_rust_libs.txt', 'r', encoding='utf-8')


if __name__ == '__main__':
  unittest.main()
