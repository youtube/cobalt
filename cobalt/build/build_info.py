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

COMMIT_COUNT_BUILD_ID_OFFSET = 1000000

_FILE_DIR = os.path.dirname(__file__)

_BUILD_ID_PATTERN = '^(Build-Id: |BUILD_NUMBER=)([1-9][0-9]{6,})$'
_GIT_REV_PATTERN = '^GitOrigin-RevId: ([0-9a-f]{40})$'
_COBALT_VERSION_PATTERN = '^#define COBALT_VERSION "(.*)"$'


def get_build_id(cwd=_FILE_DIR):
  build_id, _ = _get_build_id_and_git_rev(cwd=cwd, build_rev=None)
  return build_id


def _get_build_id_and_git_rev(cwd, build_rev):
  # Internal repository
  build_id, git_rev = get_build_id_and_git_rev_from_commits(cwd=cwd)
  if build_id is None:
    # Git repository
    build_id = get_build_id_from_commit_count(cwd=cwd)
    git_rev = build_rev
  return build_id, git_rev


def get_build_id_and_git_rev_from_commits(cwd):
  # Only used with internal repository.
  # Build id and git rev must come from the same commit.
  output = subprocess.check_output(
      ['git', 'log', '--grep', _BUILD_ID_PATTERN, '-1', '-E', '--pretty=%b'],
      cwd=cwd).decode()

  # Gets build id.
  compiled_build_id_pattern = re.compile(_BUILD_ID_PATTERN, flags=re.MULTILINE)
  match_build_id = compiled_build_id_pattern.search(output)
  if not match_build_id:
    return None, None
  build_id = match_build_id.group(2)

  # Gets git rev.
  compiled_git_rev_pattern = re.compile(_GIT_REV_PATTERN, flags=re.MULTILINE)
  match_git_rev = compiled_git_rev_pattern.search(output)
  if not match_git_rev:
    git_rev = ''
  else:
    git_rev = match_git_rev.group(1)

  return build_id, git_rev


def get_build_id_from_commit_count(cwd=_FILE_DIR):
  # Only used with git repository.
  output = subprocess.check_output(['git', 'rev-list', '--count', 'HEAD'],
                                   cwd=cwd)
  build_id = int(output.strip().decode()) + COMMIT_COUNT_BUILD_ID_OFFSET
  return str(build_id)


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


def _get_cobalt_version():
  version_header_path = os.path.join(os.path.dirname(_FILE_DIR), 'version.h')
  contents = ''
  with open(version_header_path, 'r', encoding='utf-8') as f:
    contents = f.read()
  compiled_cobalt_version_pattern = re.compile(
      _COBALT_VERSION_PATTERN, flags=re.MULTILINE)
  return compiled_cobalt_version_pattern.search(contents).group(1)


def write_build_json(output_path, cwd=_FILE_DIR):
  """Writes a Cobalt build_info json file."""
  build_rev = _get_hash_from_last_commit(cwd=cwd)
  build_id, git_rev = _get_build_id_and_git_rev(cwd=cwd, build_rev=build_rev)
  cobalt_version = _get_cobalt_version()
  build_time = datetime.datetime.now().ctime()
  author = _get_author_from_last_commit(cwd=cwd)
  commit = _get_subject_from_last_commit(cwd=cwd)

  info_json = {
      'build_id': build_id,
      'build_rev': build_rev,
      'git_rev': git_rev,
      'cobalt_version': cobalt_version,
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
Cobalt Version: {cobalt_version}
Build Time: {build_time}
Author: {author}
Commit: {commit}
''')


BUILD_ID_HEADER_TEMPLATE = """
#ifndef _COBALT_BUILD_ID_H_
#define _COBALT_BUILD_ID_H_

#ifndef COBALT_BUILD_VERSION_NUMBER
#define COBALT_BUILD_VERSION_NUMBER "{cobalt_build_version_number}"
#endif  // COBALT_BUILD_VERSION_NUMBER


#endif  // _COBALT_BUILD_ID_H_
"""


def write_build_id_header(output_path):
  """Writes a cobalt_build_id header file.

  Args:
    output_path: Location to write Cobalt build ID header to.
  Returns:
    Nothing
  """
  with open(output_path, 'w', encoding='utf-8') as f:
    f.write(
        BUILD_ID_HEADER_TEMPLATE.format(
            cobalt_build_version_number=get_build_id()))


def write_licenses(output_path):
  """Writes a Cobalt licenses_cobalt txt file."""
  out_dir = os.getcwd()
  src_dir = os.path.dirname(os.path.dirname(out_dir))
  command = [
      'tools/licenses/licenses.py',
      'license_file',
      os.path.join(out_dir, output_path),
      '--gn-target',
      'cobalt:gn_all',
      '--gn-out-dir',
      out_dir,
  ]
  subprocess.call(command, cwd=src_dir)


if __name__ == '__main__':
  write_build_id_header(sys.argv[1])
  write_build_json(sys.argv[2])
  write_licenses(sys.argv[3])
