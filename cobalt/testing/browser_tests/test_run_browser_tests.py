#!/usr/bin/env python3
# Copyright 2026 The Cobalt Authors. All Rights Reserved.
#
# Tests for run_browser_tests.py.
"""Tests for the Cobalt browser test runner script."""

import importlib.util
import os
import shutil
import tempfile
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

  def setUp(self):
    self.test_dir = tempfile.mkdtemp()

  def tearDown(self):
    shutil.rmtree(self.test_dir)

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
  @mock.patch('tempfile.mkdtemp')
  @mock.patch('shutil.rmtree')
  # pylint: disable=too-many-positional-arguments
  def test_main_execution_flow(self, mock_rmtree, mock_mkdtemp, mock_isfile,
                               mock_run, mock_check_output):
    """Verifies the main execution flow and XML merging."""
    del mock_rmtree, mock_isfile  # Unused.
    final_xml = os.path.join(self.test_dir, 'results.xml')
    mock_mkdtemp.return_value = self.test_dir

    # Mock sys.argv
    argv = [
        'run_browser_tests.py', './binary', f'--gtest_output=xml:{final_xml}'
    ]
    with mock.patch('sys.argv', argv):
      # Mock gtest_list_tests output
      mock_check_output.return_value = 'Suite.\n  Test1\n'

      # Mock individual test run success
      mock_run.return_value.returncode = 0

      # Create a fake individual XML result that the script will read
      def side_effect(*args, **kwargs):
        del args, kwargs  # Unused.
        test_xml = os.path.join(self.test_dir, 'test_result.xml')
        with open(test_xml, 'w', encoding='utf-8') as f:
          f.write('<testsuites><testsuite name="Suite" tests="1">'
                  '<testcase name="Test1"/></testsuite></testsuites>')
        return mock.Mock(returncode=0)

      mock_run.side_effect = side_effect

      with mock.patch('sys.exit') as mock_exit:
        run_browser_tests.main()

        mock_exit.assert_called_once_with(0)

        # Verify final XML content
        with open(final_xml, 'r', encoding='utf-8') as f:
          content = f.read()
          self.assertIn('<testsuite name="Suite" tests="1">', content)
          self.assertIn('<testcase name="Test1"/>', content)
          self.assertIn('tests="1"', content)  # Total count updated

  @mock.patch('subprocess.check_output')
  @mock.patch('os.path.isfile', return_value=True)
  def test_parsing_ignores_summary(self, mock_isfile, mock_check_output):
    """Verifies that the script ignores summary lines during parsing."""
    del mock_isfile  # Unused.
    with mock.patch('sys.argv', ['run_browser_tests.py', './binary']):
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
