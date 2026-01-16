#!/usr/bin/env python3
# Copyright 2025 The Cobalt Authors. All Rights Reserved.
#
# Tests for collect_test_artifacts.py.
"""Tests for the collect_test_artifacts script."""

import unittest
from unittest import mock
import os
from pathlib import Path
import collect_test_artifacts


class TestCollectTestArtifacts(unittest.TestCase):
  """Unit tests for artifact collection logic."""

  def test_find_runtime_deps_exact_match(self):
    with mock.patch('pathlib.Path.rglob') as mock_rglob:
      mock_rglob.return_value = [
          Path('out/dir/cobalt_browsertests__test_runner_script.runtime_deps')
      ]
      result = collect_test_artifacts.find_runtime_deps('out/dir')
      self.assertEqual(
          result,
          Path('out/dir/cobalt_browsertests__test_runner_script.runtime_deps'))
      mock_rglob.assert_any_call(
          'cobalt_browsertests__test_runner_script.runtime_deps')

  def test_find_runtime_deps_fallback(self):
    with mock.patch('pathlib.Path.rglob') as mock_rglob:
      # First call (exact) returns empty, second call (fallback) returns match
      mock_rglob.side_effect = [
          [], [Path('out/dir/some_browsertests.runtime_deps')]
      ]
      result = collect_test_artifacts.find_runtime_deps('out/dir')
      self.assertEqual(result, Path('out/dir/some_browsertests.runtime_deps'))
      self.assertEqual(mock_rglob.call_count, 2)

  def test_get_test_runner_android(self):
    result = collect_test_artifacts.get_test_runner(
        'out/android', is_android=True)
    self.assertEqual(result, os.path.join('bin', 'run_cobalt_browsertests'))

  def test_get_test_runner_linux_bin(self):
    with mock.patch('os.path.isfile') as mock_isfile:
      # Simulate bin/run_cobalt_browsertests exists
      mock_isfile.side_effect = lambda x: 'bin/run_cobalt_browsertests' in x
      result = collect_test_artifacts.get_test_runner(
          'out/linux', is_android=False)
      self.assertEqual(result, os.path.join('bin', 'run_cobalt_browsertests'))

  def test_get_test_runner_linux_fallback(self):
    with mock.patch('os.path.isfile', return_value=False):
      result = collect_test_artifacts.get_test_runner(
          'out/linux', is_android=False)
      self.assertEqual(result, 'cobalt_browsertests')

  @mock.patch('subprocess.call')
  @mock.patch('os.makedirs')
  @mock.patch('os.path.isdir', return_value=True)
  def test_copy_fast_dir(self, mock_isdir, mock_makedirs, mock_call):
    del mock_isdir  # Unused argument.
    collect_test_artifacts.copy_fast('src/dir', 'dst/dir')
    mock_makedirs.assert_called_once_with('dst', exist_ok=True)
    mock_call.assert_called_once_with(['cp', '-a', 'src/dir', 'dst/'],
                                      stderr=mock.ANY)

  @mock.patch('builtins.open', new_callable=mock.mock_open)
  @mock.patch('os.chmod')
  def test_generate_runner_py(self, mock_chmod, mock_file):
    collect_test_artifacts.generate_runner_py('run.py', 'out/dir', 'runner',
                                              'deps')
    mock_file.assert_called_once_with('run.py', 'w', encoding='utf-8')
    handle = mock_file()
    # Check if key variables are in the written content
    written_content = handle.write.call_args[0][0]
    self.assertIn('deps', written_content)
    self.assertIn('runner', written_content)
    self.assertIn('vpython3', written_content)
    mock_chmod.assert_called_once_with('run.py', 0o755)


if __name__ == '__main__':
  unittest.main()

  @mock.patch('builtins.open', new_callable=mock.mock_open)
  @mock.patch('os.chmod')
  def test_generate_runner_py(self, mock_chmod, mock_file):
    collect_test_artifacts.generate_runner_py('run.py', 'out/dir', 'runner',
                                              'deps')
    mock_file.assert_called_once_with('run.py', 'w', encoding='utf-8')
    handle = mock_file()
    # Check if key variables are in the written content
    written_content = handle.write.call_args[0][0]
    self.assertIn('deps', written_content)
    self.assertIn('runner', written_content)
    self.assertIn('vpython3', written_content)
    mock_chmod.assert_called_once_with('run.py', 0o755)


if __name__ == '__main__':
  unittest.main()
