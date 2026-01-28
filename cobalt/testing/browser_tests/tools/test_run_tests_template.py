#!/usr/bin/env python3
# Copyright 2025 The Cobalt Authors. All Rights Reserved.
#
# Tests for run_tests.template.py.
"""Tests for the portable test runner template."""

import importlib.util
import os
import unittest
import shutil
import tempfile
from unittest import mock
from pathlib import Path

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
    self.test_dir = tempfile.mkdtemp()
    self.src_dir = os.path.join(self.test_dir, 'src')
    os.makedirs(self.src_dir)

    self.target_map = {
        'android_target': {
            'is_android': True,
            'deps': 'gen/deps.runtime_deps',
            'runner': 'bin/run_test',
            'build_dir': 'out/android'
        },
        'linux_target': {
            'is_android': False,
            'deps': 'gen/linux.runtime_deps',
            'runner': 'cobalt_browsertests',
            'build_dir': 'out/linux'
        }
    }
    # Reset TARGET_MAP for each test
    run_tests_template.TARGET_MAP = self.target_map

  def tearDown(self):
    shutil.rmtree(self.test_dir)

  def _touch(self, *parts):
    path = os.path.join(self.src_dir, *parts)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    Path(path).touch()
    return path

  @mock.patch('shutil.which', return_value='/usr/bin/vpython3')
  @mock.patch('subprocess.call', return_value=0)
  @mock.patch('sys.argv', ['run_tests.py', 'android_target'])
  @mock.patch('os.walk', return_value=[('/tmp', [], [])])
  @mock.patch('os.listdir', return_value=[])
  def test_android_execution(self, unused_mock_listdir, unused_mock_walk,
                             mock_call):
    # Setup dummy files
    self._touch('gen/deps.runtime_deps')
    self._touch('bin/run_test')

    with mock.patch.object(run_tests_template, '__file__',
                           os.path.join(self.test_dir, 'run_tests.py')):
      exit_code = run_tests_template.main()

    self.assertEqual(exit_code, 0)
    expected_cmd = [
        os.path.abspath('/usr/bin/vpython3'),
        os.path.join(self.src_dir, 'bin/run_test'), '--runtime-deps-path',
        os.path.join(self.src_dir, 'gen/deps.runtime_deps')
    ]
    mock_call.assert_called_once_with(expected_cmd, cwd=self.src_dir)

  @mock.patch('shutil.which', return_value='/usr/bin/vpython3')
  @mock.patch('subprocess.call', return_value=0)
  @mock.patch('sys.argv', ['run_tests.py', 'linux_target'])
  @mock.patch('sys.executable', '/usr/bin/python3')
  @mock.patch('os.walk', return_value=[('/tmp', [], [])])
  @mock.patch('os.listdir', return_value=[])
  def test_linux_execution(self, unused_mock_listdir, unused_mock_walk,
                           mock_call):
    self._touch('gen/linux.runtime_deps')
    self._touch('cobalt_browsertests')
    self._touch('testing/xvfb.py')
    self._touch('cobalt/testing/browser_tests/run_browser_tests.py')

    with mock.patch.object(run_tests_template, '__file__',
                           os.path.join(self.test_dir, 'run_tests.py')):
      exit_code = run_tests_template.main()

    self.assertEqual(exit_code, 0)
    expected_cmd = [
        os.path.abspath('/usr/bin/vpython3'),
        os.path.join(self.src_dir, 'testing/xvfb.py'), '/usr/bin/python3',
        os.path.join(self.src_dir,
                     'cobalt/testing/browser_tests/run_browser_tests.py'),
        os.path.join(self.src_dir, 'cobalt_browsertests')
    ]
    mock_call.assert_called_once_with(expected_cmd, cwd=self.src_dir)

  @mock.patch('sys.argv', ['run_tests.py'])
  @mock.patch('sys.exit', side_effect=SystemExit(1))
  @mock.patch('logging.error')
  @mock.patch('os.walk', return_value=[('/tmp', [], [])])
  @mock.patch('os.listdir', return_value=[])
  def test_multiple_targets_no_selection_error(self, unused_mock_listdir,
                                               unused_mock_walk,
                                               unused_mock_log_error,
                                               mock_exit):
    with self.assertRaises(SystemExit):
      run_tests_template.main()
    mock_exit.assert_called_once_with(1)

  @mock.patch('shutil.which', return_value='/usr/bin/vpython3')
  @mock.patch('subprocess.call', return_value=0)
  @mock.patch('sys.argv', ['run_tests.py'])
  @mock.patch('os.walk', return_value=[('/tmp', [], [])])
  @mock.patch('os.listdir', return_value=[])
  def test_default_single_target(self, unused_mock_listdir, unused_mock_walk,
                                 mock_call):
    run_tests_template.TARGET_MAP = {
        'single': {
            'is_android': True,
            'deps': 'd',
            'runner': 'r',
            'build_dir': 'out/single'
        }
    }
    self._touch('d')
    self._touch('r')

    with mock.patch.object(run_tests_template, '__file__',
                           os.path.join(self.test_dir, 'run_tests.py')):
      run_tests_template.main()

    self.assertEqual(mock_call.call_args[0][0][1],
                     os.path.join(self.src_dir, 'r'))
    self.assertEqual(mock_call.call_args[1]['cwd'], self.src_dir)

  @mock.patch('shutil.which', return_value=None)
  @mock.patch('sys.exit', side_effect=SystemExit(1))
  @mock.patch('sys.argv', ['run_tests.py', 'android_target'])
  @mock.patch('os.walk', return_value=[('/tmp', [], [])])
  @mock.patch('os.listdir', return_value=[])
  def test_missing_vpython_error(self, unused_mock_listdir, unused_mock_walk,
                                 mock_exit):
    # Dummy files to pass earlier checks
    self._touch('gen/deps.runtime_deps')
    self._touch('bin/run_test')

    with mock.patch.object(run_tests_template, '__file__',
                           os.path.join(self.test_dir, 'run_tests.py')):
      with self.assertRaises(SystemExit):
        run_tests_template.main()
    mock_exit.assert_called_once_with(1)

  @mock.patch('shutil.which', return_value='/usr/bin/vpython3')
  @mock.patch('subprocess.call', return_value=0)
  @mock.patch('subprocess.run')
  @mock.patch('sys.argv',
              ['run_tests.py', '--init-command', 'ls -l', 'android_target'])
  @mock.patch('os.walk', return_value=[('/tmp', [], [])])
  @mock.patch('os.listdir', return_value=[])
  def test_init_command_execution(self, unused_mock_listdir, unused_mock_walk,
                                  mock_run, unused_mock_call):
    self._touch('gen/deps.runtime_deps')
    self._touch('bin/run_test')

    with mock.patch.object(run_tests_template, '__file__',
                           os.path.join(self.test_dir, 'run_tests.py')):
      exit_code = run_tests_template.main()

    self.assertEqual(exit_code, 0)
    mock_run.assert_called_once_with(
        'ls -l', shell=True, check=True, cwd=self.src_dir)


if __name__ == '__main__':
  unittest.main()
