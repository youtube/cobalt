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


def find_failing_tests(
    junit_xml_files: list[str]) -> dict[str, list[tuple[str, str]]]:
  """Parses a list of JUnit XML files to find failing test cases."""
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
          failing_tests[rel_path].append((f'{suite_name}.{test_name}', message))
  return failing_tests


def main(xml_files: list[str], optional_targets: list[str] = None) -> int:
  """Main entry point."""
  failing_tests = find_failing_tests(xml_files)
  optional_targets = optional_targets or []

  if failing_tests:
    has_required_failure = False
    logging.info('Failing Tests:')
    for target, test_status in sorted(failing_tests.items()):
      logging.info('%s', target)
      for test, message in sorted(test_status):
        logging.info('[  FAILED  ] %s', test)
        if message:
          logging.info('%s', message)
      logging.info('')  # Blank line between targets

      if not any(opt in target for opt in optional_targets):
        has_required_failure = True

    if not has_required_failure:
      logging.info('Optional failures ignored.')
      return 0
    return 1

  if xml_files:
    logging.info('No failing tests found in the test results.')
  return 0


if __name__ == '__main__':
  logging.basicConfig(level=logging.INFO, format='%(message)s')

  parser = argparse.ArgumentParser(
      description='A util that prints failing tests from JUnit xml.')
  parser.add_argument(
      '--optional-targets',
      nargs='*',
      default=[],
      help='List of optional targets to ignore exit codes for')
  parser.add_argument('xml_files', nargs='*', help='Paths to JUnit XML files')

  args = parser.parse_args()
  sys.exit(main(args.xml_files, args.optional_targets))
