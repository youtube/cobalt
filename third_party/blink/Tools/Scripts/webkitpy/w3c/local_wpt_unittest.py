# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from webkitpy.common.host_mock import MockHost
from webkitpy.common.system.executive_mock import MockExecutive, mock_git_commands
from webkitpy.common.system.filesystem_mock import MockFileSystem
from webkitpy.w3c.local_wpt import LocalWPT


class LocalWPTTest(unittest.TestCase):

    def test_fetch_requires_gh_token(self):
        host = MockHost()
        local_wpt = LocalWPT(host)

        with self.assertRaises(AssertionError):
            local_wpt.fetch()

    def test_fetch_when_wpt_dir_exists(self):
        host = MockHost()
        host.filesystem = MockFileSystem(files={
            '/tmp/wpt': ''
        })

        local_wpt = LocalWPT(host, 'token')
        local_wpt.fetch()

        self.assertEqual(host.executive.calls, [
            ['git', 'fetch', 'origin'],
            ['git', 'checkout', 'origin/master'],
        ])

    def test_fetch_when_wpt_dir_does_not_exist(self):
        host = MockHost()
        host.filesystem = MockFileSystem()

        local_wpt = LocalWPT(host, 'token')
        local_wpt.fetch()

        self.assertEqual(host.executive.calls, [
            ['git', 'clone', 'https://token@github.com/w3c/web-platform-tests.git', '/tmp/wpt'],
        ])

    def test_constructor(self):
        host = MockHost()
        LocalWPT(host, 'token')
        self.assertEqual(len(host.executive.calls), 0)

    def test_run(self):
        host = MockHost()
        host.filesystem = MockFileSystem()
        local_wpt = LocalWPT(host, 'token')
        local_wpt.run(['echo', 'rutabaga'])
        self.assertEqual(host.executive.calls, [['echo', 'rutabaga']])

    def test_last_wpt_exported_commit(self):
        host = MockHost()
        host.executive = mock_git_commands({
            'rev-list': '9ea4fc353a4b1c11c6e524270b11baa4d1ddfde8',
            'footers': 'Cr-Commit-Position: 123',
            'crrev-parse': 'add087a97844f4b9e307d9a216940582d96db306',
        }, strict=True)
        host.filesystem = MockFileSystem()
        local_wpt = LocalWPT(host, 'token')

        wpt_sha, chromium_commit = local_wpt.most_recent_chromium_commit()
        self.assertEqual(wpt_sha, '9ea4fc353a4b1c11c6e524270b11baa4d1ddfde8')
        self.assertEqual(chromium_commit.position, '123')
        self.assertEqual(chromium_commit.sha, 'add087a97844f4b9e307d9a216940582d96db306')

    def test_last_wpt_exported_commit_not_found(self):
        host = MockHost()
        host.executive = MockExecutive(run_command_fn=lambda _: '')
        host.filesystem = MockFileSystem()
        local_wpt = LocalWPT(host, 'token')

        commit = local_wpt.most_recent_chromium_commit()
        self.assertEqual(commit, (None, None))

    def test_create_branch_with_patch(self):
        host = MockHost()
        host.filesystem = MockFileSystem()

        local_wpt = LocalWPT(host, 'token')
        local_wpt.fetch()

        local_branch_name = local_wpt.create_branch_with_patch('message', 'patch', 'author')
        self.assertEqual(local_branch_name, 'chromium-export-try')
        self.assertEqual(host.executive.calls, [
            ['git', 'clone', 'https://token@github.com/w3c/web-platform-tests.git', '/tmp/wpt'],
            ['git', 'reset', '--hard', 'HEAD'],
            ['git', 'clean', '-fdx'],
            ['git', 'checkout', 'origin/master'],
            ['git', 'branch', '-a'],
            ['git', 'branch', '-a'],
            ['git', 'checkout', '-b', 'chromium-export-try'],
            ['git', 'apply', '-'],
            ['git', 'add', '.'],
            ['git', 'commit', '--author', 'author', '-am', 'message'],
            ['git', 'push', '-f', 'origin', 'chromium-export-try']])
