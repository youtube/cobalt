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
"""Runs a test binary with retries on crash."""

import argparse
import datetime
import html
import os
import pathlib
import re
import subprocess
import sys
import time

RUN_MARKER = '[ RUN      ]'
END_MARKERS = (
    '[       OK ]',
    '[  FAILED  ]',
    '[  SKIPPED ]',
)

def _get_test_name_from_run_line(line: str) -> str:
  """Extracts test name like 'Suite.Test' from a gtest marker line."""
  match = re.search(rf"{re.escape(RUN_MARKER)}\s*([^\s]+)$", line)
  return match.group(1) if match else 'UnknownSuite.UnknownTest'

def _extract_crash_info(log_path: pathlib.Path):
  """
  Identifies the crashed test and its log output from a gtest log file.
  A crashed test will have a run marker but no end marker.

  Returns:
    A tuple `(test_suite, test_name, log_output_for_crashed_test)`.
    Returns `("UnknownSuite", "UnknownTest", "")` if no crash is detected.
  """
  if not log_path.exists():
    return 'UnknownSuite', 'UnknownTest', ''

  with log_path.open('r', encoding='utf-8', errors='replace') as f:
    lines = f.readlines()

  for i, line in reversed(list(enumerate(lines))):
    if RUN_MARKER in line:
      log = ''.join(lines[i+1:])
      if not any(marker in log for marker in END_MARKERS):
        test_name = _get_test_name_from_run_line(line)
        suite, name = test_name.split('.', 1) if '.' in test_name else ('UnknownSuite', test_name)
        return suite, name, log
      break

  return 'UnknownSuite', 'UnknownTest', ''

def print_junit_xml(xml_path: pathlib.Path, crashed_test: list[tuple[str, str, str]]):
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

def main():
  parser = argparse.ArgumentParser(
      description='Runs a test command with retries on crash.')
  parser.add_argument(
      '--xml_output_file',
      required=True,
      help='Path to the JUnit XML output file.')
  parser.add_argument(
      '--log_file',
      required=True,
      help='Path to the test log file.')
  parser.add_argument(
      '--max_retries',
      type=int,
      default=100,
      help='Maximum number of retries.')
  parser.add_argument(
      '--filter_file',
      help='Path to a file to store the crash filter.')
  parser.add_argument(
      'command',
      nargs=argparse.REMAINDER,
      help='The command to run.')

  args = parser.parse_args()

  # Ensure command is not empty
  if not args.command:
    print("Error: No command provided.", file=sys.stderr)
    sys.exit(1)

  # Check if -- is passed as the first argument in command and remove it
  command = args.command
  if command and command[0] == '--':
    command = command[1:]

  xml_path = pathlib.Path(args.xml_output_file)
  log_path = pathlib.Path(args.log_file)
  max_retries = args.max_retries
  current_filter = None

  # Initial filter scan to find existing filter in command
  # We assume the filter is passed as --gtest_filter=...
  # If not present, we will inject it.
  filter_arg_index = -1
  for i, arg in enumerate(command):
    if arg.startswith('--gtest_filter='):
      current_filter = arg.split('=', 1)[1]
      filter_arg_index = i
      break

  if current_filter == '*':
    current_filter = ''
    if filter_arg_index != -1:
      del command[filter_arg_index] # Remove the * filter argument
      filter_arg_index = -1

  for attempt in range(max_retries + 1):
    # Construct command with current filter
    cmd = list(command)
    if current_filter:
      if filter_arg_index != -1 and filter_arg_index < len(cmd):
          cmd[filter_arg_index] = f'--gtest_filter={current_filter}'
      else:
          cmd.append(f'--gtest_filter={current_filter}')
          filter_arg_index = len(cmd) - 1

    # Ensure output directory exists
    xml_path.parent.mkdir(parents=True, exist_ok=True)
    if xml_path.exists():
      xml_path.unlink()

    with log_path.open('w', encoding='utf-8') as log_file:
      # We use bufsize=0 (unbuffered) equivalent or line buffered to capture output live if needed,
      # but here we just redirect to file.
      # Bash used: ... 2>&1 | tee ${log_path}
      # We will pipe to stdout and file.
      process = subprocess.Popen(
          cmd,
          stdout=subprocess.PIPE,
          stderr=subprocess.STDOUT,
          text=True,
          bufsize=1  # Line buffered
      )

      while True:
        line = process.stdout.readline()
        if not line and process.poll() is not None:
          break
        if line:
          sys.stdout.write(line)
          log_file.write(line)

    if xml_path.exists():
      sys.exit(process.poll())

    suite, name, _ = _extract_crash_info(log_path)

    if suite == 'UnknownSuite' and name == 'UnknownTest':
      print("Could not identify crashed test. Aborting retries.")
      sys.exit(1) # Unknown crash, cannot filter

    crashed_test = f"{suite}.{name}"
    print(f"Identified crashed test: {crashed_test}")

    # Update filter
    if not current_filter:
      current_filter = f"-{crashed_test}"
    elif '-' in current_filter:
      # Already has negative filter, append
      current_filter += f":{crashed_test}"
    else:
      # Has positive filter, assume we want to keep it and exclude crashed?
      # GTest filter syntax: Positive patterns [-Negative patterns]
      # If current_filter has no '-', acts as positive.
      current_filter += f"-{crashed_test}"

    print(f"Updated filter: {current_filter}")
    if args.filter_file:
        with open(args.filter_file, 'w') as f:
            f.write(current_filter)

  # If we reached here, we ran out of retries
  print("Max retries reached.")

  # Generate fake XML for the last crash
  # We reuse the last log analysis
  suite, name, log_content = _extract_crash_info(log_path)
  print_junit_xml(xml_path, suite, name, log_content)

  sys.exit(1)

if __name__ == '__main__':
  main()
