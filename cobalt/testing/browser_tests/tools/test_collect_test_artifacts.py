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

  @mock.patch('subprocess.run')
  @mock.patch('os.makedirs')
  @mock.patch('os.path.isdir', return_value=True)
  def test_copy_fast_dir(self, mock_isdir, mock_makedirs, mock_run):
    del mock_isdir  # Unused argument.
    collect_test_artifacts.copy_fast('src/dir', 'dst/dir')
    mock_makedirs.assert_called_once_with('dst', exist_ok=True)
    mock_run.assert_called_once_with(['cp', '-af', 'src/dir', 'dst/'],
                                     check=True)

  @mock.patch('subprocess.run')
  @mock.patch('os.makedirs')
  @mock.patch('os.path.isdir', return_value=False)
  def test_copy_fast_file(self, mock_isdir, mock_makedirs, mock_run):
    del mock_isdir  # Unused argument.
    collect_test_artifacts.copy_fast('src/file.txt', 'dst/file.txt')
    mock_makedirs.assert_called_once_with('dst', exist_ok=True)
    mock_run.assert_called_once_with(
        ['cp', '-af', 'src/file.txt', 'dst/file.txt'], check=True)

  @mock.patch('collect_test_artifacts.copy_fast')
  @mock.patch('os.path.exists', return_value=True)
  def test_copy_if_needed(self, mock_exists, mock_copy):
    del mock_exists  # Unused argument.
    copied_sources = set()

    # First copy should succeed
    collect_test_artifacts.copy_if_needed('src/file1', 'dst/file1',
                                          copied_sources)
    self.assertEqual(mock_copy.call_count, 1)
    self.assertIn(os.path.abspath('src/file1'), copied_sources)

    # Second copy of same file should be skipped
    collect_test_artifacts.copy_if_needed('src/file1', 'dst/file1',
                                          copied_sources)
    self.assertEqual(mock_copy.call_count, 1)

    # Copy of file in already copied directory should be skipped
    copied_sources.add(os.path.abspath('src/dir'))
    collect_test_artifacts.copy_if_needed('src/dir/file2', 'dst/dir/file2',
                                          copied_sources)
    self.assertEqual(mock_copy.call_count, 1)

  @mock.patch(
      'builtins.open',
      new_callable=mock.mock_open,
      read_data='content TARGET_MAP = {}')
  @mock.patch('os.chmod')
  def test_generate_runner_py(self, mock_chmod, mock_file):
    target_map = {
        'target': {
            'deps': 'deps_path',
            'runner': 'runner_path',
            'is_android': False
        }
    }
    collect_test_artifacts.generate_runner_py('run.py', target_map)

    # Verify template was read and output was written
    mock_file.assert_any_call(mock.ANY, 'r', encoding='utf-8')
    mock_file.assert_any_call('run.py', 'w', encoding='utf-8')

    handle = mock_file()
    # Find the write call that matches our expected output
    written_content = ''
    for call in handle.write.call_args_list:
      if 'deps_path' in call[0][0]:
        written_content = call[0][0]

    self.assertIn('deps_path', written_content)
    self.assertIn('runner_path', written_content)
    mock_chmod.assert_called_once_with('run.py', 0o755)


if __name__ == '__main__':
  unittest.main()
