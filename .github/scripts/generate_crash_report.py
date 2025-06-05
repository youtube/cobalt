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

import re
import html
import datetime
import pathlib
import argparse

RUN_MARKER = '[ RUN      ]'
END_MARKERS = (
    '[       OK ]',
    '[  FAILED  ]',
    '[  SKIPPED ]',
)


def _get_test_name_from_run_line(line: str) -> str | None:
  """Extracts test name like 'Suite.Test' from a gtest marker line."""
  # "[ RUN      ] Foo.Bar" -> "Foo.Bar"
  match = re.search(rf"{re.escape(RUN_MARKER)}\s*([^\s]+)$", line)
  return match.group(1) if match else None


def _extract_crash(log_path: pathlib.Path) -> tuple[str, str, str]:
  """
  Identifies the crashed test and its log output from a gtest log file.
  A crashed test will have a run marker but no end marker.

  Returns:
    A tuple `(test_suite, test_name, log_output_for_crashed_test)`.
    Returns `("UnknownSuite", "UnknownTest", "")` if no crash is detected.
  """
  with log_path.open('r', encoding='utf-8', errors='replace') as f:
    lines = f.readlines()

  run_line_idx: int = -1
  current_test: str | None = None
  for i, line_content in enumerate(lines):
    line_strip = line_content.strip()
    if RUN_MARKER in line_strip:
      new_test = _get_test_name_from_run_line(line_strip)
      if new_test and current_test:
        break
      run_line_idx = i
      current_test = new_test
    elif current_test and line_strip.startswith(END_MARKERS):
      current_test = None

  if current_test:
    test_suite, test_name = current_test.split('.')
    return test_suite, test_name, ''.join(lines[run_line_idx + 1:])
  return 'UnknownSuite', 'UnknownTest', 'No crash detected in test log.'


def write_junit_xml(xml_path: pathlib.Path, suite: str, name: str, log: str):
  # pylint: disable=line-too-long
  now = datetime.datetime.now(datetime.timezone.utc)
  with xml_path.open('w', encoding='utf-8') as f:
    f.write(f"""<?xml version="1.0" encoding="UTF-8"?>
<testsuites tests="1" failures="0" disabled="0" errors="1" time="0">
  <testsuite name="{html.escape(suite)}" tests="1" failures="0" disabled="0" errors="1" time="0" timestamp="{now.strftime('%Y-%m-%dT%H:%M:%SZ')}">
    <testcase name="{html.escape(name)}" classname="{html.escape(suite)}" time="0">
      <error message="Test crashed">
        <![CDATA[ {log} ]]>
      </error>
    </testcase>
  </testsuite>
</testsuites>
""")


if __name__ == '__main__':

  def main():
    parser = argparse.ArgumentParser(
        description='Generates a JUnit XML report from a test log.')
    parser.add_argument(
        'log_path', type=pathlib.Path, help='Path to the input log file.')
    parser.add_argument(
        'xml_path',
        type=pathlib.Path,
        help='Path to the output XML report file.')
    args = parser.parse_args()

    test_suite, test_name, log_tail = _extract_crash(args.log_path)
    args.xml_path.parent.mkdir(parents=True, exist_ok=True)
    write_junit_xml(args.xml_path, test_suite, test_name, log_tail)

  main()
