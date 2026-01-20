#!/usr/bin/env python3
# Copyright 2024 The Cobalt Authors. All Rights Reserved.
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
'''Create a snapshot of an Starboard Android TV implementation under
   'starboard/android/shared/media_<version>/'.  This helps with running
   multiple Starboard media implementations side by side.'''

import gn_utils
import os
import source_utils
import utils

_GN_TARGETS = [
    '//starboard/common:common',
    '//starboard/android/shared:starboard_platform',
    '//starboard/shared/starboard/media:media_util',
    '//starboard/shared/starboard/player/filter:filter_based_player_sources',
]


def find_inbound_dependencies(project_root_dir, ninja_output_pathname):
  project_root_dir = os.path.abspath(os.path.expanduser(project_root_dir))
  assert os.path.isdir(project_root_dir)
  assert os.path.isdir(os.path.join(project_root_dir, ninja_output_pathname))

  source_files = []

  for target in _GN_TARGETS:
    source_files += gn_utils.get_source_pathnames(project_root_dir,
                                                  ninja_output_pathname, target)

  source_files.sort()

  non_media_files = [f for f in source_files if not utils.is_media_file(f)]

  inbound_dependencies = {}

  for file in non_media_files:
    with open(file, encoding='utf-8') as f:
      content = f.read()

    headers = source_utils.extract_project_includes(content)
    for header in headers:
      if utils.is_media_file(header):
        if header in inbound_dependencies:
          inbound_dependencies[header].append(file)
        else:
          inbound_dependencies[header] = [file]

  for header, sources in inbound_dependencies.items():
    print(header)
    for source in sources:
      print(' ', source)
    print()
