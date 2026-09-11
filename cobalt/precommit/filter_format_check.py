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

_ALLOWED_TEST_CHARS = re.compile(r'^[a-zA-Z0-9_*.?/:<>-]+$')
_ALLOWED_START_CHARS = re.compile(r'^[a-zA-Z0-9_*?]')


def check_filter_file(filepath: str) -> List[str]:
  """Validates a single .filter file.

  Args:
    filepath: Path to the .filter file.

  Returns:
    A list of error message strings describing formatting violations.
  """
  errors: List[str] = []
  if not os.path.isfile(filepath):
    return [f'{filepath} is not a regular file.']

  with open(filepath, 'r', encoding='utf-8') as f:
    for line_num, raw_line in enumerate(f, start=1):
      line = raw_line.rstrip('\r\n')
      hash_pos = line.find('#')
      if hash_pos != -1:
        line = line[:hash_pos]
      trimmed = line.strip()

      if trimmed.startswith('//'):
        errors.append(
            f'{filepath}:{line_num}: Starts with //, use # for comments.')
        continue

      if not trimmed:
        continue

      if trimmed.startswith('+'):
        pattern = trimmed[1:].strip()
        if not pattern or not _ALLOWED_TEST_CHARS.match(pattern):
          errors.append(
              f'{filepath}:{line_num}: Invalid positive filter pattern'
              f' "{trimmed}".')
      elif trimmed.startswith('-'):
        pattern = trimmed[1:].strip()
        if not pattern or not _ALLOWED_TEST_CHARS.match(pattern):
          errors.append(
              f'{filepath}:{line_num}: Invalid negative filter pattern'
              f' "{trimmed}".')
      elif _ALLOWED_START_CHARS.match(trimmed) and _ALLOWED_TEST_CHARS.match(
          trimmed):
        # Valid bare positive filter pattern
        pass
      else:
        errors.append(
            f'{filepath}:{line_num}: Unrecognized line format "{trimmed}".')

  return errors


def check_filter_files(filepaths: Sequence[str]) -> int:
  """Validates multiple .filter files and prints any formatting errors.

  Args:
    filepaths: List of filepaths to validate.

  Returns:
    1 if any formatting errors were detected, 0 otherwise.
  """
  all_errors: List[str] = []
  for filepath in filepaths:
    all_errors.extend(check_filter_file(filepath))

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

  if not args.files:
    return 0

  return check_filter_files(args.files)


if __name__ == '__main__':
  sys.exit(main())
