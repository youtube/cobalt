# Copyright 2019 The Cobalt Authors. All Rights Reserved.
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
"""Helper script to create formatted ready-to-commit files from raw DMP files.

This script will produce two files for each DMP:
- A copy of the DMP, renamed with the sha1 of the file (with no extension).
- A placeholder file, with the same name (with .dmp.sha1 extension), with the
  contents of the file being the sha1 of the DMP.

The DMP copy is to be uploaded to static storage.

The placeholder copy is to be committed to test files folder.
"""

import hashlib
import os
import shutil
import sys


def main(argv):
  dmp_dir = argv[1]

  dmp_sha1_dir = os.path.join(dmp_dir, "dmp_sha1_files")
  gs_dir = os.path.join(dmp_dir, "gs_files")

  if not os.path.exists(dmp_sha1_dir):
    os.mkdir(dmp_sha1_dir)
  if not os.path.exists(gs_dir):
    os.mkdir(gs_dir)

  for filename in os.listdir(dmp_dir):
    _, ext = os.path.splitext(filename)
    if ext == ".dmp":
      print "*"
      print "File: " + filename
      # Compute the sha1 of the file
      filepath = os.path.join(dmp_dir, filename)
      with open(filepath, "rb") as filehandle:
        sha1 = hashlib.sha1()
        buf = filehandle.read(65536)
        while buf:
          sha1.update(buf)
          buf = filehandle.read(65536)
        sha1sum = sha1.hexdigest()

        print "SHA1: " + sha1sum

        # Rename the original DMP to its |SHA1| with no extension.
        gs_out = os.path.join(gs_dir, sha1sum)
        shutil.copyfile(filepath, gs_out)
        print "Copied \'" + filepath + "\' => \'" + gs_out + "\'"

        # Create a new file with the |SHA1| of the DMP as its contents.
        dmp_sha1_filename = os.path.join(dmp_sha1_dir, filename + ".sha1")
        with open(dmp_sha1_filename, "w") as dmp_sha1_file:
          dmp_sha1_file.write(sha1sum)
          dmp_sha1_file.close()
          print "Created \'" + dmp_sha1_filename + "\'"


if __name__ == "__main__":
  sys.exit(main(sys.argv))
