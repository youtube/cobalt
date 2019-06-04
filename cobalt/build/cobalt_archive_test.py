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
import unittest

import _env  # pylint: disable=relative-import,unused-import
import cobalt.build.cobalt_archive as cobalt_archive
import starboard.build.filelist as filelist
import starboard.build.filelist_test as filelist_test
import starboard.build.port_symlink as port_symlink
from starboard.tools.util import SetupDefaultLoggingConfig


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
    assert filelist.GetFileType(out_metadata_file) == filelist.TYPE_FILE
    with open(out_metadata_file) as fd:
      text = fd.read()
      js = json.loads(text)
      assert js
      assert js['sdk_version'] == 'fake_sdk'
      assert js['platform'] == 'fake'
      assert js['config'] == 'devel'

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
    assert filelist.GetFileType(out_from_dir) == filelist.TYPE_DIRECTORY
    assert filelist.GetFileType(out_from_dir_lnk) == filelist.TYPE_SYMLINK_DIR
    resolved_from_link_path = os.path.join(
        out_dir, port_symlink.ReadSymLink(out_from_dir_lnk))
    assert(os.path.abspath(out_from_dir) ==
           os.path.abspath(resolved_from_link_path))


if __name__ == '__main__':
  SetupDefaultLoggingConfig()
  unittest.main(verbosity=2)
