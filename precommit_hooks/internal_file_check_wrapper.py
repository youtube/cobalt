#!/usr/bin/env python3
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
"""Wrapper for internal_file_check.py."""

import sys
try:
  from internal.internal_file_check import CheckFiles
except ImportError:
  print('internal_file_check.py not found, skipping')
  sys.exit(0)

if __name__ == '__main__':
  sys.exit(CheckFiles(sys.argv[1:]))
