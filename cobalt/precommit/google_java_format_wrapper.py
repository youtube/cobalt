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

import os
import platform
import shutil
import subprocess
import sys

if __name__ == '__main__':
  if platform.system() != 'Linux':
    sys.exit(0)

  gjf = (
      shutil.which('google-java-format') or
      shutil.which('google-java-format', path='/usr/bin'))
  if not gjf:
    if os.environ.get('CI') == 'true':
      print('google-java-format not found in CI.', file=sys.stderr)
      sys.exit(1)
    print('google-java-format not found, skipping.')
    sys.exit(0)

  google_java_format_args = sys.argv[1:]
  try:
    sys.exit(subprocess.call([gjf] + google_java_format_args))
  except FileNotFoundError:
    print('google-java-format not found, skipping.')
    sys.exit(0)
