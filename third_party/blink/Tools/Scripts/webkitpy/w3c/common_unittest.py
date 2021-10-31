# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from webkitpy.common.host_mock import MockHost
from webkitpy.common.system.executive_mock import mock_git_commands
from webkitpy.w3c.chromium_commit import ChromiumCommit
from webkitpy.w3c.common import exportable_commits_since


class MockLocalWPT(object):

    def test_patch(self, patch, chromium_commit):  # pylint: disable=unused-argument
        return 'patch'


class CommonTest(unittest.TestCase):

    def test_exportable_commits_since(self):
        host = MockHost()
        host.executive = mock_git_commands({
            'show': 'fake message',
            'rev-list': 'add087a97844f4b9e307d9a216940582d96db306',
            'rev-parse': 'add087a97844f4b9e307d9a216940582d96db306',
            'crrev-parse': 'add087a97844f4b9e307d9a216940582d96db306',
            'diff': 'fake diff',
            'diff-tree': 'some\nfiles',
            'format-patch': 'hey I\'m a patch',
            'footers': 'cr-rev-position',
        })

        commits = exportable_commits_since('beefcafe', host, MockLocalWPT())
        self.assertEqual(len(commits), 1)
        self.assertIsInstance(commits[0], ChromiumCommit)
        self.assertEqual(host.executive.calls, [
            ['git', 'rev-parse', '--show-toplevel'],
            ['git', 'rev-list', 'beefcafe..HEAD', '--reverse', '--',
             'add087a97844f4b9e307d9a216940582d96db306/third_party/WebKit/LayoutTests/external/wpt/'],
            ['git', 'diff-tree', '--name-only', '--no-commit-id', '-r', 'add087a97844f4b9e307d9a216940582d96db306', '--',
             '/mock-checkout/third_party/WebKit/LayoutTests/external/wpt'],
            ['git', 'format-patch', '-1', '--stdout', 'add087a97844f4b9e307d9a216940582d96db306', '--', 'some', 'files'],
            ['git', 'show', '--format=%B', '--no-patch', 'add087a97844f4b9e307d9a216940582d96db306'],
            ['git', 'show', '--format=%B', '--no-patch', 'add087a97844f4b9e307d9a216940582d96db306']
        ])

    def test_ignores_commits_with_noexport_true(self):
        host = MockHost()
        host.executive = mock_git_commands({
            'show': 'Commit message\nNOEXPORT=true',
            'rev-list': 'add087a97844f4b9e307d9a216940582d96db306',
            'rev-parse': 'add087a97844f4b9e307d9a216940582d96db306',
            'footers': 'cr-rev-position',
        })

        commits = exportable_commits_since('add087a97844f4b9e307d9a216940582d96db306', host, MockLocalWPT())
        self.assertEqual(commits, [])
        self.assertEqual(host.executive.calls, [
            ['git', 'rev-parse', '--show-toplevel'],
            ['git', 'rev-list', 'add087a97844f4b9e307d9a216940582d96db306..HEAD', '--reverse', '--',
             'add087a97844f4b9e307d9a216940582d96db306/third_party/WebKit/LayoutTests/external/wpt/'],
            ['git', 'diff-tree', '--name-only', '--no-commit-id', '-r', 'add087a97844f4b9e307d9a216940582d96db306', '--',
             '/mock-checkout/third_party/WebKit/LayoutTests/external/wpt'],
            ['git', 'show', '--format=%B', '--no-patch', 'add087a97844f4b9e307d9a216940582d96db306']
        ])

    def test_ignores_reverted_commits_with_noexport_true(self):
        host = MockHost()
        host.executive = mock_git_commands({
            'show': 'Commit message\n> NOEXPORT=true',
            'rev-list': 'add087a97844f4b9e307d9a216940582d96db306',
            'rev-parse': 'add087a97844f4b9e307d9a216940582d96db306',
            'footers': 'cr-rev-position',
            'diff-tree': '',
        }, strict=True)

        commits = exportable_commits_since('add087a97844f4b9e307d9a216940582d96db306', host, MockLocalWPT())
        self.assertEqual(len(commits), 0)
        self.assertEqual(host.executive.calls, [
            ['git', 'rev-parse', '--show-toplevel'],
            ['git', 'rev-list', 'add087a97844f4b9e307d9a216940582d96db306..HEAD', '--reverse', '--',
             'add087a97844f4b9e307d9a216940582d96db306/third_party/WebKit/LayoutTests/external/wpt/'],
            ['git', 'diff-tree', '--name-only', '--no-commit-id', '-r', 'add087a97844f4b9e307d9a216940582d96db306', '--',
             '/mock-checkout/third_party/WebKit/LayoutTests/external/wpt'],
            ['git', 'show', '--format=%B', '--no-patch', 'add087a97844f4b9e307d9a216940582d96db306']
        ])

    def test_ignores_commits_that_start_with_import(self):
        host = MockHost()
        host.executive = mock_git_commands({
            'show': 'Import rutabaga@deadbeef',
            'rev-list': 'add087a97844f4b9e307d9a216940582d96db306',
            'rev-parse': 'add087a97844f4b9e307d9a216940582d96db306',
            'footers': 'cr-rev-position',
        })

        commits = exportable_commits_since('beefcafe', host, MockLocalWPT())
        self.assertEqual(commits, [])
        self.assertEqual(host.executive.calls, [
            ['git', 'rev-parse', '--show-toplevel'],
            ['git', 'rev-list', 'beefcafe..HEAD', '--reverse', '--',
             'add087a97844f4b9e307d9a216940582d96db306/third_party/WebKit/LayoutTests/external/wpt/'],
            ['git', 'diff-tree', '--name-only', '--no-commit-id', '-r', 'add087a97844f4b9e307d9a216940582d96db306', '--',
             '/mock-checkout/third_party/WebKit/LayoutTests/external/wpt'],
            ['git', 'show', '--format=%B', '--no-patch', 'add087a97844f4b9e307d9a216940582d96db306'],
            ['git', 'show', '--format=%B', '--no-patch', 'add087a97844f4b9e307d9a216940582d96db306'],
        ])
