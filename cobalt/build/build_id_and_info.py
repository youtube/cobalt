# Copyright 2015 The Cobalt Authors. All Rights Reserved.
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
"""Generate a Cobalt build ID header."""

import json
import sys

BUILD_ID_HEADER_TEMPLATE = """
#ifndef _COBALT_BUILD_ID_H_
#define _COBALT_BUILD_ID_H_

#ifndef COBALT_BUILD_VERSION_NUMBER
#define COBALT_BUILD_VERSION_NUMBER "{build_id}"
#endif  // COBALT_BUILD_VERSION_NUMBER


#endif  // _COBALT_BUILD_ID_H_
"""


def BuildId(output_path, build_id):
  """Write a Cobalt build_id header file.

  Args:
    output_path: Location of the build id header to write.
    build_id: Build version number, generated when gn gen is run.
  Returns:
    0 on success.
  """
  with open(output_path, 'w', encoding='utf-8') as f:
    f.write(BUILD_ID_HEADER_TEMPLATE.format(build_id=build_id))


def BuildInfo(output_path, info):
  """Write a Cobalt build_info json file."""
  build_id, build_rev, git_rev, build_time, author, encoded_commit = info

  info_json = {}
  info_json['build_id'] = build_id
  info_json['build_rev'] = build_rev
  info_json['git_rev'] = git_rev
  info_json['build_time'] = build_time
  info_json['author'] = author

  commit = ''.join([chr(int(c)) for c in encoded_commit.split(',')])

  info_json['commit'] = commit

  with open(output_path, 'w', encoding='utf-8') as f:
    f.write(json.dumps(info_json, indent=4))


if __name__ == '__main__':
  BuildId(sys.argv[1], sys.argv[3])
  BuildInfo(sys.argv[2], sys.argv[3:])
