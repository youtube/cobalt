#!/usr/bin/env python3
# Copyright 2020 The Cobalt Authors. All Rights Reserved.
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
"""Check that the copyright header line adheres to the required format."""

import datetime
import os
import re
import subprocess
import sys
from typing import List

_LINES_TO_CHECK = 15
_ALLOWED_AUTHOR = 'The Cobalt Authors'


def MetadataInParentDirectory(filename: str) -> bool:
  current_directory = os.path.dirname(os.path.normpath(filename))
  while current_directory != '':
    if os.path.exists(os.path.join(current_directory, 'METADATA')):
      return True
    current_directory = os.path.dirname(current_directory)
  return False


def GetCreatedFiles() -> List[str]:
  from_ref = os.environ['PRE_COMMIT_FROM_REF']
  to_ref = os.environ['PRE_COMMIT_TO_REF']

  new_files = subprocess.check_output(
      ['git', 'diff', from_ref, to_ref, '--name-only',
       '--diff-filter=A']).splitlines()
  return [f.strip().decode('utf-8') for f in new_files]


def CheckCopyrightYear(filenames: List[str]) -> bool:
  """Checks that source files have the expected copyright year."""

  new_files = GetCreatedFiles()
  filenames = [
      filename for filename in filenames
      if not MetadataInParentDirectory(filename)
  ]

  copyright_re = re.compile(r'Copyright (?P<created>\d{4})'
                            r'(-(?P<current>\d{4}))? (?P<author>[\w\s]+)')
  current_year = datetime.datetime.today().year
  errors = []

  for filename in filenames:
    with open(filename) as f:
      for line_num, line in enumerate(f):
        if line_num == _LINES_TO_CHECK:
          print(f'Could not find copyright header in {filename} '
                f'in the first {_LINES_TO_CHECK} lines.')
          break

        match = copyright_re.search(line)
        if match:
          created_year = int(match.group('created'))
          if filename in new_files and created_year != current_year:
            errors.append(f'Copyright header for file {filename}'
                          f' has wrong year {created_year}')

          year = match.group('current')
          if year and int(year) != current_year:
            errors.append(f'Copyright header for file {filename}'
                          f' has wrong ending year {year}')

          # Strip to get rid of possible newline
          author = match.group('author').rstrip()
          if author != _ALLOWED_AUTHOR:
            errors.append(f'Update author to be "{_ALLOWED_AUTHOR}" instead of'
                          f' "{author}" for {filename}.')
          break

  if errors:
    print()  # Separate errors from warnings by a newline.
    print('The following copyright errors were found:')
    for error in errors:
      print('  ' + error)
    return True
  return False


if __name__ == '__main__':
  sys.exit(CheckCopyrightYear(sys.argv[1:]))
