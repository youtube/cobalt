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

"""Prints the path to the given starboard platform's configuration directory.
"""

import sys

import _env  # pylint: disable=unused-import
from starboard.tools import platform


def main():
  platform_info = platform.Get(sys.argv[1])
  if not platform_info:
    return 1
  print platform_info.path
  return 0


if __name__ == '__main__':
  sys.exit(main())
