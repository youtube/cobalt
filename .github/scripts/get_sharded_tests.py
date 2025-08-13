#!/usr/bin/env python3
# Copyright 2024 The Cobalt Authors. All Rights Reserved.
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
"""
Takes the output of --gtest_list_tests and shards it.

This script reads the full test list from a gtest binary from stdin,
divides the tests among a total number of shards, and prints a
colon-separated gtest_filter string containing only the tests for a specific
shard index.
"""

import sys


def main():
  if len(sys.argv) != 3:
    print(
        'Usage: python shard_tests.py <num_shards> <shard_index>',
        file=sys.stderr)
    print(
        'Expects the output of --gtest_list_tests from stdin.', file=sys.stderr)
    sys.exit(1)

  try:
    num_shards = int(sys.argv[1])
    shard_index = int(sys.argv[2])
  except ValueError:
    print('Error: num_shards and shard_index must be integers.', file=sys.stderr)
    sys.exit(1)

  if num_shards <= 0:
    print('Error: num_shards must be positive.', file=sys.stderr)
    sys.exit(1)

  test_list_output = sys.stdin.read()

  all_tests = []
  current_suite = ''
  for line in test_list_output.splitlines():
    # Keep the original line to check for indentation.
    original_line = line
    line = line.strip()

    if not line:
      continue

    # A line is a test case if it's indented. Otherwise, it's a new suite
    # or a full test name on a single line.
    if original_line.startswith('  '):
      # This is a test case, so prepend the current suite.
      test_name = line.split('#')[0].strip()
      all_tests.append(f'{current_suite}{test_name}')
    else:
      # This is not indented. It's either a suite or a full test name.
      if line.endswith('.'):
        # It's a suite name.
        current_suite = line
      else:
        # It's a full test name. Don't prepend any suite.
        test_name = line.split('#')[0].strip()
        all_tests.append(test_name)

  # Distribute tests to the current shard using round-robin.
  shard_tests = [
      test for i, test in enumerate(all_tests) if i % num_shards == shard_index
  ]

  # Join the test names with a colon to create the gtest_filter string.
  gtest_filter = ':'.join(shard_tests)
  print(gtest_filter)


if __name__ == '__main__':
  main()
