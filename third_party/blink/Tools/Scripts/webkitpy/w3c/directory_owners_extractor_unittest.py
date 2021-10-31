# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from webkitpy.common.system.filesystem_mock import MockFileSystem
from webkitpy.w3c.directory_owners_extractor import DirectoryOwnersExtractor


class DirectoryOwnersExtractorTest(unittest.TestCase):

    def setUp(self):
        self.filesystem = MockFileSystem()
        self.extractor = DirectoryOwnersExtractor(self.filesystem)

    def test_lines_to_owner_map(self):
        lines = [
            'external/wpt/webgl [ Skip ]',
            '## Owners: mek@chromium.org',
            '# external/wpt/webmessaging [ Pass ]',
            '## Owners: hta@chromium.org',
            '# external/wpt/webrtc [ Pass ]',
            'external/wpt/websockets [ Skip ]',
            '## Owners: michaeln@chromium.org,jsbell@chromium.org',
            '# external/wpt/webstorage [ Pass ]',
            'external/wpt/webvtt [ Skip ]',
        ]

        self.assertEqual(
            self.extractor.lines_to_owner_map(lines),
            {
                'external/wpt/webmessaging': ['mek@chromium.org'],
                'external/wpt/webrtc': ['hta@chromium.org'],
                'external/wpt/webstorage': ['michaeln@chromium.org', 'jsbell@chromium.org'],
            })

    def test_list_owners(self):
        self.extractor.owner_map = {
            'external/wpt/foo': ['a@chromium.org', 'c@chromium.org'],
            'external/wpt/bar': ['b@chromium.org'],
            'external/wpt/baz': ['a@chromium.org', 'c@chromium.org'],
        }
        self.filesystem.files = {
            '/mock-checkout/third_party/WebKit/LayoutTests/external/wpt/foo/x/y.html': '',
            '/mock-checkout/third_party/WebKit/LayoutTests/external/wpt/bar/x/y.html': '',
            '/mock-checkout/third_party/WebKit/LayoutTests/external/wpt/baz/x/y.html': '',
            '/mock-checkout/third_party/WebKit/LayoutTests/external/wpt/quux/x/y.html': '',
        }
        changed_files = [
            'third_party/WebKit/LayoutTests/external/wpt/foo/x/y.html',
            'third_party/WebKit/LayoutTests/external/wpt/baz/x/y.html',
            'third_party/WebKit/LayoutTests/external/wpt/quux/x/y.html',
        ]
        self.assertEqual(
            self.extractor.list_owners(changed_files),
            {('a@chromium.org', 'c@chromium.org'): ['external/wpt/foo', 'external/wpt/baz']})

    def test_extract_owner_positive_cases(self):
        self.assertEqual(self.extractor.extract_owners('## Owners: foo@chromium.org'), ['foo@chromium.org'])
        self.assertEqual(self.extractor.extract_owners('# Owners: foo@chromium.org'), ['foo@chromium.org'])
        self.assertEqual(self.extractor.extract_owners('## Owners: a@x.com,b@x.com'), ['a@x.com', 'b@x.com'])
        self.assertEqual(self.extractor.extract_owners('## Owners: a@x.com, b@x.com'), ['a@x.com', 'b@x.com'])
        self.assertEqual(self.extractor.extract_owners('## Owner: foo@chromium.org'), ['foo@chromium.org'])

    def test_extract_owner_negative_cases(self):
        self.assertIsNone(self.extractor.extract_owners(''))
        self.assertIsNone(self.extractor.extract_owners('## Something: foo@chromium.org'))
        self.assertIsNone(self.extractor.extract_owners('## Owners: not an email address'))

    def test_extract_directory_positive_cases(self):
        self.assertEqual(self.extractor.extract_directory('external/a/b [ Pass ]'), 'external/a/b')
        self.assertEqual(self.extractor.extract_directory('# external/c/d [ Pass ]'), 'external/c/d')
        self.assertEqual(self.extractor.extract_directory('# external/e/f [ Skip ]'), 'external/e/f')
        self.assertEqual(self.extractor.extract_directory('# external/g/h/i [ Skip ]'), 'external/g/h/i')

    def test_extract_directory_negative_cases(self):
        self.assertIsNone(self.extractor.extract_directory(''))
        self.assertIsNone(self.extractor.extract_directory('external/a/b [ Skip ]'))
        self.assertIsNone(self.extractor.extract_directory('# some comment'))
