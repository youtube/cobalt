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

from starboard.build import filelist
from starboard.build import port_symlink
from starboard.tools import util


def _MakeDirs(path):
  if not os.path.isdir(path):
    os.makedirs(path)


class TempFileSystem(object):
  """Generates a test file structure with file/dir/symlink for testing."""

  def __init__(self, root_sub_dir='bundler'):
    root_sub_dir = os.path.normpath(root_sub_dir)
    self.root_tmp = os.path.join(tempfile.gettempdir(), root_sub_dir)
    self.root_in_tmp = os.path.join(self.root_tmp, 'in')
    self.test_txt = os.path.join(self.root_in_tmp, 'from_dir', 'test.txt')
    self.sym_dir = os.path.join(self.root_in_tmp, 'from_dir_lnk')
    self.from_dir = os.path.join(self.root_in_tmp, 'from_dir')

  def Make(self):
    _MakeDirs(self.root_in_tmp)
    _MakeDirs(os.path.dirname(self.test_txt))
    port_symlink.MakeSymLink(self.from_dir, self.sym_dir)
    with open(self.test_txt, 'w') as fd:
      fd.write('TEST')

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
    flist = filelist.FileList()
    flist.AddAllFilesInPath(tf.root_in_tmp, tf.root_in_tmp)
    self.assertTrue(flist.symlink_dir_list)
    self.assertTrue(flist.file_list)

  def testAddSymlink(self):
    tf = TempFileSystem()
    tf.Make()
    flist = filelist.FileList()
    flist.AddFile(tf.root_tmp, tf.sym_dir)
    flist.Print()
    self.assertTrue(flist.symlink_dir_list)
    self.assertFalse(flist.file_list)


if __name__ == '__main__':
  util.SetupDefaultLoggingConfig()
  unittest.main(verbosity=2)
