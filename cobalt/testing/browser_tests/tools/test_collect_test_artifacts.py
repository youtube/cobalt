#!/usr/bin/env python3
# Copyright 2025 The Cobalt Authors. All Rights Reserved.
#
# Tests for collect_test_artifacts.py.
"""Tests for the collect_test_artifacts script."""

import shutil
import tempfile
import unittest
from pathlib import Path
from unittest import mock

import collect_test_artifacts


class TestCollectTestArtifacts(unittest.TestCase):
  """Unit tests for the artifact collection script."""

  def setUp(self):
    self.test_dir = Path(tempfile.mkdtemp())

  def tearDown(self):
    shutil.rmtree(self.test_dir)

  def test_find_runtime_deps_exact_match(self):
    build_dir = self.test_dir / 'out/dir'
    deps_file = build_dir / (
        'gen.runtime/cobalt/testing/browser_tests/'
        'cobalt_browsertests__test_runner_script.runtime_deps')
    deps_file.parent.mkdir(parents=True)
    deps_file.write_text('dummy content', encoding='utf-8')

    result = collect_test_artifacts.find_runtime_deps(build_dir)
    self.assertEqual(result, deps_file)

  def test_get_test_runner_android(self):
    result = collect_test_artifacts.get_test_runner(Path('out/dir'), True)
    self.assertEqual(result, Path('bin/run_cobalt_browsertests'))

  @mock.patch('subprocess.run')
  def test_copy_fast_dir(self, mock_run):
    src = self.test_dir / 'src/dir'
    dst = self.test_dir / 'dst/dir'
    src.mkdir(parents=True)

    collect_test_artifacts.copy_fast(src, dst)
    mock_run.assert_called_once()
    self.assertIn('cp', mock_run.call_args[0][0])

  @mock.patch('subprocess.run')
  def test_copy_fast_file(self, mock_run):
    src = self.test_dir / 'src/file.txt'
    dst = self.test_dir / 'dst/file.txt'
    src.parent.mkdir(parents=True)
    src.write_text('content', encoding='utf-8')

    collect_test_artifacts.copy_fast(src, dst)
    mock_run.assert_called_once()
    self.assertIn('/src/file.txt', str(mock_run.call_args[0][0]))

  def test_copy_if_needed(self):
    copied_sources = set()
    src = self.test_dir / 'src/file1'
    dst = self.test_dir / 'dst/file1'
    src.parent.mkdir(parents=True)
    src.write_text('content', encoding='utf-8')

    with mock.patch('collect_test_artifacts.copy_fast') as mock_copy:
      collect_test_artifacts.copy_if_needed(src, dst, copied_sources)
      mock_copy.assert_called_once()
      self.assertIn(src.resolve(), copied_sources)

      mock_copy.reset_mock()
      collect_test_artifacts.copy_if_needed(src, dst, copied_sources)
      mock_copy.assert_not_called()

  @mock.patch('subprocess.run')
  def test_create_tarball(self, mock_run):
    collect_test_artifacts.create_tarball(Path('out.tar.gz'), 'gz', Path('src'))
    mock_run.assert_called_once()
    self.assertIn('tar', mock_run.call_args[0][0])

  @mock.patch.object(Path, 'chmod')
  def test_generate_runner_py(self, mock_chmod):
    dst = self.test_dir / 'run.py'
    target_map = {'target': 'config'}
    # We need a real run_tests.template.py or mock its read
    with mock.patch.object(
        Path,
        'open',
        new_callable=mock.mock_open,
        read_data='TEMPLATE_CONTENT TARGET_MAP = {}') as mock_open:
      collect_test_artifacts.generate_runner_py(dst, target_map)
      self.assertEqual(mock_open.call_count, 2)
      mock_chmod.assert_called_once_with(0o755)


class TestCollectTestArtifactsMain(unittest.TestCase):
  """Tests for the main function of collect_test_artifacts."""

  def setUp(self):
    self.test_dir = Path(tempfile.mkdtemp())
    self.cwd_patcher = mock.patch.object(
        Path, 'cwd', return_value=self.test_dir)
    self.cwd_patcher.start()

  def tearDown(self):
    self.cwd_patcher.stop()
    shutil.rmtree(self.test_dir)

  @mock.patch('argparse.ArgumentParser.error')
  def test_main_absolute_output_with_output_dir_fails(self, mock_error):
    mock_error.side_effect = SystemExit
    test_args = [
        'collect_test_artifacts.py', 'out/dir', '--output_dir', '/tmp/out',
        '--output', '/tmp/absolute/path.tar.gz'
    ]
    with mock.patch('sys.argv', test_args):
      with self.assertRaises(SystemExit):
        collect_test_artifacts.main()

  @mock.patch('collect_test_artifacts.create_tarball')
  @mock.patch('collect_test_artifacts.generate_runner_py')
  @mock.patch('collect_test_artifacts.copy_if_needed')
  def test_main_split_mode(self, unused_mock_copy, unused_mock_gen_runner,
                           mock_create_tar):
    build_dir = self.test_dir / 'out/dir'
    deps_file = build_dir / (
        'gen.runtime/cobalt/testing/browser_tests/'
        'cobalt_browsertests__test_runner_script.runtime_deps')
    deps_file.parent.mkdir(parents=True)
    deps_file.write_text('dep1\n', encoding='utf-8')
    (build_dir / 'dep1').write_text('content', encoding='utf-8')

    test_args = [
        'collect_test_artifacts.py',
        str(build_dir), '--split', '--output', 'artifacts.tar.gz',
        '--runner_output', 'runner.tar.gz'
    ]
    with mock.patch('sys.argv', test_args):
      collect_test_artifacts.main()

    self.assertEqual(mock_create_tar.call_count, 2)
    call_args = [str(call[0][0]) for call in mock_create_tar.call_args_list]
    self.assertIn('artifacts.tar.gz', call_args)
    self.assertIn('runner.tar.gz', call_args)


if __name__ == '__main__':
  unittest.main()
