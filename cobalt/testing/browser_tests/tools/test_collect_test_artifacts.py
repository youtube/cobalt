#!/usr/bin/env python3
# Copyright 2025 The Cobalt Authors. All Rights Reserved.
#
# Tests for collect_test_artifacts.py.
"""Tests for the collect_test_artifacts script."""

import os
from pathlib import Path
import sys
import tarfile
import tempfile
import unittest
from unittest import mock

import collect_test_artifacts


class TestCollectTestArtifacts(unittest.TestCase):
  """Unit tests for artifact collection logic."""

  def test_find_runtime_deps_exact_match(self):
    with mock.patch('collect_test_artifacts.Path.rglob') as mock_rglob:
      expected_path = Path(
          'out/dir/gen.runtime/cobalt/testing/browser_tests/'
          'cobalt_browsertests__test_runner_script.runtime_deps')
      mock_rglob.return_value = [expected_path]
      result = collect_test_artifacts.find_runtime_deps('out/dir')
      self.assertEqual(result, expected_path)

  def test_get_test_runner_android(self):
    result = collect_test_artifacts.get_test_runner()
    self.assertEqual(result, os.path.join('bin', 'run_cobalt_browsertests'))

  @mock.patch('os.path.isfile', return_value=True)
  def test_get_test_runner_loader(self, mock_isfile):
    result = collect_test_artifacts.get_test_runner('out/aosp_devel')
    self.assertEqual(result,
                     os.path.join('bin', 'run_cobalt_browsertests_loader'))
    mock_isfile.assert_called_once_with(
        os.path.join('out/aosp_devel', 'bin', 'run_cobalt_browsertests_loader'))

  @mock.patch('subprocess.run')
  @mock.patch('os.makedirs')
  @mock.patch('os.path.islink', return_value=False)
  @mock.patch('os.path.isdir', return_value=True)
  def test_copy_fast_dir(self, mock_isdir, mock_islink, mock_makedirs,
                         mock_run):
    del mock_isdir, mock_islink  # Unused arguments.
    collect_test_artifacts.copy_fast('src/custom_dir', 'dst/renamed_dir')
    mock_makedirs.assert_called_once_with('dst', exist_ok=True)
    mock_run.assert_called_once_with(
        ['cp', '-afT', 'src/custom_dir', 'dst/renamed_dir'], check=True)

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
  @mock.patch('os.path.isdir', return_value=False)
  def test_copy_if_needed(self, mock_isdir, mock_exists, mock_copy):
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
    # Simulate that 'src/dir' was already copied as a directory.
    abs_dir = os.path.abspath('src/dir')
    copied_sources.add(abs_dir)

    # We need mock_isdir to return True for the parent 'src/dir'
    # to trigger the parent directory optimization.
    def is_dir_side_effect(path):
      if path == abs_dir:
        return True
      return False

    mock_isdir.side_effect = is_dir_side_effect

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


class TestCollectTestArtifactsMain(unittest.TestCase):
  """Tests for the main() entry point and argument parsing."""

  @mock.patch('collect_test_artifacts.find_runtime_deps')
  @mock.patch('os.makedirs')
  @mock.patch('argparse.ArgumentParser.error')
  def test_main_absolute_output_with_output_dir_fails(self, mock_error,
                                                      unused_makedirs,
                                                      unused_find_deps):
    mock_error.side_effect = SystemExit
    test_args = [
        'collect_test_artifacts.py', 'out/dir', '--output_dir', '/tmp/out',
        '--output', '/tmp/absolute/path.tar.gz'
    ]
    with mock.patch('sys.argv', test_args):
      with self.assertRaises(SystemExit):
        collect_test_artifacts.main()

      mock_error.assert_called_once()
      self.assertIn('cannot be an absolute path', mock_error.call_args[0][0])

  def test_main_toolchain_only_creates_slim_archive(self):
    with tempfile.TemporaryDirectory() as tmp_dir:
      src_root = os.path.join(tmp_dir, 'src')
      dest_dir = os.path.join(tmp_dir, 'dest')
      dt_dir = os.path.join(tmp_dir, 'dt')
      for rel in ('build/android/test_runner.py', 'build/gn_helpers.py',
                  'build/config/gclient_args.gni',
                  'third_party/android_build_tools/aapt2/aapt2',
                  'third_party/android_build_tools/bundletool/bt.jar',
                  'third_party/android_build_tools/lint/lint.jar',
                  'third_party/catapult/third_party/vinn/vinn',
                  'third_party/llvm-build/Release+Asserts/bin/llvm-symbolizer',
                  'out/android-arm_devel/build_vars.json',
                  'out/android-arm_devel/url_unittests.apk'):
        Path(src_root, rel).parent.mkdir(parents=True, exist_ok=True)
        Path(src_root, rel).write_text('x', encoding='utf-8')
      for rel in ('vpython3', 'python3', 'bootstrap-3.11/bin/py',
                  '.cipd_bin/reclient/rewrapper'):
        Path(dt_dir, rel).parent.mkdir(parents=True, exist_ok=True)
        Path(dt_dir, rel).write_text('x', encoding='utf-8')

      old_cwd = os.getcwd()
      os.chdir(src_root)
      try:
        argv = [
            'collect_test_artifacts.py', 'out/android-arm_devel', '-o',
            'android_test_host_deps.tar.gz', '--output_dir', dest_dir,
            '--compression', 'gz', '--toolchain-only'
        ]
        with mock.patch('sys.argv', argv), mock.patch(
            'shutil.which', return_value=os.path.join(dt_dir, 'vpython3')):
          collect_test_artifacts.main()
      finally:
        os.chdir(old_cwd)

      extract_dir = os.path.join(tmp_dir, 'extracted')
      with tarfile.open(
          os.path.join(dest_dir, 'android_test_host_deps.tar.gz'),
          'r:gz') as tar:
        kw = {'filter': 'data'} if sys.version_info >= (3, 12) else {}
        tar.extractall(extract_dir, **kw)

      for inc in ('src/out/android-arm_devel/build_vars.json',
                  'src/build/config/gclient_args.gni',
                  'src/third_party/android_build_tools/aapt2/aapt2',
                  'src/third_party/android_build_tools/bundletool/bt.jar',
                  'depot_tools/vpython3'):
        self.assertTrue(os.path.isfile(os.path.join(extract_dir, inc)))
      for exc in ('src/third_party/android_build_tools/lint',
                  'src/third_party/catapult/third_party/vinn',
                  'src/third_party/llvm-build',
                  'src/out/android-arm_devel/url_unittests.apk',
                  'depot_tools/python3', 'depot_tools/bootstrap-3.11',
                  'depot_tools/.cipd_bin/reclient', 'run_tests.py'):
        self.assertFalse(os.path.exists(os.path.join(extract_dir, exc)))


if __name__ == '__main__':
  unittest.main()
