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
import shutil
import tempfile
import unittest

import _env  # pylint: disable=relative-import,unused-import

from starboard.tools import port_symlink
from starboard.tools import util

_TARGET_FILENAME = 'target.txt'


# Replace this function signature for other implementations of symlink
# functions.
def MakeSymLink(*args, **kwargs):
  return port_symlink.MakeSymLink(*args, **kwargs)


def IsSymLink(*args, **kwargs):
  return port_symlink.IsSymLink(*args, **kwargs)


def ReadSymLink(*args, **kwargs):
  return port_symlink.ReadSymLink(*args, **kwargs)


def Rmtree(*args, **kwargs):
  return port_symlink.Rmtree(*args, **kwargs)


def OsWalk(*args, **kwargs):
  return port_symlink.OsWalk(*args, **kwargs)


class PortSymlinkTest(unittest.TestCase):

  def setUp(self):
    super(PortSymlinkTest, self).setUp()
    self.tmp_dir = os.path.join(tempfile.gettempdir(), 'port_symlink')
    if os.path.exists(self.tmp_dir):
      Rmtree(self.tmp_dir)
    self.target_dir = os.path.join(self.tmp_dir, 'target')
    self.inner_dir = os.path.join(self.target_dir, 'inner')
    self.link_dir = os.path.join(self.tmp_dir, 'link')
    self.target_file = os.path.join(self.target_dir, _TARGET_FILENAME)
    _MakeDirs(self.tmp_dir)
    _MakeDirs(self.target_dir)
    _MakeDirs(self.inner_dir)
    with open(self.target_file, 'w') as fd:
      fd.write('hallo welt!')
    MakeSymLink(self.target_dir, self.link_dir)

  def tearDown(self):
    Rmtree(self.tmp_dir)
    super(PortSymlinkTest, self).tearDown()

  def testSanity(self):
    self.assertTrue(os.path.isdir(self.tmp_dir))
    self.assertTrue(os.path.isdir(self.target_dir))
    self.assertTrue(os.path.isdir(self.inner_dir))

  def testReadSymlinkNormalPath(self):
    self.assertIsNone(ReadSymLink(self.target_dir))

  def testSymlinkPath(self):
    self.assertTrue(os.path.exists(self.link_dir))
    self.assertTrue(IsSymLink(self.link_dir))
    from_dir_2 = ReadSymLink(self.link_dir)
    self.assertTrue(_IsSamePath(from_dir_2, self.target_dir))

  def testRelativeSymlinkPath(self):
    rel_link_dir = os.path.join(self.tmp_dir, 'foo', 'rel_link')
    rel_dir_path = os.path.relpath(self.target_dir, rel_link_dir)
    MakeSymLink(rel_dir_path, rel_link_dir)
    self.assertTrue(IsSymLink(rel_link_dir))
    link_value = ReadSymLink(rel_link_dir)
    self.assertIn(
        '..', link_value, msg='Expected ".." in relative path %s' % link_value)

  def testDelSymlink(self):
    link_dir2 = os.path.join(self.tmp_dir, 'link2')
    MakeSymLink(self.target_dir, link_dir2)
    self.assertTrue(IsSymLink(link_dir2))
    port_symlink.DelSymLink(link_dir2)
    self.assertFalse(os.path.exists(link_dir2))

  def testRmtreeRemovesLink(self):
    Rmtree(self.link_dir)
    self.assertFalse(os.path.exists(self.link_dir))
    self.assertTrue(os.path.exists(self.target_dir))

  def testRmtreeRemovesBrokenLink(self):
    Rmtree(self.target_dir)
    # os.path.exists() will return false for broken links (not true for reparse
    # points on Windows) since their target does not exist. Rmtree
    # implementations should still be able to remove the link.
    if not port_symlink.IsWindows():
      self.assertFalse(os.path.exists(self.link_dir))
    self.assertTrue(IsSymLink(self.link_dir))
    Rmtree(self.link_dir)
    self.assertFalse(IsSymLink(self.link_dir))

  def testRmtreeDoesNotFollowSymlinks(self):
    """Tests that Rmtree(...) will delete the symlink and not the target."""
    external_temp_dir = tempfile.mkdtemp()
    try:
      external_temp_file = os.path.join(external_temp_dir, _TARGET_FILENAME)
      with open(external_temp_file, 'w') as fd:
        fd.write('hallo!')
      link_dir = os.path.join(self.tmp_dir, 'foo', 'link_dir')
      MakeSymLink(external_temp_file, link_dir)
      Rmtree(self.tmp_dir)
      # The target file should still exist.
      self.assertTrue(os.path.isfile(external_temp_file))
    finally:
      shutil.rmtree(external_temp_file, ignore_errors=True)

  def testOsWalk(self):
    paths_nofollow_links = _GetAllPaths(self.tmp_dir, followlinks=False)
    paths_follow_links = _GetAllPaths(self.tmp_dir, followlinks=True)
    print '\nOsWalk Follow links:'
    for path in paths_follow_links:
      print '  ' + path + ' (' + _PathTypeToString(path) + ')'
    print '\nOsWalk No-Follow links:'
    for path in paths_nofollow_links:
      print '  ' + path + ' (' + _PathTypeToString(path) + ')'
    print ''
    self.assertIn(self.link_dir, paths_nofollow_links)
    self.assertIn(self.link_dir, paths_follow_links)
    self.assertIn(
        os.path.join(self.link_dir, _TARGET_FILENAME), paths_follow_links)
    self.assertNotIn(
        os.path.join(self.link_dir, _TARGET_FILENAME), paths_nofollow_links)


def _MakeDirs(path):
  if not os.path.isdir(path):
    os.makedirs(path)


def _PathTypeToString(path):
  if IsSymLink(path):
    return 'link'
  if os.path.isdir(path):
    return 'dir'
  return 'file'


def _GetAllPaths(start_dir, followlinks):
  paths = []
  for root, dirs, files in OsWalk(start_dir, followlinks=followlinks):
    for name in files:
      path = os.path.join(root, name)
      paths.append(path)
    for name in dirs:
      path = os.path.join(root, name)
      paths.append(path)
  return paths


def _IsSamePath(p1, p2):
  if not p1:
    p1 = None
  if not p2:
    p2 = None
  if p1 == p2:
    return True
  if (not p1) or (not p2):
    return False
  p1 = os.path.abspath(os.path.normpath(p1))
  p2 = os.path.abspath(os.path.normpath(p2))
  if p1 == p2:
    return True
  try:
    return os.stat(p1) == os.stat(p2)
  except Exception:  # pylint: disable=broad-except
    return False


if __name__ == '__main__':
  util.SetupDefaultLoggingConfig()
  unittest.main(verbosity=2)
