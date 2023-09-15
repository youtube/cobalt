#!/usr/bin/env python3
#
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
"""Compatibility layer for buildbot."""

import os
import sys

_HERE = os.path.realpath(os.path.dirname(__file__))
_REPOSITORY_ROOT = os.path.realpath(os.path.join(_HERE, os.pardir, os.pardir))
sys.path.append(_REPOSITORY_ROOT)

<<<<<<< HEAD:lbshell/build/package_application.py
from internal.lbshell.build.package_application import main  # pylint: disable=wrong-import-position
=======
def main():
  try:
    # Try to import this file and compare its path to the current file.
    import starboard.build.is_on_path  # pylint: disable=import-outside-toplevel
    this_file = os.path.realpath(__file__)
    imported_file = os.path.realpath(starboard.build.is_on_path.__file__)
    print(str(this_file == imported_file).lower())
  except ImportError:
    print('false')

>>>>>>> feaaba19f6d (Fix invoking gn.py within Docker containers (#1562)):starboard/build/is_on_path.py

if __name__ == '__main__':
  sys.exit(main())
