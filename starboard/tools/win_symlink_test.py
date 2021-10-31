#!/usr/bin/python
# Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

"""Tests win_symlink."""

import os
import shutil
import sys
import tempfile
import unittest


import _env  # pylint: disable=relative-import,unused-import


if __name__ == '__main__' and sys.platform == 'win32':
  from starboard.tools import port_symlink_test  # pylint: disable=g-import-not-at-top
  from starboard.tools import util  # pylint: disable=g-import-not-at-top
  from starboard.tools import win_symlink  # pylint: disable=g-import-not-at-top

  # Override the port_symlink_test symlink functions to point to our win_symlink
  # versions.
  def MakeSymLink(*args, **kwargs):
    return win_symlink.CreateReparsePoint(*args, **kwargs)

  def IsSymLink(*args, **kwargs):
    return win_symlink.IsReparsePoint(*args, **kwargs)

  def ReadSymLink(*args, **kwargs):
    return win_symlink.ReadReparsePoint(*args, **kwargs)

  def Rmtree(*args, **kwargs):
    return win_symlink.RmtreeShallow(*args, **kwargs)

  def OsWalk(*args, **kwargs):
    return win_symlink.OsWalk(*args, **kwargs)

  port_symlink_test.MakeSymLink = MakeSymLink
  port_symlink_test.IsSymLink = IsSymLink
  port_symlink_test.ReadSymLink = ReadSymLink
  port_symlink_test.Rmtree = Rmtree
  port_symlink_test.OsWalk = OsWalk

  # Makes a unit test available to the unittest.main (through magic).
  class WinSymlinkTest(port_symlink_test.PortSymlinkTest):

    def testRmtreeOsWalkDoesNotFollowSymlinks(self):
      """_RmtreeOsWalk(...) will delete the symlink and not the target."""
      external_temp_dir = tempfile.mkdtemp()
      try:
        external_temp_file = os.path.join(external_temp_dir, 'test.txt')
        with open(external_temp_file, 'w') as fd:
          fd.write('HI')
        link_dir = os.path.join(self.tmp_dir, 'foo', 'link_dir')
        MakeSymLink(external_temp_file, link_dir)
        win_symlink._RmtreeOsWalk(self.tmp_dir)
        # The target file should still exist
        self.assertTrue(os.path.isfile(external_temp_file))
      finally:
        shutil.rmtree(external_temp_dir, ignore_errors=True)

    def testRmtreeCmdShellDoesNotFollowSymlinks(self):
      """_RmtreeShellCmd(...) will delete the symlink and not the target."""
      external_temp_dir = tempfile.mkdtemp()
      try:
        external_temp_file = os.path.join(external_temp_dir, 'test.txt')
        with open(external_temp_file, 'w') as fd:
          fd.write('HI')
        link_dir = os.path.join(self.tmp_dir, 'foo', 'link_dir')
        MakeSymLink(external_temp_file, link_dir)
        win_symlink._RmtreeShellCmd(self.tmp_dir)
        # The target file should still exist
        self.assertTrue(os.path.isfile(external_temp_file))
      finally:
        shutil.rmtree(external_temp_dir, ignore_errors=True)


  util.SetupDefaultLoggingConfig()
  unittest.main(verbosity=2)
