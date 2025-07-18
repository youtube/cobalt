#!/usr/bin/env vpython3
# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import sys
import unittest

if sys.version_info.major == 2:
  import mock
else:
  from unittest import mock

DEPOT_TOOLS_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, DEPOT_TOOLS_ROOT)

from testing_support import coverage_utils
import utils


class GitCacheTest(unittest.TestCase):
  def setUp(self):
    pass

  @mock.patch('subprocess.check_output', lambda x, **kwargs: b'foo')
  def testVersionWithGit(self):
    version = utils.depot_tools_version()
    self.assertEqual(version, 'git-foo')

  @mock.patch('subprocess.check_output')
  @mock.patch('os.path.getmtime', lambda x: 42)
  def testVersionWithNoGit(self, mock_subprocess):
    mock_subprocess.side_effect = Exception
    version = utils.depot_tools_version()
    self.assertEqual(version, 'recipes.cfg-42')

  @mock.patch('subprocess.check_output')
  @mock.patch('os.path.getmtime')
  def testVersionWithNoGit(self, mock_subprocess, mock_getmtime):
    mock_subprocess.side_effect = Exception
    mock_getmtime.side_effect = Exception
    version = utils.depot_tools_version()
    self.assertEqual(version, 'unknown')


if __name__ == '__main__':
  logging.basicConfig(
      level=logging.DEBUG if '-v' in sys.argv else logging.ERROR)
  sys.exit(
      coverage_utils.covered_main(
          (os.path.join(DEPOT_TOOLS_ROOT, 'git_cache.py')),
          required_percentage=0))
