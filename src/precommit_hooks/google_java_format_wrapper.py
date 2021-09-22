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
"""Wrapper to check for google-java-format tool."""

import platform
import subprocess
import sys

if __name__ == '__main__':
  if platform.system() != 'Linux':
    sys.exit(0)

  google_java_format_args = sys.argv[1:]
  try:
    sys.exit(subprocess.call(['google-java-format'] + google_java_format_args))
  except FileNotFoundError:
    print('google-java-format not found, skipping.')
    sys.exit(0)
