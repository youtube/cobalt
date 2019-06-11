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

from starboard.build import port_symlink
from starboard.tools import util


class PortSymlinkTest(unittest.TestCase):

  def setUp(self):
    super(PortSymlinkTest, self).setUp()
    self.tmp_dir = os.path.join(tempfile.gettempdir(), 'port_symlink')
    self.from_dir = os.path.join(self.tmp_dir, 'from_dir')
    self.test_txt = os.path.join(self.from_dir, 'test.txt')
    self.inner_dir = os.path.join(self.from_dir, 'inner_dir')
    self.link_dir = os.path.join(self.tmp_dir, 'link')
    _MakeDirs(self.tmp_dir)
    _MakeDirs(self.from_dir)
    _MakeDirs(self.inner_dir)
    port_symlink.MakeSymLink(self.from_dir, self.link_dir)
    with open(self.test_txt, 'w') as fd:
      fd.write('hello world')

  def tearDown(self):
    port_symlink.Rmtree(self.tmp_dir)
    super(PortSymlinkTest, self).tearDown()

  def testSanity(self):
    self.assertTrue(os.path.isdir(self.tmp_dir))
    self.assertTrue(os.path.isdir(self.from_dir))
    self.assertTrue(os.path.isdir(self.inner_dir))

  def testReadSymlinkNormalDirectory(self):
    self.assertEqual(None, port_symlink.ReadSymLink(self.from_dir))

  def testReadSymlinkNormalFile(self):
    self.assertEqual(None, port_symlink.ReadSymLink(self.test_txt))

  def testSymlinkDir(self):
    self.assertTrue(os.path.exists(self.link_dir))
    self.assertTrue(port_symlink.IsSymLink(self.link_dir))
    from_dir_2 = port_symlink.ReadSymLink(self.link_dir)
    self.assertTrue(_IsSamePath(from_dir_2, self.from_dir))

  def testDelSymlink(self):
    link_dir2 = os.path.join(self.tmp_dir, 'link2')
    port_symlink.MakeSymLink(self.from_dir, link_dir2)
    self.assertTrue(port_symlink.IsSymLink(link_dir2))
    port_symlink.DelSymLink(link_dir2)
    self.assertFalse(os.path.exists(link_dir2))

  def testRmtreeRemovesLink(self):
    port_symlink.Rmtree(self.link_dir)
    self.assertFalse(os.path.exists(self.link_dir))
    self.assertTrue(os.path.exists(self.from_dir))

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
    self.assertIn(os.path.join(self.link_dir, 'test.txt'),
                  paths_follow_links)
    self.assertNotIn(os.path.join(self.link_dir, 'test.txt'),
                     paths_nofollow_links)


def _MakeDirs(path):
  if not os.path.isdir(path):
    os.makedirs(path)


def _PathTypeToString(path):
  if port_symlink.IsSymLink(path):
    return 'link'
  if os.path.isdir(path):
    return 'dir'
  return 'file'


def _GetAllPaths(start_dir, followlinks):
  paths = []
  for root, dirs, files in port_symlink.OsWalk(start_dir,
                                               followlinks=followlinks):
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
