#!/usr/bin/env python3
# Copyright 2025 The Cobalt Authors. All Rights Reserved.
#
# Tests for run_tests.template.py.
"""Tests for the portable test runner template script."""

import os
import sys
import unittest
from unittest import mock
import tempfile
import shutil


# To test the template, we need to read it, substitute TARGET_MAP,
# and then import/execute it.
class TestRunTestsTemplate(unittest.TestCase):
  """Unit tests for the test runner template logic."""

  def setUp(self):
    self.test_dir = tempfile.mkdtemp()
    self.template_path = os.path.join(
        os.path.dirname(__file__), 'run_tests.template.py')
    self.run_tests_path = os.path.join(self.test_dir, 'run_tests.py')

    # Create a dummy src directory to satisfy path checks
    self.src_dir = os.path.join(self.test_dir, 'src')
    os.makedirs(self.src_dir)
    self.dummy_deps = os.path.join(self.src_dir, 'dummy.runtime_deps')
    self.dummy_runner = os.path.join(self.src_dir, 'dummy_runner.py')
    with open(self.dummy_deps, 'w', encoding='utf-8') as f:
      f.write('# dummy deps')
    with open(self.dummy_runner, 'w', encoding='utf-8') as f:
      f.write('#!/usr/bin/env python3\nimport sys\nsys.exit(0)')
    os.chmod(self.dummy_runner, 0o755)

    # Create a dummy depot_tools
    self.depot_tools = os.path.join(self.test_dir, 'depot_tools')
    os.makedirs(self.depot_tools)
    with open(
        os.path.join(self.depot_tools, 'vpython3'), 'w', encoding='utf-8') as f:
      f.write('#!/bin/sh\nexit 0')
    os.chmod(os.path.join(self.depot_tools, 'vpython3'), 0o755)

  def tearDown(self):
    shutil.rmtree(self.test_dir)

  def prepare_script(self, target_map):
    with open(self.template_path, 'r', encoding='utf-8') as f:
      content = f.read()
    content = content.replace('TARGET_MAP = {}',
                              f'TARGET_MAP = {repr(target_map)}')
    with open(self.run_tests_path, 'w', encoding='utf-8') as f:
      f.write(content)

  @mock.patch('subprocess.Popen')
  @mock.patch('subprocess.call', return_value=0)
  @mock.patch('shutil.which')
  def test_init_command_with_timeout(self, mock_which, mock_call, mock_popen):
    del mock_call  # Unused.
    target_map = {
        'test_target': {
            'deps': 'dummy.runtime_deps',
            'runner': 'dummy_runner.py',
            'build_dir': '.'
        }
    }
    self.prepare_script(target_map)
    mock_which.return_value = os.path.join(self.depot_tools, 'vpython3')

    # Import the generated script
    sys.path.insert(0, self.test_dir)
    if 'run_tests' in sys.modules:
      del sys.modules['run_tests']
    # pylint: disable=import-outside-toplevel
    import run_tests

    test_args = [
        'run_tests.py', '--init-command', 'socat addr1 addr2',
        '--socat-timeout', '10', 'test_target'
    ]
    with mock.patch('sys.argv', test_args):
      with mock.patch('os.path.isfile', return_value=True):
        run_tests.main()

    # Verify Popen was called with the command AND the timeout inserted
    # correctly
    # Command should be: ['socat', '-t', '10', 'addr1', 'addr2']
    mock_popen.assert_called_once()
    args = mock_popen.call_args[0][0]
    self.assertEqual(args, ['socat', '-t', '10', 'addr1', 'addr2'])


if __name__ == '__main__':
  unittest.main()
