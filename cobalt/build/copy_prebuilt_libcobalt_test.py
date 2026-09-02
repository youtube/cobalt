#!/usr/bin/env python3
#
# Copyright 2026 The Cobalt Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
"""Unit tests for copy_prebuilt_libcobalt module."""

import os
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

# Add repository root to sys.path to support running directly.
REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
if REPO_ROOT not in sys.path:
  sys.path.insert(0, REPO_ROOT)

from cobalt.build import copy_prebuilt_libcobalt  # pylint: disable=wrong-import-position


# pylint: disable=consider-using-with
class CopyPrebuiltLibcobaltTest(unittest.TestCase):
  """Tests copy_or_decompress_prebuilt and path resolution."""

  def setUp(self):
    self.temp_dir = tempfile.TemporaryDirectory()
    self.test_content = b'\x7fELF' + b'dummy_shared_object_data_1234567890' * 50

  def tearDown(self):
    self.temp_dir.cleanup()

  def test_copy_uncompressed_so(self):
    src_so = os.path.join(self.temp_dir.name, 'prebuilt_libcobalt.so')
    dst_so = os.path.join(self.temp_dir.name, 'out', 'libcobalt.so')
    with open(src_so, 'wb') as f:
      f.write(self.test_content)

    copy_prebuilt_libcobalt.copy_or_decompress_prebuilt(src_so, dst_so)
    self.assertTrue(os.path.exists(dst_so))
    with open(dst_so, 'rb') as f:
      self.assertEqual(f.read(), self.test_content)

  def test_copy_versioned_so(self):
    src_so = os.path.join(self.temp_dir.name, 'libcobalt.so.1.2')
    dst_so = os.path.join(self.temp_dir.name, 'out', 'libcobalt.so')
    with open(src_so, 'wb') as f:
      f.write(self.test_content)

    copy_prebuilt_libcobalt.copy_or_decompress_prebuilt(src_so, dst_so)
    self.assertTrue(os.path.exists(dst_so))
    with open(dst_so, 'rb') as f:
      self.assertEqual(f.read(), self.test_content)

  def test_decompress_lz4(self):
    import lz4.frame  # pylint: disable=import-outside-toplevel

    src_lz4 = os.path.join(self.temp_dir.name, 'libcobalt.lz4')
    dst_so = os.path.join(self.temp_dir.name, 'out', 'libcobalt.so')
    with open(src_lz4, 'wb') as f:
      f.write(lz4.frame.compress(self.test_content))

    copy_prebuilt_libcobalt.copy_or_decompress_prebuilt(src_lz4, dst_so)
    self.assertTrue(os.path.exists(dst_so))
    with open(dst_so, 'rb') as f:
      self.assertEqual(f.read(), self.test_content)

  def test_decompress_zstd(self):
    src_so = os.path.join(self.temp_dir.name, 'temp.so')
    with open(src_so, 'wb') as f:
      f.write(self.test_content)

    src_zst = os.path.join(self.temp_dir.name, 'libcobalt.zst')
    subprocess.run(['zstd', '-f', src_so, '-o', src_zst], check=True)

    dst_so = os.path.join(self.temp_dir.name, 'out', 'libcobalt.so')
    copy_prebuilt_libcobalt.copy_or_decompress_prebuilt(src_zst, dst_so)
    self.assertTrue(os.path.exists(dst_so))
    with open(dst_so, 'rb') as f:
      self.assertEqual(f.read(), self.test_content)

  def test_corrupt_lz4_archive(self):
    corrupt_lz4 = os.path.join(self.temp_dir.name, 'libcobalt.lz4')
    dst_so = os.path.join(self.temp_dir.name, 'out', 'libcobalt.so')
    with open(corrupt_lz4, 'wb') as f:
      f.write(b'this is definitely not a valid lz4 archive')

    with self.assertRaises(RuntimeError) as ctx:
      copy_prebuilt_libcobalt.copy_or_decompress_prebuilt(corrupt_lz4, dst_so)
    self.assertIn('Corrupt or invalid LZ4 archive', str(ctx.exception))
    self.assertFalse(os.path.exists(dst_so))
    self.assertFalse(os.path.exists(dst_so + '.tmp'))

  def test_corrupt_zstd_archive(self):
    corrupt_zst = os.path.join(self.temp_dir.name, 'libcobalt.zst')
    dst_so = os.path.join(self.temp_dir.name, 'out', 'libcobalt.so')
    with open(corrupt_zst, 'wb') as f:
      f.write(b'this is definitely not a valid zstd archive')

    with self.assertRaises(RuntimeError) as ctx:
      copy_prebuilt_libcobalt.copy_or_decompress_prebuilt(corrupt_zst, dst_so)
    self.assertIn('Corrupt or invalid Zstandard archive', str(ctx.exception))
    self.assertFalse(os.path.exists(dst_so))
    self.assertFalse(os.path.exists(dst_so + '.tmp'))

  def test_atomicity_and_cleanup_on_failure(self):
    src_so = os.path.join(self.temp_dir.name, 'prebuilt.so')
    dst_so = os.path.join(self.temp_dir.name, 'out', 'libcobalt.so')
    with open(src_so, 'wb') as f:
      f.write(self.test_content)

    def fail_copy(src, dst):
      del src
      with open(dst, 'wb') as f:
        f.write(b'partial data')
      raise IOError('Simulated disk error during copy')

    with mock.patch('shutil.copyfile', side_effect=fail_copy):
      with self.assertRaises(IOError):
        copy_prebuilt_libcobalt.copy_or_decompress_prebuilt(src_so, dst_so)

    self.assertFalse(os.path.exists(dst_so))
    self.assertFalse(os.path.exists(dst_so + '.tmp'))

  def test_paths_with_spaces_and_so_in_dir_name(self):
    import lz4.frame  # pylint: disable=import-outside-toplevel

    complex_dir = os.path.join(self.temp_dir.name, 'lib.so.dir',
                               'dir with spaces')
    os.makedirs(complex_dir)
    src_lz4 = os.path.join(complex_dir, 'libcobalt.lz4')
    dst_so = os.path.join(self.temp_dir.name, 'out dir with spaces',
                          'libcobalt.so')
    with open(src_lz4, 'wb') as f:
      f.write(lz4.frame.compress(self.test_content))

    copy_prebuilt_libcobalt.copy_or_decompress_prebuilt(src_lz4, dst_so)
    self.assertTrue(os.path.exists(dst_so))
    with open(dst_so, 'rb') as f:
      self.assertEqual(f.read(), self.test_content)

  def test_path_resolution_repo_relative(self):
    repo_root = os.path.join(self.temp_dir.name, 'repo')
    os.makedirs(os.path.join(repo_root, 'prebuilts'))
    so_file = os.path.join(repo_root, 'prebuilts', 'libcobalt.so')
    with open(so_file, 'wb') as f:
      f.write(self.test_content)

    # Test "//prebuilts/libcobalt.so"
    resolved = copy_prebuilt_libcobalt.resolve_path(
        '//prebuilts/libcobalt.so', repo_root=repo_root)
    self.assertEqual(resolved, so_file)

    # Test "prebuilts/libcobalt.so" with repo_root
    resolved2 = copy_prebuilt_libcobalt.resolve_path(
        'prebuilts/libcobalt.so', repo_root=repo_root)
    self.assertEqual(resolved2, so_file)

  def test_path_resolution_build_dir_relative(self):
    build_dir = os.path.join(self.temp_dir.name, 'out', 'evergreen')
    os.makedirs(build_dir)
    so_file = os.path.join(build_dir, 'custom_libcobalt.so')
    with open(so_file, 'wb') as f:
      f.write(self.test_content)

    resolved = copy_prebuilt_libcobalt.resolve_path(
        'custom_libcobalt.so', build_dir=build_dir)
    self.assertEqual(resolved, so_file)

  def test_unsupported_extension_error(self):
    bad_file = os.path.join(self.temp_dir.name, 'libcobalt.txt')
    with open(bad_file, 'wb') as f:
      f.write(b'hello')
    dst_so = os.path.join(self.temp_dir.name, 'out', 'libcobalt.so')

    with self.assertRaises(ValueError) as ctx:
      copy_prebuilt_libcobalt.copy_or_decompress_prebuilt(bad_file, dst_so)
    self.assertIn('Unsupported prebuilt libcobalt file format',
                  str(ctx.exception))

  def test_nonexistent_file_error(self):
    nonexistent = os.path.join(self.temp_dir.name, 'does_not_exist.so')
    dst_so = os.path.join(self.temp_dir.name, 'out', 'libcobalt.so')

    with self.assertRaises(FileNotFoundError) as ctx:
      copy_prebuilt_libcobalt.copy_or_decompress_prebuilt(nonexistent, dst_so)
    self.assertIn('Prebuilt libcobalt file not found', str(ctx.exception))

  def test_unstripped_output(self):
    src_so = os.path.join(self.temp_dir.name, 'libcobalt.so')
    dst_so = os.path.join(self.temp_dir.name, 'out', 'libcobalt.so')
    dst_unstripped = os.path.join(self.temp_dir.name, 'out', 'lib.unstripped',
                                  'libcobalt.so')
    with open(src_so, 'wb') as f:
      f.write(self.test_content)

    copy_prebuilt_libcobalt.copy_or_decompress_prebuilt(
        src_so, dst_so, unstripped_output=dst_unstripped)
    self.assertTrue(os.path.exists(dst_so))
    self.assertTrue(os.path.exists(dst_unstripped))
    with open(dst_unstripped, 'rb') as f:
      self.assertEqual(f.read(), self.test_content)

  def test_lz4_cli_fallback_or_missing_dep(self):
    src_lz4 = os.path.join(self.temp_dir.name, 'libcobalt.lz4')
    dst_so = os.path.join(self.temp_dir.name, 'out', 'libcobalt.so')
    with open(src_lz4, 'wb') as f:
      f.write(b'dummy')

    with mock.patch.dict('sys.modules', {'lz4.frame': None}):
      with mock.patch('shutil.which', return_value=None):
        with self.assertRaises(RuntimeError) as ctx:
          copy_prebuilt_libcobalt.decompress_lz4(src_lz4, dst_so)
        self.assertIn('neither the python \'lz4\' module nor the system',
                      str(ctx.exception))

  def test_zstd_missing_dependencies_error(self):
    src_zst = os.path.join(self.temp_dir.name, 'libcobalt.zst')
    dst_so = os.path.join(self.temp_dir.name, 'out', 'libcobalt.so')
    with open(src_zst, 'wb') as f:
      f.write(b'dummy')

    with mock.patch.dict('sys.modules', {'zstandard': None, 'zstd': None}):
      with mock.patch('shutil.which', return_value=None):
        with self.assertRaises(RuntimeError) as ctx:
          copy_prebuilt_libcobalt.decompress_zstd(src_zst, dst_so)
        self.assertIn('neither python \'zstandard\'/\'zstd\' module nor the',
                      str(ctx.exception))

  def test_lz4_cli_fallback_success(self):
    src_lz4 = os.path.join(self.temp_dir.name, 'libcobalt.lz4')
    dst_so = os.path.join(self.temp_dir.name, 'out', 'libcobalt.so')
    with open(src_lz4, 'wb') as f:
      f.write(b'dummy_lz4_bytes')

    def mock_run(cmd, check=True, capture_output=True, text=True):
      del check, capture_output, text
      # cmd is [lz4_cmd, '-d', '-f', input_path, output_path]
      with open(cmd[4], 'wb') as f:
        f.write(self.test_content)
      return subprocess.CompletedProcess(cmd, 0)

    with mock.patch.dict('sys.modules', {'lz4.frame': None}):
      with mock.patch(
          'shutil.which',
          side_effect=lambda x: '/usr/bin/lz4' if x == 'lz4' else None):
        with mock.patch('subprocess.run', side_effect=mock_run):
          copy_prebuilt_libcobalt.decompress_lz4(src_lz4, dst_so)

    self.assertTrue(os.path.exists(dst_so))
    with open(dst_so, 'rb') as f:
      self.assertEqual(f.read(), self.test_content)

  def test_lz4_unlz4_cli_fallback_success(self):
    src_lz4 = os.path.join(self.temp_dir.name, 'libcobalt.lz4')
    dst_so = os.path.join(self.temp_dir.name, 'out', 'libcobalt.so')
    with open(src_lz4, 'wb') as f:
      f.write(b'dummy_lz4_bytes')

    def mock_run(cmd, stdout=None, stderr=None, check=True, text=True):
      del stderr, check, text
      stdout.write(self.test_content)
      return subprocess.CompletedProcess(cmd, 0)

    with mock.patch.dict('sys.modules', {'lz4.frame': None}):
      with mock.patch(
          'shutil.which',
          side_effect=lambda x: '/usr/bin/unlz4' if x == 'unlz4' else None):
        with mock.patch('subprocess.run', side_effect=mock_run):
          copy_prebuilt_libcobalt.decompress_lz4(src_lz4, dst_so)

    self.assertTrue(os.path.exists(dst_so))
    with open(dst_so, 'rb') as f:
      self.assertEqual(f.read(), self.test_content)

  def test_zstd_unzstd_cli_fallback_success(self):
    src_zst = os.path.join(self.temp_dir.name, 'libcobalt.zst')
    dst_so = os.path.join(self.temp_dir.name, 'out', 'libcobalt.so')
    with open(src_zst, 'wb') as f:
      f.write(b'dummy_zst_bytes')

    def mock_run(cmd, stdout=None, stderr=None, check=True, text=True):
      del stderr, check, text
      stdout.write(self.test_content)
      return subprocess.CompletedProcess(cmd, 0)

    with mock.patch.dict('sys.modules', {'zstandard': None, 'zstd': None}):
      with mock.patch(
          'shutil.which',
          side_effect=lambda x: '/usr/bin/unzstd' if x == 'unzstd' else None):
        with mock.patch('subprocess.run', side_effect=mock_run):
          copy_prebuilt_libcobalt.decompress_zstd(src_zst, dst_so)

    self.assertTrue(os.path.exists(dst_so))
    with open(dst_so, 'rb') as f:
      self.assertEqual(f.read(), self.test_content)

  def test_zstandard_python_module_streaming(self):
    src_zst = os.path.join(self.temp_dir.name, 'libcobalt.zst')
    dst_so = os.path.join(self.temp_dir.name, 'out', 'libcobalt.so')
    with open(src_zst, 'wb') as f:
      f.write(b'dummy_zstd_data')

    mock_zstandard = mock.MagicMock()
    mock_dctx = mock.MagicMock()

    def mock_copy_stream(f_in,
                         f_out,
                         read_size=64 * 1024,
                         write_size=64 * 1024):
      del f_in, read_size, write_size
      f_out.write(self.test_content)

    mock_dctx.copy_stream.side_effect = mock_copy_stream
    mock_zstandard.ZstdDecompressor.return_value = mock_dctx

    with mock.patch.dict('sys.modules', {'zstandard': mock_zstandard}):
      copy_prebuilt_libcobalt.decompress_zstd(src_zst, dst_so)

    self.assertTrue(os.path.exists(dst_so))
    with open(dst_so, 'rb') as f:
      self.assertEqual(f.read(), self.test_content)

  def test_zstd_python_module_fallback(self):
    src_zst = os.path.join(self.temp_dir.name, 'libcobalt.zst')
    dst_so = os.path.join(self.temp_dir.name, 'out', 'libcobalt.so')
    with open(src_zst, 'wb') as f:
      f.write(b'dummy_zstd_data')

    mock_zstd = mock.MagicMock()
    mock_zstd.decompress.return_value = self.test_content

    with mock.patch.dict('sys.modules', {'zstandard': None, 'zstd': mock_zstd}):
      copy_prebuilt_libcobalt.decompress_zstd(src_zst, dst_so)

    self.assertTrue(os.path.exists(dst_so))
    with open(dst_so, 'rb') as f:
      self.assertEqual(f.read(), self.test_content)

  def test_main_cli_success(self):
    src_so = os.path.join(self.temp_dir.name, 'prebuilt.so')
    dst_so = os.path.join(self.temp_dir.name, 'out', 'libcobalt.so')
    dst_unstripped = os.path.join(self.temp_dir.name, 'out', 'unstripped.so')
    with open(src_so, 'wb') as f:
      f.write(self.test_content)

    test_args = [
        'copy_prebuilt_libcobalt.py',
        '--input',
        src_so,
        '--output',
        dst_so,
        '--unstripped-output',
        dst_unstripped,
    ]
    with mock.patch('sys.argv', test_args):
      exit_code = copy_prebuilt_libcobalt.main()
      self.assertEqual(exit_code, 0)

    self.assertTrue(os.path.exists(dst_so))
    self.assertTrue(os.path.exists(dst_unstripped))

  def test_main_cli_error(self):
    test_args = [
        'copy_prebuilt_libcobalt.py',
        '--input',
        os.path.join(self.temp_dir.name, 'nonexistent.so'),
        '--output',
        os.path.join(self.temp_dir.name, 'out', 'libcobalt.so'),
    ]
    with mock.patch('sys.argv', test_args):
      exit_code = copy_prebuilt_libcobalt.main()
      self.assertEqual(exit_code, 1)


if __name__ == '__main__':
  unittest.main()
