#!/usr/bin/env python2
# Copyright 2021 The Cobalt Authors. All Rights Reserved.
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
"""Runs all Python 2 unit tests in a directory with a changed file."""

import glob
import os
import re
import subprocess
import sys

PROJECT_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir))


def RunPyTests(files):
  """Checks that Python unit tests pass.

  Use unit test discovery to find and run all tests in directories
  with changed files.
  Args:
    input_api: presubmit_support input API object
    output_api: presubmit_support output API object
    source_file_filter: FileFilterGenerator

  Returns:
    List of failed checks
  """
  print('Running Python unit tests.')

  # Get the set of directories to look in.
  dirs_to_check = set(os.path.dirname(f) for f in files)
  unittest_re = re.compile(r'(import\s+unittest)|(from\s+unittest\s+import)')

  def _ImportsUnittestAndIsPython2Compatible(filename):
    with open(filename) as f:
      for line_number, line in enumerate(f):
        if line_number == 0 and line.startswith('#!') and 'python3' in line:
          return False
        if unittest_re.search(line):
          return True
    return False

  # Discover and run in each directory.
  for d in dirs_to_check:
    tests = glob.glob(d + '/*_test.py')
    if not tests:
      continue

    # Check if each one actually import the unittest module.
    # If not, don't run it.
    tests = [x for x in tests if _ImportsUnittestAndIsPython2Compatible(x)]
    for test in tests:
      res = subprocess.call([sys.executable, test])
      if res:
        return True

  return False


if __name__ == '__main__':
  sys.exit(RunPyTests(sys.argv[1:]))
