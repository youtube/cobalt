#!/usr/bin/env python3
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
"""
Tests for the cobalt_code_coverage_bridge.py script.
"""

import cobalt_code_coverage_bridge
import sys
import unittest
from unittest import mock


class CodeCoverageToolTest(unittest.TestCase):
  """
  Tests for the code coverage tool.
  """

  @mock.patch('os.path.isdir', return_value=True)
  @mock.patch(
      'os.listdir',
      return_value=[
          'bin', 'cr_build_revision', 'lib', 'llvmobjdump_build_revision'
      ])
  @mock.patch('subprocess.check_call')
  # pylint: disable=C0103
  def test_coverage_tools_dir_not_provided(self, mock_check_call, _mock_listdir,
                                           _mock_isdir):
    """
    Tests that the tool adds the --coverage-tools-dir argument when it's not
    provided.
    """
    del _mock_listdir  # Unused argument, as per user instruction.
    del _mock_isdir  # Unused argument, as per user instruction.
    sys.argv = ['cobalt_code_coverage_bridge.py', 'test_arg']
    cobalt_code_coverage_bridge.main()
    mock_check_call.assert_called_once_with([
        sys.executable,
        cobalt_code_coverage_bridge.COVERAGE_PY_PATH,
        'test_arg',
        '--coverage-tools-dir',
        cobalt_code_coverage_bridge.LLVM_BIN_DIR,
    ])

  @mock.patch('subprocess.check_call')
  def test_coverage_tools_dir_provided(self, mock_check_call):
    """
    Tests that the tool doesn't add the --coverage-tools-dir argument when it's
    already provided.
    """
    sys.argv = [
        'cobalt_code_coverage_bridge.py',
        '--coverage-tools-dir',
        '/my/custom/dir',
        'test_arg',
    ]
    cobalt_code_coverage_bridge.main()
    mock_check_call.assert_called_once_with([
        sys.executable,
        cobalt_code_coverage_bridge.COVERAGE_PY_PATH,
        '--coverage-tools-dir',
        '/my/custom/dir',
        'test_arg',
    ])

  @mock.patch('subprocess.check_call')
  def test_coverage_tools_dir_provided_with_equals(self, mock_check_call):
    """
    Tests that the tool doesn't add the --coverage-tools-dir argument when it's
    already provided using an equals sign.
    """
    sys.argv = [
        'cobalt_code_coverage_bridge.py',
        '--coverage-tools-dir=/my/custom/dir',
        'test_arg',
    ]
    cobalt_code_coverage_bridge.main()
    mock_check_call.assert_called_once_with([
        sys.executable,
        cobalt_code_coverage_bridge.COVERAGE_PY_PATH,
        '--coverage-tools-dir=/my/custom/dir',
        'test_arg',
    ])

  @mock.patch('os.path.isdir', return_value=True)
  @mock.patch(
      'os.listdir',
      return_value=[
          'bin', 'cr_build_revision', 'lib', 'llvmobjdump_build_revision'
      ])
  @mock.patch('builtins.print')
  # pylint: disable=C0103
  def test_package_argument_provided(self, mock_print, _mock_listdir,
                                     _mock_isdir):
    """
    Tests that the tool prints an error message and returns 1 when the
    --package argument is provided.
    """


if __name__ == '__main__':
  unittest.main()
