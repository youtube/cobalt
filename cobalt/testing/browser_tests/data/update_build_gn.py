#!/usr/bin/env python
# Copyright 2025 The Cobalt Authors. All Rights Reserved.
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
"""Copies all test data to the specified output directory, skipping symlinks."""

import os
import shutil
import sys


def copy_test_data(repo_root):
  """Copies all test data to a specified sub-directory."""
  source_dir = os.path.join(repo_root, 'content', 'test', 'data')
  dest_dir = os.path.join(repo_root, 'cobalt', 'testing', 'browser_tests',
                          'data')

  if not os.path.exists(dest_dir):
    os.makedirs(dest_dir)
  else:
    return

  for dirpath, _, filenames in os.walk(source_dir):
    for filename in filenames:
      full_path = os.path.join(dirpath, filename)
      # Explicitly skip symbolic links.
      if os.path.islink(full_path):
        continue
      rel_path = os.path.relpath(full_path, source_dir)
      dest_path = os.path.join(dest_dir, rel_path)

      if not os.path.exists(os.path.dirname(dest_path)):
        os.makedirs(os.path.dirname(dest_path))

      shutil.copy(full_path, dest_path)


def main():
  # Assume the script is in cobalt/testing/browser_tests/data, so the repo root
  # is four levels up.
  repo_root = os.path.abspath(
      os.path.join(os.path.dirname(__file__), '..', '..', '..', '..'))
  copy_test_data(repo_root)
  return 0


if __name__ == '__main__':
  sys.exit(main())
