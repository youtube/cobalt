#!/usr/bin/env python
# Copyright 2019 Google Inc. All Rights Reserved.
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

"""Finds depot tools path and prints it with forward slashes"""

import sys
from find_depot_tools import add_depot_tools_to_path

DEPOT_TOOLS_PATH = add_depot_tools_to_path()

def main():
  if DEPOT_TOOLS_PATH is None:
    return 1
  print DEPOT_TOOLS_PATH.replace('\\', '/')
  return 0

if __name__ == '__main__':
  sys.exit(main())
