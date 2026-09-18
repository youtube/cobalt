#!/usr/bin/env python3
#
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
"""Pre-commit check to validate Chromium/buildbot .filter file formatting."""

import argparse
import os
import re
import sys
from typing import List, Optional, Sequence

_VALID_FILTER_LINE = re.compile(r'^-?[a-zA-Z0-9_*?][a-zA-Z0-9_*.?/:<>-]*$')


def check_filter_file(filepath: str) -> List[str]:
  """Validates a single .filter file.

  Args:
    filepath: Path to the .filter file.

  Returns:
    A list of error message strings describing formatting violations.
  """
  if not os.path.isfile(filepath):
    return [f'{filepath} is not a regular file.']

  errors: List[str] = []
  with open(filepath, 'r', encoding='utf-8') as f:
    for line_num, raw_line in enumerate(f, start=1):
      line = raw_line.strip()

      if '#' in line:
        code, _, _ = line.partition('#')
        if code and not code.endswith(' '):
          errors.append(
              f'{filepath}:{line_num}: Comment after # must be preceded by a'
              ' space.')
        line = code.strip()

      if line.startswith('//'):
        errors.append(
            f'{filepath}:{line_num}: Line starts with //, use # for comments.')
        continue

      if not line:
        continue

      if not _VALID_FILTER_LINE.match(line):
        errors.append(f'{filepath}:{line_num}: Invalid filter format "{line}".')

  return errors


def check_filter_files(filepaths: Sequence[str]) -> int:
  """Validates multiple .filter files and prints any formatting errors.

  Args:
    filepaths: List of filepaths to validate.

  Returns:
    1 if any formatting errors were detected, 0 otherwise.
  """
  all_errors = [e for path in filepaths for e in check_filter_file(path)]
  if all_errors:
    print('Filter file format validation errors found:')
    for err in all_errors:
      print(f'  {err}')
    return 1

  return 0


def main(argv: Optional[Sequence[str]] = None) -> int:
  parser = argparse.ArgumentParser(
      description='Validate formatting of buildbot .filter simple list files.')
  parser.add_argument(
      'files', nargs='*', help='One or more .filter files to validate.')
  args = parser.parse_args(argv)
  return check_filter_files(args.files)


if __name__ == '__main__':
  sys.exit(main())
