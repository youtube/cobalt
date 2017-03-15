# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import base64
import unittest

from webkitpy.common.host_mock import MockHost
from webkitpy.w3c.wpt_github import WPTGitHub


class WPTGitHubTest(unittest.TestCase):

    def test_init(self):
        wpt_github = WPTGitHub(MockHost(), user='rutabaga', token='deadbeefcafe')
        self.assertEqual(wpt_github.user, 'rutabaga')
        self.assertEqual(wpt_github.token, 'deadbeefcafe')

    def test_auth_token(self):
        wpt_github = wpt_github = WPTGitHub(MockHost(), user='rutabaga', token='deadbeefcafe')
        self.assertEqual(
            wpt_github.auth_token(),
            base64.encodestring('rutabaga:deadbeefcafe').strip())
