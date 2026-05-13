#!/usr/bin/env python3
#
# Copyright 2024 The Cobalt Authors. All Rights Reserved.
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
"""Wrapper to run google-java-format tool."""

import platform
import subprocess
import sys

import os

if __name__ == '__main__':
  if platform.system() != 'Linux':
    sys.exit(0)

  google_java_format_args = sys.argv[1:]

  # Verify that the actual google-java-format binary is checked out locally,
  # rather than executing depot_tools' placeholder wrapper which exits with 2.
  primary_solution_path = os.path.abspath(
      os.path.join(os.path.dirname(__file__), '..', '..'))
  bin_path = os.path.join(primary_solution_path, 'third_party',
                          'google-java-format', 'google-java-format')
  if not os.path.exists(bin_path):
    print('google-java-format binary not found, skipping.')
    sys.exit(0)

  try:
    sys.exit(subprocess.call([bin_path] + google_java_format_args))
  except FileNotFoundError:
    print('google-java-format not found, skipping.')
    sys.exit(0)
