# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Updates layout test expectations and baselines when updating w3c tests.

Specifically, this class fetches results from try bots for the current CL, then
(1) downloads new baseline files for any tests that can be rebaselined, and
(2) updates the generic TestExpectations file for any other failing tests.
"""

import argparse
import copy
import logging

from webkitpy.common.memoized import memoized
from webkitpy.common.net.git_cl import GitCL
from webkitpy.common.webkit_finder import WebKitFinder
from webkitpy.layout_tests.models.test_expectations import TestExpectationLine, TestExpectations
from webkitpy.w3c.test_parser import TestParser

_log = logging.getLogger(__name__)

MARKER_COMMENT = '# ====== New tests from w3c-test-autoroller added here ======'


class WPTExpectationsUpdater(object):

    def __init__(self, host):
        self.host = host
        self.finder = WebKitFinder(self.host.filesystem)

    def run(self, args=None):
        """Downloads text new baselines and adds test expectations lines."""
        parser = argparse.ArgumentParser(description=__doc__)
        parser.add_argument('-v', '--verbose', action='store_true', help='More verbose logging.')
        args = parser.parse_args(args)

        log_level = logging.DEBUG if args.verbose else logging.INFO
        logging.basicConfig(level=log_level, format='%(message)s')

        issue_number = self.get_issue_number()
        if issue_number == 'None':
            _log.error('No issue on current branch.')
            return 1

        builds = self.get_latest_try_jobs()
        _log.debug('Latest try jobs: %r', builds)
        if not builds:
            _log.error('No try job information was collected.')
            return 1

        # Here we build up a dict of failing test results for all platforms.
        test_expectations = {}
        for build in builds:
            port_results = self.get_failing_results_dict(build)
            test_expectations = self.merge_dicts(test_expectations, port_results)

        # And then we merge results for different platforms that had the same results.
        for test_name, platform_result in test_expectations.iteritems():
            # platform_result is a dict mapping platforms to results.
            test_expectations[test_name] = self.merge_same_valued_keys(platform_result)

        test_expectations = self.download_text_baselines(test_expectations)
        test_expectation_lines = self.create_line_list(test_expectations)
        self.write_to_test_expectations(test_expectation_lines)
        return 0

    def get_issue_number(self):
        """Returns current CL number. Can be replaced in unit tests."""
        return GitCL(self.host).get_issue_number()

    def get_latest_try_jobs(self):
        """Returns the latest finished try jobs as Build objects."""
        return GitCL(self.host).latest_try_jobs(self._get_try_bots())

    def get_failing_results_dict(self, build):
        """Returns a nested dict of failing test results.

        Retrieves a full list of layout test results from a builder result URL.
        Collects the builder name, platform and a list of tests that did not
        run as expected.

        Args:
            build: A Build object.

        Returns:
            A dictionary with the structure: {
                'full-port-name': {
                    'expected': 'TIMEOUT',
                    'actual': 'CRASH',
                    'bug': 'crbug.com/11111'
                }
            }
            If there are no failing results or no results could be fetched,
            this will return an empty dictionary.
        """
        layout_test_results = self.host.buildbot.fetch_results(build)
        if layout_test_results is None:
            _log.warning('No results for build %s', build)
            return {}
        port_name = self.host.builders.port_name_for_builder_name(build.builder_name)
        test_results = layout_test_results.didnt_run_as_expected_results()
        failing_results_dict = self.generate_results_dict(port_name, test_results)
        return failing_results_dict

    def generate_results_dict(self, full_port_name, test_results):
        """Makes a dict with results for one platform.

        Args:
            full_port_name: The fully-qualified port name, e.g. "win-win10".
            test_results: A list of LayoutTestResult objects.

        Returns:
            A dict mapping the full port name to a dict with the results for
            the given test and platform.
        """
        test_dict = {}
        for result in test_results:
            test_name = result.test_name()
            test_dict[test_name] = {
                full_port_name: {
                    'expected': result.expected_results(),
                    'actual': result.actual_results(),
                    'bug': 'crbug.com/626703'
                }
            }
        return test_dict

    def merge_dicts(self, target, source, path=None):
        """Recursively merges nested dictionaries.

        Args:
            target: First dictionary, which is updated based on source.
            source: Second dictionary, not modified.

        Returns:
            An updated target dictionary.
        """
        path = path or []
        for key in source:
            if key in target:
                if (isinstance(target[key], dict)) and isinstance(source[key], dict):
                    self.merge_dicts(target[key], source[key], path + [str(key)])
                elif target[key] == source[key]:
                    pass
                else:
                    raise ValueError('The key: %s already exist in the target dictionary.' % '.'.join(path))
            else:
                target[key] = source[key]
        return target

    def merge_same_valued_keys(self, dictionary):
        """Merges keys in dictionary with same value.

        Traverses through a dict and compares the values of keys to one another.
        If the values match, the keys are combined to a tuple and the previous
        keys are removed from the dict.

        Args:
            dictionary: A dictionary with a dictionary as the value.

        Returns:
            A new dictionary with updated keys to reflect matching values of keys.
            Example: {
                'one': {'foo': 'bar'},
                'two': {'foo': 'bar'},
                'three': {'foo': 'bar'}
            }
            is converted to a new dictionary with that contains
            {('one', 'two', 'three'): {'foo': 'bar'}}
        """
        merged_dict = {}
        matching_value_keys = set()
        keys = sorted(dictionary.keys())
        while keys:
            current_key = keys[0]
            found_match = False
            if current_key == keys[-1]:
                merged_dict[current_key] = dictionary[current_key]
                keys.remove(current_key)
                break

            for next_item in keys[1:]:
                if dictionary[current_key] == dictionary[next_item]:
                    found_match = True
                    matching_value_keys.update([current_key, next_item])

                if next_item == keys[-1]:
                    if found_match:
                        merged_dict[tuple(matching_value_keys)] = dictionary[current_key]
                        keys = [k for k in keys if k not in matching_value_keys]
                    else:
                        merged_dict[current_key] = dictionary[current_key]
                        keys.remove(current_key)
            matching_value_keys = set()
        return merged_dict

    def get_expectations(self, results):
        """Returns a set of test expectations for a given test dict.

        Returns a set of one or more test expectations based on the expected
        and actual results of a given test name.

        Args:
            results: A dictionary that maps one test to its results. Example:
                {
                    'test_name': {
                        'expected': 'PASS',
                        'actual': 'FAIL',
                        'bug': 'crbug.com/11111'
                    }
                }

        Returns:
            A set of one or more test expectation strings with the first letter
            capitalized. Example: set(['Failure', 'Timeout']).
        """
        expectations = set()
        failure_types = ['TEXT', 'FAIL', 'IMAGE+TEXT', 'IMAGE', 'AUDIO', 'MISSING', 'LEAK']
        test_expectation_types = ['SLOW', 'TIMEOUT', 'CRASH', 'PASS', 'REBASELINE', 'NEEDSREBASELINE', 'NEEDSMANUALREBASELINE']
        for expected in results['expected'].split():
            for actual in results['actual'].split():
                if expected in test_expectation_types and actual in failure_types:
                    expectations.add('Failure')
                if expected in failure_types and actual in test_expectation_types:
                    expectations.add(actual.capitalize())
                if expected in test_expectation_types and actual in test_expectation_types:
                    expectations.add(actual.capitalize())
        return expectations

    def create_line_list(self, merged_results):
        """Creates list of test expectations lines.

        Traverses through the given |merged_results| dictionary and parses the
        value to create one test expectations line per key.

        Args:
            merged_results: A merged_results with the format:
                {
                    'test_name': {
                        'platform': {
                            'expected: 'PASS',
                            'actual': 'FAIL',
                            'bug': 'crbug.com/11111'
                        }
                    }
                }

        Returns:
            A list of test expectations lines with the format:
            ['BUG_URL [PLATFORM(S)] TEST_NAME [EXPECTATION(S)]']
        """
        line_list = []
        for test_name, port_results in sorted(merged_results.iteritems()):
            for port_names in sorted(port_results):
                if test_name.startswith('external'):
                    line_parts = [port_results[port_names]['bug']]
                    specifier_part = self.specifier_part(self.to_list(port_names), test_name)
                    if specifier_part:
                        line_parts.append(specifier_part)
                    line_parts.append(test_name)
                    line_parts.append('[ %s ]' % ' '.join(self.get_expectations(port_results[port_names])))
                    line_list.append(' '.join(line_parts))
        return line_list

    def specifier_part(self, port_names, test_name):
        """Returns the specifier part for a new test expectations line.

        Args:
            port_names: A list of full port names that the line should apply to.
            test_name: The test name for the expectation line.

        Returns:
            The specifier part of the new expectation line, e.g. "[ Mac ]".
            This will be an empty string if the line should apply to all platforms.
        """
        specifiers = []
        for name in sorted(port_names):
            specifiers.append(self.host.builders.version_specifier_for_port_name(name))
        port = self.host.port_factory.get()
        specifiers.extend(self.skipped_specifiers(test_name))
        specifiers = self.simplify_specifiers(specifiers, port.configuration_specifier_macros())
        if not specifiers:
            return ''
        return '[ %s ]' % ' '.join(specifiers)

    @staticmethod
    def to_list(tuple_or_value):
        """Converts a tuple to a list, and a string value to a one-item list."""
        if isinstance(tuple_or_value, tuple):
            return list(tuple_or_value)
        return [tuple_or_value]

    def skipped_specifiers(self, test_name):
        """Returns a list of platform specifiers for which the test is skipped."""
        # TODO(qyearsley): Change Port.skips_test so that this can be simplified.
        specifiers = []
        for port in self.all_try_builder_ports():
            generic_expectations = TestExpectations(port, tests=[test_name], include_overrides=False)
            full_expectations = TestExpectations(port, tests=[test_name], include_overrides=True)
            if port.skips_test(test_name, generic_expectations, full_expectations):
                specifiers.append(self.host.builders.version_specifier_for_port_name(port.name()))
        return specifiers

    @memoized
    def all_try_builder_ports(self):
        """Returns a list of Port objects for all try builders."""
        return [self.host.port_factory.get_from_builder_name(name) for name in self._get_try_bots()]

    @staticmethod
    def simplify_specifiers(specifiers, configuration_specifier_macros):  # pylint: disable=unused-argument
        """Converts some collection of specifiers to an equivalent and maybe shorter list.

        The input strings are all case-insensitive, but the strings in the
        return value will all be capitalized.

        Args:
            specifiers: A collection of lower-case specifiers.
            configuration_specifier_macros: A dict mapping "macros" for
                groups of specifiers to lists of specific specifiers. In
                practice, this is a dict mapping operating systems to
                supported versions, e.g. {"win": ["win7", "win10"]}.

        Returns:
            A shortened list of specifiers. For example, ["win7", "win10"]
            would be converted to ["Win"]. If the given list covers all
            supported platforms, then an empty list is returned.
            This list will be sorted and have capitalized specifier strings.
        """
        specifiers = {specifier.lower() for specifier in specifiers}
        for macro_specifier, version_specifiers in configuration_specifier_macros.iteritems():
            macro_specifier = macro_specifier.lower()
            version_specifiers = {specifier.lower() for specifier in version_specifiers}
            if version_specifiers.issubset(specifiers):
                specifiers -= version_specifiers
                specifiers.add(macro_specifier)
        if specifiers == {macro.lower() for macro in configuration_specifier_macros.keys()}:
            return []
        return sorted(specifier.capitalize() for specifier in specifiers)

    def write_to_test_expectations(self, line_list):
        """Writes to TestExpectations.

        The place in the file where the new lines are inserted is after a
        marker comment line. If this marker comment line is not found, it will
        be added to the end of the file.

        Args:
            line_list: A list of lines to add to the TestExpectations file.
        """
        _log.info('Lines to write to TestExpectations:')
        for line in line_list:
            _log.info('  %s', line)
        port = self.host.port_factory.get()
        expectations_file_path = port.path_to_generic_test_expectations_file()
        file_contents = self.host.filesystem.read_text_file(expectations_file_path)
        marker_comment_index = file_contents.find(MARKER_COMMENT)
        line_list = [line for line in line_list if self._test_name_from_expectation_string(line) not in file_contents]
        if not line_list:
            return
        if marker_comment_index == -1:
            file_contents += '\n%s\n' % MARKER_COMMENT
            file_contents += '\n'.join(line_list)
        else:
            end_of_marker_line = (file_contents[marker_comment_index:].find('\n')) + marker_comment_index
            file_contents = file_contents[:end_of_marker_line + 1] + '\n'.join(line_list) + file_contents[end_of_marker_line:]
        self.host.filesystem.write_text_file(expectations_file_path, file_contents)

    @staticmethod
    def _test_name_from_expectation_string(expectation_string):
        return TestExpectationLine.tokenize_line(filename='', expectation_string=expectation_string, line_number=0).name

    def download_text_baselines(self, tests_results):
        """Fetches new baseline files for tests that should be rebaselined.

        Invokes `webkit-patch rebaseline-cl` in order to download new baselines
        (-expected.txt files) for testharness.js tests that did not crash or
        time out. Then, the platform-specific test is removed from the overall
        failure test dictionary.

        Args:
            tests_results: A dict mapping test name to platform to test results.

        Returns:
            An updated tests_results dictionary without the platform-specific
            testharness.js tests that required new baselines to be downloaded
            from `webkit-patch rebaseline-cl`.
        """
        tests_to_rebaseline, tests_results = self.get_tests_to_rebaseline(tests_results)
        _log.info('Tests to rebaseline:')
        for test in tests_to_rebaseline:
            _log.info('  %s', test)
        if tests_to_rebaseline:
            webkit_patch = self.host.filesystem.join(
                self.finder.chromium_base(), self.finder.webkit_base(), self.finder.path_to_script('webkit-patch'))
            self.host.executive.run_command([
                'python',
                webkit_patch,
                'rebaseline-cl',
                '--verbose',
                '--no-trigger-jobs',
            ] + tests_to_rebaseline)
        return tests_results

    def get_tests_to_rebaseline(self, test_results):
        """Returns a list of tests to download new baselines for.

        Creates a list of tests to rebaseline depending on the tests' platform-
        specific results. In general, this will be non-ref tests that failed
        due to a baseline mismatch (rather than crash or timeout).

        Args:
            test_results: A dictionary of failing test results, mapping tests
                to platforms to result dicts.

        Returns:
            A pair: A set of tests to be rebaselined, and a modified copy of
            the test results dictionary. The tests to be rebaselined should
            include testharness.js tests that failed due to a baseline mismatch.
        """
        test_results = copy.deepcopy(test_results)
        tests_to_rebaseline = set()
        for test_path in test_results:
            if not (self.is_js_test(test_path) and test_results.get(test_path)):
                continue
            for platform in test_results[test_path].keys():
                if test_results[test_path][platform]['actual'] not in ['CRASH', 'TIMEOUT']:
                    del test_results[test_path][platform]
                    tests_to_rebaseline.add(test_path)
        return sorted(tests_to_rebaseline), test_results

    def is_js_test(self, test_path):
        """Checks whether a given file is a testharness.js test.

        Args:
            test_path: A file path relative to the layout tests directory.
                This might correspond to a deleted file or a non-test.
        """
        absolute_path = self.host.filesystem.join(self.finder.layout_tests_dir(), test_path)
        test_parser = TestParser(absolute_path, self.host)
        if not test_parser.test_doc:
            return False
        return test_parser.is_jstest()

    def _get_try_bots(self):
        return self.host.builders.all_try_builder_names()
