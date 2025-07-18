#!/usr/bin/env vpython3
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for git_migrate_default_branch.py."""

import collections
import os
import sys
import unittest

if sys.version_info.major == 2:
  import mock
else:
  from unittest import mock

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import git_migrate_default_branch


class CMDFormatTestCase(unittest.TestCase):
  def setUp(self):
    self.addCleanup(mock.patch.stopall)

  def test_no_remote(self):
    def RunMock(*args):
      if args[0] == 'remote':
        return ''
      self.fail('did not expect such run git command: %s' % args[0])

    mock.patch('git_migrate_default_branch.git_common.run', RunMock).start()
    with self.assertRaisesRegexp(RuntimeError, 'Could not find any remote'):
      git_migrate_default_branch.main()

  def test_migration_not_ready(self):
    def RunMock(*args):
      if args[0] == 'remote':
        return 'origin\ngerrit'
      raise Exception('Did not expect such run git command: %s' % args[0])

    mock.patch('git_migrate_default_branch.git_common.run', RunMock).start()
    mock.patch('git_migrate_default_branch.git_common.repo_root',
               return_value='.').start()
    mock.patch('git_migrate_default_branch.scm.GIT.GetConfig',
               return_value='https://chromium.googlesource.com').start()
    mock.patch('git_migrate_default_branch.gerrit_util.GetProjectHead',
               return_value=None).start()
    with self.assertRaisesRegexp(RuntimeError, 'not migrated yet'):
      git_migrate_default_branch.main()

  def test_migration_no_master(self):
    def RunMock(*args):
      if args[0] == 'remote':
        return 'origin\ngerrit'

      if args[0] == 'fetch':
        return

      if args[0] == 'branch':
        return

      if args[0] == 'config':
        return
      raise Exception('Did not expect such run git command: %s' % args[0])

    mock_runs = mock.patch('git_migrate_default_branch.git_common.run',
                           side_effect=RunMock).start()
    mock.patch('git_migrate_default_branch.git_common.repo_root',
               return_value='.').start()
    mock.patch('git_migrate_default_branch.scm.GIT.GetConfig',
               return_value='https://chromium.googlesource.com').start()
    mock.patch('git_migrate_default_branch.gerrit_util.GetProjectHead',
               return_value='refs/heads/main').start()

    BranchesInfo = collections.namedtuple('BranchesInfo',
                                          'hash upstream commits behind')
    branches = {
        '': None,  # always returned
        'master': BranchesInfo('0000', 'origin/master', '0', '0'),
        'feature': BranchesInfo('0000', 'master', '0', '0'),
        'another_feature': BranchesInfo('0000', 'feature', '0', '0'),
        'remote_feature': BranchesInfo('0000', 'origin/master', '0', '0'),
    }
    mock.patch('git_migrate_default_branch.git_common.get_branches_info',
               return_value=branches).start()
    mock_merge_base = mock.patch(
        'git_migrate_default_branch.git_common.remove_merge_base',
        return_value=branches).start()

    git_migrate_default_branch.main()
    mock_merge_base.assert_any_call('feature')
    mock_merge_base.assert_any_call('remote_feature')
    mock_runs.assert_any_call('branch', '-m', 'master', 'main')
    mock_runs.assert_any_call('branch', '--set-upstream-to', 'main', 'feature')
    mock_runs.assert_any_call('branch', '--set-upstream-to', 'origin/main',
                              'remote_feature')


if __name__ == '__main__':
  unittest.main()
