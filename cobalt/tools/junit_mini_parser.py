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
import json
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
              case.attrib.get('message', '').strip() + '\n' +
              (case.text or '').strip() for case in failures + errors)
          rel_path = os.path.relpath(filename)
          failing_tests[rel_path].append((f'{suite_name}.{test_name}', message))
  return failing_tests


def main(xml_files: list[str], expected_tests_json_str: str = None) -> int:
  """Main entry point."""
  failing_tests = find_failing_tests(xml_files)

  expected_tests = []
  if expected_tests_json_str:
    try:
      expected_tests = json.loads(expected_tests_json_str)
    except ValueError as e:
      logging.error('Failed to parse expected tests JSON: %s', e)
      return 1

  # Gather all tested targets from XML files.
  # Parsing all xml files to find which targets are present.
  present_targets = set()
  for filename in xml_files:
    try:
      tree = xml.etree.ElementTree.parse(filename)
      root = tree.getroot()
      # root might be <testsuites> or <testsuite>
      if root.tag == 'testsuites':
        testsuites = root.findall('testsuite')
      else:
        testsuites = [root]
      for ts in testsuites:
        target = ts.attrib.get('name')
        if target:
          present_targets.add(target)
    except (OSError, xml.etree.ElementTree.ParseError):
      pass

  missing_required = False
  for expected in expected_tests:
    if expected.get('required_passing', True):
      # Extract target name from the JSON.
      target_val = expected.get('target', '')
      target_name = target_val.split(
          ':')[-1] if ':' in target_val else target_val

      # Now check if it's in present_targets
      # But sometimes XML name has prefixes or suffixes.
      found = False
      for pt in present_targets:
        if target_name in pt:
          found = True
          break

      if not found:
        logging.error('Missing required test target: %s', target_name)
        missing_required = True

  if failing_tests:
    has_required_failure = False
    logging.info('Failing Tests:')
    for target, test_status in sorted(failing_tests.items()):
      logging.info('%s', target)
      for test_name, message in test_status:
        logging.info('  %s', test_name)
        if message:
          logging.info('%s', message)
      logging.info('')  # Blank line between targets

      # Check if this failing target was required
      is_required = True

      # Best effort matching against the expected tests JSON
      for expected in expected_tests:
        target_val = expected.get('target', '')
        target_name = target_val.split(
            ':')[-1] if ':' in target_val else target_val
        if target_name in target:
          if not expected.get('required_passing', True):
            is_required = False
          break

      if is_required:
        has_required_failure = True

    if not has_required_failure and not missing_required:
      logging.info('Optional failures ignored and no missing required targets.')
      return 0
    return 1

  if missing_required:
    return 1

  if xml_files:
    logging.info('No failing tests found in %d files.', len(xml_files))
  return 0


if __name__ == '__main__':
  logging.basicConfig(level=logging.INFO, format='%(message)s')
  parser = argparse.ArgumentParser(
      description='A util that prints failing tests from JUnit xml.')
  parser.add_argument(
      '--expected-tests-json',
      default='',
      help='JSON string containing array of expected test objects')
  parser.add_argument('xml_files', nargs='*', help='Paths to JUnit XML files')

  args = parser.parse_args()
  sys.exit(main(args.xml_files, args.expected_tests_json))
