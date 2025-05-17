#!/usr/bin/env python3
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
"""Generates a fake junit xml report from a log file (if available)."""

import sys
import re
import html
import datetime
import pathlib

RUN_MARKER = '[ RUN      ]'


def _parse_log_file(log_path: pathlib.Path) -> tuple[str, str]:
  """
  Reads the log file to find the last run test, saving all logs lines after.
  This is where the call stack and other debug info is printed.
  Returns a tuple of (last_run_line, log_tail).
  """
  with log_path.open('r', encoding='utf-8', errors='replace') as f:
    lines = f.readlines()
  last_run_line = ''
  log_tail = ''
  for i in range(len(lines) - 1, -1, -1):
    if RUN_MARKER in lines[i]:
      last_run_line = lines[i].strip()
      break
    log_tail = lines[i] + log_tail
  else:
    # Loop ran to the end, run line was not found.
    last_run_line = ''
    log_tail = 'Unable to detect the crashed test.'
  return last_run_line, log_tail


def _extract_test_details(last_run_line: str) -> tuple[str, str]:
  """
  Extracts test suite and test name from the last run line.
  Returns a tuple of (test_suite, test_name).
  """
  re_str = rf'{re.escape(RUN_MARKER)}\s+(?:(?P<suite>.*)\.)?(?P<test>[^.]+)$'
  match = re.search(re_str, last_run_line)
  if match:
    return match.group('suite') or 'UnknownSuite', match.group('test')
  return 'UnknownSuite', 'UnknownTest'


def _create_xml_report_content(test_suite: str, test_name: str,
                               log_tail_content: str) -> str:
  # pylint: disable=line-too-long
  """Creates the XML report string."""
  test_date_str = datetime.datetime.now(
      datetime.timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')
  return f"""<?xml version="1.0" encoding="UTF-8"?>
<testsuites tests="1" failures="0" disabled="0" errors="1" time="0">
  <testsuite name="{html.escape(test_suite)}" tests="1" failures="0" disabled="0" errors="1" time="0" timestamp="{test_date_str}">
    <testcase name="{html.escape(test_name)}" classname="{html.escape(test_suite)}" time="0">
      <error message="Test crashed">
        <![CDATA[ {log_tail_content} ]]>
      </error>
    </testcase>
  </testsuite>
</testsuites>
"""


def generate_crash_report(log_path: pathlib.Path, xml_path: pathlib.Path):
  last_run_line, log_tail = _parse_log_file(log_path)
  test_suite, test_name = _extract_test_details(last_run_line)
  xml_content = _create_xml_report_content(test_suite, test_name, log_tail)
  with xml_path.open('w', encoding='utf-8') as f:
    f.write(xml_content)


if __name__ == '__main__':

  def main():
    if len(sys.argv) != 3:
      print(
          f'Usage: python {sys.argv[0]} <log_path> <xml_path>', file=sys.stderr)
      sys.exit(2)
    xml_path = pathlib.Path(sys.argv[2])
    xml_path.parent.mkdir(parents=True, exist_ok=True)
    generate_crash_report(pathlib.Path(sys.argv[1]), xml_path)

  main()
