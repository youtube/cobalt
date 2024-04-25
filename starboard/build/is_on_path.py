#!/usr/bin/env python
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
"""Script for checking if the current repo on the path."""

import os


def is_first_on_pythonpath(file_path):
  """
  Checks if the passed file path is in the first entry on PYTHONPATH.

  Args:
    file_path: The path to the file that should be checked.

  Returns:
    True if the file is in the first entry on PYTHONPATH, False otherwise.
  """
  try:
    src_root_dir = os.path.join(
        os.path.dirname(file_path), os.pardir, os.pardir)
    pythonpath = os.environ['PYTHONPATH']
    if not pythonpath:
      return False

    first_path = pythonpath.split(os.pathsep)[0]
    return os.path.abspath(first_path) == os.path.abspath(src_root_dir)
  except ImportError:
    return False


if __name__ == '__main__':
  try:
    print(str(is_first_on_pythonpath(__file__)).lower())
  except KeyError:
    print('false')
