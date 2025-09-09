#!/usr/bin/env python3

# Copyright 2023 The Cobalt Authors. All Rights Reserved.
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
#
"""Parses the gtest xml output and prints the failing tests"""

import pathlib
import sys

from junitparser import JUnitXml


def get_failures(suite):
  failures = []
  for case in suite:
    if not case.is_passed:
      failures.append((f'{suite.name}.{case.name}',
                       [result.message for result in case.result]))
  return failures


def target_header(target_name):
  return f'{target_name}\n' + ('-' * len(target_name))


def main():
  if len(sys.argv) != 2:
    print(f'Usage: {sys.argv[0]} <path to unit test reports>')
    sys.exit(1)

  print('======== UNIT TEST REPORT ========')
  success = True
  for report in sorted(pathlib.Path(sys.argv[1]).rglob('*.xml')):
    # Get the target name from the filename as set by test_runner.py.
    target_name = pathlib.Path(report).stem
    try:
      junit_report = JUnitXml.fromfile(report)
      for suite in junit_report:
        failures = get_failures(suite)
        if failures:
          success = False
          print(target_header(target_name))
          for case, results in failures:
            print(case)
            print('\n'.join(results) + '\n')
    except Exception:  # pylint: disable=broad-except
      success = False
      print(target_header(target_name))
      print('Unable to read unit test report. Did the test crash?\n')

  if success:
    print('Found no test failures :)')


if __name__ == '__main__':
  main()
