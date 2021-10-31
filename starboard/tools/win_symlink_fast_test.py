#!/usr/bin/python
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

"""Tests the win_symlink_fast functionality."""

import sys
import unittest


import _env  # pylint: disable=relative-import,unused-import


if __name__ == '__main__' and sys.platform == 'win32':
  from starboard.tools import port_symlink_test  # pylint: disable=g-import-not-at-top
  from starboard.tools import util  # pylint: disable=g-import-not-at-top
  from starboard.tools import win_symlink  # pylint: disable=g-import-not-at-top
  from starboard.tools import win_symlink_fast  # pylint: disable=g-import-not-at-top

  # Override the port_symlink_test symlink functions to point to our win_symlink
  # and win_symlink_fast versions.
  def MakeSymLink(*args, **kwargs):
    return win_symlink_fast.FastCreateReparseLink(*args, **kwargs)

  def IsSymLink(*args, **kwargs):
    return win_symlink_fast.FastIsReparseLink(*args, **kwargs)

  def ReadSymLink(*args, **kwargs):
    return win_symlink_fast.FastReadReparseLink(*args, **kwargs)

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
    pass

  util.SetupDefaultLoggingConfig()
  unittest.main(verbosity=2)
