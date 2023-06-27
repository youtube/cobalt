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
"""Generates a Cobalt Build Info json."""

import datetime
import json
import os
import re
import subprocess
import sys

FILE_DIR = os.path.dirname(__file__)
COMMIT_COUNT_BUILD_ID_OFFSET = 1000000

_BUILD_ID_PATTERN = '^BUILD_NUMBER=([1-9][0-9]{6,})$'
_GIT_REV_PATTERN = '^GitOrigin-RevId: ([0-9a-f]{40})$'


def get_build_id_and_git_rev_from_commits(cwd):
  # Build id and git rev must come from the same commit.
  output = subprocess.check_output(
      ['git', 'log', '--grep', _BUILD_ID_PATTERN, '-1', '-E', '--pretty=%b'],
      cwd=cwd).decode()

  # Gets build id.
  compiled_build_id_pattern = re.compile(_BUILD_ID_PATTERN, flags=re.MULTILINE)
  match_build_id = compiled_build_id_pattern.search(output)
  if not match_build_id:
    return None, None
  build_id = match_build_id.group(1)

  # Gets git rev.
  compiled_git_rev_pattern = re.compile(_GIT_REV_PATTERN, flags=re.MULTILINE)
  match_git_rev = compiled_git_rev_pattern.search(output)
  if not match_git_rev:
    git_rev = ''
  else:
    git_rev = match_git_rev.group(1)

  return build_id, git_rev


def get_build_id_from_commit_count(cwd):
  output = subprocess.check_output(['git', 'rev-list', '--count', 'HEAD'],
                                   cwd=cwd)
  build_id = int(output.strip().decode()) + COMMIT_COUNT_BUILD_ID_OFFSET
  return build_id


def _get_last_commit_with_format(placeholder, cwd):
  output = subprocess.check_output(
      ['git', 'log', '-1', f'--pretty=format:{placeholder}'], cwd=cwd)
  return output.strip().decode()


def _get_hash_from_last_commit(cwd):
  return _get_last_commit_with_format(r'%H', cwd=cwd)


def _get_author_from_last_commit(cwd):
  return _get_last_commit_with_format(r'%an', cwd=cwd)


def _get_subject_from_last_commit(cwd):
  return _get_last_commit_with_format(r'%s', cwd=cwd)


def main(output_path, cwd=FILE_DIR):
  """Writes a Cobalt build_info json file."""
  build_rev = _get_hash_from_last_commit(cwd=cwd)
  build_id, git_rev = get_build_id_and_git_rev_from_commits(cwd=cwd)
  if build_id is None:
    build_id = get_build_id_from_commit_count(cwd=cwd)
    git_rev = build_rev
  build_time = datetime.datetime.now().ctime()
  author = _get_author_from_last_commit(cwd=cwd)
  commit = _get_subject_from_last_commit(cwd=cwd)

  info_json = {
      'build_id': build_id,
      'build_rev': build_rev,
      'git_rev': git_rev,
      'build_time': build_time,
      'author': author,
      'commit': commit
  }

  with open(output_path, 'w', encoding='utf-8') as f:
    f.write(json.dumps(info_json, indent=4))

  # Supports legacy build_info.txt.
  output_text_path = os.path.join(
      os.path.dirname(output_path), 'build_info.txt')
  with open(output_text_path, 'w', encoding='utf-8') as f:
    f.write(f'''\
Build ID: {build_id}
Build Rev: {build_rev}
Git Rev: {git_rev}
Build Time: {build_time}
Author: {author}
Commit: {commit}
''')


if __name__ == '__main__':
  main(sys.argv[1])
