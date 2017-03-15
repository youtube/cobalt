# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy

from webkitpy.common.host_mock import MockHost
from webkitpy.common.net.buildbot import Build
from webkitpy.common.net.buildbot_mock import MockBuildBot
from webkitpy.common.net.layout_test_results import LayoutTestResult, LayoutTestResults
from webkitpy.common.system.log_testing import LoggingTestCase
from webkitpy.layout_tests.builder_list import BuilderList
from webkitpy.layout_tests.port.factory_mock import MockPortFactory
from webkitpy.w3c.wpt_expectations_updater import WPTExpectationsUpdater, MARKER_COMMENT


class WPTExpectationsUpdaterTest(LoggingTestCase):

    def mock_host(self):
        super(WPTExpectationsUpdaterTest, self).setUp()
        host = MockHost()
        host.port_factory = MockPortFactory(host)
        host.builders = BuilderList({
            'MOCK Try Mac10.10': {
                'port_name': 'test-mac-mac10.10',
                'specifiers': ['Mac10.10', 'Release'],
                'is_try_builder': True,
            },
            'MOCK Try Mac10.11': {
                'port_name': 'test-mac-mac10.11',
                'specifiers': ['Mac10.11', 'Release'],
                'is_try_builder': True,
            },
            'MOCK Try Trusty': {
                'port_name': 'test-linux-trusty',
                'specifiers': ['Trusty', 'Release'],
                'is_try_builder': True,
            },
            'MOCK Try Precise': {
                'port_name': 'test-linux-precise',
                'specifiers': ['Precise', 'Release'],
                'is_try_builder': True,
            },
            'MOCK Try Win10': {
                'port_name': 'test-win-win10',
                'specifiers': ['Win10', 'Release'],
                'is_try_builder': True,
            },
            'MOCK Try Win7': {
                'port_name': 'test-win-win7',
                'specifiers': ['Win7', 'Release'],
                'is_try_builder': True,
            },
        })
        return host

    def test_get_failing_results_dict_only_passing_results(self):
        host = self.mock_host()
        host.buildbot.set_results(Build('MOCK Try Mac10.10', 123), LayoutTestResults({
            'tests': {
                'x': {
                    'passing-test.html': {
                        'expected': 'PASS',
                        'actual': 'PASS',
                    },
                },
            },
        }))
        updater = WPTExpectationsUpdater(host)
        self.assertEqual(updater.get_failing_results_dict(Build('MOCK Try Mac10.10', 123)), {})

    def test_get_failing_results_dict_no_results(self):
        host = self.mock_host()
        host.buildbot = MockBuildBot()
        host.buildbot.set_results(Build('MOCK Try Mac10.10', 123), None)
        updater = WPTExpectationsUpdater(host)
        self.assertEqual(updater.get_failing_results_dict(Build('MOCK Try Mac10.10', 123)), {})

    def test_get_failing_results_dict_some_failing_results(self):
        host = self.mock_host()
        host.buildbot.set_results(Build('MOCK Try Mac10.10', 123), LayoutTestResults({
            'tests': {
                'x': {
                    'failing-test.html': {
                        'expected': 'PASS',
                        'actual': 'IMAGE',
                        'is_unexpected': True,
                    },
                },
            },
        }))
        updater = WPTExpectationsUpdater(host)
        results_dict = updater.get_failing_results_dict(Build('MOCK Try Mac10.10', 123))
        self.assertEqual(
            results_dict,
            {
                'x/failing-test.html': {
                    'test-mac-mac10.10': {
                        'actual': 'IMAGE',
                        'expected': 'PASS',
                        'bug': 'crbug.com/626703',
                    },
                },
            })

    def test_merge_same_valued_keys_all_match(self):
        updater = WPTExpectationsUpdater(self.mock_host())
        self.assertEqual(
            updater.merge_same_valued_keys({
                'one': {'expected': 'FAIL', 'actual': 'PASS'},
                'two': {'expected': 'FAIL', 'actual': 'PASS'},
            }),
            {('two', 'one'): {'expected': 'FAIL', 'actual': 'PASS'}})

    def test_merge_same_valued_keys_one_mismatch(self):
        updater = WPTExpectationsUpdater(self.mock_host())
        self.assertEqual(
            updater.merge_same_valued_keys({
                'one': {'expected': 'FAIL', 'actual': 'PASS'},
                'two': {'expected': 'FAIL', 'actual': 'TIMEOUT'},
                'three': {'expected': 'FAIL', 'actual': 'PASS'},
            }),
            {
                ('three', 'one'): {'expected': 'FAIL', 'actual': 'PASS'},
                'two': {'expected': 'FAIL', 'actual': 'TIMEOUT'},
            })

    def test_get_expectations(self):
        updater = WPTExpectationsUpdater(self.mock_host())
        self.assertEqual(
            updater.get_expectations({'expected': 'FAIL', 'actual': 'PASS'}),
            {'Pass'})
        self.assertEqual(
            updater.get_expectations({'expected': 'FAIL', 'actual': 'TIMEOUT'}),
            {'Timeout'})
        self.assertEqual(
            updater.get_expectations({'expected': 'TIMEOUT', 'actual': 'PASS'}),
            {'Pass'})
        self.assertEqual(
            updater.get_expectations({'expected': 'PASS', 'actual': 'TIMEOUT CRASH FAIL'}),
            {'Crash', 'Failure', 'Timeout'})
        self.assertEqual(
            updater.get_expectations({'expected': 'SLOW CRASH FAIL TIMEOUT', 'actual': 'PASS'}),
            {'Pass'})

    def test_create_line_list_old_tests(self):
        # In this example, there are two failures that are not in w3c tests.
        updater = WPTExpectationsUpdater(self.mock_host())
        results = {
            'fake/test/path.html': {
                'one': {'expected': 'FAIL', 'actual': 'PASS', 'bug': 'crbug.com/test'},
                'two': {'expected': 'FAIL', 'actual': 'PASS', 'bug': 'crbug.com/test'},
            }
        }
        self.assertEqual(updater.create_line_list(results), [])

    def test_create_line_list_new_tests(self):
        # In this example, there are three unexpected results. The new
        # test expectation lines are sorted by test, and then specifier.
        updater = WPTExpectationsUpdater(self.mock_host())
        results = {
            'external/fake/test/zzzz.html': {
                'test-mac-mac10.10': {'expected': 'PASS', 'actual': 'FAIL', 'bug': 'crbug.com/test'},
            },
            'external/fake/test/path.html': {
                'test-linux-trusty': {'expected': 'FAIL', 'actual': 'PASS', 'bug': 'crbug.com/test'},
                'test-mac-mac10.11': {'expected': 'FAIL', 'actual': 'TIMEOUT', 'bug': 'crbug.com/test'},
            },
        }
        self.assertEqual(
            updater.create_line_list(results),
            [
                'crbug.com/test [ Trusty ] external/fake/test/path.html [ Pass ]',
                'crbug.com/test [ Mac10.11 ] external/fake/test/path.html [ Timeout ]',
                'crbug.com/test [ Mac10.10 ] external/fake/test/zzzz.html [ Failure ]',
            ])

    def test_specifier_part(self):
        updater = WPTExpectationsUpdater(self.mock_host())
        self.assertEqual(updater.specifier_part(['test-mac-mac10.10'], 'x/y.html'), '[ Mac10.10 ]')
        self.assertEqual(updater.specifier_part([], 'x/y.html'), '')

    def test_skipped_specifiers_when_test_is_wontfix(self):
        host = self.mock_host()
        expectations_path = '/test.checkout/LayoutTests/NeverFixTests'
        host.filesystem.files[expectations_path] = 'crbug.com/111 [ Trusty ] external/wpt/test.html [ WontFix ]\n'
        host.filesystem.files['/test.checkout/LayoutTests/external/wpt/test.html'] = ''
        updater = WPTExpectationsUpdater(host)
        self.assertEqual(updater.skipped_specifiers('external/wpt/test.html'), ['Trusty'])

    def test_simplify_specifiers(self):
        macros = {
            'mac': ['Mac10.10', 'mac10.11'],
            'win': ['Win7', 'win10'],
            'Linux': ['Trusty'],
        }
        self.assertEqual(WPTExpectationsUpdater.simplify_specifiers(['mac10.10', 'mac10.11'], macros), ['Mac'])
        self.assertEqual(WPTExpectationsUpdater.simplify_specifiers(['Mac10.10', 'Mac10.11', 'Trusty'], macros), ['Linux', 'Mac'])
        self.assertEqual(
            WPTExpectationsUpdater.simplify_specifiers(['Mac10.10', 'Mac10.11', 'Trusty', 'Win7', 'Win10'], macros), [])
        self.assertEqual(WPTExpectationsUpdater.simplify_specifiers(['a', 'b', 'c'], {}), ['A', 'B', 'C'])
        self.assertEqual(WPTExpectationsUpdater.simplify_specifiers(['Mac', 'Win', 'Linux'], macros), [])

    def test_specifier_part_with_skipped_test(self):
        host = self.mock_host()
        expectations_path = '/test.checkout/LayoutTests/NeverFixTests'
        host.filesystem.files[expectations_path] = 'crbug.com/111 [ Linux Mac10.11 ] external/wpt/test.html [ WontFix ]\n'
        host.filesystem.files['/test.checkout/LayoutTests/external/wpt/test.html'] = ''
        updater = WPTExpectationsUpdater(host)
        self.assertEqual(
            updater.specifier_part(['test-mac-mac10.10', 'test-win-win7', 'test-win-win10'], 'external/wpt/test.html'), '')
        self.assertEqual(
            updater.specifier_part(['test-win-win7', 'test-win-win10'], 'external/wpt/another.html'), '[ Win ]')

    def test_merge_dicts_with_conflict_raise_exception(self):
        updater = WPTExpectationsUpdater(self.mock_host())
        # Both dicts here have the key "one", and the value is not equal.
        with self.assertRaises(ValueError):
            updater.merge_dicts(
                {
                    'external/fake/test/path.html': {
                        'one': {'expected': 'FAIL', 'actual': 'PASS'},
                        'two': {'expected': 'FAIL', 'actual': 'TIMEOUT'},
                        'three': {'expected': 'FAIL', 'actual': 'PASS'},
                    },
                },
                {
                    'external/fake/test/path.html': {
                        'one': {'expected': 'FAIL', 'actual': 'TIMEOUT'},
                    }
                })

    def test_merge_dicts_merges_second_dict_into_first(self):
        updater = WPTExpectationsUpdater(self.mock_host())
        one = {
            'fake/test/path.html': {
                'one': {'expected': 'FAIL', 'actual': 'PASS'},
                'two': {'expected': 'FAIL', 'actual': 'PASS'},
            }
        }
        two = {
            'external/fake/test/path.html': {
                'one': {'expected': 'FAIL', 'actual': 'PASS'},
                'two': {'expected': 'FAIL', 'actual': 'TIMEOUT'},
                'three': {'expected': 'FAIL', 'actual': 'PASS'},
            }
        }
        three = {
            'external/fake/test/path.html': {
                'four': {'expected': 'FAIL', 'actual': 'PASS'},
            }
        }

        output = updater.merge_dicts(one, three)
        self.assertEqual(output, one)
        output = updater.merge_dicts(two, three)
        self.assertEqual(output, two)

    def test_generate_results_dict(self):
        updater = WPTExpectationsUpdater(self.mock_host())
        layout_test_list = [
            LayoutTestResult(
                'test/name.html', {
                    'expected': 'bar',
                    'actual': 'foo',
                    'is_unexpected': True,
                    'has_stderr': True,
                })
        ]
        self.assertEqual(updater.generate_results_dict('test-mac-mac10.10', layout_test_list), {
            'test/name.html': {
                'test-mac-mac10.10': {
                    'expected': 'bar',
                    'actual': 'foo',
                    'bug': 'crbug.com/626703',
                }
            }
        })

    def test_write_to_test_expectations_with_marker_comment(self):
        host = self.mock_host()

        expectations_path = host.port_factory.get().path_to_generic_test_expectations_file()
        host.filesystem.files[expectations_path] = MARKER_COMMENT + '\n'
        updater = WPTExpectationsUpdater(host)
        line_list = ['crbug.com/123 [ Trusty ] fake/file/path.html [ Pass ]']
        updater.write_to_test_expectations(line_list)
        value = updater.host.filesystem.read_text_file(expectations_path)
        self.assertMultiLineEqual(
            value,
            (MARKER_COMMENT + '\n'
             'crbug.com/123 [ Trusty ] fake/file/path.html [ Pass ]\n'))

    def test_write_to_test_expectations_with_no_marker_comment(self):
        host = self.mock_host()
        expectations_path = host.port_factory.get().path_to_generic_test_expectations_file()
        host.filesystem.files[expectations_path] = 'crbug.com/111 [ Trusty ] foo/bar.html [ Failure ]\n'
        updater = WPTExpectationsUpdater(host)
        line_list = ['crbug.com/123 [ Trusty ] fake/file/path.html [ Pass ]']
        updater.write_to_test_expectations(line_list)
        value = host.filesystem.read_text_file(expectations_path)
        self.assertMultiLineEqual(
            value,
            ('crbug.com/111 [ Trusty ] foo/bar.html [ Failure ]\n'
             '\n' + MARKER_COMMENT + '\n'
             'crbug.com/123 [ Trusty ] fake/file/path.html [ Pass ]'))

    def test_write_to_test_expectations_skips_existing_lines(self):
        host = self.mock_host()
        expectations_path = host.port_factory.get().path_to_generic_test_expectations_file()
        host.filesystem.files[expectations_path] = 'crbug.com/111 dont/copy/me.html [ Failure ]\n'
        updater = WPTExpectationsUpdater(host)
        line_list = [
            'crbug.com/111 dont/copy/me.html [ Failure ]',
            'crbug.com/222 do/copy/me.html [ Failure ]'
        ]
        updater.write_to_test_expectations(line_list)
        value = host.filesystem.read_text_file(expectations_path)
        self.assertEqual(
            value,
            ('crbug.com/111 dont/copy/me.html [ Failure ]\n'
             '\n' + MARKER_COMMENT + '\n'
             'crbug.com/222 do/copy/me.html [ Failure ]'))

    def test_write_to_test_expectations_with_marker_and_no_lines(self):
        host = self.mock_host()
        expectations_path = host.port_factory.get().path_to_generic_test_expectations_file()
        host.filesystem.files[expectations_path] = (
            MARKER_COMMENT + '\n'
            'crbug.com/123 [ Trusty ] fake/file/path.html [ Pass ]\n')
        updater = WPTExpectationsUpdater(host)
        updater.write_to_test_expectations([])
        value = updater.host.filesystem.read_text_file(expectations_path)
        self.assertMultiLineEqual(
            value,
            (MARKER_COMMENT + '\n'
             'crbug.com/123 [ Trusty ] fake/file/path.html [ Pass ]\n'))

    def test_is_js_test_true(self):
        host = self.mock_host()
        host.filesystem.files['/mock-checkout/third_party/WebKit/LayoutTests/foo/bar.html'] = (
            '<script src="/resources/testharness.js"></script>')
        updater = WPTExpectationsUpdater(host)
        self.assertTrue(updater.is_js_test('foo/bar.html'))

    def test_is_js_test_false(self):
        host = self.mock_host()
        host.filesystem.files['/mock-checkout/third_party/WebKit/LayoutTests/foo/bar.html'] = (
            '<script src="ref-test.html"></script>')
        updater = WPTExpectationsUpdater(host)
        self.assertFalse(updater.is_js_test('foo/bar.html'))

    def test_is_js_test_non_existent_file(self):
        updater = WPTExpectationsUpdater(self.mock_host())
        self.assertFalse(updater.is_js_test('foo/bar.html'))

    def test_get_test_to_rebaseline_returns_only_tests_with_failures(self):
        host = self.mock_host()
        host.filesystem.files['/mock-checkout/third_party/WebKit/LayoutTests/external/fake/test/path.html'] = (
            '<script src="/resources/testharness.js"></script>')
        host.filesystem.files['/mock-checkout/third_party/WebKit/LayoutTests/external/other/test/path.html'] = (
            '<script src="/resources/testharness.js"></script>')
        updater = WPTExpectationsUpdater(host)
        two = {
            'external/fake/test/path.html': {
                'one': {'expected': 'FAIL', 'actual': 'PASS'},
                'two': {'expected': 'FAIL', 'actual': 'TIMEOUT'},
                'three': {'expected': 'FAIL', 'actual': 'PASS'},
            }
        }
        tests_to_rebaseline, _ = updater.get_tests_to_rebaseline(two)
        # The other test doesn't have an entry in the test results dict, so it is not listed as a test to rebaseline.
        self.assertEqual(tests_to_rebaseline, ['external/fake/test/path.html'])

    def test_get_test_to_rebaseline_returns_only_js_tests(self):
        host = self.mock_host()
        host.filesystem.files['/mock-checkout/third_party/WebKit/LayoutTests/external/fake/test/path.html'] = (
            'this file does not look like a testharness JS test.')
        updater = WPTExpectationsUpdater(host)
        two = {
            'external/fake/test/path.html': {
                'one': {'expected': 'FAIL', 'actual': 'PASS', 'bug': 'crbug.com/test'},
                'two': {'expected': 'FAIL', 'actual': 'TIMEOUT', 'bug': 'crbug.com/test'},
                'three': {'expected': 'FAIL', 'actual': 'PASS', 'bug': 'crbug.com/test'},
            }
        }
        tests_to_rebaseline, _ = updater.get_tests_to_rebaseline(two)
        self.assertEqual(tests_to_rebaseline, [])

    def test_get_tests_to_rebaseline_returns_updated_dict(self):
        host = self.mock_host()
        test_results_dict = {
            'external/fake/test/path.html': {
                'one': {'expected': 'PASS', 'actual': 'TEXT'},
                'two': {'expected': 'PASS', 'actual': 'TIMEOUT'},
            },
        }
        test_results_dict_copy = copy.deepcopy(test_results_dict)
        host.filesystem.files['/mock-checkout/third_party/WebKit/LayoutTests/external/fake/test/path.html'] = (
            '<script src="/resources/testharness.js"></script>')
        updater = WPTExpectationsUpdater(host)
        tests_to_rebaseline, modified_test_results = updater.get_tests_to_rebaseline(
            test_results_dict)
        self.assertEqual(tests_to_rebaseline, ['external/fake/test/path.html'])
        # The record for the builder with a timeout is kept, but not with a text mismatch,
        # since that should be covered by downloading a new baseline.
        self.assertEqual(modified_test_results, {
            'external/fake/test/path.html': {
                'two': {'expected': 'PASS', 'actual': 'TIMEOUT'},
            },
        })
        # The original dict isn't modified.
        self.assertEqual(test_results_dict, test_results_dict_copy)

    def test_get_tests_to_rebaseline_also_returns_slow_tests(self):
        host = self.mock_host()
        test_results_dict = {
            'external/fake/test/path.html': {
                'one': {'expected': 'SLOW', 'actual': 'TEXT'},
                'two': {'expected': 'SLOW', 'actual': 'TIMEOUT'},
            },
        }
        test_results_dict_copy = copy.deepcopy(test_results_dict)
        host.filesystem.files['/mock-checkout/third_party/WebKit/LayoutTests/external/fake/test/path.html'] = (
            '<script src="/resources/testharness.js"></script>')
        updater = WPTExpectationsUpdater(host)
        tests_to_rebaseline, modified_test_results = updater.get_tests_to_rebaseline(
            test_results_dict)
        self.assertEqual(tests_to_rebaseline, ['external/fake/test/path.html'])
        # The record for the builder with a timeout is kept, but not with a text mismatch,
        # since that should be covered by downloading a new baseline.
        self.assertEqual(modified_test_results, {
            'external/fake/test/path.html': {
                'two': {'expected': 'SLOW', 'actual': 'TIMEOUT'},
            },
        })
        # The original dict isn't modified.
        self.assertEqual(test_results_dict, test_results_dict_copy)

    def test_run_no_issue_number(self):
        # TODO(qyearsley): For testing: Consider making a MockGitCL class
        # and use that class to set fake return values when using git cl.
        updater = WPTExpectationsUpdater(self.mock_host())
        updater.get_issue_number = lambda: 'None'
        self.assertEqual(1, updater.run(args=[]))
        self.assertLog(['ERROR: No issue on current branch.\n'])

    def test_run_no_try_results(self):
        updater = WPTExpectationsUpdater(self.mock_host())
        updater.get_latest_try_jobs = lambda: []
        self.assertEqual(1, updater.run(args=[]))
        self.assertLog(['ERROR: No try job information was collected.\n'])
