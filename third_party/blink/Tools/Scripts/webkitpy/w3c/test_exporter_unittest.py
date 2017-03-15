# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from webkitpy.common.host_mock import MockHost
from webkitpy.common.system.executive_mock import MockExecutive
from webkitpy.w3c.test_exporter import TestExporter
from webkitpy.w3c.wpt_github_mock import MockWPTGitHub


class TestExporterTest(unittest.TestCase):

    def setUp(self):
        self.host = MockHost()

    def test_stops_if_more_than_one_pr_is_in_flight(self):
        host = MockHost()
        test_exporter = TestExporter(host, 'gh-username', 'gh-token')
        test_exporter.wpt_github = MockWPTGitHub(pull_requests=[{'id': 1}, {'id': 2}])

        # TODO: make Exception more specific
        with self.assertRaises(Exception):
            test_exporter.run()

    def test_if_pr_exists_merges_it(self):
        host = MockHost()
        test_exporter = TestExporter(host, 'gh-username', 'gh-token')
        test_exporter.wpt_github = MockWPTGitHub(pull_requests=[{'number': 1, 'title': 'abc'}])
        test_exporter.run()

        self.assertIn('merge_pull_request', test_exporter.wpt_github.calls)

    def test_merge_failure_errors_out(self):
        host = MockHost()
        test_exporter = TestExporter(host, 'gh-username', 'gh-token')
        test_exporter.wpt_github = MockWPTGitHub(pull_requests=[{'number': 1, 'title': 'abc'}],
                                                 unsuccessful_merge=True)

        # TODO: make Exception more specific
        with self.assertRaises(Exception):
            test_exporter.run()

    def test_dry_run_stops_before_creating_pr(self):
        host = MockHost()
        host.executive = MockExecutive(output='beefcafe')
        test_exporter = TestExporter(host, 'gh-username', 'gh-token', dry_run=True)
        test_exporter.wpt_github = MockWPTGitHub(pull_requests=[{'number': 1, 'title': 'abc'}])
        test_exporter.run()

        self.assertEqual(test_exporter.wpt_github.calls, ['in_flight_pull_requests'])

    def test_creates_pull_request_for_earliest_commit(self):
        host = MockHost()

        def mock_command(args):
            canned_git_outputs = {
                'show': 'newer fake text' if 'add087a97844f4b9e307d9a216940582d96db306' in args else 'older fake text',
                'rev-list': 'c881563d734a86f7d9cd57ac509653a61c45c240\nadd087a97844f4b9e307d9a216940582d96db306',
                'footers': 'fake-cr-position',
                'remote': 'github',
                'format-patch': 'fake patch',
                'diff': 'fake patch diff',
                'diff-tree': 'fake\n\files\nchanged',
                'crrev-parse': 'c881563d734a86f7d9cd57ac509653a61c45c240',
            }
            return canned_git_outputs.get(args[1], '')

        host.executive = MockExecutive(run_command_fn=mock_command)
        test_exporter = TestExporter(host, 'gh-username', 'gh-token')
        test_exporter.wpt_github = MockWPTGitHub(pull_requests=[])
        test_exporter.run()

        self.assertEqual(test_exporter.wpt_github.calls, ['in_flight_pull_requests', 'create_pr'])
        self.assertEqual(test_exporter.wpt_github.pull_requests_created,
                         [('chromium-export-try', 'older fake text', 'older fake text')])
