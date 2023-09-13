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
"""Script for checking if we have the starboard python modules on the path."""

import os
import sys

SRC_DIR = os.path.abspath(os.path.join(
    os.path.dirname(__file__), os.pardir, os.pardir))

def main():
  abs_path_to_me = os.path.abspath(__file__)
  rel_path_to_me = os.path.relpath(__file__, start=SRC_DIR)
  for path in sys.path:
    path_to_check = os.path.abspath(os.path.join(path, rel_path_to_me))
    if os.path.exists(path_to_check):
      # The file exists, check if it's this repo or another one.
      print(str(path_to_check == abs_path_to_me).lower())
      return
  # No repos were found on the path.
  print('false')


if __name__ == '__main__':
  main()
