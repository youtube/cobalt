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

from cobalt.build import build_info
from cobalt.build.build_number import GetOrGenerateNewBuildNumber


def get_build_number_from_server():
  # Note $BUILD_ID_SERVER_URL will always be set in CI.
  build_id_server_url = os.environ.get('BUILD_ID_SERVER_URL')
  if not build_id_server_url:
    return None

  build_num = GetOrGenerateNewBuildNumber(version_server=build_id_server_url)
  if build_num == 0:
    raise ValueError('The build number received was zero.')
  return build_num


def main(cwd=build_info.FILE_DIR):
  build_number, _ = build_info.get_build_id_and_git_rev_from_commits(cwd=cwd)
  if not build_number:
    build_number = get_build_number_from_server()
  if not build_number:
    build_number = build_info.get_build_id_from_commit_count(cwd=cwd)
  return build_number


if __name__ == '__main__':
  print(main())
