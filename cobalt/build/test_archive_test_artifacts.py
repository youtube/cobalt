#!/usr/bin/env python3
# Copyright 2025 The Cobalt Authors. All Rights Reserved.
#
# Tests for archive_test_artifacts.py.
"""Tests for the archive_test_artifacts script."""

import os
import shutil
import tempfile
import unittest
from unittest import mock
from pathlib import Path

import archive_test_artifacts


class TestArchiveTestArtifacts(unittest.TestCase):
  """Unit tests for archive collection logic."""

  def setUp(self):
    self.test_dir = tempfile.mkdtemp()
    self.old_cwd = os.getcwd()
    os.chdir(self.test_dir)
    # Create a deeper structure so ../../ from out resolves to src
    self.source_dir = os.path.join(self.test_dir, 'src')
    self.out_dir = os.path.join(self.source_dir, 'out', 'Default')
    self.dest_dir = os.path.join(self.test_dir, 'dest')
    os.makedirs(self.source_dir)
    os.makedirs(self.out_dir)
    os.makedirs(self.dest_dir)

  def tearDown(self):
    os.chdir(self.old_cwd)
    shutil.rmtree(self.test_dir)

  def _touch(self, *parts):
    path = os.path.join(*parts)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    Path(path).touch()
    return path

  def test_get_test_runner_android(self):
    result = archive_test_artifacts.get_test_runner(
        'out/android', is_android=True)
    self.assertEqual(result, os.path.join('bin', 'run_cobalt_browsertests'))

  def test_get_test_runner_linux_bin(self):
    build_dir = os.path.join(self.out_dir, 'linux')
    runner_path = os.path.join(build_dir, 'bin', 'run_cobalt_browsertests')
    os.makedirs(os.path.dirname(runner_path))
    Path(runner_path).touch()

    result = archive_test_artifacts.get_test_runner(build_dir, is_android=False)
    self.assertEqual(result, os.path.join('bin', 'run_cobalt_browsertests'))

  def test_get_test_runner_linux_fallback(self):
    result = archive_test_artifacts.get_test_runner(
        'out/linux', is_android=False)
    self.assertEqual(result, 'cobalt_browsertests')

  @mock.patch('subprocess.check_call')
  def test_make_tar(self, mock_call):
    archive_path = os.path.join(self.dest_dir, 'test.tar.gz')
    file_lists = [(['file1', 'file2'], self.source_dir)]

    # Mock getsize to avoid error when checking created file
    with mock.patch('os.path.getsize', return_value=1024):
      # pylint: disable=protected-access
      archive_test_artifacts._make_tar(archive_path, 'gz', 1, file_lists)

    self.assertTrue(mock_call.called)
    tar_cmd = mock_call.call_args[0][0]
    self.assertIn('tar', tar_cmd)
    self.assertIn('-cvf', tar_cmd)

  @mock.patch('archive_test_artifacts._make_tar')
  def test_create_archive_basic(self, mock_make_tar):
    # Setup mock runtime_deps
    target_name = 'my_test'
    deps_file = os.path.join(self.out_dir, f'{target_name}.runtime_deps')
    with open(deps_file, 'w', encoding='utf-8') as f:
      f.write('base_unittests\n')
      f.write('../../cobalt/test/data/file.txt\n')
      f.write('obj/some_file.o\n')  # Should be excluded

    # Create dummy files to pass existence checks
    self._touch(self.out_dir, 'base_unittests')
    self._touch(self.source_dir, 'cobalt/test/data/file.txt')

    archive_test_artifacts.create_archive(
        targets=['cobalt/test:my_test'],
        source_dir=self.source_dir,
        out_dir=self.out_dir,
        destination_dir=self.dest_dir,
        archive_per_target=False,
        use_android_deps_path=False,
        compression='gz',
        compression_level=1,
        flatten_deps=False)

    self.assertTrue(mock_make_tar.called)
    file_list = mock_make_tar.call_args[0][3][0][0]

    # Verify base_unittests is included (relative to source_dir)
    self.assertIn('out/Default/base_unittests', file_list)
    # Verify source file is included (relative to source_dir)
    self.assertIn('cobalt/test/data/file.txt', file_list)
    # Verify obj/ file is excluded
    self.assertNotIn('out/Default/obj/some_file.o', file_list)

  @mock.patch('archive_test_artifacts.generate_runner_py')
  @mock.patch('archive_test_artifacts._make_tar')
  @mock.patch('shutil.which', return_value='/path/to/vpython3')
  def test_create_archive_browsertests(self, unused_mock_which, mock_make_tar,
                                       unused_mock_gen):
    # Setup mock runtime_deps
    target_name = 'cobalt_browsertests'
    deps_file = os.path.join(self.out_dir, f'{target_name}.runtime_deps')
    with open(deps_file, 'w', encoding='utf-8') as f:
      f.write('cobalt_browsertests\n')

    # Create dummy runtime_deps for the portable deps target
    portable_deps_file = os.path.join(
        self.out_dir, 'cobalt_browsertests_portable_deps.runtime_deps')
    with open(portable_deps_file, 'w', encoding='utf-8') as f:
      f.write('../../testing/file\n')
      f.write('../../.vpython3\n')

    # Create dummy files to pass existence checks
    self._touch(self.out_dir, 'cobalt_browsertests')
    self._touch(self.source_dir, 'testing/file')
    self._touch(self.source_dir, '.vpython3')

    archive_test_artifacts.create_archive(
        targets=['cobalt/testing/browser_tests:cobalt_browsertests'],
        source_dir=self.source_dir,
        out_dir=self.out_dir,
        destination_dir=self.dest_dir,
        archive_per_target=False,
        use_android_deps_path=False,
        compression='gz',
        compression_level=1,
        flatten_deps=False,
        include_browsertests_runner=True)

    self.assertTrue(mock_make_tar.called)
    file_lists = mock_make_tar.call_args[0][3]

    # Check for combined_deps
    combined_deps = file_lists[1][0]
    self.assertIn('testing/file', combined_deps)
    self.assertIn('.vpython3', combined_deps)
    self.assertIn('out/Default/cobalt_browsertests', combined_deps)

  @mock.patch('archive_test_artifacts.generate_runner_py')
  @mock.patch('archive_test_artifacts._make_tar')
  @mock.patch('shutil.which', return_value='/path/to/vpython3')
  def test_create_archive_runner_and_deps_paths(self, unused_mock_which,
                                                mock_make_tar, mock_gen):
    # Setup mock runtime_deps
    target_name = 'cobalt_browsertests'
    target_dir = 'cobalt/testing/browser_tests'
    deps_file = os.path.join(self.out_dir, f'{target_name}.runtime_deps')
    os.makedirs(os.path.dirname(deps_file), exist_ok=True)
    with open(deps_file, 'w', encoding='utf-8') as f:
      f.write('cobalt_browsertests\n')

    # Create dummy runtime_deps for the portable deps target
    portable_deps_file = os.path.join(
        self.out_dir, 'cobalt_browsertests_portable_deps.runtime_deps')
    with open(portable_deps_file, 'w', encoding='utf-8') as f:
      f.write('cobalt_browsertests\n')

    # Create dummy files to pass existence checks
    self._touch(self.out_dir, 'cobalt_browsertests')
    # Create dummy runner
    self._touch(self.out_dir, 'bin/run_cobalt_browsertests')

    archive_test_artifacts.create_archive(
        targets=[f'{target_dir}:{target_name}'],
        source_dir=self.source_dir,
        out_dir=self.out_dir,
        destination_dir=self.dest_dir,
        archive_per_target=False,
        use_android_deps_path=False,
        compression='gz',
        compression_level=1,
        flatten_deps=False,
        include_browsertests_runner=True)

    # Verify generate_runner_py was called with correct target_map
    target_map = mock_gen.call_args[0][1]
    config = target_map[target_name]

    # Paths in target_map MUST be relative to source_dir (archive root)
    self.assertEqual(config['deps'],
                     'out/Default/cobalt_browsertests.runtime_deps')
    self.assertEqual(config['runner'],
                     'out/Default/bin/run_cobalt_browsertests')

    # Verify both files are in the file list to be archived
    file_lists = mock_make_tar.call_args[0][3]
    combined_deps = file_lists[1][0]
    self.assertIn('out/Default/cobalt_browsertests.runtime_deps', combined_deps)
    self.assertIn('out/Default/bin/run_cobalt_browsertests', combined_deps)

  @mock.patch('archive_test_artifacts._make_tar')
  def test_create_archive_depot_tools(self, mock_make_tar):
    # Setup mock runtime_deps
    target_name = 'my_test'
    deps_file = os.path.join(self.out_dir, f'{target_name}.runtime_deps')
    with open(deps_file, 'w', encoding='utf-8') as f:
      f.write('my_test\n')

    # Create dummy files to pass existence checks
    self._touch(self.out_dir, 'my_test')

    # Create dummy depot_tools
    depot_tools_dir = os.path.join(self.test_dir, 'my_depot_tools')
    os.makedirs(depot_tools_dir)
    vpython_path = os.path.join(depot_tools_dir, 'vpython3')
    Path(vpython_path).touch()
    Path(os.path.join(depot_tools_dir, 'gclient')).touch()

    with mock.patch('shutil.which', return_value=vpython_path):
      archive_test_artifacts.create_archive(
          targets=['cobalt/test:my_test'],
          source_dir=self.source_dir,
          out_dir=self.out_dir,
          destination_dir=self.dest_dir,
          archive_per_target=False,
          use_android_deps_path=False,
          compression='gz',
          compression_level=1,
          flatten_deps=False,
          include_depot_tools=True)

    self.assertTrue(mock_make_tar.called)
    file_lists = mock_make_tar.call_args[0][3]

    # Check for depot_tools entry
    depot_tools_entry = file_lists[0]
    self.assertEqual(depot_tools_entry[0], ['my_depot_tools'])
    self.assertEqual(depot_tools_entry[1], self.test_dir)

    # Check for combined_deps entry
    combined_deps_entry = file_lists[1]
    self.assertIn('out/Default/my_test', combined_deps_entry[0])

  @mock.patch('archive_test_artifacts._make_tar')
  def test_create_archive_android_fallback(self, mock_make_tar):
    # Setup mock runtime_deps in the Android script-based location
    target_name = 'cobalt_browsertests'
    target_path = 'cobalt/testing/browser_tests'
    deps_dir = os.path.join(self.out_dir, 'gen.runtime', target_path)
    os.makedirs(deps_dir)
    deps_file = os.path.join(deps_dir,
                             f'{target_name}__test_runner_script.runtime_deps')
    with open(deps_file, 'w', encoding='utf-8') as f:
      f.write('cobalt_browsertests\n')

    # Create dummy files to pass existence checks
    self._touch(self.out_dir, 'cobalt_browsertests')

    # Should find it via fallback logic even without --use-android-deps-path
    archive_test_artifacts.create_archive(
        targets=[f'{target_path}:{target_name}'],
        source_dir=self.source_dir,
        out_dir=self.out_dir,
        destination_dir=self.dest_dir,
        archive_per_target=False,
        use_android_deps_path=False,
        compression='gz',
        compression_level=1,
        flatten_deps=False)

    self.assertTrue(mock_make_tar.called)
    file_list = mock_make_tar.call_args[0][3][0][0]
    self.assertIn(os.path.relpath(deps_file, self.source_dir), file_list)

  @mock.patch('archive_test_artifacts._make_tar')
  def test_create_archive_flattened(self, mock_make_tar):
    # Setup mock runtime_deps
    target_name = 'my_test'
    deps_file = os.path.join(self.out_dir, f'{target_name}.runtime_deps')
    with open(deps_file, 'w', encoding='utf-8') as f:
      f.write('base_unittests\n')
      f.write('../../cobalt/test/data/file.txt\n')

    # Create dummy files to pass existence checks
    self._touch(self.out_dir, 'base_unittests')
    self._touch(self.source_dir, 'cobalt/test/data/file.txt')

    # Enable flatten_deps and archive_per_target (which is required for
    # flatten_deps)
    archive_test_artifacts.create_archive(
        targets=['cobalt/test:my_test'],
        source_dir=self.source_dir,
        out_dir=self.out_dir,
        destination_dir=self.dest_dir,
        archive_per_target=True,
        use_android_deps_path=False,
        compression='gz',
        compression_level=1,
        flatten_deps=True)

    self.assertTrue(mock_make_tar.called)
    file_lists = mock_make_tar.call_args[0][3]

    # target_deps should be relative to out_dir
    target_deps = file_lists[0][0]
    self.assertIn('base_unittests', target_deps)
    self.assertIn(f'{target_name}.runtime_deps', target_deps)

    # target_src_root_deps should be relative to source_dir
    # (../../ stripped)
    src_deps = file_lists[1][0]
    self.assertIn('cobalt/test/data/file.txt', src_deps)


if __name__ == '__main__':
  unittest.main()
