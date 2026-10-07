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
"""Filters test targets for CI reporting based on active test configuration."""

import argparse
import json
import sys
from typing import Any, Dict, List


def filter_expected_tests(targets: List[Dict[str, Any]],
                          run_flags: Dict[str, bool]) -> List[Dict[str, Any]]:
  """Filters test targets that should be executed and reported on."""
  expected = []
  for t in targets:
    if not isinstance(t, dict) or not t.get('target'):
      continue
    test_type = t.get('test_type')
    if not test_type:
      continue
    runs_on = t.get('runs_on', 'host')
    flag_key = f'gtest_{runs_on}' if test_type in ('gtest',
                                                   'junit') else test_type
    if run_flags.get(flag_key, False):
      expected.append(t)
  return expected


def main() -> int:
  parser = argparse.ArgumentParser(
      description='Filter expected tests based on active run flags.')
  parser.add_argument(
      '--targets_file',
      required=True,
      help='Path to test_targets.json file.',
  )
  parser.add_argument(
      '--run_flags',
      required=True,
      help='JSON string mapping test types / runner keys to bool.',
  )
  args = parser.parse_args()

  with open(args.targets_file, 'r', encoding='utf-8') as f:
    targets = json.load(f)

  run_flags = json.loads(args.run_flags)
  expected = filter_expected_tests(targets, run_flags)
  print(json.dumps(expected, separators=(',', ':')))
  return 0


if __name__ == '__main__':
  sys.exit(main())
