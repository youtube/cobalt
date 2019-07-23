#!/usr/bin/python
#
# Copyright 2019 The Cobalt Authors. All Rights Reserved.
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


import json
import os
import shutil
import stat
import subprocess
import unittest

import _env  # pylint: disable=relative-import,unused-import
from cobalt.build import cobalt_archive
from starboard.build import filelist
from starboard.build import filelist_test
from starboard.build import port_symlink
from starboard.tools import util


class CobaltArchiveTest(unittest.TestCase):

  def testFoldIdenticalFiles(self):
    tf_root = filelist_test.TempFileSystem('bundler_fold')
    tf_root.Clear()
    tf1 = filelist_test.TempFileSystem(os.path.join('bundler_fold', '1'))
    tf2 = filelist_test.TempFileSystem(os.path.join('bundler_fold', '2'))
    tf1.Make()
    tf2.Make()
    flist = filelist.FileList()
    subdirs = [tf1.root_in_tmp, tf2.root_in_tmp]
    flist.AddAllFilesInPaths(tf_root.root_tmp, subdirs)
    flist.Print()
    identical_files = [tf1.test_txt, tf2.test_txt]
    physical_files, copy_files = cobalt_archive._FoldIdenticalFiles(
        identical_files)
    self.assertEqual(tf1.test_txt, physical_files[0])
    self.assertIn(tf1.test_txt, copy_files[0][0])
    self.assertIn(tf2.test_txt, copy_files[0][1])

  def testMakesDeployInfo(self):
    flist = filelist.FileList()
    tf = filelist_test.TempFileSystem()
    tf.Clear()
    tf.Make()
    bundle_zip = os.path.join(tf.root_tmp, 'bundle.zip')
    car = cobalt_archive.CobaltArchive(bundle_zip)
    car.MakeArchive(platform_name='fake',
                    platform_sdk_version='fake_sdk',
                    config='devel',
                    file_list=flist)
    out_dir = os.path.join(tf.root_tmp, 'out')
    car.ExtractTo(out_dir)
    out_metadata_file = os.path.join(out_dir, cobalt_archive._OUT_METADATA_PATH)
    self.assertEqual(filelist.GetFileType(out_metadata_file),
                     filelist.TYPE_FILE)
    with open(out_metadata_file) as fd:
      text = fd.read()
      js = json.loads(text)
      self.assertTrue(js)
      self.assertEqual(js['sdk_version'], 'fake_sdk')
      self.assertEqual(js['platform'], 'fake')
      self.assertEqual(js['config'], 'devel')

  def testExtractTo(self):
    flist = filelist.FileList()
    tf = filelist_test.TempFileSystem()
    tf.Clear()
    tf.Make()
    flist.AddSymLink(tf.root_in_tmp, tf.sym_dir)
    bundle_zip = os.path.join(tf.root_tmp, 'bundle.zip')
    car = cobalt_archive.CobaltArchive(bundle_zip)
    car.MakeArchive(platform_name='fake',
                    platform_sdk_version='fake_sdk',
                    config='devel',
                    file_list=flist)
    out_dir = os.path.join(tf.root_tmp, 'out')
    car.ExtractTo(out_dir)
    out_from_dir = os.path.join(out_dir, 'from_dir')
    out_from_dir_lnk = os.path.join(out_dir, 'from_dir_lnk')
    self.assertEqual(filelist.GetFileType(out_from_dir),
                     filelist.TYPE_DIRECTORY)
    self.assertEqual(filelist.GetFileType(out_from_dir_lnk),
                     filelist.TYPE_SYMLINK_DIR)
    resolved_from_link_path = os.path.join(
        out_dir, port_symlink.ReadSymLink(out_from_dir_lnk))
    self.assertEqual(os.path.abspath(out_from_dir),
                     os.path.abspath(resolved_from_link_path))

  def testExtractFileWithLongFileName(self):
    """Tests that a long file name can be archived and extracted."""
    flist = filelist.FileList()
    tf = filelist_test.TempFileSystem()
    tf.Clear()
    tf.Make()
    self.assertTrue(os.path.exists(tf.root_in_tmp))
    suffix_path = os.path.join(
        'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa',
        'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa',
        'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa',
        'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa',
        'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa',
        'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa',
        'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa',
        'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa',
        'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa',
        'test.txt'
    )
    input_dst = os.path.join(tf.root_in_tmp, suffix_path)
    out_dir = os.path.join(tf.root_tmp, 'out')
    output_dst = os.path.join(out_dir, suffix_path)
    _MoveFileWithLongPath(tf.test_txt, input_dst)
    self.assertTrue(_LongPathExists(input_dst))
    flist.AddFile(tf.root_in_tmp, input_dst)

    bundle_zip = os.path.join(tf.root_tmp, 'bundle.zip')
    car = cobalt_archive.CobaltArchive(bundle_zip)
    car.MakeArchive(platform_name='fake',
                    platform_sdk_version='fake_sdk',
                    config='devel',
                    file_list=flist)
    car.ExtractTo(out_dir)
    self.assertTrue(_LongPathExists(output_dst))

  @unittest.skipIf(port_symlink.IsWindows(), 'Any platform but windows.')
  def testExecutionAttribute(self):
    flist = filelist.FileList()
    tf = filelist_test.TempFileSystem()
    tf.Make()
    # Execution bit seems to turn off the read bit, so we just set all
    # read/write/execute bit for the user.
    write_flags = stat.S_IXUSR | stat.S_IWUSR | stat.S_IRUSR
    os.chmod(tf.test_txt, write_flags)
    self.assertNotEqual(
        0, write_flags & cobalt_archive._GetFilePermissions(tf.test_txt))
    flist.AddFile(tf.root_tmp, tf.test_txt)
    bundle_zip = os.path.join(tf.root_tmp, 'bundle.zip')
    car = cobalt_archive.CobaltArchive(bundle_zip)
    car.MakeArchive(platform_name='fake',
                    platform_sdk_version='fake_sdk',
                    config='devel',
                    file_list=flist)
    # Now grab the json file and check that the file appears in the
    # executable_file list.
    json_str = car.ReadFile(
        '__cobalt_archive/finalize_decompression/decompress.json')
    decompress_dict = json.loads(json_str)
    executable_files = decompress_dict.get('executable_files')
    # Expect that the executable file appears in the executable_files.
    self.assertTrue(executable_files)
    archive_path = os.path.relpath(tf.test_txt, tf.root_tmp)
    self.assertIn(archive_path, executable_files)
    out_dir = os.path.join(tf.root_tmp, 'out')
    car.ExtractTo(output_dir=out_dir)
    out_file = os.path.join(out_dir, tf.test_txt)
    self.assertTrue(_LongPathExists(out_file))
    perms = cobalt_archive._GetFilePermissions(out_file)
    self.assertTrue(perms & stat.S_IXUSR)


def _SilentCall(cmd_str):
  proc = subprocess.Popen(cmd_str,
                          shell=True,
                          stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT)
  (_, _) = proc.communicate()
  return proc.returncode


def _LongPathExists(p):
  if port_symlink.IsWindows():
    rc = _SilentCall('dir /s /b "%s"' % p)
    return rc == 0
  else:
    return os.path.isfile(p)


def _MoveFileWithLongPath(src_file, dst_file):
  dst_dir = os.path.dirname(dst_file)
  if port_symlink.IsWindows():
    # Work around for file-length path limitations on Windows.
    src_dir = os.path.dirname(src_file)
    file_name = os.path.basename(src_file)
    shell_cmd = 'robocopy "%s" "%s" "%s" /MOV' % (src_dir, dst_dir, file_name)
    rc = _SilentCall(shell_cmd)
    if 1 != rc:  # Robocopy returns 1 if a file was copied.
      raise OSError('File %s was not copied' % src_file)
    expected_out_file = os.path.join(dst_dir, file_name)
    if not _LongPathExists(expected_out_file):
      raise OSError('File did not end up in %s' % dst_dir)
    return
  else:
    if not os.path.isdir(dst_dir):
      os.makedirs(dst_dir)
    shutil.move(src_file, dst_file)
    if not os.path.isfile(dst_file):
      raise OSError('File did not end up in %s' % dst_dir)


if __name__ == '__main__':
  util.SetupDefaultLoggingConfig()
  unittest.main(verbosity=2)
