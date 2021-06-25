#!/usr/bin/env python3
#
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
"""Wrapper script to run arbitrary bash files from GN.

This script should be used only when absolutely necessary and never in a
cross-platform way (that is, it should only be used for an action on a
particular platform, not an platform-independent target).
"""

import logging
import subprocess
import sys

if __name__ == '__main__':
  logging_format = '[%(levelname)s:%(filename)s:%(lineno)s] %(message)s'
  logging.basicConfig(
      level=logging.INFO, format=logging_format, datefmt='%H:%M:%S')
  logging.warning('Calling a bash process during GN build. '
                  'Avoid doing this whenever possible.')

  sys.exit(subprocess.call(sys.argv[1:]))
