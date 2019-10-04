#!/usr/bin/env python
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


import os
import tempfile
import unittest

import _env  # pylint: disable=relative-import,unused-import

from cobalt.build import cobalt_archive_extract
from starboard.build import filelist
from starboard.tools import port_symlink
from starboard.tools import util


LONG_DIR_NAME_1 = 'really_l' + 'o' * 120 + 'ng_dir_name'
LONG_DIR_NAME_2 = 'another_really_l' + 'o' * 120 + 'ng_dir_name'
LONG_SUB_DIRS = os.path.join(LONG_DIR_NAME_1, LONG_DIR_NAME_2)


def _MakeDirs(path):
  if not os.path.isdir(path):
    os.makedirs(path)


class TempFileSystem(object):
  """Generates a test file structure with file/dir/symlink for testing.

  <temp_dir>
   |-> <root_sub_dir>
   |   |-> in
   |   |   |-> from_dir
   |   |   |   |-> test.txt
   |   |   |-> from_dir_lnk -> <temp_dir>/<root_sub_dir>/in/from_dir
  """

  def __init__(self, root_sub_dir='bundler'):
    root_sub_dir = os.path.normpath(root_sub_dir)
    self.root_tmp = os.path.join(tempfile.gettempdir(), root_sub_dir)
    if os.path.exists(self.root_tmp):
      port_symlink.Rmtree(self.root_tmp)
    self.root_in_tmp = os.path.join(self.root_tmp, 'in')
    self.sym_dir = os.path.join(self.root_in_tmp, 'from_dir_lnk')
    self.from_dir = os.path.join(self.root_in_tmp, 'from_dir')
    self.test_txt = os.path.join(self.from_dir, 'test.txt')

  def Make(self):
    _MakeDirs(self.root_in_tmp)
    _MakeDirs(os.path.dirname(self.test_txt))
    port_symlink.MakeSymLink(self.from_dir, self.sym_dir)
    with open(self.test_txt, 'w') as fd:
      fd.write('TEST')

  def MakeLongPathFile(self):
    long_path_txt = os.path.join(self.from_dir, LONG_SUB_DIRS, 'test2.txt')
    self.long_path_txt = long_path_txt
    long_path_txt = port_symlink.ToLongPath(long_path_txt)
    _MakeDirs(os.path.dirname(long_path_txt))
    with open(long_path_txt, 'w') as fd:
      fd.write('TEST BIS')

  def Clear(self):
    port_symlink.Rmtree(self.root_tmp)

  def Print(self):
    def P(f):
      t = filelist.GetFileType(f)
      print '{:<13} {}'.format(t, f)
    P(self.root_tmp)
    P(self.root_in_tmp)
    P(self.test_txt)
    P(self.from_dir)
    P(self.sym_dir)


class FileListTest(unittest.TestCase):

  def testTempFileSystem(self):
    """Sanity test to ensure TempFileSystem works correctly on the platform."""
    tf = TempFileSystem()
    tf.Clear()
    self.assertEqual(filelist.GetFileType(tf.sym_dir), filelist.TYPE_NONE)
    self.assertEqual(filelist.GetFileType(tf.root_tmp), filelist.TYPE_NONE)
    self.assertEqual(filelist.GetFileType(tf.root_in_tmp), filelist.TYPE_NONE)
    self.assertEqual(filelist.GetFileType(tf.from_dir), filelist.TYPE_NONE)
    self.assertEqual(filelist.GetFileType(tf.test_txt), filelist.TYPE_NONE)
    tf.Make()
    self.assertEqual(filelist.GetFileType(tf.sym_dir),
                     filelist.TYPE_SYMLINK_DIR)
    self.assertEqual(filelist.GetFileType(tf.root_tmp), filelist.TYPE_DIRECTORY)
    self.assertEqual(filelist.GetFileType(tf.root_in_tmp),
                     filelist.TYPE_DIRECTORY)
    self.assertEqual(filelist.GetFileType(tf.from_dir), filelist.TYPE_DIRECTORY)
    self.assertEqual(filelist.GetFileType(tf.test_txt), filelist.TYPE_FILE)
    tf.Clear()
    self.assertEqual(filelist.GetFileType(tf.sym_dir), filelist.TYPE_NONE)
    self.assertEqual(filelist.GetFileType(tf.root_tmp), filelist.TYPE_NONE)
    self.assertEqual(filelist.GetFileType(tf.root_in_tmp), filelist.TYPE_NONE)
    self.assertEqual(filelist.GetFileType(tf.from_dir), filelist.TYPE_NONE)
    self.assertEqual(filelist.GetFileType(tf.test_txt), filelist.TYPE_NONE)

  def testAddFile(self):
    flist = filelist.FileList()
    flist.AddFile(root_path=r'd1/d2', file_path=r'd1/d2/test.txt')
    self.assertEqual(flist.file_list, [['d1/d2/test.txt', 'test.txt']])

  def testAddAllFilesInPath(self):
    tf = TempFileSystem()
    tf.Make()
    tf.MakeLongPathFile()
    flist = filelist.FileList()
    flist.AddAllFilesInPath(tf.root_in_tmp, tf.root_in_tmp)
    self.assertTrue(flist.symlink_dir_list)
    expected_file_list = [
        [tf.test_txt, os.path.join('from_dir', 'test.txt')],
        [tf.long_path_txt,
         os.path.join('from_dir', LONG_SUB_DIRS , 'test2.txt')]]
    self.assertEqual(flist.file_list, expected_file_list)

  def testAddSymlink(self):
    tf = TempFileSystem()
    tf.Make()
    flist = filelist.FileList()
    flist.AddFile(tf.root_tmp, tf.sym_dir)
    flist.Print()
    self.assertTrue(flist.symlink_dir_list)
    self.assertFalse(flist.file_list)

  def testAddRelativeSymlink(self):
    """Tests that adding a relative symlink works as expected."""
    tf = TempFileSystem()
    tf.Make()
    flist = filelist.FileList()
    in2 = os.path.join(tf.root_in_tmp, 'subdir', 'in2')
    target_path = os.path.relpath(tf.from_dir, os.path.dirname(in2))
    # Sanity check that target_path is relative.
    self.assertEqual(target_path, os.path.join('..', 'from_dir'))
    # Create the link and check that it points to the correct folder.
    port_symlink.MakeSymLink(target_path, in2)
    self.assertTrue(port_symlink.IsSymLink(in2))
    self.assertEqual(port_symlink.ReadSymLink(in2), target_path)
    self.assertEqual(os.listdir(in2), ['test.txt'])
    # Add the symlink to flist and check its content.
    flist.AddFile(tf.root_tmp, in2)
    flist.Print()
    self.assertTrue(flist.symlink_dir_list)
    expected = [
        tf.root_tmp,
        os.path.join('in', 'subdir', 'in2'),
        os.path.join('in', 'from_dir')]
    self.assertEqual(flist.symlink_dir_list[0], expected)

  def testOsGetRelpathFallback(self):
    path = (
        'src/out/tmp/cobalt_archive/archive/____app_launcher/third_party/'
        'web_platform_tests/custom-elements/registering-custom-elements/'
        'unresolved-element-pseudoclass/'
        'unresolved-element-pseudoclass-css-test-registered-type-extension-ref'
        '.html').replace('/', os.sep)
    root = (
        'src/out/tmp/cobalt_archive/archive/____app_launcher'
    ).replace('/', os.sep)
    expected_result = (
        'third_party/web_platform_tests/custom-elements/'
        'registering-custom-elements/unresolved-element-pseudoclass/'
        'unresolved-element-pseudoclass-css-test-registered-type-extension-ref'
        '.html').replace('/', os.sep)
    rel_path = filelist._FallbackOsGetRelPath(path, start_dir=root)
    self.assertEqual(expected_result, rel_path)


if __name__ == '__main__':
  util.SetupDefaultLoggingConfig()
  unittest.main(verbosity=2)
