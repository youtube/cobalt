#!/usr/bin/env python3
# Copyright 2025 The Cobalt Authors. All Rights Reserved.
#
# Tests for run_tests.template.py.
"""Tests for the portable test runner template script."""

import importlib
import json
import shutil
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock


# To test the template, we need to read it, substitute TARGET_MAP,
# and then import/execute it.
class TestRunTestsTemplate(unittest.TestCase):
  """Unit tests for the test runner template logic."""

  def setUp(self):
    self.test_dir = Path(tempfile.mkdtemp())
    self.template_path = (Path(__file__).parent /
                          'run_tests.template.py').resolve()
    self.run_tests_path = self.test_dir / 'run_tests.py'

    # Create a dummy src directory to satisfy path checks
    self.src_dir = self.test_dir / 'src'
    self.src_dir.mkdir()
    self.dummy_deps = self.src_dir / 'dummy.runtime_deps'
    self.dummy_runner = self.src_dir / 'dummy_runner.py'
    self.dummy_deps.write_text('# dummy deps', encoding='utf-8')
    self.dummy_runner.write_text(
        '#!/usr/bin/env python3\nimport sys\nsys.exit(0)', encoding='utf-8')
    self.dummy_runner.chmod(0o755)

    # Create a dummy depot_tools
    self.depot_tools = self.test_dir / 'depot_tools'
    self.depot_tools.mkdir()
    (self.depot_tools / 'vpython3').write_text(
        '#!/bin/sh\nexit 0', encoding='utf-8')
    (self.depot_tools / 'vpython3').chmod(0o755)

  def tearDown(self):
    shutil.rmtree(self.test_dir)

  def prepare_script(self, target_map):
    content = self.template_path.read_text(encoding='utf-8')
    content = content.replace('TARGET_MAP = {}',
                              f'TARGET_MAP = {repr(target_map)}')
    self.run_tests_path.write_text(content, encoding='utf-8')

  def load_run_tests(self):
    if str(self.test_dir) not in sys.path:
      sys.path.insert(0, str(self.test_dir))
    if 'run_tests' in sys.modules:
      return importlib.reload(sys.modules['run_tests'])
    return importlib.import_module('run_tests')

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
    mock_which.return_value = str(self.test_dir / 'depot_tools/vpython3')

    # Create dummy target_map.json to avoid FileNotFoundError
    (self.test_dir / 'target_map.json').write_text('{}', encoding='utf-8')

    run_tests = self.load_run_tests()

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

  @mock.patch('subprocess.check_call')
  def test_ingest_artifacts(self, mock_check_call):
    artifacts_tar = self.test_dir / 'artifacts.tar'
    artifacts_tar.write_text('fake tar content', encoding='utf-8')

    self.prepare_script({})
    run_tests = self.load_run_tests()
    run_tests.ingest_artifacts(artifacts_tar, self.test_dir)

    mock_check_call.assert_called_once()
    args = mock_check_call.call_args[0][0]
    self.assertIn('tar', args)
    self.assertIn(str(artifacts_tar), args)
    self.assertIn(str(self.test_dir), args)

  def test_load_target_map(self):
    target_map_data = {
        'new_target': {
            'deps': 'new.deps',
            'runner': 'new_runner',
            'build_dir': '.'
        }
    }
    (self.test_dir / 'target_map.json').write_text(
        json.dumps(target_map_data), encoding='utf-8')

    self.prepare_script({})
    run_tests = self.load_run_tests()
    # Reset TARGET_MAP for test
    run_tests.TARGET_MAP = {}
    run_tests.load_target_map(self.test_dir)

    self.assertEqual(run_tests.TARGET_MAP, target_map_data)


if __name__ == '__main__':
  unittest.main()
