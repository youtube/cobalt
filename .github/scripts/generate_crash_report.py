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

import html
import datetime
import pathlib
import argparse
import re
from typing import Optional, Tuple
import xml.etree.ElementTree as ET

RUN_MARKER = '[ RUN      ]'
END_MARKERS = (
    '[       OK ]',
    '[  FAILED  ]',
    '[  SKIPPED ]',
)


def _extract_crash(log_path: pathlib.Path) -> Optional[Tuple[str, str, str]]:
  """Identifies the crashed test and its log output from a gtest log file.

  A crashed test will have a run marker but no end marker, or be marked
  as CRASHED in the log.

  Returns:
    A tuple `(test_suite, test_name, log_output_for_crashed_test)` or
    `None` if no crash is detected.
  """
  if not log_path.is_file():
    return None
  with log_path.open('r', encoding='utf-8', errors='replace') as f:
    lines = f.readlines()

  for i, line in reversed(list(enumerate(lines))):
    if RUN_MARKER in line:
      log = ''.join(lines[i:])
      # If the test crashed there are no end markers.
      if any(marker in log for marker in END_MARKERS):
        break
      test_name = line[len(RUN_MARKER):].strip()
      suite, name = test_name.split(
          '.', 1) if '.' in test_name else ('UnknownSuite', test_name)
      return suite, name, log

  for line in reversed(lines):
    m = re.search(r'\[\s*FAILED\s*\]\s+(\S+)\s+\(CRASHED\)', line) or re.search(
        r'\[CRASH\]\s*([^:\n]+):', line)
    if m:
      test_name = m.group(1).strip()
      suite, name = test_name.split(
          '.', 1) if '.' in test_name else ('UnknownSuite', test_name)
      return suite, name, f'Test crashed: {test_name}'

  return None


def write_junit_xml(xml_path: pathlib.Path, suite: str, name: str, log: str):
  # pylint: disable=line-too-long
  now = datetime.datetime.now(
      datetime.timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')

  if xml_path.is_file():
    try:
      tree = ET.parse(xml_path)
      root = tree.getroot()
      ts = root.find('testsuite')
      if ts is None:
        ts = ET.SubElement(root, 'testsuite', name=suite, timestamp=now)
      tc = ET.SubElement(ts, 'testcase', name=name, classname=suite, time='0')
      err = ET.SubElement(tc, 'error', message='Test crashed')
      err.text = f' {log} '
      if 'errors' in root.attrib:
        root.set('errors', str(int(root.attrib.get('errors', 0)) + 1))
      if 'errors' in ts.attrib:
        ts.set('errors', str(int(ts.attrib.get('errors', 0)) + 1))
      xml_path.parent.mkdir(parents=True, exist_ok=True)
      tree.write(xml_path, encoding='utf-8', xml_declaration=True)
      return
    except (ET.ParseError, OSError):
      pass

  with xml_path.open('w', encoding='utf-8') as f:
    f.write(f"""<?xml version="1.0" encoding="UTF-8"?>
<testsuites tests="1" failures="0" disabled="0" errors="1" time="0">
  <testsuite name="{html.escape(suite)}" tests="1" failures="0" disabled="0" errors="1" time="0" timestamp="{now}">
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
        description='Converts a gtest crash log to a JUnit XML report.')
    parser.add_argument(
        'log_path', type=pathlib.Path, help='Path to the input log file.')
    parser.add_argument(
        'xml_path',
        type=pathlib.Path,
        help='Path to the output XML report file.')
    args = parser.parse_args()

    crash_info = _extract_crash(args.log_path)
    if crash_info:
      args.xml_path.parent.mkdir(parents=True, exist_ok=True)
      write_junit_xml(args.xml_path, *crash_info)

  main()
