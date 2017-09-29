#!/usr/bin/env python
# Copyright 2017 Google Inc. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http:#www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Gyp helper to find private files if present.

The first argument is the result of "<(DEPTH)" from gyp, the second argument
is the path/pattern from <(DEPTH)/starboard/private/. Patterns should be given
using Unix path style '/'.

python find_private_files.py "../.." "*.h"
Would return files matching ../../starboard/private/*.h if any exist.

python find_private_files.py "../.." "nplb/*_test.cc"
Would return files matching ../../starboard/private/nplb/*_test.cc if any exist.

NOTE: gyp errors often produce no warnings. Be sure to structure usages of this
script by gyp files like the line below, where the'<!@' is a gyp command
expansion that will process the results into a list of returned file paths.
Quoting arguments protects against wildcard expansion and other undesirable
gyp/shell behavior.
'<!@(python "<(DEPTH)/starboard/tools/find_private_files.py" "<(DEPTH)" "*.h")',
"""

import glob
import os
import sys


def find_private_files(depth,
                       target_pattern,
                       private_dir_path='starboard/private'):
  """Assembles search glob and finds files matching the target pattern.

  Args:
    depth: The string result of "<(DEPTH)"" from gyp.
    target_pattern: The string path/pattern from <(DEPTH)/|private_dir_path|
    private_dir_path: Optional The path to the private directory, which
      defaults to 'starboard/private'.
  """
  path = os.path.normpath(os.path.join(depth, private_dir_path, target_pattern))
  for f in glob.iglob(path):
    # Switch to Unix style '/' for gyp.
    print f.replace('\\', '/')


if __name__ == '__main__':
  depth_arg = sys.argv[1]
  target_pattern_arg = sys.argv[2]
  if len(sys.argv) > 3:
    private_dir_path_arg = sys.argv[3]
    find_private_files(depth_arg, target_pattern_arg, private_dir_path_arg)
  else:
    find_private_files(depth_arg, target_pattern_arg)

  sys.exit(0)
