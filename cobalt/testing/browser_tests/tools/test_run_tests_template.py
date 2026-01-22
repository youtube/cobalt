#!/usr/bin/env python3
# Copyright 2025 The Cobalt Authors. All Rights Reserved.
#
# Tests for run_tests.template.py.
"""Tests for the portable test runner template."""

import importlib.util
import os
import unittest
from unittest import mock

# Load run_tests.template.py as a module
template_path = os.path.join(
    os.path.dirname(os.path.abspath(__file__)), 'run_tests.template.py')
spec = importlib.util.spec_from_file_location('run_tests_template',
                                              template_path)
run_tests_template = importlib.util.module_from_spec(spec)
spec.loader.exec_module(run_tests_template)


class TestRunTestsTemplate(unittest.TestCase):
  """Unit tests for the template runner logic."""

  def setUp(self):
    self.target_map = {
        'android_target': {
            'is_android': True,
            'deps': 'gen/deps.runtime_deps',
            'runner': 'bin/run_test'
        },
        'linux_target': {
            'is_android': False,
            'deps': 'gen/linux.runtime_deps',
            'runner': 'cobalt_browsertests'
        }
    }
    # Reset TARGET_MAP for each test
    run_tests_template.TARGET_MAP = self.target_map

  @mock.patch('os.path.abspath', return_value='/tmp/stage')
  @mock.patch('os.path.isfile', return_value=True)
  @mock.patch('shutil.which', return_value='/usr/bin/vpython3')
  @mock.patch('subprocess.call', return_value=0)
  @mock.patch('sys.argv', ['run_tests.py', 'android_target'])
  def test_android_execution(self, mock_call, *args):
    del args  # Unused.
    exit_code = run_tests_template.main()
    self.assertEqual(exit_code, 0)

    # Verify command construction
    # abspath('/tmp/stage') -> dirname is '/tmp' -> src_dir is '/tmp/src'
    expected_cmd = [
        '/usr/bin/vpython3', '/tmp/src/bin/run_test', '--runtime-deps-path',
        '/tmp/src/gen/deps.runtime_deps'
    ]
    mock_call.assert_called_once_with(expected_cmd)

  @mock.patch('os.path.abspath', return_value='/tmp/stage')
  @mock.patch('os.path.isfile', return_value=True)
  @mock.patch('shutil.which', return_value='/usr/bin/vpython3')
  @mock.patch('subprocess.call', return_value=0)
  @mock.patch('sys.argv', ['run_tests.py', 'linux_target'])
  @mock.patch('sys.executable', '/usr/bin/python3')
  def test_linux_execution(self, mock_call, *args):
    del args  # Unused.
    exit_code = run_tests_template.main()
    self.assertEqual(exit_code, 0)

    # Verify command construction with xvfb.py
    expected_cmd = [
        '/usr/bin/vpython3', '/tmp/src/testing/xvfb.py', '/usr/bin/python3',
        '/tmp/src/cobalt/testing/browser_tests/run_browser_tests.py',
        '/tmp/src/cobalt_browsertests'
    ]
    mock_call.assert_called_once_with(expected_cmd)

  @mock.patch('sys.argv', ['run_tests.py'])
  @mock.patch('sys.exit', side_effect=SystemExit(1))
  @mock.patch('logging.error')
  def test_multiple_targets_no_selection_error(self, mock_log_error, mock_exit):
    with self.assertRaises(SystemExit):
      run_tests_template.main()
    mock_exit.assert_called_once_with(1)
    mock_log_error.assert_any_call(
        'Multiple targets available. Please specify one: %s',
        ['android_target', 'linux_target'])

  @mock.patch('os.path.abspath', return_value='/tmp/stage')
  @mock.patch('os.path.isfile', return_value=True)
  @mock.patch('shutil.which', return_value='/usr/bin/vpython3')
  @mock.patch('subprocess.call', return_value=0)
  @mock.patch('sys.argv', ['run_tests.py'])
  def test_default_single_target(self, mock_call, *args):
    del args  # Unused.
    run_tests_template.TARGET_MAP = {
        'single': {
            'is_android': True,
            'deps': 'd',
            'runner': 'r'
        }
    }
    run_tests_template.main()
    self.assertEqual(mock_call.call_args[0][0][1], '/tmp/src/r')

  @mock.patch('shutil.which', return_value=None)
  @mock.patch('os.path.isfile', return_value=True)
  @mock.patch('sys.exit', side_effect=SystemExit(1))
  @mock.patch('sys.argv', ['run_tests.py', 'android_target'])
  def test_missing_vpython_error(self, mock_exit, *args):
    del args  # Unused.
    with self.assertRaises(SystemExit):
      run_tests_template.main()
    mock_exit.assert_called_once_with(1)


if __name__ == '__main__':
  unittest.main()
