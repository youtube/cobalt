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

import collections
import logging
import os
import sys
import xml.etree.ElementTree


def find_failing_tests(
    junit_xml_files: list[str]) -> dict[str, list[tuple[str, str]]]:
  """Parses a list of JUnit XML files to find failing test cases.

  Args:
    junit_xml_files (list): A list of paths to JUnit XML files.

  Returns:
    A map of test target -> list of (failing test name, failure message) tuples.
  """
  failing_tests = collections.defaultdict(list)
  for filename in junit_xml_files:
    try:
      tree = xml.etree.ElementTree.parse(filename)
    except (xml.etree.ElementTree.ParseError, FileNotFoundError) as e:
      logging.error('Failed to parse %s: %s', filename, e)
      continue

    root = tree.getroot()
    for testsuite in root.findall('testsuite'):
      suite_name = testsuite.get('name', os.path.basename(filename))
      for testcase in testsuite.findall('testcase'):
        test_name = testcase.get('name')
        failures = testcase.findall('failure')
        errors = testcase.findall('error')
        if failures or errors:
          message = '\n'.join(
              case.attrib.get('message', '') for case in failures + errors)
          rel_path = os.path.relpath(filename)
          failing_tests[rel_path].append((f'{suite_name}.{test_name}', message))
  return failing_tests


def main(xml_files: list[str]) -> int:
  """Main entry point.

  Args:
    xml_files (list): A list of paths to JUnit XML files.

  Returns:
    1 if failing tests are found, 0 otherwise.
  """
  failing_tests = find_failing_tests(xml_files)

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

  if xml_files:
    logging.info('No failing tests found in the test results.')
  return 0


if __name__ == '__main__':
  logging.basicConfig(level=logging.INFO, format='%(message)s')
  if len(sys.argv) == 1:
    logging.error('Usage: python junit_mini_parser.py '
                  '<junit_xml_file1> <junit_xml_file2> ...')
    logging.error('Please provide a list of JUnit XML files as command line '
                  'arguments.')
    sys.exit(2)
  sys.exit(main(sys.argv[1:]))
