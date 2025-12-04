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
"""Tests the execution of shell commands within this suite of coverage tools"""

import unittest
import subprocess
from cobalt.tools.code_coverage.command_runner import run_command
from unittest import mock


class CommandRunnerTest(unittest.TestCase):

  @mock.patch('subprocess.Popen')
  def test_run_command_success(self, mock_popen):
    """Test a successful command execution."""
    mock_process = mock.Mock()
    mock_process.returncode = 0
    mock_process.stdout.readline.side_effect = ['output\n', '']
    # Configure the mock to be a context manager
    mock_popen.return_value.__enter__.return_value = mock_process

    run_command(['echo', 'hello'])
    mock_popen.assert_called_once_with(['echo', 'hello'],
                                       stdout=subprocess.PIPE,
                                       stderr=subprocess.STDOUT,
                                       text=True)

  @mock.patch('sys.exit')
  @mock.patch('subprocess.Popen')
  def test_run_command_failure_with_check(self, mock_popen, mock_exit):
    """Test a failing command with check=True exits."""
    mock_process = mock.Mock()
    mock_process.returncode = 1
    mock_process.stdout.readline.side_effect = ['error\n', '']
    # Configure the mock to be a context manager
    mock_popen.return_value.__enter__.return_value = mock_process

    run_command(['false'])
    mock_exit.assert_called_once_with(1)

  @mock.patch('sys.exit')
  @mock.patch('subprocess.Popen')
  def test_run_command_failure_without_check(self, mock_popen, mock_exit):
    """Test a failing command with check=False does not exit."""
    mock_process = mock.Mock()
    mock_process.returncode = 1
    mock_process.stdout.readline.side_effect = ['error\n', '']
    # Configure the mock to be a context manager
    mock_popen.return_value.__enter__.return_value = mock_process

    run_command(['false'], check=False)
    mock_exit.assert_not_called()

  @mock.patch('sys.exit')
  @mock.patch(
      'subprocess.Popen', side_effect=FileNotFoundError('Command not found'))
  def test_run_command_file_not_found(self, mock_popen, mock_exit):
    """Test that a FileNotFoundError is caught and causes an exit."""
    del mock_popen  # unused argument
    run_command(['non_existent_command'])
    mock_exit.assert_called_once_with(1)

  @mock.patch('sys.exit')
  @mock.patch('subprocess.Popen', side_effect=Exception('Something broke'))
  def test_run_command_generic_exception(self, mock_popen, mock_exit):
    """Test that a generic exception is caught and causes an exit."""
    del mock_popen  # unused argument
    run_command(['any_command'])
    mock_exit.assert_called_once_with(1)


if __name__ == '__main__':
  unittest.main()
