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
"""A simple util that prints failing tests from JUnit xml using only the Python
standard library."""

import argparse
import collections
import json
import os
import sys
from typing import Optional, Sequence
import xml.etree.ElementTree


def find_failing_tests(
    junit_xml_files: list[str]) -> dict[str, list[dict[str, str]]]:
  """Parses a list of JUnit XML files to find failing test cases.

  Args:
    junit_xml_files: A list of paths to JUnit XML files.

  Returns:
    A map of test target -> list of dicts containing failing test name
    and message.
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
              case.attrib.get('message', '').strip() + '\n' +
              (case.text or '').strip() for case in failures + errors)
          rel_path = os.path.relpath(filename)
          failing_tests[rel_path].append({
              'name': f'{suite_name}.{test_name}',
              'message': message.strip()
          })
  return failing_tests


def main(argv: Optional[Sequence[str]] = None) -> int:
  """Main entry point.

  Args:
    argv: A list of command-line arguments (flags and XML files).

  Returns:
    1 if failing tests are found, 0 otherwise.
  """
  parser = argparse.ArgumentParser(
      description='Print failing tests from JUnit XML files.')
  parser.add_argument(
      '--format',
      choices=['json', 'filter'],
      default='json',
      help='Output format: "json" (default) or "filter".')
  parser.add_argument('files', nargs='*', help='Paths to JUnit XML files.')

  args = parser.parse_args(argv)

  failing_tests = find_failing_tests(args.files)
  if args.format == 'filter':
    unique_failures = sorted({
        test['name']
        for tests in failing_tests.values()
        for test in tests
        if test.get('name')
    })
    if unique_failures:
      for test in unique_failures:
        print(test)
    else:
      print('-*')
  else:
    output = {'failing_tests': failing_tests}
    print(json.dumps(output, indent=2))

  return 1 if failing_tests else 0


if __name__ == '__main__':
  sys.exit(main())
