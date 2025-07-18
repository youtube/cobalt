#!/usr/bin/env vpython3
# Copyright (c) 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Smoke tests for gclient.py.

Shell out 'gclient' and run cipd tests.
"""

import logging
import os
import sys
import unittest

import gclient_smoketest_base

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


class GClientSmokeCipd(gclient_smoketest_base.GClientSmokeBase):
  def setUp(self):
    super(GClientSmokeCipd, self).setUp()
    self.enabled = self.FAKE_REPOS.set_up_git()
    if not self.enabled:
      self.skipTest('git fake repos not available')
    self.env['PATH'] = (os.path.join(ROOT_DIR, 'testing_support')
                        + os.pathsep + self.env['PATH'])

  def testSyncCipd(self):
    self.gclient(['config', self.git_base + 'repo_14', '--name', 'src'])
    self.gclient(['sync'])

    tree = self.mangle_git_tree(('repo_14@1', 'src'))
    tree.update({
        '_cipd': '\n'.join([
            '$ParanoidMode CheckPresence',
            '$OverrideInstallMode copy',
            '',
            '@Subdir src/another_cipd_dep',
            'package1 1.1-cr0',
            'package2 1.13',
            '',
            '@Subdir src/cipd_dep',
            'package0 0.1',
            '',
            '@Subdir src/cipd_dep_with_cipd_variable',
            'package3/${platform} 1.2',
            '',
            '',
        ]),
        'src/another_cipd_dep/_cipd': '\n'.join([
            'package1 1.1-cr0',
            'package2 1.13',
        ]),
        'src/cipd_dep/_cipd': 'package0 0.1',
        'src/cipd_dep_with_cipd_variable/_cipd': 'package3/${platform} 1.2',
    })
    self.assertTree(tree)

  def testConvertGitToCipd(self):
    self.gclient(['config', self.git_base + 'repo_13', '--name', 'src'])

    # repo_13@1 has src/repo12 as a git dependency.
    self.gclient(
        ['sync', '-v', '-v', '-v', '--revision', self.githash('repo_13', 1)])

    tree = self.mangle_git_tree(('repo_13@1', 'src'),
                                ('repo_12@1', 'src/repo12'))
    self.assertTree(tree)

    # repo_13@3 has src/repo12 as a cipd dependency.
    self.gclient(
        ['sync', '-v', '-v', '-v', '--revision', self.githash('repo_13', 3),
         '--delete_unversioned_trees'])

    tree = self.mangle_git_tree(('repo_13@3', 'src'))
    tree.update({
        '_cipd': '\n'.join([
            '$ParanoidMode CheckPresence',
            '$OverrideInstallMode copy',
            '',
            '@Subdir src/repo12',
            'foo 1.3',
            '',
            '',
        ]),
        'src/repo12/_cipd': 'foo 1.3',
    })
    self.assertTree(tree)

  def testConvertCipdToGit(self):
    self.gclient(['config', self.git_base + 'repo_13', '--name', 'src'])

    # repo_13@3 has src/repo12 as a cipd dependency.
    self.gclient(
        ['sync', '-v', '-v', '-v', '--revision', self.githash('repo_13', 3),
         '--delete_unversioned_trees'])

    tree = self.mangle_git_tree(('repo_13@3', 'src'))
    tree.update({
        '_cipd': '\n'.join([
            '$ParanoidMode CheckPresence',
            '$OverrideInstallMode copy',
            '',
            '@Subdir src/repo12',
            'foo 1.3',
            '',
            '',
        ]),
        'src/repo12/_cipd': 'foo 1.3',
    })
    self.assertTree(tree)

    # repo_13@1 has src/repo12 as a git dependency.
    self.gclient(
        ['sync', '-v', '-v', '-v', '--revision', self.githash('repo_13', 1)])

    tree = self.mangle_git_tree(('repo_13@1', 'src'),
                                ('repo_12@1', 'src/repo12'))
    tree.update({
        '_cipd': '\n'.join([
            '$ParanoidMode CheckPresence',
            '$OverrideInstallMode copy',
            '',
            '@Subdir src/repo12',
            'foo 1.3',
            '',
            '',
        ]),
        'src/repo12/_cipd': 'foo 1.3',
    })
    self.assertTree(tree)


if __name__ == '__main__':
  if '-v' in sys.argv:
    logging.basicConfig(level=logging.DEBUG)
  unittest.main()
