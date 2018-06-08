# Copyright 2018 Google Inc. All Rights Reserved.
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
"""List dmp files corresponding to dmp.sha1 files found in a directory.

Since the output of this script is intended to be use by GYP, all resulting
paths are using Unix-style forward slashes.
"""

import os
import sys

import _env  # pylint: disable=unused-import
from starboard.tools import paths


def main(argv):
  dmp_files = []
  directory_from_repo_root = argv[1]
  dmp_sha1_dir = os.path.join(paths.REPOSITORY_ROOT, directory_from_repo_root)
  for filename in os.listdir(dmp_sha1_dir):
    if filename[-8:] == 'dmp.sha1':
      dmp_files.append((os.path.join(dmp_sha1_dir, filename)[:-5]).replace(
          '\\', '/'))
  print ' '.join(dmp_files)


if __name__ == '__main__':
  sys.exit(main(sys.argv))
