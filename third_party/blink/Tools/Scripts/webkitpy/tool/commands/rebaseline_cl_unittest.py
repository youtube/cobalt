# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import optparse

from webkitpy.common.net.buildbot import Build
from webkitpy.common.net.git_cl import GitCL
from webkitpy.common.checkout.git_mock import MockGit
from webkitpy.common.net.layout_test_results import LayoutTestResults
from webkitpy.common.system.log_testing import LoggingTestCase
from webkitpy.layout_tests.builder_list import BuilderList
from webkitpy.tool.commands.rebaseline_cl import RebaselineCL
from webkitpy.tool.commands.rebaseline_unittest import BaseTestCase


class RebaselineCLTest(BaseTestCase, LoggingTestCase):
    command_constructor = RebaselineCL

    def setUp(self):
        BaseTestCase.setUp(self)
        LoggingTestCase.setUp(self)

        builds = [
            Build('MOCK Try Win', 5000),
            Build('MOCK Try Mac', 4000),
            Build('MOCK Try Linux', 6000),
        ]

        git_cl = GitCL(self.tool)
        git_cl.get_issue_number = lambda: '11112222'
        git_cl.latest_try_jobs = lambda _: builds
        self.command.git_cl = lambda: git_cl

        git = MockGit(filesystem=self.tool.filesystem, executive=self.tool.executive)
        git.changed_files = lambda **_: [
            'third_party/WebKit/LayoutTests/fast/dom/prototype-inheritance.html',
            'third_party/WebKit/LayoutTests/fast/dom/prototype-taco.html',
        ]
        self.tool.git = lambda: git

        self.tool.builders = BuilderList({
            'MOCK Try Win': {
                'port_name': 'test-win-win7',
                'specifiers': ['Win7', 'Release'],
                'is_try_builder': True,
            },
            'MOCK Try Linux': {
                'port_name': 'test-linux-trusty',
                'specifiers': ['Trusty', 'Release'],
                'is_try_builder': True,
            },
            'MOCK Try Mac': {
                'port_name': 'test-mac-mac10.11',
                'specifiers': ['Mac10.11', 'Release'],
                'is_try_builder': True,
            },
        })
        layout_test_results = LayoutTestResults({
            'tests': {
                'fast': {
                    'dom': {
                        'prototype-inheritance.html': {
                            'expected': 'PASS',
                            'actual': 'TEXT',
                            'is_unexpected': True,
                        },
                        'prototype-banana.html': {
                            'expected': 'FAIL',
                            'actual': 'PASS',
                            'is_unexpected': True,
                        },
                        'prototype-taco.html': {
                            'expected': 'PASS',
                            'actual': 'PASS TEXT',
                            'is_unexpected': True,
                        },
                        'prototype-chocolate.html': {
                            'expected': 'FAIL',
                            'actual': 'IMAGE+TEXT'
                        },
                        'prototype-crashy.html': {
                            'expected': 'PASS',
                            'actual': 'CRASH',
                            'is_unexpected': True,
                        },
                        'prototype-newtest.html': {
                            'expected': 'PASS',
                            'actual': 'MISSING',
                            'is_unexpected': True,
                            'is_missing_text': True,
                        },
                        'prototype-slowtest.html': {
                            'expected': 'SLOW',
                            'actual': 'TEXT',
                            'is_unexpected': True,
                        },
                    }
                },
                'svg': {
                    'dynamic-updates': {
                        'SVGFEDropShadowElement-dom-stdDeviation-attr.html': {
                            'expected': 'PASS',
                            'actual': 'IMAGE',
                            'has_stderr': True,
                            'is_unexpected': True,
                        }
                    }
                }
            }
        })

        for build in builds:
            self.tool.buildbot.set_results(build, layout_test_results)
            self.tool.buildbot.set_retry_sumary_json(build, json.dumps({
                'failures': [
                    'fast/dom/prototype-inheritance.html',
                    'fast/dom/prototype-newtest.html',
                    'fast/dom/prototype-slowtest.html',
                    'fast/dom/prototype-taco.html',
                    'svg/dynamic-updates/SVGFEDropShadowElement-dom-stdDeviation-attr.html',
                ],
                'ignored': [],
            }))

        # Write to the mock filesystem so that these tests are considered to exist.
        tests = [
            'fast/dom/prototype-taco.html',
            'fast/dom/prototype-inheritance.html',
            'fast/dom/prototype-newtest.html',
            'svg/dynamic-updates/SVGFEDropShadowElement-dom-stdDeviation-attr.html',
        ]
        for test in tests:
            self._write(self.mac_port.host.filesystem.join(self.mac_port.layout_tests_dir(), test), 'contents')

    def tearDown(self):
        BaseTestCase.tearDown(self)
        LoggingTestCase.tearDown(self)

    @staticmethod
    def command_options(**kwargs):
        options = {
            'only_changed_tests': False,
            'dry_run': False,
            'optimize': True,
            'results_directory': None,
            'verbose': False,
            'trigger_jobs': False,
        }
        options.update(kwargs)
        return optparse.Values(dict(**options))

    def test_execute_with_issue_number_given(self):
        return_code = self.command.execute(self.command_options(), [], self.tool)
        self.assertEqual(return_code, 0)
        self.assertLog([
            'INFO: Rebaselining fast/dom/prototype-inheritance.html\n',
            'INFO: Rebaselining fast/dom/prototype-newtest.html\n',
            'INFO: Rebaselining fast/dom/prototype-slowtest.html\n',
            'INFO: Rebaselining fast/dom/prototype-taco.html\n',
            'INFO: Rebaselining svg/dynamic-updates/SVGFEDropShadowElement-dom-stdDeviation-attr.html\n',
        ])

    def test_execute_with_no_issue_number(self):
        git_cl = GitCL(self.tool)
        git_cl.get_issue_number = lambda: 'None'
        self.command.git_cl = lambda: git_cl
        return_code = self.command.execute(self.command_options(), [], self.tool)
        self.assertEqual(return_code, 1)
        self.assertLog(['ERROR: No issue number for current branch.\n'])

    def test_execute_with_issue_number_from_branch(self):
        return_code = self.command.execute(self.command_options(), [], self.tool)
        self.assertEqual(return_code, 0)
        self.assertLog([
            'INFO: Rebaselining fast/dom/prototype-inheritance.html\n',
            'INFO: Rebaselining fast/dom/prototype-newtest.html\n',
            'INFO: Rebaselining fast/dom/prototype-slowtest.html\n',
            'INFO: Rebaselining fast/dom/prototype-taco.html\n',
            'INFO: Rebaselining svg/dynamic-updates/SVGFEDropShadowElement-dom-stdDeviation-attr.html\n',
        ])

    def test_execute_with_only_changed_tests_option(self):
        return_code = self.command.execute(self.command_options(only_changed_tests=True), [], self.tool)
        self.assertEqual(return_code, 0)
        # svg/dynamic-updates/SVGFEDropShadowElement-dom-stdDeviation-attr.html
        # is in the list of failed tests, but not in the list of files modified
        # in the given CL; it should be included because all_tests is set to True.
        self.assertLog([
            'INFO: Rebaselining fast/dom/prototype-inheritance.html\n',
            'INFO: Rebaselining fast/dom/prototype-taco.html\n',
        ])

    def test_execute_with_flaky_test_that_fails_on_retry(self):
        # In this example, the --only-changed-tests flag is not given, but
        # svg/dynamic-updates/SVGFEDropShadowElement-dom-stdDeviation-attr.html
        # failed both with and without the patch in the try job, so it is not
        # rebaselined.
        builds = [
            Build('MOCK Try Win', 5000),
            Build('MOCK Try Mac', 4000),
            Build('MOCK Try Linux', 6000),
        ]
        for build in builds:
            self.tool.buildbot.set_retry_sumary_json(build, json.dumps({
                'failures': [
                    'fast/dom/prototype-taco.html',
                    'fast/dom/prototype-inheritance.html',
                ],
                'ignored': ['svg/dynamic-updates/SVGFEDropShadowElement-dom-stdDeviation-attr.html'],
            }))
        return_code = self.command.execute(self.command_options(), [], self.tool)
        self.assertEqual(return_code, 0)
        self.assertLog([
            'INFO: Rebaselining fast/dom/prototype-inheritance.html\n',
            'INFO: Rebaselining fast/dom/prototype-taco.html\n',
        ])

    def test_execute_with_no_retry_summary_downloaded(self):
        # In this example, the --only-changed-tests flag is not given, but
        # svg/dynamic-updates/SVGFEDropShadowElement-dom-stdDeviation-attr.html
        # failed both with and without the patch in the try job, so it is not
        # rebaselined.
        self.tool.buildbot.set_retry_sumary_json(Build('MOCK Try Win', 5000), None)
        return_code = self.command.execute(self.command_options(), [], self.tool)
        self.assertEqual(return_code, 0)
        self.assertLog([
            'WARNING: No retry summary available for build Build(builder_name=\'MOCK Try Win\', build_number=5000).\n',
            'INFO: Rebaselining fast/dom/prototype-inheritance.html\n',
            'INFO: Rebaselining fast/dom/prototype-newtest.html\n',
            'INFO: Rebaselining fast/dom/prototype-slowtest.html\n',
            'INFO: Rebaselining fast/dom/prototype-taco.html\n',
            'INFO: Rebaselining svg/dynamic-updates/SVGFEDropShadowElement-dom-stdDeviation-attr.html\n',
        ])

    def test_execute_with_trigger_jobs_option(self):
        builds = [
            Build('MOCK Try Win', 5000),
            Build('MOCK Try Mac', 4000),
        ]
        git_cl = GitCL(self.tool)
        git_cl.get_issue_number = lambda: '11112222'
        git_cl.latest_try_jobs = lambda _: builds
        self.command.git_cl = lambda: git_cl

        return_code = self.command.execute(self.command_options(trigger_jobs=True), [], self.tool)

        self.assertEqual(return_code, 1)
        self.assertLog([
            'INFO: Triggering try jobs for:\n',
            'INFO:   MOCK Try Linux\n',
            'INFO: Please re-run webkit-patch rebaseline-cl once all pending try jobs have finished.\n',
        ])
        self.assertEqual(
            self.tool.executive.calls,
            [
                [
                    'python',
                    '/mock-checkout/third_party/WebKit/Tools/Scripts/webkitpy/thirdparty/wpt/wpt/manifest',
                    '--work',
                    '--tests-root',
                    '/mock-checkout/third_party/WebKit/LayoutTests/external/wpt'
                ],
                ['git', 'cl', 'try', '-m', 'tryserver.blink', '-b', 'MOCK Try Linux']
            ])

    def test_rebaseline_calls(self):
        """Tests the list of commands that are invoked when rebaseline is called."""
        # First write test contents to the mock filesystem so that
        # fast/dom/prototype-taco.html is considered a real test to rebaseline.
        port = self.tool.port_factory.get('test-win-win7')
        self._write(
            port.host.filesystem.join(port.layout_tests_dir(), 'fast/dom/prototype-taco.html'),
            'test contents')

        self.command.rebaseline(
            self.command_options(),
            {'fast/dom/prototype-taco.html': {Build('MOCK Try Win', 5000): ['txt', 'png']}})

        self.assertEqual(
            self.tool.executive.calls,
            [
                [['python', 'echo', 'copy-existing-baselines-internal', '--suffixes', 'txt',
                  '--builder', 'MOCK Try Win', '--test', 'fast/dom/prototype-taco.html']],
                [['python', 'echo', 'rebaseline-test-internal', '--suffixes', 'txt',
                  '--builder', 'MOCK Try Win', '--test', 'fast/dom/prototype-taco.html', '--build-number', '5000']],
                [['python', 'echo', 'optimize-baselines', '--suffixes', 'txt', 'fast/dom/prototype-taco.html']]
            ])

    def test_trigger_builds(self):
        # The trigger_builds method just uses git cl to trigger jobs for the given builders.
        self.command.trigger_builds(['MOCK Try Linux', 'MOCK Try Win'])
        self.assertEqual(
            self.tool.executive.calls,
            [['git', 'cl', 'try', '-m', 'tryserver.blink', '-b', 'MOCK Try Linux', '-b', 'MOCK Try Win']])
        self.assertLog([
            'INFO: Triggering try jobs for:\n',
            'INFO:   MOCK Try Linux\n',
            'INFO:   MOCK Try Win\n',
        ])

    def test_builders_with_pending_builds(self):
        # A build number of None implies that a job has been started but not finished yet.
        self.assertEqual(
            self.command.builders_with_pending_builds([Build('MOCK Try Linux', None), Build('MOCK Try Win', 123)]),
            {'MOCK Try Linux'})

    def test_bails_when_one_build_is_missing_results(self):
        self.tool.buildbot.set_results(Build('MOCK Try Win', 5000), None)
        return_code = self.command.execute(self.command_options(), [], self.tool)
        self.assertEqual(return_code, 1)
        self.assertLog([
            'ERROR: Failed to fetch results from '
            '"https://storage.googleapis.com/chromium-layout-test-archives/MOCK_Try_Win/5000/layout-test-results".\n'
            'Try starting a new job for MOCK Try Win by running :\n'
            '  git cl try -b MOCK Try Win\n'
        ])

    def test_bails_when_there_are_unstaged_baselines(self):
        git = self.tool.git()
        git.unstaged_changes = lambda: {'third_party/WebKit/LayoutTests/my-test-expected.txt': '?'}
        return_code = self.command.execute(self.command_options(), [], self.tool)
        self.assertEqual(return_code, 1)
        self.assertLog([
            'ERROR: Aborting: there are unstaged baselines:\n',
            'ERROR:   /mock-checkout/third_party/WebKit/LayoutTests/my-test-expected.txt\n',
        ])
