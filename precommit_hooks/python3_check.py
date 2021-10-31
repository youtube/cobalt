#!/usr/bin/env python3
#
# Copyright 2020 The Cobalt Authors. All Rights Reserved.
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
"""Warns if python code isn't compatible with python 3."""

import os
import py_compile
import sys
import tempfile
from typing import List


def _CheckFileForPython3Compatibility(filename: str) -> bool:
  if not os.path.exists(filename):
    print(f'{filename} is not a valid path, skipping.')
    return False

  temp_directory = tempfile.TemporaryDirectory()
  temp_file = os.path.join(temp_directory.name, 'cfile')
  had_errors = False

  try:
    py_compile.compile(filename, cfile=temp_file, doraise=True)
  except py_compile.PyCompileError as e:
    print(e)
    print(f'{filename} is not valid in Python 3, consider updating it.')
    had_errors = True

  temp_directory.cleanup()
  return had_errors


def CheckFilesForPython3Compatibility(files: List[str]) -> bool:
  rets = [_CheckFileForPython3Compatibility(file) for file in files]
  return any(rets)


if __name__ == '__main__':
  sys.exit(CheckFilesForPython3Compatibility(sys.argv[1:]))
