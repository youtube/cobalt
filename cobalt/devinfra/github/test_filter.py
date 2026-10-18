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
"""Utilities for reading and converting Cobalt test filters.

Converts buildbot .filter files into a standard gtest filter string.
"""

import argparse
import logging
import os
import sys
from typing import List, Optional, Sequence, Tuple, Union


def parse_filter_file(filter_file: str) -> Tuple[List[str], List[str]]:
  """Parses a buildbot .filter file into positive and negative filter lists.

  Follows Chromium's TestLauncher::LoadFilterFile logic:
  - Empty lines and lines starting with '#' are ignored.
  - If '#' is present and not preceded by a space (unless at start of line),
    a warning is logged.
  - Lines starting with '//' raise a ValueError.
  - Lines starting with '-' are treated as negative (failing/excluded) filters.
  - All other lines are treated as positive (included) filters.

  Args:
    filter_file: Path to the filter file.

  Returns:
    A tuple of (positive_filters, negative_filters).
  """
  if not os.path.isfile(filter_file):
    return [], []

  positive: List[str] = []
  negative: List[str] = []
  with open(filter_file, 'r', encoding='utf-8') as f:
    for line_num, raw_line in enumerate(f, start=1):
      line = raw_line.strip()
      if '#' in line:
        code, _, _ = line.partition('#')
        # Warn if '#' is not at the start of the line and not preceded by space.
        if code and not code.endswith(' '):
          logging.warning(
              'Content of line %d in %s after # is treated as a comment, %s',
              line_num, filter_file, line)
        line = code.strip()

      if line.startswith('//'):
        raise ValueError(
            f'Line {line_num} in {filter_file} starts with //, use # for'
            ' comments.')

      # Treat an empty line (or comment-only line) as a comment / skip.
      if not line:
        continue

      if line.startswith('-'):
        negative.append(line[1:])
      else:
        positive.append(line)

  return positive, negative


def format_gtest_filter(positive: Sequence[str],
                        negative: Sequence[str]) -> str:
  """Formats positive and negative filter lists into a gtest filter string.

  Examples:
    format_gtest_filter(['A', 'B'], []) -> 'A:B'
    format_gtest_filter([], ['A', 'B']) -> '-A:B'
    format_gtest_filter(['A'], ['B']) -> 'A-B'
    format_gtest_filter([], []) -> '*'

  Args:
    positive: Test patterns to include.
    negative: Test patterns to exclude.

  Returns:
    A formatted gtest filter string.
  """
  pos_str = ':'.join(filter(None, positive))
  neg_str = ':'.join(filter(None, negative))

  if pos_str and neg_str:
    return f'{pos_str}-{neg_str}'
  if pos_str:
    return pos_str
  if neg_str:
    return f'-{neg_str}'
  return '*'


def find_filter_file(
    filter_dir: str,
    target_name: str,
    shard_index: Optional[Union[int, str]] = None,
) -> Optional[str]:
  """Locates the .filter file for a target and optional shard."""
  if not target_name:
    return None
  target_name = target_name.split(':')[-1]

  if shard_index is not None and str(shard_index) != '':
    shard_file = os.path.join(filter_dir, f'{target_name}_{shard_index}.filter')
    if os.path.isfile(shard_file):
      return shard_file

  target_file = os.path.join(filter_dir, f'{target_name}.filter')
  if os.path.isfile(target_file):
    return target_file

  return None


def get_gtest_filter(
    filter_dir: str,
    target_name: str,
    shard_index: Optional[Union[int, str]] = None,
) -> str:
  """Retrieves gtest filters for a given target and optional shard.

  Looks for a shard-specific filter file first (if shard_index is provided),
  then falls back to the target filter file.

  Args:
    filter_dir: Directory containing filter files.
    target_name: The name of the gtest target.
    shard_index: Optional shard index (e.g. 0, '0').

  Returns:
    A string containing the gtest filter (e.g. '*', '-*', 'TestA:TestB',
    '-TestA:TestB', 'TestA-TestB').
  """
  filter_file = find_filter_file(filter_dir, target_name, shard_index)
  if not filter_file:
    return '*'

  positive, negative = parse_filter_file(filter_file)
  return format_gtest_filter(positive, negative)


def main(argv: Optional[Sequence[str]] = None) -> int:
  """Main CLI entry point."""
  parser = argparse.ArgumentParser(description='Cobalt test filter utility.')
  parser.add_argument(
      '--filter-dir',
      required=True,
      help='Directory containing test filter files.')
  parser.add_argument(
      '--target', required=True, help='Name of the test target.')
  parser.add_argument('--shard', default=None, help='Optional shard index.')

  args = parser.parse_args(argv)
  print(get_gtest_filter(args.filter_dir, args.target, args.shard))
  return 0


if __name__ == '__main__':
  sys.exit(main())
