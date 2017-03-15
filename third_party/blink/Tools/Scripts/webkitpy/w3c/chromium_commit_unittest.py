# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from webkitpy.common.host_mock import MockHost
from webkitpy.common.system.executive_mock import MockExecutive, mock_git_commands
from webkitpy.w3c.chromium_commit import ChromiumCommit

CHROMIUM_WPT_DIR = 'third_party/WebKit/LayoutTests/external/wpt/'


class ChromiumCommitTest(unittest.TestCase):

    def test_accepts_sha(self):
        chromium_commit = ChromiumCommit(MockHost(), sha='c881563d734a86f7d9cd57ac509653a61c45c240')

        self.assertEqual(chromium_commit.sha, 'c881563d734a86f7d9cd57ac509653a61c45c240')
        self.assertIsNone(chromium_commit.position)

    def test_derives_sha_from_position(self):
        host = MockHost()
        host.executive = MockExecutive(output='c881563d734a86f7d9cd57ac509653a61c45c240')
        pos = 'Cr-Commit-Position: refs/heads/master@{#789}'
        chromium_commit = ChromiumCommit(host, position=pos)

        self.assertEqual(chromium_commit.position, 'refs/heads/master@{#789}')
        self.assertEqual(chromium_commit.sha, 'c881563d734a86f7d9cd57ac509653a61c45c240')

    def test_filtered_changed_files_blacklist(self):
        host = MockHost()

        fake_files = ['file1', 'MANIFEST.json', 'file3']
        qualified_fake_files = [CHROMIUM_WPT_DIR + f for f in fake_files]

        host.executive = mock_git_commands({
            'diff-tree': '\n'.join(qualified_fake_files),
            'crrev-parse': 'c881563d734a86f7d9cd57ac509653a61c45c240',
        })

        position_footer = 'Cr-Commit-Position: refs/heads/master@{#789}'
        chromium_commit = ChromiumCommit(host, position=position_footer)

        files = chromium_commit.filtered_changed_files()

        expected_files = ['file1', 'file3']
        qualified_expected_files = [CHROMIUM_WPT_DIR + f for f in expected_files]

        self.assertEqual(files, qualified_expected_files)
