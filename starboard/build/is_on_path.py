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

SRC_ROOT = src_root_dir = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir))


def is_on_same_path(file_path):
  """
  Checks if the passed file path is in the same repo as this file.

  Args:
    file_path: The path to the file that should be checked.

  Returns:
    True if the file is in the first entry on PYTHONPATH, False otherwise.
  """
  return os.path.abspath(file_path).startswith(SRC_ROOT)


if __name__ == '__main__':
  try:
    import starboard.build.is_on_path  # pylint: disable=import-outside-toplevel
    # Use imported function instead of calling directly to ensure imports are
    # being made from the same repo as this file is in.
    print(str(starboard.build.is_on_path.is_on_same_path(__file__)).lower())
  except ImportError:
    print('false')
