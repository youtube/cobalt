#!/usr/bin/env python3
# Copyright 2026 The Cobalt Authors. All Rights Reserved.
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
"""Utilities for manipulating JUnit XML files."""

import argparse
import logging
import sys
import xml.etree.ElementTree as ET


def merge_trees(root_base: ET.Element, root_retry: ET.Element):
  """Merges root_retry into root_base, overriding results in memory.

  Args:
      root_base: The base XML element (usually <testsuites> or <testsuite>).
      root_retry: The retry XML element to merge in.
  """
  # Handle single testsuite root
  if root_base.tag == 'testsuite':
    base_suites = {root_base.get('name'): root_base}
  else:
    base_suites = {s.get('name'): s for s in root_base.findall('testsuite')}

  if root_retry.tag == 'testsuite':
    suites_retry = [root_retry]
  else:
    suites_retry = root_retry.findall('testsuite')

  for suite_retry in suites_retry:
    suite_name = suite_retry.get('name')
    suite_base = base_suites.get(suite_name)

    if suite_base is None:
      if root_base.tag == 'testsuites':
        root_base.append(suite_retry)
      else:
        logging.warning('Cannot add new suite to single testsuite root.')
      continue

    retry_cases = {cr.get('name'): cr for cr in suite_retry.findall('testcase')}
    new_children = []

    for child in suite_base:
      if child.tag == 'testcase':
        name = child.get('name')
        if name in retry_cases:
          new_children.append(retry_cases.pop(name))
        else:
          new_children.append(child)
      else:
        new_children.append(child)

    # Add any remaining retry cases that were not in base
    for cr in retry_cases.values():
      new_children.append(cr)

    suite_base[:] = new_children

  # Recalculate counts to ensure accuracy
  total_tests = 0
  total_failures = 0
  total_errors = 0

  if root_base.tag == 'testsuite':
    suites_to_update = [root_base]
  else:
    suites_to_update = root_base.findall('testsuite')

  for suite in suites_to_update:
    suite_tests = 0
    suite_failures = 0
    suite_errors = 0

    for case in suite.findall('testcase'):
      suite_tests += 1
      failures = case.findall('failure')
      errors = case.findall('error')
      suite_failures += len(failures)
      suite_errors += len(errors)

    suite.set('tests', str(suite_tests))
    suite.set('failures', str(suite_failures))
    suite.set('errors', str(suite_errors))

    total_tests += suite_tests
    total_failures += suite_failures
    total_errors += suite_errors

  if root_base.tag == 'testsuites':
    root_base.set('tests', str(total_tests))
    root_base.set('failures', str(total_failures))
    root_base.set('errors', str(total_errors))


def merge_reports(base_report: str, retry_report: str, output_report: str):
  """Merges retry_report into base_report, overriding results.

  Args:
      base_report: Path to the base JUnit XML report.
      retry_report: Path to the retry JUnit XML report.
      output_report: Path to save the merged report.

  Returns:
      True if successful, False otherwise.
  """
  logging.info(f'Merging {retry_report} into {base_report} -> {output_report}')
  try:
    tree_base = ET.parse(base_report)
    root_base = tree_base.getroot()

    tree_retry = ET.parse(retry_report)
    root_retry = tree_retry.getroot()
  except Exception as error:
    logging.error(f'Failed to parse XML files: {error}')
    return False

  merge_trees(root_base, root_retry)

  try:
    tree_base.write(output_report, encoding='utf-8', xml_declaration=True)
    return True
  except Exception as error:
    logging.error(f'Failed to write merged report: {error}')
    return False


def filter_failures(input_report: str, output_report: str):
  """Filters input_report to keep only failing test cases.

  Args:
      input_report: Path to the input JUnit XML report.
      output_report: Path to save the filtered report.

  Returns:
      True if successful, False otherwise.
  """
  logging.info(f'Filtering failures from {input_report} -> {output_report}')
  try:
    tree = ET.parse(input_report)
    root = tree.getroot()
  except Exception as error:
    logging.error(f'Failed to parse XML file: {error}')
    return False

  # Handle single testsuite root
  if root.tag == 'testsuite':
    suites = [root]
  else:
    suites = root.findall('testsuite')

  total_tests = 0
  total_failures = 0
  total_errors = 0

  for suite in suites:
    new_children = []
    suite_tests = 0
    suite_failures = 0
    suite_errors = 0

    for child in suite:
      if child.tag != 'testcase':
        new_children.append(child)
        continue

      failures = child.findall('failure')
      errors = child.findall('error')
      if failures or errors:
        new_children.append(child)
        suite_tests += 1
        suite_failures += len(failures)
        suite_errors += len(errors)

    suite[:] = new_children

    suite.set('tests', str(suite_tests))
    suite.set('failures', str(suite_failures))
    suite.set('errors', str(suite_errors))

    total_tests += suite_tests
    total_failures += suite_failures
    total_errors += suite_errors

  if root.tag == 'testsuites':
    root[:] = [
        s for s in root if s.tag != 'testsuite' or int(s.get('tests', 0)) > 0
    ]
    root.set('tests', str(total_tests))
    root.set('failures', str(total_failures))
    root.set('errors', str(total_errors))

  try:
    tree.write(output_report, encoding='utf-8', xml_declaration=True)
    return True
  except Exception as error:
    logging.error(f'Failed to write filtered report: {error}')
    return False


def main():
  logging.basicConfig(level=logging.INFO, format='%(levelname)s: %(message)s')

  parser = argparse.ArgumentParser(description='JUnit XML utilities.')
  subparsers = parser.add_subparsers(dest='command', required=True)

  merge_parser = subparsers.add_parser('merge', help='Merge two JUnit reports')
  merge_parser.add_argument('--base', required=True, help='Base report file')
  merge_parser.add_argument('--retry', required=True, help='Retry report file')
  merge_parser.add_argument(
      '--output', required=True, help='Output report file')

  filter_parser = subparsers.add_parser(
      'filter', help='Filter failures from a JUnit report')
  filter_parser.add_argument('--input', required=True, help='Input report file')
  filter_parser.add_argument(
      '--output', required=True, help='Output report file')

  args = parser.parse_args()

  if args.command == 'merge':
    success = merge_reports(args.base, args.retry, args.output)
  elif args.command == 'filter':
    success = filter_failures(args.input, args.output)
  else:
    success = False

  if not success:
    sys.exit(1)


if __name__ == '__main__':
  sys.exit(main())
