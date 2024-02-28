#!/usr/bin/env python3
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
"""Get git hashes from Cobalt build ids."""

import argparse
import subprocess
from typing import List, Tuple

from cobalt.build import build_info

_BUILD_ID_QUERY_URL = (
    'https://carbon-airlock-95823.appspot.com/build_version/search')
_BUILD_ID_QUERY_PARAMETER_NAME = 'build_number'
_BUILD_ID_REQUEST_TIMEOUT_SECONDS = 30
_GITHUB_REMOTE = 'https://github.com/youtube/cobalt.git'


def parse_args():
  parser = argparse.ArgumentParser()
  parser.add_argument('build_id', nargs=1, type=int)
  parser.add_argument('-b', '--branch', default='main', type=str)
  return parser.parse_args()


def get_git_hashes_locally(build_id: int, branch: str) -> List[Tuple[str]]:
  current_build_number = build_info.get_build_id_from_commit_count()
  skip_count = current_build_number - build_id
  output = subprocess.check_output([
      'git', 'log', f'origin/{branch}', '--pretty=%H', f'--skip={skip_count}',
      '--max-count=1'
  ])
  git_hash = output.strip().decode('utf-8')
  return [(_GITHUB_REMOTE, git_hash)]


def print_hashes(hashes: List[Tuple[str]]):
  for remote_url, git_hash in hashes:
    print(remote_url, git_hash)


def main(build_id: int, branch: str):
  if build_id < build_info.COMMIT_COUNT_BUILD_ID_OFFSET:
    print('Use go/cobalt-build-id to identify the git hash.')
    return
  else:
    hashes = get_git_hashes_locally(build_id, branch)

  print_hashes(hashes)


if __name__ == '__main__':
  args = parse_args()
  main(args.build_id[0], args.branch)
