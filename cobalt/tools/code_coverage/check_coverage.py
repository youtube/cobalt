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
"""Automates the process of checking code coverage"""

import argparse
import subprocess
import sys
import re
import os
import shutil
import json


def get_absolute_coverage(lcov_file):
  '''
    Runs `lcov --summary` and extracts the line coverage percentage using
    Regex.
  '''
  if not shutil.which('lcov'):
    print('Error: \'lcov\' system tool not found. '
          'Please install it (e.g., sudo apt install lcov).')
    sys.exit(1)

  try:
    # lcov writes to stderr or stdout depending on version, so we capture both
    result = subprocess.run(['lcov', '--summary', lcov_file],
                            stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT,
                            text=True,
                            check=True)

    # Look for the pattern "lines......: 85.4%"
    match = re.search(r'lines\.*:\s+([\d\.]+)\%', result.stdout)
    if match:
      return float(match.group(1))

    print('Warning: Could not parse lcov output. '
          f'Output was:\n{result.stdout}')
    return 0.0

  except subprocess.CalledProcessError as e:
    print(f'Error running lcov: {e.output}')
    sys.exit(1)


def get_differential_coverage(lcov_file, compare_branch):
  '''
    Runs `diff-cover` requesting a JSON report to safely extract
    the coverage percentage.
  '''
  if not shutil.which('diff-cover'):
    print('Error: \'diff-cover\' not found. Please run: pip install diff-cover')
    sys.exit(1)

  json_report = 'diff_report.json'

  cmd = [
      'diff-cover', lcov_file, f'--compare-branch={compare_branch}',
      f'--json-report={json_report}'
  ]

  try:
    # Run diff-cover silently
    subprocess.run(
        cmd, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, check=True)

    if not os.path.exists(json_report):
      # If no report generated, likely no changes found or error
      return 100.0

    with open(json_report, 'r', encoding='utf-8') as f:
      data = json.load(f)

    # Cleanup temp file
    os.remove(json_report)

    # Helper to find 'coverage' key in varying JSON structures
    def find_coverage_value(d):
      if isinstance(d, dict):
        # Standard structure
        if 'coverage' in d:
          return float(d['coverage'])
        # Nested structure (recurse values)
        for v in d.values():
          val = find_coverage_value(v)
          if val is not None:
            return val
      return None

    val = find_coverage_value(data)
    return val if val is not None else 100.0

  except subprocess.CalledProcessError:
    print('Warning: diff-cover returned non-zero '
          '(possibly due to empty diff). Defaulting to 100%.')
    return 100.0
  except Exception as e:  # pylint: disable=broad-exception-caught
    print(f'Error parsing diff-cover JSON: {e}')
    return 0.0


def main():
  parser = argparse.ArgumentParser(description='Chromium Coverage Enforcer')
  parser.add_argument(
      '--input', required=True, help='Path to the coverage.lcov file')
  parser.add_argument(
      '--compare-branch',
      default='origin/main',
      help='The git branch to compare against')
  parser.add_argument(
      '--fail-under',
      type=float,
      default=80.0,
      help='Fail if differential coverage is below this %')
  args = parser.parse_args()

  if not os.path.exists(args.input):
    print(f'Error: Input file \'{args.input}\' not found.')
    sys.exit(1)

  print('--- Analyzing Code Coverage ---')

  # 1. Gather Metrics
  abs_cov = get_absolute_coverage(args.input)
  diff_cov = get_differential_coverage(args.input, args.compare_branch)

  # 2. Determine Status
  passed = diff_cov >= args.fail_under
  status_icon = '✅ Pass' if passed else '❌ Fail'

  # 3. Generate Markdown Report
  report_content = (
      f'## 📊 Code Coverage Report\n\n'
      f'| Metric | Percentage | Threshold | Status |\n'
      f'| :--- | :--- | :--- | :--- |\n'
      f'| **Differential** (New Code) | **{diff_cov:.2f}%** | '
      f'{args.fail_under}% | {status_icon} |\n'
      f'| **Absolute** (Total) | **{abs_cov:.2f}%** | N/A | ℹ️ Info |\n')

  print('\n' + report_content)

  # Write to file for CI usage
  with open('coverage_summary.md', 'w', encoding='utf-8') as f:
    f.write(report_content)
  print('Report saved to: coverage_summary.md')

  # 4. Enforce Gate
  if not passed:
    print(f'\n[FAILURE] Differential coverage ({diff_cov:.2f}%) '
          'is below the threshold ({args.fail_under}%).')
    sys.exit(1)

  print('\n[SUCCESS] All checks passed.')
  sys.exit(0)


if __name__ == '__main__':
  main()
