#!/usr/bin/env python3
# Copyright 2026 The Cobalt Authors. All Rights Reserved.
#
# Tests for run_browser_tests.py.
"""Tests for the Cobalt browser test runner script."""

import importlib.util
import os
import unittest
from unittest import mock

# Load run_browser_tests.py as a module
script_path = os.path.join(
    os.path.dirname(os.path.abspath(__file__)), 'run_browser_tests.py')
spec = importlib.util.spec_from_file_location('run_browser_tests', script_path)
run_browser_tests = importlib.util.module_from_spec(spec)
spec.loader.exec_module(run_browser_tests)


class TestRunBrowserTests(unittest.TestCase):
  """Unit tests for the browser test runner logic."""

  def test_sanitize_filename(self):
    """Verifies that filename sanitization works correctly."""
    self.assertEqual(
        run_browser_tests.sanitize_filename('Suite.Test'), 'Suite.Test')
    self.assertEqual(
        run_browser_tests.sanitize_filename('Suite:Test'), 'Suite_Test')
    self.assertEqual(
        run_browser_tests.sanitize_filename('Test*With?Invalid"Chars'),
        'Test_With_Invalid_Chars')
    self.assertEqual(
        run_browser_tests.sanitize_filename('Path/To/Test'), 'Path_To_Test')

  def test_is_valid_test_name(self):
    """Verifies that the script correctly identifies valid gtest names."""
    self.assertTrue(run_browser_tests.is_valid_test_name('MySuite'))
    self.assertTrue(run_browser_tests.is_valid_test_name('Test_Case_1'))
    self.assertFalse(run_browser_tests.is_valid_test_name('Total: 14, P: 14'))
    self.assertFalse(run_browser_tests.is_valid_test_name('Invalid Name!'))
    self.assertFalse(run_browser_tests.is_valid_test_name(''))

  @mock.patch('subprocess.check_output')
  @mock.patch('subprocess.run')
  @mock.patch('os.path.isfile', return_value=True)
  @mock.patch('tempfile.mkdtemp', return_value='/tmp/user_data_dir')
  @mock.patch('shutil.rmtree')
  @mock.patch(
      'sys.argv',
      ['run_browser_tests.py', './binary', '--gtest_output=xml:results.xml'])
  # pylint: disable=too-many-positional-arguments
  def test_main_execution_flow(self, mock_rmtree, mock_mkdtemp, mock_isfile,
                               mock_run, mock_check_output):
    """Verifies the main execution flow and command construction."""
    del mock_rmtree, mock_mkdtemp, mock_isfile  # Unused.

    # Mock gtest_list_tests output
    mock_check_output.return_value = 'Suite.\n  Test1\n  Test2\n'

    # Mock individual test run success
    mock_run.return_value.returncode = 0

    with mock.patch('sys.exit') as mock_exit:
      run_browser_tests.main()

      # Should call sys.exit(0) on success
      mock_exit.assert_called_once_with(0)

      # Verify listing command
      mock_check_output.assert_called_once()
      list_cmd = mock_check_output.call_args[0][0]
      self.assertIn('--gtest_list_tests', list_cmd)

      # Verify run commands for both tests
      self.assertEqual(mock_run.call_count, 2)

      # Verify first test run command
      first_run_cmd = mock_run.call_args_list[0][0][0]
      self.assertIn('--gtest_filter=Suite.Test1', first_run_cmd)
      self.assertIn('--gtest_output=xml:results_Suite.Test1.xml', first_run_cmd)
      self.assertIn('--user-data-dir=/tmp/user_data_dir', first_run_cmd)

  @mock.patch('subprocess.check_output')
  @mock.patch('os.path.isfile', return_value=True)
  @mock.patch('sys.argv', ['run_browser_tests.py', './binary'])
  def test_parsing_ignores_summary(self, mock_isfile, mock_check_output):
    """Verifies that the script ignores summary lines during parsing."""
    del mock_isfile  # Unused.

    # Mock output containing both valid tests and summary noise
    mock_check_output.return_value = (
        'Suite.\n  Test1\nNavigationBaseBrowserTest.\n  Total: 14, P: 14\n')

    with mock.patch('subprocess.run') as mock_run:
      with mock.patch('sys.exit'):
        run_browser_tests.main()

        # Should only run Test1, not the summary line
        self.assertEqual(mock_run.call_count, 1)
        self.assertIn('--gtest_filter=Suite.Test1', mock_run.call_args[0][0])


if __name__ == '__main__':
  unittest.main()
