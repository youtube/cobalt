#!/usr/bin/env python
# Copyright 2023 The Cobalt Authors. All Rights Reserved.
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
"""Prints out Cobalt Git Build Info."""

import argparse
import datetime
import os
import re
import subprocess
import sys

_FILE_DIR = os.path.dirname(__file__)
COMMIT_COUNT_BUILD_NUMBER_OFFSET = 1000000

# Matches numbers > 1000000. The pattern is basic so git log --grep is able to
# interpret it.
GIT_BUILD_NUMBER_PATTERN = r'[1-9]' + r'[0-9]' * 6 + r'[0-9]*'
GIT_HASH_PATTERN = r'[0-9a-f]' * 40

# git log --grep can't handle capture groups.
GIT_BUILD_NUMBER_PATTERN_WITH_CAPTURE = f'({GIT_BUILD_NUMBER_PATTERN})'
GIT_HASH_PATTERN_WITH_CAPTURE = f'({GIT_HASH_PATTERN})'

BUILD_NUMBER_PATTERN = r'^BUILD_NUMBER={}$'
HASH_PATTERN = r'^GitOrigin-RevId: {}$'


def get_build_number_and_hash_from_commits(cwd):
  grep_pattern = BUILD_NUMBER_PATTERN.format(GIT_BUILD_NUMBER_PATTERN)
  output = subprocess.check_output(
      ['git', 'log', '--grep', grep_pattern, '-1', '--pretty=%b'],
      cwd=cwd).decode()

  # Gets git build number.
  compiled_build_number_pattern = re.compile(
      BUILD_NUMBER_PATTERN.format(GIT_BUILD_NUMBER_PATTERN_WITH_CAPTURE),
      flags=re.MULTILINE)
  match_build_number = compiled_build_number_pattern.search(output)

  if match_build_number is None:
    return None
  build_number = match_build_number.group(1)

  # Gets matching git hash.
  compiled_hash_pattern = re.compile(
      HASH_PATTERN.format(GIT_HASH_PATTERN_WITH_CAPTURE), flags=re.MULTILINE)
  match_hash = compiled_hash_pattern.search(output)

  if match_hash is None:
    git_hash = None
  else:
    git_hash = match_hash.group(1)

  return (build_number, git_hash)


def get_build_number_from_commit_count(cwd):
  output = subprocess.check_output(['git', 'rev-list', '--count', 'HEAD'],
                                   cwd=cwd)
  build_number = int(output.strip().decode('utf-8'))
  return build_number + COMMIT_COUNT_BUILD_NUMBER_OFFSET


def get_from_last_commit(placeholder, cwd):
  output = subprocess.check_output(
      ['git', 'log', '-1', f'--pretty=format:{placeholder}'], cwd=cwd)
  return output.strip().decode('utf-8')


def get_hash_from_last_commit(cwd):
  return get_from_last_commit(r'%H', cwd=cwd)


def get_author_from_last_commit(cwd):
  return get_from_last_commit(r'%an', cwd=cwd)


def get_subject_from_last_commit(cwd):
  return get_from_last_commit(r'%s', cwd=cwd)


def main(build_number_only=False, cwd=_FILE_DIR):
  build_rev = get_hash_from_last_commit(cwd=cwd)

  build_number_hash = get_build_number_and_hash_from_commits(cwd=cwd)
  if build_number_hash is not None:
    build_number = build_number_hash[0]
    git_rev = build_number_hash[1]
  else:
    build_number = get_build_number_from_commit_count(cwd=cwd)
    git_rev = build_rev

  build_time = datetime.datetime.now().ctime()
  author = get_author_from_last_commit(cwd=cwd)
  commit = get_subject_from_last_commit(cwd=cwd)

  encoded_commit = ','.join([str(ord(c)) for c in commit])

  if build_number_only:
    return build_number
  return (f'{build_number}|{build_rev}|{git_rev}|{build_time}|{author}|'
          f'{encoded_commit}')


if __name__ == '__main__':
  parser = argparse.ArgumentParser()
  parser.add_argument('--build_number', action='store_true')
  args = parser.parse_args(sys.argv[1:])
  print(main(args.build_number))
