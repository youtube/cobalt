# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from webkitpy.common.host_mock import MockHost
from webkitpy.common.net.buildbot import Build
from webkitpy.common.net.git_cl import GitCL
from webkitpy.common.system.executive_mock import MockExecutive


class GitCLTest(unittest.TestCase):

    def test_run(self):
        host = MockHost()
        host.executive = MockExecutive(output='mock-output')
        git_cl = GitCL(host)
        output = git_cl.run(['command'])
        self.assertEqual(output, 'mock-output')
        self.assertEqual(host.executive.calls, [['git', 'cl', 'command']])

    def test_run_with_auth(self):
        host = MockHost()
        host.executive = MockExecutive(output='mock-output')
        git_cl = GitCL(host, auth_refresh_token_json='token.json')
        git_cl.run(['try', '-b', 'win10_blink_rel'])
        self.assertEqual(
            host.executive.calls,
            [['git', 'cl', 'try', '-b', 'win10_blink_rel', '--auth-refresh-token-json', 'token.json']])

    def test_some_commands_not_run_with_auth(self):
        host = MockHost()
        host.executive = MockExecutive(output='mock-output')
        git_cl = GitCL(host, auth_refresh_token_json='token.json')
        git_cl.run(['issue'])
        self.assertEqual(host.executive.calls, [['git', 'cl', 'issue']])

    def test_get_issue_number(self):
        host = MockHost()
        host.executive = MockExecutive(output='Issue number: 12345 (http://crrev.com/12345)')
        git_cl = GitCL(host)
        self.assertEqual(git_cl.get_issue_number(), '12345')

    def test_get_issue_number_none(self):
        host = MockHost()
        host.executive = MockExecutive(output='Issue number: None (None)')
        git_cl = GitCL(host)
        self.assertEqual(git_cl.get_issue_number(), 'None')

    def test_all_jobs_finished_empty(self):
        self.assertTrue(GitCL.all_jobs_finished([]))

    def test_wait_for_try_jobs_time_out(self):
        host = MockHost()
        git_cl = GitCL(host)
        git_cl.fetch_try_results = lambda: [
            {
                'builder_name': 'some-builder',
                'status': 'STARTED',
                'result': None,
            },
        ]
        git_cl.wait_for_try_jobs()
        self.assertEqual(
            host.stdout.getvalue(),
            'Waiting for try jobs (timeout: 7200 seconds).\n'
            'Waiting. 600 seconds passed.\n'
            'Waiting. 1800 seconds passed.\n'
            'Waiting. 3000 seconds passed.\n'
            'Waiting. 4200 seconds passed.\n'
            'Waiting. 5400 seconds passed.\n'
            'Waiting. 6600 seconds passed.\n'
            'Timed out waiting for try results.\n')

    def test_wait_for_try_jobs_done(self):
        host = MockHost()
        git_cl = GitCL(host)
        git_cl.fetch_try_results = lambda: [
            {
                'builder_name': 'some-builder',
                'status': 'COMPLETED',
                'result': 'FAILURE',
            },
        ]
        git_cl.wait_for_try_jobs()
        self.assertEqual(
            host.stdout.getvalue(),
            'Waiting for try jobs (timeout: 7200 seconds).\n'
            'All jobs finished.\n')

    def test_all_jobs_finished_with_started_jobs(self):
        self.assertFalse(GitCL.all_jobs_finished([
            {
                'builder_name': 'some-builder',
                'status': 'COMPLETED',
                'result': 'FAILURE',
                'url': 'http://build.chromium.org/p/master/builders/some-builder/builds/90',
            },
            {
                'builder_name': 'some-builder',
                'status': 'STARTED',
                'result': None,
                'url': 'http://build.chromium.org/p/master/builders/some-builder/builds/100',
            },
        ]))

    def test_all_jobs_finished_only_completed_jobs(self):
        self.assertTrue(GitCL.all_jobs_finished([
            {
                'builder_name': 'some-builder',
                'status': 'COMPLETED',
                'result': 'FAILURE',
                'url': 'http://build.chromium.org/p/master/builders/some-builder/builds/90',
            },
            {
                'builder_name': 'some-builder',
                'status': 'COMPLETED',
                'result': 'SUCCESS',
                'url': 'http://build.chromium.org/p/master/builders/some-builder/builds/100',
            },
        ]))

    def test_has_failing_try_results_empty(self):
        self.assertFalse(GitCL.has_failing_try_results([]))

    def test_has_failing_try_results_only_success_and_started(self):
        self.assertFalse(GitCL.has_failing_try_results([
            {
                'builder_name': 'some-builder',
                'status': 'COMPLETED',
                'result': 'SUCCESS',
                'url': 'http://build.chromium.org/p/master/builders/some-builder/builds/90',
            },
            {
                'builder_name': 'some-builder',
                'status': 'STARTED',
                'result': None,
                'url': 'http://build.chromium.org/p/master/builders/some-builder/builds/100',
            },
        ]))

    def test_has_failing_try_results_with_failing_results(self):
        self.assertTrue(GitCL.has_failing_try_results([
            {
                'builder_name': 'some-builder',
                'status': 'COMPLETED',
                'result': 'FAILURE',
            },
        ]))

    def test_latest_try_builds(self):
        git_cl = GitCL(MockHost())
        git_cl.fetch_try_results = lambda: [
            {
                'builder_name': 'builder-b',
                'status': 'COMPLETED',
                'result': 'SUCCESS',
                'url': 'http://build.chromium.org/p/master/builders/some-builder/builds/100',
            },
            {
                'builder_name': 'builder-b',
                'status': 'COMPLETED',
                'result': 'SUCCESS',
                'url': 'http://build.chromium.org/p/master/builders/some-builder/builds/90',
            },
            {
                'builder_name': 'builder-a',
                'status': 'SCHEDULED',
                'result': None,
                'url': None,
            },
            {
                'builder_name': 'builder-c',
                'status': 'COMPLETED',
                'result': 'SUCCESS',
                'url': 'http://build.chromium.org/p/master/builders/some-builder/builds/123',
            },
        ]
        self.assertEqual(
            git_cl.latest_try_jobs(['builder-a', 'builder-b']),
            [Build('builder-a'), Build('builder-b', 100)])
