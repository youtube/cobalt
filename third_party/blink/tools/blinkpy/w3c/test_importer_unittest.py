# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import six
import unittest

from blinkpy.common.checkout.git_mock import MockGit
from blinkpy.common.host_mock import MockHost
from blinkpy.common.net.git_cl import TryJobStatus
from blinkpy.common.net.git_cl_mock import MockGitCL
from blinkpy.common.net.network_transaction import NetworkTimeout
from blinkpy.common.net.results_fetcher import Build
from blinkpy.common.path_finder import RELATIVE_WEB_TESTS
from blinkpy.common.system.executive_mock import MockCall
from blinkpy.common.system.executive_mock import MockExecutive
from blinkpy.common.system.log_testing import LoggingTestCase
from blinkpy.w3c.chromium_commit_mock import MockChromiumCommit
from blinkpy.w3c.local_wpt import LocalWPT
from blinkpy.w3c.local_wpt_mock import MockLocalWPT
from blinkpy.w3c.test_importer import TestImporter, ROTATIONS_URL, SHERIFF_EMAIL_FALLBACK, RUBBER_STAMPER_BOT
from blinkpy.w3c.wpt_github_mock import MockWPTGitHub
from blinkpy.w3c.wpt_manifest import BASE_MANIFEST_NAME
from blinkpy.web_tests.port.android import ANDROID_DISABLED_TESTS
from blinkpy.web_tests.builder_list import BuilderList

MOCK_WEB_TESTS = '/mock-checkout/' + RELATIVE_WEB_TESTS
MANIFEST_INSTALL_CMD = [
    'python3', '/mock-checkout/third_party/wpt_tools/wpt/wpt', 'manifest',
    '-v', '--no-download', '--tests-root', MOCK_WEB_TESTS + 'external/wpt'
]


class TestImporterTest(LoggingTestCase):

    def mock_host(self):
        host = MockHost()
        host.builders = BuilderList({
            'cq-builder-a': {
                'port_name': 'linux-trusty',
                'specifiers': ['Trusty', 'Release'],
                'is_try_builder': True,
            },
            'cq-builder-b': {
                'port_name': 'mac-mac12',
                'specifiers': ['Mac12', 'Release'],
                'is_try_builder': True,
            },
            'cq-wpt-builder-c': {
                'port_name': 'linux-trusty',
                'specifiers': ['Trusty', 'Release'],
                'is_try_builder': True,
                'steps': {
                    'wpt_tests_suite (with patch)': {
                        'uses_wptrunner': True,
                    },
                }
            },
            'CI Builder D': {
                'port_name': 'linux-trusty',
                'specifiers': ['Trusty', 'Release'],
            },
        })
        port = host.port_factory.get()
        MANIFEST_INSTALL_CMD[0] = port.python3_command()
        host.filesystem.write_text_file(ANDROID_DISABLED_TESTS, '')
        return host

    @staticmethod
    def _get_test_importer(host, wpt_github=None):
        port = host.port_factory.get()
        return TestImporter(
            host,
            wpt_github=wpt_github,
            wpt_manifests=[port.wpt_manifest('external/wpt')])

    def test_update_expectations_for_cl_no_results(self):
        host = self.mock_host()
        host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'W3CImportExpectations', '')
        importer = self._get_test_importer(host)
        importer.git_cl = MockGitCL(host, time_out=True)
        success = importer.update_expectations_for_cl()
        self.assertFalse(success)
        self.assertLog([
            'INFO: Triggering try jobs for updating expectations.\n',
            'INFO: For rebaselining:\n',
            'INFO:   cq-builder-a\n',
            'INFO:   cq-builder-b\n',
            'INFO: For updating WPT metadata:\n',
            'INFO:   cq-wpt-builder-c\n',
            'ERROR: No initial try job results, aborting.\n',
        ])
        self.assertEqual(importer.git_cl.calls[-1], ['git', 'cl', 'set-close'])

    def test_update_expectations_for_cl_closed_cl(self):
        host = self.mock_host()
        host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'W3CImportExpectations', '')
        importer = self._get_test_importer(host)
        importer.git_cl = MockGitCL(
            host,
            status='closed',
            try_job_results={
                Build('builder-a', 123): TryJobStatus('COMPLETED', 'SUCCESS'),
            })
        success = importer.update_expectations_for_cl()
        self.assertFalse(success)
        self.assertLog([
            'INFO: Triggering try jobs for updating expectations.\n',
            'INFO: For rebaselining:\n',
            'INFO:   cq-builder-a\n',
            'INFO:   cq-builder-b\n',
            'INFO: For updating WPT metadata:\n',
            'INFO:   cq-wpt-builder-c\n',
            'ERROR: The CL was closed, aborting.\n',
        ])

    def test_update_expectations_for_cl_all_jobs_pass(self):
        host = self.mock_host()
        host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'W3CImportExpectations', '')
        importer = self._get_test_importer(host)
        importer.git_cl = MockGitCL(
            host,
            status='lgtm',
            try_job_results={
                Build('builder-a', 123): TryJobStatus('COMPLETED', 'SUCCESS'),
            })
        success = importer.update_expectations_for_cl()
        self.assertLog([
            'INFO: Triggering try jobs for updating expectations.\n',
            'INFO: For rebaselining:\n',
            'INFO:   cq-builder-a\n',
            'INFO:   cq-builder-b\n',
            'INFO: For updating WPT metadata:\n',
            'INFO:   cq-wpt-builder-c\n',
            'INFO: All jobs finished.\n',
        ])
        self.assertTrue(success)

    def test_update_expectations_for_cl_fail_but_no_changes(self):
        host = self.mock_host()
        host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'W3CImportExpectations', '')
        importer = self._get_test_importer(host)
        importer.git_cl = MockGitCL(
            host,
            status='lgtm',
            try_job_results={
                Build('builder-a', 123): TryJobStatus('COMPLETED', 'FAILURE'),
            })
        importer.fetch_new_expectations_and_baselines = lambda: None
        success = importer.update_expectations_for_cl()
        self.assertTrue(success)
        self.assertLog([
            'INFO: Triggering try jobs for updating expectations.\n',
            'INFO: For rebaselining:\n',
            'INFO:   cq-builder-a\n',
            'INFO:   cq-builder-b\n',
            'INFO: For updating WPT metadata:\n',
            'INFO:   cq-wpt-builder-c\n',
            'INFO: All jobs finished.\n',
            'INFO: Output of update-metadata:\n',
            'INFO:   update-metadata: MOCK output of child process\n',
            'INFO: -- end of update-metadata output --\n',
        ])
        self.assertIn([
            'python', '/mock-checkout/third_party/blink/tools/blink_tool.py',
            'update-metadata', '--no-trigger-jobs'
        ], host.executive.calls)

    def test_run_commit_queue_for_cl_pass(self):
        host = self.mock_host()
        host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'W3CImportExpectations', '')
        importer = self._get_test_importer(host)
        # Only the latest job for each builder is counted.
        importer.git_cl = MockGitCL(
            host,
            status='lgtm',
            try_job_results={
                Build('cq-builder-a', 120): TryJobStatus(
                    'COMPLETED', 'FAILURE'),
                Build('cq-builder-a', 123): TryJobStatus(
                    'COMPLETED', 'SUCCESS'),
            })
        success = importer.run_commit_queue_for_cl()
        self.assertTrue(success)
        self.assertLog([
            'INFO: Triggering CQ try jobs.\n',
            'INFO: All jobs finished.\n',
            'INFO: CQ appears to have passed; sending to the rubber-stamper '
            'bot for CR+1 and commit.\n',
            'INFO: If the rubber-stamper bot rejects the CL, you either need '
            'to modify the benign file patterns, or manually CR+1 and land the '
            'import yourself if it touches code files. See https://chromium.'
            'googlesource.com/infra/infra/+/refs/heads/main/go/src/infra/'
            'appengine/rubber-stamper/README.md\n',
            'INFO: Update completed.\n',
        ])
        self.assertEqual(importer.git_cl.calls, [
            ['git', 'cl', 'try'],
            [
                'git', 'cl', 'upload', '-f', '--send-mail',
                '--enable-auto-submit', '--reviewers', RUBBER_STAMPER_BOT
            ],
        ])

    def test_run_commit_queue_for_cl_fail_cq(self):
        host = self.mock_host()
        host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'W3CImportExpectations', '')
        importer = self._get_test_importer(host)
        importer.git_cl = MockGitCL(
            host,
            status='lgtm',
            try_job_results={
                Build('cq-builder-a', 120): TryJobStatus(
                    'COMPLETED', 'SUCCESS'),
                Build('cq-builder-a', 123): TryJobStatus(
                    'COMPLETED', 'FAILURE'),
                Build('cq-builder-b', 200): TryJobStatus(
                    'COMPLETED', 'SUCCESS'),
            })
        importer.fetch_new_expectations_and_baselines = lambda: None
        success = importer.run_commit_queue_for_cl()
        self.assertFalse(success)
        self.assertLog([
            'INFO: Triggering CQ try jobs.\n',
            'INFO: All jobs finished.\n',
            'ERROR: CQ appears to have failed; aborting.\n',
        ])
        self.assertEqual(importer.git_cl.calls, [
            ['git', 'cl', 'try'],
            ['git', 'cl', 'set-close'],
        ])

    def test_run_commit_queue_for_cl_fail_to_land(self):
        host = self.mock_host()
        host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'W3CImportExpectations', '')
        importer = self._get_test_importer(host)
        # Only the latest job for each builder is counted.
        importer.git_cl = MockGitCL(
            host,
            status='lgtm',
            try_job_results={
                Build('cq-builder-a', 120): TryJobStatus(
                    'COMPLETED', 'FAILURE'),
                Build('cq-builder-a', 123): TryJobStatus(
                    'COMPLETED', 'SUCCESS'),
            })
        importer._need_sheriff_attention = lambda: False
        importer.git_cl.wait_for_closed_status = lambda timeout_seconds: False
        success = importer.run_commit_queue_for_cl()
        self.assertFalse(success)
        self.assertLog([
            'INFO: Triggering CQ try jobs.\n',
            'INFO: All jobs finished.\n',
            'INFO: CQ appears to have passed; sending to the rubber-stamper '
            'bot for CR+1 and commit.\n',
            'INFO: If the rubber-stamper bot rejects the CL, you either need '
            'to modify the benign file patterns, or manually CR+1 and land the '
            'import yourself if it touches code files. See https://chromium.'
            'googlesource.com/infra/infra/+/refs/heads/main/go/src/infra/'
            'appengine/rubber-stamper/README.md\n',
            'ERROR: Cannot submit CL; aborting.\n',
        ])
        self.assertEqual(importer.git_cl.calls, [
            ['git', 'cl', 'try'],
            [
                'git', 'cl', 'upload', '-f', '--send-mail',
                '--enable-auto-submit', '--reviewers', RUBBER_STAMPER_BOT
            ],
            ['git', 'cl', 'set-close'],
        ])

    def test_run_commit_queue_for_cl_closed_cl(self):
        host = self.mock_host()
        host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'W3CImportExpectations', '')
        importer = self._get_test_importer(host)
        importer.git_cl = MockGitCL(
            host,
            status='closed',
            try_job_results={
                Build('cq-builder-a', 120): TryJobStatus(
                    'COMPLETED', 'SUCCESS'),
                Build('cq-builder-b', 200): TryJobStatus(
                    'COMPLETED', 'SUCCESS'),
            })
        success = importer.run_commit_queue_for_cl()
        self.assertFalse(success)
        self.assertLog([
            'INFO: Triggering CQ try jobs.\n',
            'ERROR: The CL was closed; aborting.\n',
        ])
        self.assertEqual(importer.git_cl.calls, [
            ['git', 'cl', 'try'],
        ])

    def test_run_commit_queue_for_cl_timeout(self):
        # This simulates the case where we time out while waiting for try jobs.
        host = self.mock_host()
        importer = self._get_test_importer(host)
        importer.git_cl = MockGitCL(host, time_out=True)
        success = importer.run_commit_queue_for_cl()
        self.assertFalse(success)
        self.assertLog([
            'INFO: Triggering CQ try jobs.\n',
            'ERROR: Timed out waiting for CQ; aborting.\n'
        ])
        self.assertEqual(importer.git_cl.calls,
                         [['git', 'cl', 'try'], ['git', 'cl', 'set-close']])

    def test_submit_cl_timeout_and_already_merged(self):
        # Here we simulate a case where we timeout waiting for the CQ to submit a
        # CL because we miss the notification that it was merged. We then get an
        # error when trying to close the CL because it's already been merged.
        host = self.mock_host()
        host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'W3CImportExpectations', '')
        importer = self._get_test_importer(host)
        # Define some error text that looks like a typical ScriptError.
        git_error_text = (
            'This is a git Script Error\n'
            '...there is usually a stack trace here with some calls\n'
            '...and maybe other calls\n'
            'And finally, there is the exception:\n'
            'GerritError: Conflict: change is merged\n')
        importer.git_cl = MockGitCL(
            host,
            status='lgtm',
            git_error_output={'set-close': git_error_text},
            # Only the latest job for each builder is counted.
            try_job_results={
                Build('cq-builder-a', 120): TryJobStatus(
                    'COMPLETED', 'FAILURE'),
                Build('cq-builder-a', 123): TryJobStatus(
                    'COMPLETED', 'SUCCESS')
            })
        importer._need_sheriff_attention = lambda: False
        importer.git_cl.wait_for_closed_status = lambda timeout_seconds: False
        success = importer.run_commit_queue_for_cl()
        # Since the CL is already merged, we absorb the error and treat it as success.
        self.assertTrue(success)
        self.assertLog([
            'INFO: Triggering CQ try jobs.\n',
            'INFO: All jobs finished.\n',
            'INFO: CQ appears to have passed; sending to the rubber-stamper '
            'bot for CR+1 and commit.\n',
            'INFO: If the rubber-stamper bot rejects the CL, you either need '
            'to modify the benign file patterns, or manually CR+1 and land the '
            'import yourself if it touches code files. See https://chromium.'
            'googlesource.com/infra/infra/+/refs/heads/main/go/src/infra/'
            'appengine/rubber-stamper/README.md\n',
            'ERROR: Cannot submit CL; aborting.\n',
            'ERROR: CL is already merged; treating as success.\n',
        ])
        self.assertEqual(importer.git_cl.calls, [
            ['git', 'cl', 'try'],
            [
                'git', 'cl', 'upload', '-f', '--send-mail',
                '--enable-auto-submit', '--reviewers', RUBBER_STAMPER_BOT
            ],
            ['git', 'cl', 'set-close'],
        ])

    def test_apply_exportable_commits_locally(self):
        # TODO(robertma): Consider using MockLocalWPT.
        host = self.mock_host()
        importer = self._get_test_importer(
            host, wpt_github=MockWPTGitHub(pull_requests=[]))
        importer.wpt_git = MockGit(cwd='/tmp/wpt', executive=host.executive)
        fake_commit = MockChromiumCommit(
            host,
            subject='My fake commit',
            patch=('Fake patch contents...\n'
                   '--- a/' + RELATIVE_WEB_TESTS +
                   'external/wpt/css/css-ui-3/outline-004.html\n'
                   '+++ b/' + RELATIVE_WEB_TESTS +
                   'external/wpt/css/css-ui-3/outline-004.html\n'
                   '@@ -20,7 +20,7 @@\n'
                   '...'))
        importer.exportable_but_not_exported_commits = lambda _: [fake_commit]
        applied = importer.apply_exportable_commits_locally(LocalWPT(host))
        self.assertEqual(applied, [fake_commit])
        # This assertion is implementation details of LocalWPT.apply_patch.
        # TODO(robertma): Move this to local_wpt_unittest.py.
        self.assertEqual(host.executive.full_calls, [
            MockCall(MANIFEST_INSTALL_CMD,
                     kwargs={
                         'input': None,
                         'cwd': None,
                         'env': None
                     }),
            MockCall(
                ['git', 'apply', '-'], {
                    'input': ('Fake patch contents...\n'
                              '--- a/css/css-ui-3/outline-004.html\n'
                              '+++ b/css/css-ui-3/outline-004.html\n'
                              '@@ -20,7 +20,7 @@\n'
                              '...'),
                    'cwd':
                    '/tmp/wpt',
                    'env':
                    None
                }),
            MockCall(['git', 'add', '.'],
                     kwargs={
                         'input': None,
                         'cwd': '/tmp/wpt',
                         'env': None
                     })
        ])
        self.assertEqual(
            importer.wpt_git.local_commits(),
            [['Applying patch 14fd77e88e42147c57935c49d9e3b2412b8491b7']])

    def test_apply_exportable_commits_locally_returns_none_on_failure(self):
        host = self.mock_host()
        wpt_github = MockWPTGitHub(pull_requests=[])
        importer = self._get_test_importer(host, wpt_github=wpt_github)
        commit = MockChromiumCommit(host, subject='My fake commit')
        importer.exportable_but_not_exported_commits = lambda _: [commit]
        # Failure to apply patch.
        local_wpt = MockLocalWPT(apply_patch=['Failed'])
        applied = importer.apply_exportable_commits_locally(local_wpt)
        self.assertIsNone(applied)

    def test_get_directory_owners(self):
        host = self.mock_host()
        host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'W3CImportExpectations', '')
        host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'external/wpt/foo/OWNERS',
            'someone@chromium.org\n')
        importer = self._get_test_importer(host)
        importer.chromium_git.changed_files = lambda: [RELATIVE_WEB_TESTS + 'external/wpt/foo/x.html']
        self.assertEqual(importer.get_directory_owners(),
                         {('someone@chromium.org', ): ['external/wpt/foo']})

    def test_get_directory_owners_no_changed_files(self):
        host = self.mock_host()
        host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'W3CImportExpectations', '')
        host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'external/wpt/foo/OWNERS',
            'someone@chromium.org\n')
        importer = self._get_test_importer(host)
        self.assertEqual(importer.get_directory_owners(), {})

    # Tests for protected methods - pylint: disable=protected-access

    def test_commit_changes(self):
        host = self.mock_host()
        importer = self._get_test_importer(host)
        importer._commit_changes('dummy message')
        self.assertEqual(importer.chromium_git.local_commits(),
                         [['dummy message']])

    def test_commit_message(self):
        importer = self._get_test_importer(self.mock_host())
        self.assertEqual(
            importer._commit_message('aaaa', '1111'), 'Import 1111\n\n'
            'Using wpt-import in Chromium aaaa.\n\n'
            'No-Export: true')

    def test_cl_description_with_empty_environ(self):
        host = self.mock_host()
        host.executive = MockExecutive(output='Last commit message\n\n')
        importer = self._get_test_importer(host)
        description = importer._cl_description(directory_owners={})
        self.assertEqual(
            description, 'Last commit message\n\n'
            'Note to sheriffs: This CL imports external tests and adds\n'
            'expectations for those tests; if this CL is large and causes\n'
            'a few new failures, please fix the failures by adding new\n'
            'lines to TestExpectations rather than reverting. See:\n'
            'https://chromium.googlesource.com'
            '/chromium/src/+/main/docs/testing/web_platform_tests.md\n\n'
            'NOAUTOREVERT=true\n'
            'No-Export: true\n'
            'Cq-Include-Trybots: luci.chromium.try:linux-wpt-identity-fyi-rel,'
            'linux-wpt-input-fyi-rel,linux-blink-rel')
        print(host.executive.calls)
        self.assertEqual(host.executive.calls,
                         [MANIFEST_INSTALL_CMD] +
                         [['git', 'log', '-1', '--format=%B']])

    def test_cl_description_moves_noexport_tag(self):
        host = self.mock_host()
        host.executive = MockExecutive(output='Summary\n\nNo-Export: true\n\n')
        importer = self._get_test_importer(host)
        description = importer._cl_description(directory_owners={})
        self.assertIn('No-Export: true', description)

    def test_cl_description_with_directory_owners(self):
        host = self.mock_host()
        host.executive = MockExecutive(output='Last commit message\n\n')
        importer = self._get_test_importer(host)
        description = importer._cl_description(
            directory_owners={
                ('someone@chromium.org', ):
                ['external/wpt/foo', 'external/wpt/bar'],
                ('x@chromium.org', 'y@chromium.org'): ['external/wpt/baz'],
            })
        self.assertIn(
            'Directory owners for changes in this CL:\n'
            'someone@chromium.org:\n'
            '  external/wpt/foo\n'
            '  external/wpt/bar\n'
            'x@chromium.org, y@chromium.org:\n'
            '  external/wpt/baz\n\n', description)

    def test_sheriff_email_no_response_uses_backup(self):
        host = self.mock_host()
        importer = self._get_test_importer(host)
        self.assertEqual(SHERIFF_EMAIL_FALLBACK, importer.sheriff_email())
        if six.PY3:
            self.assertLog([
                'ERROR: Exception while fetching current sheriff: '
                'Expecting value: line 1 column 1 (char 0)\n'
            ])
        else:
            self.assertLog([
                'ERROR: Exception while fetching current sheriff: '
                'No JSON object could be decoded\n'
            ])

    def test_sheriff_email_no_emails_field(self):
        host = self.mock_host()
        host.web.urls[ROTATIONS_URL] = json.dumps(
            {'updated_unix_timestamp': '1591108191'})
        importer = self._get_test_importer(host)
        self.assertEqual(SHERIFF_EMAIL_FALLBACK, importer.sheriff_email())
        self.assertLog([
            'ERROR: No email found for current sheriff. Retrieved content: %s\n'
            % host.web.urls[ROTATIONS_URL]
        ])

    def test_sheriff_email_nobody_on_rotation(self):
        host = self.mock_host()
        host.web.urls[ROTATIONS_URL] = json.dumps({
            'emails': [],
            'updated_unix_timestamp':
            '1591108191'
        })
        importer = self._get_test_importer(host)
        self.assertEqual(SHERIFF_EMAIL_FALLBACK, importer.sheriff_email())
        self.assertLog([
            'ERROR: No email found for current sheriff. Retrieved content: %s\n'
            % host.web.urls[ROTATIONS_URL]
        ])

    def test_sheriff_email_rotations_url_unavailable(self):
        def raise_exception(*_):
            raise NetworkTimeout

        host = self.mock_host()
        host.web.get_binary = raise_exception
        importer = self._get_test_importer(host)
        self.assertEqual(SHERIFF_EMAIL_FALLBACK, importer.sheriff_email())
        self.assertLog(['ERROR: Cannot fetch %s\n' % ROTATIONS_URL])

    def test_sheriff_email(self):
        host = self.mock_host()
        host.web.urls[ROTATIONS_URL] = json.dumps({
            'emails': ['current-sheriff@chromium.org'],
            'updated_unix_timestamp':
            '1591108191',
        })
        importer = self._get_test_importer(host)
        self.assertEqual('current-sheriff@chromium.org',
                         importer.sheriff_email())
        self.assertLog([])

    def test_generate_manifest_successful_run(self):
        # This test doesn't test any aspect of the real manifest script, it just
        # asserts that TestImporter._generate_manifest would invoke the script.
        host = self.mock_host()
        importer = self._get_test_importer(host)
        host.filesystem.write_text_file(
            MOCK_WEB_TESTS + 'external/wpt/MANIFEST.json', '{}')
        importer._generate_manifest()
        self.assertEqual(host.executive.calls, [MANIFEST_INSTALL_CMD] * 2)
        self.assertEqual(importer.chromium_git.added_paths,
                         {MOCK_WEB_TESTS + 'external/' + BASE_MANIFEST_NAME})

    def test_has_wpt_changes(self):
        host = self.mock_host()
        importer = self._get_test_importer(host)
        importer.chromium_git.changed_files = lambda: [
            RELATIVE_WEB_TESTS + 'external/' + BASE_MANIFEST_NAME,
            RELATIVE_WEB_TESTS + 'external/wpt/foo/x.html']
        self.assertTrue(importer._has_wpt_changes())

        importer.chromium_git.changed_files = lambda: [
            RELATIVE_WEB_TESTS + 'external/' + BASE_MANIFEST_NAME,
            RELATIVE_WEB_TESTS + 'TestExpectations']
        self.assertFalse(importer._has_wpt_changes())

        importer.chromium_git.changed_files = lambda: [
            RELATIVE_WEB_TESTS + 'external/' + BASE_MANIFEST_NAME]
        self.assertFalse(importer._has_wpt_changes())

    def test_need_sheriff_attention(self):
        host = self.mock_host()
        importer = self._get_test_importer(host)
        importer.chromium_git.changed_files = lambda: [
            RELATIVE_WEB_TESTS + 'external/' + BASE_MANIFEST_NAME,
            RELATIVE_WEB_TESTS + 'external/wpt/foo/x.html']
        self.assertFalse(importer._need_sheriff_attention())

        importer.chromium_git.changed_files = lambda: [
            RELATIVE_WEB_TESTS + 'external/' + BASE_MANIFEST_NAME,
            RELATIVE_WEB_TESTS + 'external/wpt/foo/x.html',
            RELATIVE_WEB_TESTS + 'external/wpt/foo/y.sh']
        self.assertTrue(importer._need_sheriff_attention())

        importer.chromium_git.changed_files = lambda: [
            RELATIVE_WEB_TESTS + 'external/' + BASE_MANIFEST_NAME,
            RELATIVE_WEB_TESTS + 'external/wpt/foo/x.html',
            RELATIVE_WEB_TESTS + 'external/wpt/foo/y.py']
        self.assertTrue(importer._need_sheriff_attention())

        importer.chromium_git.changed_files = lambda: [
            RELATIVE_WEB_TESTS + 'external/' + BASE_MANIFEST_NAME,
            RELATIVE_WEB_TESTS + 'external/wpt/foo/x.html',
            RELATIVE_WEB_TESTS + 'external/wpt/foo/y.bat']
        self.assertTrue(importer._need_sheriff_attention())

    # TODO(crbug.com/800570): Fix orphan baseline finding in the presence of
    # variant tests.
    @unittest.skip('Finding orphaned baselines is broken')
    def test_delete_orphaned_baselines_basic(self):
        host = self.mock_host()
        importer = self._get_test_importer(host)
        dest_path = importer.dest_path
        host.filesystem.write_text_file(
            dest_path + '/MANIFEST.json',
            json.dumps({
                'items': {
                    'testharness': {
                        'a.html': ['abcdef123', [None, {}]],
                    },
                    'manual': {},
                    'reftest': {},
                },
            }))
        host.filesystem.write_text_file(dest_path + '/a.html', '')
        host.filesystem.write_text_file(dest_path + '/a-expected.txt', '')
        host.filesystem.write_text_file(dest_path + '/orphaned-expected.txt',
                                        '')
        importer._delete_orphaned_baselines()
        self.assertFalse(
            host.filesystem.exists(dest_path + '/orphaned-expected.txt'))
        self.assertTrue(host.filesystem.exists(dest_path + '/a-expected.txt'))

    # TODO(crbug.com/800570): Fix orphan baseline finding in the presence of
    # variant tests.
    @unittest.skip('Finding orphaned baselines is broken')
    def test_delete_orphaned_baselines_worker_js_tests(self):
        # This test checks that baselines for existing tests shouldn't be
        # deleted, even if the test name isn't the same as the file name.
        host = self.mock_host()
        importer = self._get_test_importer(host)
        dest_path = importer.dest_path
        host.filesystem.write_text_file(
            dest_path + '/MANIFEST.json',
            json.dumps({
                'items': {
                    'testharness': {
                        'a.any.js': [
                            'abcdef123',
                            ['a.any.html', {}],
                            ['a.any.worker.html', {}],
                        ],
                        'b.worker.js': ['abcdef123', ['b.worker.html', {}]],
                        'c.html': [
                            'abcdef123',
                            ['c.html?q=1', {}],
                            ['c.html?q=2', {}],
                        ],
                    },
                    'manual': {},
                    'reftest': {},
                },
            }))
        host.filesystem.write_text_file(dest_path + '/a.any.js', '')
        host.filesystem.write_text_file(dest_path + '/a.any-expected.txt', '')
        host.filesystem.write_text_file(
            dest_path + '/a.any.worker-expected.txt', '')
        host.filesystem.write_text_file(dest_path + '/b.worker.js', '')
        host.filesystem.write_text_file(dest_path + '/b.worker-expected.txt',
                                        '')
        host.filesystem.write_text_file(dest_path + '/c.html', '')
        host.filesystem.write_text_file(dest_path + '/c-expected.txt', '')
        importer._delete_orphaned_baselines()
        self.assertTrue(
            host.filesystem.exists(dest_path + '/a.any-expected.txt'))
        self.assertTrue(
            host.filesystem.exists(dest_path + '/a.any.worker-expected.txt'))
        self.assertTrue(
            host.filesystem.exists(dest_path + '/b.worker-expected.txt'))
        self.assertTrue(host.filesystem.exists(dest_path + '/c-expected.txt'))

    def test_clear_out_dest_path(self):
        host = self.mock_host()
        importer = self._get_test_importer(host)
        dest_path = importer.dest_path
        host.filesystem.write_text_file(dest_path + '/foo-test.html', '')
        host.filesystem.write_text_file(dest_path + '/foo-test-expected.txt',
                                        '')
        host.filesystem.write_text_file(dest_path + '/OWNERS', '')
        host.filesystem.write_text_file(dest_path + '/DIR_METADATA', '')
        host.filesystem.write_text_file(dest_path + '/bar/baz/OWNERS', '')
        # When the destination path is cleared, OWNERS files and baselines
        # are kept.
        importer._clear_out_dest_path()
        self.assertFalse(host.filesystem.exists(dest_path + '/foo-test.html'))
        self.assertTrue(
            host.filesystem.exists(dest_path + '/foo-test-expected.txt'))
        self.assertTrue(host.filesystem.exists(dest_path + '/OWNERS'))
        self.assertTrue(host.filesystem.exists(dest_path + '/DIR_METADATA'))
        self.assertTrue(host.filesystem.exists(dest_path + '/bar/baz/OWNERS'))
