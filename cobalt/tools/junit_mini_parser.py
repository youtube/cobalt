"""A simple util that prints failing tests from JUnit xml using only the Python
standard library."""
#!/usr/bin/env python3
#
# Copyright 2025 The Cobalt Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import argparse
import collections
import logging
import os
import sys
import xml.etree.ElementTree


def _parse_xml(filename: str) -> xml.etree.ElementTree.ElementTree | None:
  """Parses a JUnit XML file safely."""
  try:
    return xml.etree.ElementTree.parse(filename)
  except (xml.etree.ElementTree.ParseError, FileNotFoundError) as error:
    logging.error('Failed to parse %s: %s', filename, error)
    return None


def find_failing_tests(
    xml_files: list[str]) -> tuple[dict[str, list[tuple[str, str]]], list[str]]:
  """Parses JUnit XML files to find failing tests."""
  failing_tests = collections.defaultdict(list)
  unparseable_files = []
  for filename in xml_files:
    try:
      tree = _parse_xml(filename)
      if tree is None:
        unparseable_files.append(filename)
        continue

      root = tree.getroot()
      for testsuite in root.findall('testsuite'):
        suite_name = testsuite.attrib.get('name', '')
        if not suite_name:
          suite_name = os.path.basename(filename)
        for case in testsuite.findall('testcase'):
          test_name = case.attrib.get('name', '')
          failures = case.findall('failure')
          errors = case.findall('error')
          if failures or errors:
            message = '\n'.join(
                case.attrib.get('message', '').strip() + '\n' +
                (case.text or '').strip() for case in failures + errors)
            rel_path = os.path.relpath(filename)
            failing_tests[rel_path].append(
                (f'{suite_name}.{test_name}', message))
    except Exception as error:  # pylint: disable=broad-except
      logging.error('Error parsing %s: %s', filename, error)
      unparseable_files.append(filename)
  return failing_tests, unparseable_files


def generate_filter_string(
    failing_tests: dict[str, list[tuple[str, str]]]) -> str:
  """Generates a positive GTest filter string from failing tests."""
  all_failed = [test for tests in failing_tests.values() for test, _ in tests]
  return ':'.join(all_failed) or '-*'


def scrub_passing_tests(xml_file: str):
  """Removes passing tests from a JUnit XML file."""
  tree = _parse_xml(xml_file)
  if tree is None:
    return

  root = tree.getroot()
  # Iterate through all testsuites in the XML.
  for testsuite in root.findall('testsuite'):
    testcases = testsuite.findall('testcase')
    remaining_count = 0
    failures_count = 0
    errors_count = 0

    # Remove testcases that did not fail or error.
    for testcase in testcases:
      has_failure = testcase.find('failure') is not None
      has_error = testcase.find('error') is not None

      if has_failure or has_error:
        remaining_count += 1
        if has_failure:
          failures_count += 1
        if has_error:
          errors_count += 1
      else:
        testsuite.remove(testcase)

    # If all testcases in a suite passed, remove the suite.
    if remaining_count == 0:
      root.remove(testsuite)
    else:
      # Update counts for the testsuite.
      testsuite.set('tests', str(remaining_count))
      testsuite.set('failures', str(failures_count))
      testsuite.set('errors', str(errors_count))

  # Update root attributes by summing counts from remaining testsuites.
  total_tests, total_failures, total_errors = 0, 0, 0
  for remaining_testsuite in root.findall('testsuite'):
    total_tests += int(remaining_testsuite.get('tests', 0))
    total_failures += int(remaining_testsuite.get('failures', 0))
    total_errors += int(remaining_testsuite.get('errors', 0))

  root.set('tests', str(total_tests))
  root.set('failures', str(total_failures))
  root.set('errors', str(total_errors))

  tree.write(xml_file, encoding='utf-8', xml_declaration=True)


def main(args: list[str] | None = None) -> int:
  """Main entry point."""
  parser = argparse.ArgumentParser(
      description='Parses JUnit XML files and handles test results.')
  parser.add_argument(
      'xml_files', nargs='*', help='List of JUnit XML files to process.')
  parser.add_argument(
      '--generate-filter',
      action='store_true',
      help='Output a positive GTest filter string for failing tests.')
  parser.add_argument(
      '--scrub-passing',
      action='store_true',
      help='Scrub passing tests from the provided XML files in place.')

  args = parser.parse_args(args)

  if args.scrub_passing:
    for xml_file in args.xml_files:
      scrub_passing_tests(xml_file)
    return 0

  failing_tests, unparseable_files = find_failing_tests(args.xml_files)

  if unparseable_files:
    logging.error('Failed to parse some XML files: %s', unparseable_files)
    return 1

  if args.generate_filter:
    filter_str = generate_filter_string(failing_tests)
    print(filter_str)
    return 0

  if failing_tests:
    logging.info('Failing Tests:')
    for target, test_status in sorted(failing_tests.items()):
      logging.info('%s', target)
      for test, message in sorted(test_status):
        logging.info('[  FAILED  ] %s', test)
        if message:
          logging.info('%s', message)
      logging.info('')  # Blank line between targets
    return 1

  if args.xml_files:
    logging.info('No failing tests found in the test results.')
  return 0


if __name__ == '__main__':
  logging.basicConfig(level=logging.INFO, format='%(message)s')
  sys.exit(main())
