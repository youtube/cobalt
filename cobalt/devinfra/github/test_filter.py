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
"""Utilities for reading and evaluating Cobalt test filters."""

import argparse
import json
import os
import sys
from typing import Optional, Sequence, Union


def get_gtest_filter(
    filter_json_dir: str,
    target_name: str,
    shard_index: Optional[Union[int, str]] = None,
) -> str:
  """Retrieves gtest filters for a given target and optional shard.

  Looks for a shard-specific filter file first (if shard_index is provided),
  then falls back to the target filter file.

  Args:
    filter_json_dir: Directory containing filter JSON files.
    target_name: The name of the gtest target.
    shard_index: Optional shard index (e.g. 0, '0').

  Returns:
    A string containing the gtest filter (e.g. '*', '-*', 'TestA:TestB',
    '-TestA:TestB', 'TestA-TestB').
  """
  if target_name:
    target_name = target_name.split(':')[-1]

  shard_file = os.path.join(filter_json_dir,
                            f'{target_name}_{shard_index}_filter.json')
  target_file = os.path.join(filter_json_dir, f'{target_name}_filter.json')

  filter_file = target_file
  if (shard_index is not None and str(shard_index) != '' and
      os.path.exists(shard_file)):
    filter_file = shard_file

  if not os.path.exists(filter_file):
    return '*'

  with open(filter_file, 'r', encoding='utf-8') as f:
    filter_data = json.load(f)

  positive = ':'.join(filter_data.get('tests_to_run') or [])
  negative = ':'.join(filter_data.get('failing_tests') or [])

  if positive and negative:
    return f'{positive}-{negative}'
  if positive:
    return positive
  if negative:
    return f'-{negative}'
  return '*'


def main(argv: Optional[Sequence[str]] = None) -> int:
  """Main CLI entry point."""
  parser = argparse.ArgumentParser(description='Cobalt test filter utility.')
  parser.add_argument(
      '--filter-dir',
      required=True,
      help='Directory containing test filter JSON files.')
  parser.add_argument(
      '--target', required=True, help='Name of the test target.')
  parser.add_argument('--shard', default=None, help='Optional shard index.')

  args = parser.parse_args(argv)
  print(get_gtest_filter(args.filter_dir, args.target, args.shard))
  return 0


if __name__ == '__main__':
  sys.exit(main())
