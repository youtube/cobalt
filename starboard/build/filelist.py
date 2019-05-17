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

import _env

from starboard.build.port_symlink import \
    IsSymLink, ReadSymLink, OsWalk, Rmtree, MakeSymLink

class FileList:
  def __init__(self):
    self.file_list = []
    self.symlink_dir_list = []

  def AddAllFilesInPath(self, root_dir, sub_dir):
    all_files = []
    all_symlinks = []
    if not os.path.isdir(sub_dir):
      raise IOError('Expected root directory to exist: ' + str(sub_dir))
    for root, dirs, files in OsWalk(sub_dir):
      for f in files:
        full_path = os.path.abspath(os.path.join(root, f))
        all_files.append(full_path)
      for dir_name in dirs:
        full_path = os.path.abspath(os.path.join(root, dir_name))
        if IsSymLink(full_path):
          all_symlinks.append(full_path)
    for f in all_files + all_symlinks:
      self.AddFile(root_dir, f)

  def AddAllFilesInPaths(self, root_dir, sub_dirs):
    if not os.path.isdir(root_dir):
      raise IOError('Expected root directory to exist: ' + str(root_dir))
    for sub_d in sub_dirs:
      self.AddAllFilesInPath(root_dir=root_dir, sub_dir=sub_d)

  def AddFile(self, root_path, file_path):
    if IsSymLink(file_path):
      self.AddSymLink(root_path, file_path)
    else:
      archive_name = os.path.relpath(file_path, root_path)
      self.file_list.append([file_path, archive_name])

  def AddSymLink(self, relative_dir, link_file):
    rel_link_path = os.path.relpath(link_file, relative_dir)
    phy_path = ReadSymLink(link_file)
    assert os.path.isdir(phy_path), phy_path
    rel_phy_path = os.path.relpath(phy_path, relative_dir)
    self.symlink_dir_list.append([relative_dir, rel_link_path, rel_phy_path])

  def Print(self):
    for f in self.file_list:
      print('File:    ' + str(f))
    for s in self.symlink_dir_list:
      print('Symlink: ' + str(s))


TYPE_NONE = 'NONE'
TYPE_SYMLINK_DIR = 'SYMLINK DIR'
TYPE_DIRECTORY = 'DIR'
TYPE_FILE = 'FILE'


def GetFileType(f):
  if not os.path.exists(f):
    return TYPE_NONE
  elif IsSymLink(f):
    return TYPE_SYMLINK_DIR
  elif os.path.isdir(f):
    return TYPE_DIRECTORY
  else:
    assert(os.path.isfile(f))
    return TYPE_FILE

def _MakeDirs(path):
  if not os.path.isdir(path):
    os.makedirs(path)


################################################################################
#                              UNIT TEST                                       #
################################################################################


# TODO: There is enough test coverage now to warrant using the python unittest
# module.
class TempFileSystem:
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
    MakeSymLink(self.from_dir, self.sym_dir)
    with open(self.test_txt, 'w') as fd:
      fd.write('TEST')

  def Clear(self):
    Rmtree(self.root_tmp)

  def Print(self):
    def P(f):
      t = GetFileType(f)
      print('{:<13} {}'.format(t, f))
    P(self.root_tmp)
    P(self.root_in_tmp)
    P(self.test_txt)
    P(self.from_dir)
    P(self.sym_dir)


def UnitTestTempFileSystem():
  tf = TempFileSystem()
  tf.Clear()
  assert(GetFileType(tf.sym_dir) == TYPE_NONE)
  assert(GetFileType(tf.root_tmp) == TYPE_NONE)
  assert(GetFileType(tf.root_in_tmp) == TYPE_NONE)
  assert(GetFileType(tf.from_dir) == TYPE_NONE)
  assert(GetFileType(tf.test_txt) == TYPE_NONE)
  tf.Make()
  assert(GetFileType(tf.sym_dir) == TYPE_SYMLINK_DIR)
  assert(GetFileType(tf.root_tmp) == TYPE_DIRECTORY)
  assert(GetFileType(tf.root_in_tmp) == TYPE_DIRECTORY)
  assert(GetFileType(tf.from_dir) == TYPE_DIRECTORY)
  assert(GetFileType(tf.test_txt) == TYPE_FILE)
  tf.Clear()
  assert(GetFileType(tf.sym_dir) == TYPE_NONE)
  assert(GetFileType(tf.root_tmp) == TYPE_NONE)
  assert(GetFileType(tf.root_in_tmp) == TYPE_NONE)
  assert(GetFileType(tf.from_dir) == TYPE_NONE)
  assert(GetFileType(tf.test_txt) == TYPE_NONE)


def UnitTestFileList_AddFile():
  flist = FileList()
  flist.AddFile(root_path=r'd1/d2', file_path=r'd1/d2/test.txt')
  assert(flist.file_list == [['d1/d2/test.txt', 'test.txt']])


def UnitTestFileList_AddAllFilesInPath():
  tf = TempFileSystem()
  tf.Make()
  flist = FileList()
  flist.AddAllFilesInPath(tf.root_in_tmp, tf.root_in_tmp)
  assert(flist.symlink_dir_list)
  assert(flist.file_list)


def UnitTestFileList_AddSymlink():
  tf = TempFileSystem()
  tf.Make()
  flist = FileList()
  flist.AddFile(tf.root_tmp, tf.sym_dir)
  flist.Print()
  assert(flist.symlink_dir_list)
  assert(not flist.file_list)


def UnitTest():
  UnitTestTempFileSystem()
  UnitTestFileList_AddFile()
  UnitTestFileList_AddAllFilesInPath()
  UnitTestFileList_AddSymlink()


def UnitTest():
  tests = [
    ['UnitTestTempFileSystem', UnitTestTempFileSystem],
    ['UnitTestFileList_AddFile', UnitTestFileList_AddFile],
    ['UnitTestFileList_AddSymlink', UnitTestFileList_AddSymlink],
    ['UnitTestFileList_AddAllFilesInPath', UnitTestFileList_AddAllFilesInPath],
  ]
  failed_tests = []
  for name, test in tests:
    try:
      print('\n' + name + ' started.')
      test()
      print(name + ' passed.')
    except Exception as err:
      failed_tests.append(name)
      traceback.print_exc()
      print('Error happened during test ' + name)

  if failed_tests:
    print '\n\nThe following tests failed: ' + ','.join(failed_tests)
    return 1
  else:
    return 0


if __name__ == "__main__":
  UnitTest()
