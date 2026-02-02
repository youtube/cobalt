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

import archive_test_artifacts


class TestArchiveTestArtifacts(unittest.TestCase):
  """Unit tests for archive collection logic."""

  def setUp(self):
    self.test_dir = tempfile.mkdtemp()
    self.old_cwd = os.getcwd()
    os.chdir(self.test_dir)
    # Match the likely real-world scenario where out is inside src
    self.source_dir = os.path.join(self.test_dir, 'src')
    self.out_dir = os.path.join(self.source_dir, 'out')
    self.dest_dir = os.path.join(self.test_dir, 'dest')
    os.makedirs(self.source_dir)
    os.makedirs(self.out_dir)
    os.makedirs(self.dest_dir)

  def tearDown(self):
    os.chdir(self.old_cwd)
    shutil.rmtree(self.test_dir)

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
    self.assertIn('gzip -1', tar_cmd)

  @mock.patch('archive_test_artifacts._make_tar')
  def test_create_archive_linux_style(self, mock_make_tar):
    # Setup mock runtime_deps
    target_name = 'my_test'
    deps_file = os.path.join(self.out_dir, f'{target_name}.runtime_deps')
    with open(deps_file, 'w', encoding='utf-8') as f:
      f.write('base_unittests\n')
      f.write('../../cobalt/test/data/file.txt\n')
      f.write('obj/some_file.o\n')  # Should be excluded

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

    # os.path.relpath(os.path.join(out_dir, 'base_unittests'))
    # Since out_dir is absolute and we are in test_dir:
    # it will be 'src/out/base_unittests'
    self.assertIn(
        os.path.relpath(os.path.join(self.out_dir, 'base_unittests')),
        file_list)
    # Rebase all files to be relative to their respective root (source or
    # out dir) to be able to flatten them below. Chromium test runners
    # have access to the source directory in '../..' which ours (ODTs
    # especially) do not.
    # rel_path = os.path.relpath(os.path.join(tar_root, line.strip()))
    self.assertIn('cobalt/test/data/file.txt', file_list)

  @mock.patch('archive_test_artifacts._make_tar')
  def test_create_archive_android_style(self, mock_make_tar):
    # Android style: flatten_deps=True, archive_per_target=True
    target_name = 'my_test'
    deps_file = os.path.join(self.out_dir, f'{target_name}.runtime_deps')
    with open(deps_file, 'w', encoding='utf-8') as f:
      f.write('base_unittests\n')
      f.write('../../cobalt/test/data/file.txt\n')

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

    out_deps = file_lists[0][0]
    src_deps = file_lists[1][0]

    # base_unittests goes to out_deps because it doesn't start with ../../
    # rel_path = os.path.relpath(os.path.join(tar_root, line.strip()))
    # tar_root = '.'
    # join('.', 'base_unittests') -> 'base_unittests'
    # relpath('base_unittests') -> 'base_unittests'
    self.assertIn('base_unittests', out_deps)

    # ../../cobalt/test/data/file.txt starts with ../../
    # target_src_root_deps.add(line.strip()[6:])
    # 'cobalt/test/data/file.txt'
    self.assertIn('cobalt/test/data/file.txt', src_deps)

  @mock.patch('os.path.getsize', return_value=1024)
  @mock.patch('subprocess.check_call')
  def test_create_archive_browsertests_redirection(  # pylint: disable=line-too-long
      self, mock_call, unused_mock_getsize):
    # Setup mock runtime_deps for another target to ensure it still runs if mixed
    target_name = 'my_test'
    deps_file = os.path.join(self.out_dir, f'{target_name}.runtime_deps')
    with open(deps_file, 'w', encoding='utf-8') as f:
      f.write('base_unittests\n')

    archive_test_artifacts.create_archive(
        targets=[
            'cobalt/test:my_test',
            'cobalt/testing/browser_tests:cobalt_browsertests'
        ],
        source_dir=self.source_dir,
        out_dir=self.out_dir,
        destination_dir=self.dest_dir,
        archive_per_target=False,
        use_android_deps_path=False,
        compression='gz',
        compression_level=1,
        flatten_deps=False)

    # Verify specialized script was called
    # The first call should be to collect_test_artifacts.py
    # The second call (from _make_tar) should be to tar
    self.assertEqual(mock_call.call_count, 2)

    collect_cmd = mock_call.call_args_list[0][0][0]
    self.assertIn('collect_test_artifacts.py', collect_cmd[1])
    self.assertIn(self.out_dir, collect_cmd)
    self.assertIn('cobalt_browsertests_artifacts.tar.gz', collect_cmd)
    self.assertIn('--compression', collect_cmd)
    self.assertIn('gz', collect_cmd)

    tar_cmd = mock_call.call_args_list[1][0][0]
    self.assertIn('tar', tar_cmd)

  @mock.patch('archive_test_artifacts._make_tar')
  def test_runtime_deps_discovery(self, unused_mock_make_tar):
    # Test that it finds deps in gen.runtime even if not explicitly asked
    target_path = 'cobalt/test'
    target_name = 'my_test'
    gen_deps_dir = os.path.join(self.out_dir, 'gen.runtime', target_path)
    os.makedirs(gen_deps_dir)
    deps_file = os.path.join(gen_deps_dir,
                             f'{target_name}__test_runner_script.runtime_deps')
    with open(deps_file, 'w', encoding='utf-8') as f:
      f.write('file1\n')

    # This should NOT throw FileNotFoundError now
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

    # Test FileNotFoundError when missing
    with self.assertRaises(FileNotFoundError):
      archive_test_artifacts.create_archive(
          targets=['nonexistent:target'],
          source_dir=self.source_dir,
          out_dir=self.out_dir,
          destination_dir=self.dest_dir,
          archive_per_target=False,
          use_android_deps_path=False,
          compression='gz',
          compression_level=1,
          flatten_deps=False)


if __name__ == '__main__':
  unittest.main()
