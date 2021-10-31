#!/usr/bin/env python3
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
"""Get correct clang-format installation and run it."""

import os
import platform
import subprocess
import sys

if __name__ == '__main__':
  clang_format_args = sys.argv[1:]
  path_to_clang_format = [os.getcwd(), 'buildtools']

  system = platform.system()
  if system == 'Linux':
    path_to_clang_format += ['linux64', 'clang-format']
  elif system == 'Darwin':
    path_to_clang_format += ['mac', 'clang-format']
  elif system == 'Windows':
    path_to_clang_format += ['win', 'clang-format.exe']
  else:
    sys.exit(1)

  clang_format_executable = os.path.join(*path_to_clang_format)
  sys.exit(subprocess.call([clang_format_executable] + clang_format_args))
