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
import stat
import unittest

import _env  # pylint: disable=relative-import,unused-import
import cobalt.build.cobalt_archive as cobalt_archive
import starboard.build.filelist as filelist
import starboard.build.filelist_test as filelist_test
import starboard.build.port_symlink as port_symlink
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

  @unittest.skipIf(port_symlink.IsWindows(), 'Any platform but windows.')
  def testExecutionAttribute(self):
    flist = filelist.FileList()
    tf = filelist_test.TempFileSystem()
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
    self.assertTrue(os.path.isfile(out_file))
    perms = cobalt_archive._GetFilePermissions(out_file)
    self.assertTrue(perms & stat.S_IXUSR)


if __name__ == '__main__':
  util.SetupDefaultLoggingConfig()
  unittest.main(verbosity=2)
