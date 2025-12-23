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
"""Unit tests for the run_android_tests.py script."""

import unittest
from unittest.mock import patch, ANY
import run_android_tests


class RunAndroidTestsTest(unittest.TestCase):

  @patch('run_android_tests.os.remove')
  @patch('run_android_tests.tempfile.NamedTemporaryFile')
  @patch('run_android_tests.argparse.ArgumentParser')
  @patch('run_android_tests.run_command')
  def test_main_with_gtest_filter(self, mock_run_command, mock_argparse,
                                  mock_tempfile, mock_os_remove):
    """Tests that the main function with gtest_filter calls Popen with the
    correct args."""
    # Mock the command line arguments
    mock_args = mock_argparse.return_value.parse_args.return_value
    mock_args.apk_path = 'test.apk'
    mock_args.build_dir = '/build/dir'
    mock_args.test_list = None
    mock_args.android_serial = None
    mock_args.gtest_filter = '*Test*'

    # Mock the tempfile
    mock_file = mock_tempfile.return_value.__enter__.return_value
    mock_file.name = 'tempfile'

    run_android_tests.main()
    mock_os_remove.assert_called_once_with('tempfile')

    # Assert the adb push command for the flags file
    mock_run_command.assert_any_call(
        ['adb', 'push', 'tempfile', '/data/local/tmp/test_list.txt'], env=ANY)

    # Assert that the adb shell sh command is called.
    mock_run_command.assert_any_call(
        ['adb', 'shell', 'sh', '/data/local/tmp/run_instrumentation_tests.sh'],
        env=ANY)


if __name__ == '__main__':
  unittest.main()
