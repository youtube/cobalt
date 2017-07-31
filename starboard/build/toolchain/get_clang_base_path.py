#!/usr/bin/env python
# Copyright 2017 Google Inc. All Rights Reserved.
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

"""Finds and prints the Clang base path.

If Clang is not present, it will be downloaded.
"""

import os.path
import sys

sys.path.append(
    os.path.realpath(
        os.path.join(
            os.path.dirname(__file__), os.pardir, os.pardir, os.pardir)))
from cobalt.build.gyp_utils import EnsureClangAvailable  # pylint: disable=g-import-not-at-top
from cobalt.build.gyp_utils import GetClangBasePath
from cobalt.build.gyp_utils import GetClangBinPath


def main():
  base_dir = GetClangBasePath()
  bin_path = GetClangBinPath()
  EnsureClangAvailable(base_dir, bin_path)
  print os.path.join(base_dir, 'llvm-build', 'Release+Asserts')

if __name__ == '__main__':
  main()
