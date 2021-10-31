# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections

from webkitpy.common.checkout.git_mock import MockGit
from webkitpy.common.host_mock import MockHost
from webkitpy.common.system.executive_mock import MockExecutive
from webkitpy.common.system.log_testing import LoggingTestCase
from webkitpy.w3c.test_importer import TestImporter


MockChromiumCommit = collections.namedtuple('ChromiumCommit', ('sha', 'position'))


class TestImporterTest(LoggingTestCase):

    def test_abort_on_exportable_commits(self):
        importer = TestImporter(MockHost())
        importer.exportable_but_not_exported_commits = lambda _: [
            MockChromiumCommit(sha='deadbeef', position=123)]
        importer.checkout_is_okay = lambda _: True
        return_code = importer.main(['wpt'])
        self.assertEqual(return_code, 0)
        self.assertLog([
            'INFO: Cloning repo: https://chromium.googlesource.com/external/w3c/web-platform-tests.git\n',
            'INFO: Local path: /mock-checkout/third_party/WebKit/wpt\n',
            'INFO: There were exportable but not-yet-exported commits:\n',
            'INFO:   https://chromium.googlesource.com/chromium/src/+/deadbeef\n',
            'INFO: Aborting import to prevent clobbering these commits.\n',
            'INFO: Deleting temp repo directory /mock-checkout/third_party/WebKit/wpt.\n',
        ])

    def test_update_test_expectations(self):
        host = MockHost()
        host.filesystem.files['/mock-checkout/third_party/WebKit/LayoutTests/TestExpectations'] = (
            'Bug(test) some/test/a.html [ Failure ]\n'
            'Bug(test) some/test/b.html [ Failure ]\n'
            'Bug(test) some/test/c.html [ Failure ]\n')
        host.filesystem.files['/mock-checkout/third_party/WebKit/LayoutTests/VirtualTestSuites'] = '[]'
        host.filesystem.files['/mock-checkout/third_party/WebKit/LayoutTests/new/a.html'] = ''
        host.filesystem.files['/mock-checkout/third_party/WebKit/LayoutTests/new/b.html'] = ''
        importer = TestImporter(host)
        deleted_tests = ['some/test/b.html']
        renamed_test_pairs = {
            'some/test/a.html': 'new/a.html',
            'some/test/c.html': 'new/c.html',
        }
        importer.update_all_test_expectations_files(deleted_tests, renamed_test_pairs)
        self.assertMultiLineEqual(
            host.filesystem.read_text_file('/mock-checkout/third_party/WebKit/LayoutTests/TestExpectations'),
            ('Bug(test) new/a.html [ Failure ]\n'
             'Bug(test) new/c.html [ Failure ]\n'))

    # Tests for protected methods - pylint: disable=protected-access

    def test_commit_changes(self):
        host = MockHost()
        importer = TestImporter(host)
        importer._has_changes = lambda: True
        importer._commit_changes('dummy message')
        self.assertEqual(
            host.executive.calls,
            [['git', 'commit', '--all', '-F', '-']])

    def test_commit_message(self):
        importer = TestImporter(MockHost())
        self.assertEqual(
            importer._commit_message('aaaa', '1111'),
            'Import 1111\n\n'
            'Using wpt-import in Chromium aaaa.\n\n'
            'NOEXPORT=true')

    def test_cl_description_with_empty_environ(self):
        host = MockHost()
        host.executive = MockExecutive(output='Last commit message\n\n')
        importer = TestImporter(host)
        description = importer._cl_description(directory_owners={})
        self.assertEqual(
            description,
            ('Last commit message\n\n'
             'TBR=qyearsley@chromium.org\n'
             'NOEXPORT=true'))
        self.assertEqual(host.executive.calls, [['git', 'log', '-1', '--format=%B']])

    def test_cl_description_with_environ_variables(self):
        host = MockHost()
        host.executive = MockExecutive(output='Last commit message\n')
        importer = TestImporter(host)
        importer.host.environ['BUILDBOT_MASTERNAME'] = 'my.master'
        importer.host.environ['BUILDBOT_BUILDERNAME'] = 'b'
        importer.host.environ['BUILDBOT_BUILDNUMBER'] = '123'
        description = importer._cl_description(directory_owners={})
        self.assertEqual(
            description,
            ('Last commit message\n'
             'Build: https://build.chromium.org/p/my.master/builders/b/builds/123\n\n'
             'TBR=qyearsley@chromium.org\n'
             'NOEXPORT=true'))
        self.assertEqual(host.executive.calls, [['git', 'log', '-1', '--format=%B']])

    def test_cl_description_moves_noexport_tag(self):
        host = MockHost()
        host.executive = MockExecutive(output='Summary\n\nNOEXPORT=true\n\n')
        importer = TestImporter(host)
        description = importer._cl_description(directory_owners={})
        self.assertEqual(
            description,
            ('Summary\n\n'
             'TBR=qyearsley@chromium.org\n'
             'NOEXPORT=true'))

    def test_cl_description_with_directory_owners(self):
        host = MockHost()
        host.executive = MockExecutive(output='Last commit message\n\n')
        importer = TestImporter(host)
        description = importer._cl_description(directory_owners={
            ('someone@chromium.org',): ['external/wpt/foo', 'external/wpt/bar'],
            ('x@chromium.org', 'y@chromium.org'): ['external/wpt/baz'],
        })
        self.assertEqual(
            description,
            ('Last commit message\n\n'
             'Directory owners for changes in this CL:\n'
             'someone@chromium.org:\n'
             '  external/wpt/foo\n'
             '  external/wpt/bar\n'
             'x@chromium.org, y@chromium.org:\n'
             '  external/wpt/baz\n\n'
             'TBR=qyearsley@chromium.org\n'
             'NOEXPORT=true'))

    def test_generate_manifest_command_not_found(self):
        # If we're updating csswg-test, then the manifest file won't be found.
        host = MockHost()
        host.filesystem.files = {}
        importer = TestImporter(host)
        importer._generate_manifest(
            '/mock-checkout/third_party/WebKit/LayoutTests/external/csswg-test')
        self.assertEqual(host.executive.calls, [])

    def test_generate_manifest_successful_run(self):
        # This test doesn't test any aspect of the real manifest script, it just
        # asserts that TestImporter._generate_manifest would invoke the script.
        host = MockHost()
        importer = TestImporter(host)
        blink_path = '/mock-checkout/third_party/WebKit'
        host.filesystem.write_text_file(blink_path + '/LayoutTests/external/wpt/MANIFEST.json', '{}')
        importer._generate_manifest(blink_path + '/LayoutTests/external/wpt')
        self.assertEqual(
            host.executive.calls,
            [
                [
                    'python',
                    blink_path + '/Tools/Scripts/webkitpy/thirdparty/wpt/wpt/manifest',
                    '--work',
                    '--tests-root',
                    blink_path + '/LayoutTests/external/wpt',
                ],
                [
                    'git',
                    'add',
                    blink_path + '/LayoutTests/external/WPT_BASE_MANIFEST.json',
                ]
            ])

    def test_get_directory_owners(self):
        host = MockHost()
        host.filesystem.write_text_file(
            '/mock-checkout/third_party/WebKit/LayoutTests/W3CImportExpectations',
            '## Owners: someone@chromium.org\n'
            '# external/wpt/foo [ Pass ]\n')
        git = MockGit()
        git.changed_files = lambda: ['third_party/WebKit/LayoutTests/external/wpt/foo/x.html']
        host.git = lambda: git
        importer = TestImporter(host)
        self.assertEqual(importer.get_directory_owners(), {('someone@chromium.org',): ['external/wpt/foo']})

    def test_get_directory_owners_no_changed_files(self):
        host = MockHost()
        host.filesystem.write_text_file(
            '/mock-checkout/third_party/WebKit/LayoutTests/W3CImportExpectations',
            '## Owners: someone@chromium.org\n'
            '# external/wpt/foo [ Pass ]\n')
        importer = TestImporter(host)
        self.assertEqual(importer.get_directory_owners(), {})

    def test_cc_part(self):
        directory_owners = {
            ('someone@chromium.org',): ['external/wpt/foo', 'external/wpt/bar'],
            ('x@chromium.org', 'y@chromium.org'): ['external/wpt/baz'],
        }
        self.assertEqual(
            TestImporter._cc_part(directory_owners),
            ['--cc=someone@chromium.org', '--cc=x@chromium.org', '--cc=y@chromium.org'])

    def test_delete_orphaned_baselines(self):
        host = MockHost()
        dest_path = '/mock-checkout/third_party/WebKit/LayoutTests/external/wpt'
        host.filesystem.write_text_file(dest_path + '/b-expected.txt', '')
        host.filesystem.write_text_file(dest_path + '/b.x-expected.txt', '')
        host.filesystem.write_text_file(dest_path + '/b.x.html', '')
        importer = TestImporter(host)
        importer._delete_orphaned_baselines(dest_path)
        self.assertFalse(host.filesystem.exists(dest_path + '/b-expected.txt'))
        self.assertTrue(host.filesystem.exists(dest_path + '/b.x-expected.txt'))
        self.assertTrue(host.filesystem.exists(dest_path + '/b.x.html'))
