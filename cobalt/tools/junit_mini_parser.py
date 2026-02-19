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
import os
import sys
import xml.etree.ElementTree


def find_failing_tests(junit_xml_files: list[str]) -> dict[str, list[str]]:
  """Parses a list of JUnit XML files to find failing test cases.

  Args:
    junit_xml_files (list): A list of paths to JUnit XML files.

  Returns:
    A map of test target -> list of failing tests.
  """
  failing_tests = collections.defaultdict(list)
  for filename in junit_xml_files:
    tree = xml.etree.ElementTree.parse(filename)
    root = tree.getroot()
    for testsuite in root.findall('testsuite'):
      suite_name = testsuite.get('name', os.path.basename(filename))
      for testcase in testsuite.findall('testcase'):
        test_name = testcase.get('name')
        failures = testcase.findall('failure')
        errors = testcase.findall('error')
        if failures or errors:
          message = '\n'.join(
              case.attrib.get('message', '').strip() + '\n' + case.text.strip()
              for case in failures + errors)
          rel_path = os.path.relpath(filename)
          failing_tests[rel_path].append((f'{suite_name}.{test_name}', message))
  return failing_tests


def main(xml_files: str):
  failing_tests = find_failing_tests(xml_files)

  if failing_tests:
    print('Failing Tests:')
    for target, test_status in sorted(failing_tests.items()):
      print(f'{target}')
      for test, message in sorted(test_status):
        print(f'[  FAILED  ] {test}')
        if message:
          print(message)
      else:
        print()
  else:
    print('No failing tests found in the test results.')


if __name__ == '__main__':
  if len(sys.argv) == 1:
    print('Usage: python junit_mini_parser.py '
          '<junit_xml_file1> <junit_xml_file2> ...')
    print('Please provide a list of JUnit XML files as command line arguments.')
  main(sys.argv[1:])
