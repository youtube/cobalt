#!/usr/bin/env python
# Copyright 2017 The Cobalt Authors. All Rights Reserved.
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
"""Prints out the Cobalt Build ID."""

import os
import re
import subprocess

_FILE_DIR = os.path.dirname(__file__)
COMMIT_COUNT_BUILD_NUMBER_OFFSET = 1000000

# Matches numbers > 1000000. The pattern is basic so git log --grep is able to
# interpret it.
GIT_BUILD_NUMBER_PATTERN = r'[1-9]' + r'[0-9]' * 6 + r'[0-9]*'
BUILD_NUMBER_TAG_PATTERN = r'^BUILD_NUMBER={}$'

# git log --grep can't handle capture groups.
BUILD_NUBER_PATTERN_WITH_CAPTURE = f'({GIT_BUILD_NUMBER_PATTERN})'


def get_build_number_from_commits(cwd=_FILE_DIR):
  full_pattern = BUILD_NUMBER_TAG_PATTERN.format(GIT_BUILD_NUMBER_PATTERN)
  output = subprocess.check_output(
      ['git', 'log', '--grep', full_pattern, '-1', '--pretty=%b'],
      cwd=cwd).decode()

  full_pattern_with_capture = re.compile(
      BUILD_NUMBER_TAG_PATTERN.format(BUILD_NUBER_PATTERN_WITH_CAPTURE),
      flags=re.MULTILINE)
  match = full_pattern_with_capture.search(output)
  return match.group(1) if match else None


def get_build_number_from_commit_count(cwd=_FILE_DIR):
  output = subprocess.check_output(['git', 'rev-list', '--count', 'HEAD'],
                                   cwd=cwd)
  build_number = int(output.strip().decode('utf-8'))
  return build_number + COMMIT_COUNT_BUILD_NUMBER_OFFSET


def main(cwd=_FILE_DIR):
  build_number = get_build_number_from_commits(cwd=cwd)

  if not build_number:
    build_number = get_build_number_from_commit_count(cwd=cwd)

  return build_number


if __name__ == '__main__':
  print(main())
