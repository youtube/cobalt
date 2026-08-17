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
import json
import logging
import os
import sys
import xml.etree.ElementTree


def find_failing_tests(
    junit_xml_files: list[str]) -> dict[str, list[dict[str, str]]]:
  """Parses a list of JUnit XML files to find failing test cases.

  Args:
    junit_xml_files (list): A list of paths to JUnit XML files.

  Returns:
    A map of test target -> list of dicts containing failing test name
    and message.
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
              case.attrib.get('message', '').strip() + '\n' +
              (case.text or '').strip() for case in failures + errors)
          rel_path = os.path.relpath(filename)
          failing_tests[rel_path].append({
              'name': f'{suite_name}.{test_name}',
              'message': message.strip()
          })
  return failing_tests


def main(xml_files: list[str]) -> int:
  """Main entry point.

  Args:
    xml_files (list): A list of paths to JUnit XML files.

  Returns:
    1 if failing tests are found, 0 otherwise.
  """
  failing_tests = find_failing_tests(xml_files)
  output = {'failing_tests': failing_tests}
  print(json.dumps(output, indent=2))
  return 1 if failing_tests else 0


if __name__ == '__main__':
  logging.basicConfig(level=logging.INFO, format='%(message)s')
  if len(sys.argv) == 1:
    logging.error('Usage: python junit_mini_parser.py '
                  '<junit_xml_file1> <junit_xml_file2> ...')
    logging.error('Please provide a list of JUnit XML files as command line '
                  'arguments.')
    sys.exit(2)
  sys.exit(main(sys.argv[1:]))
