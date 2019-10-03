#!/usr/bin/python
# Copyright 2019 The Cobalt Authors. All Rights Reserved.
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
"""Ensures a directory exists, similar to 'mkdir -p'."""

import argparse
import os
import sys

import _env  # pylint: disable=unused-import, relative-import
from starboard.tools import port_symlink


def main():
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument(
      '-c', '--clean',
      action='store_true',
      help='Delete the contents of any existing directory.')
  parser.add_argument(
      '-s', '--stamp_file',
      type=str,
      help='Path to the stamp file to touch after making the directory.')
  parser.add_argument('directory', help='Path to the directory to be created.')
  arguments = parser.parse_args()

  if arguments.clean:
    port_symlink.Rmtree(arguments.directory)

  if not os.path.isdir(arguments.directory):
    os.makedirs(arguments.directory)

  if arguments.stamp_file:
    open(arguments.stamp_file, 'a').close()


if __name__ == '__main__':
  sys.exit(main())
