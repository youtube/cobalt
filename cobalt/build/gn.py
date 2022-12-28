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
"""Thin wrapper for building Starboard platforms with GN."""

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path
from typing import List

from starboard.build.platforms import PLATFORMS

_BUILD_TYPES = ['debug', 'devel', 'qa', 'gold']


def main(out_directory: str, platform: str, build_type: str,
         overwrite_args: bool, gn_gen_args: List[str]):
  platform_path = PLATFORMS[platform]
  dst_args_gn_file = os.path.join(out_directory, 'args.gn')
  src_args_gn_file = os.path.join(platform_path, 'args.gn')

  Path(out_directory).mkdir(parents=True, exist_ok=True)

  if overwrite_args or not os.path.exists(dst_args_gn_file):
    shutil.copy(src_args_gn_file, dst_args_gn_file)
    with open(dst_args_gn_file, 'a') as f:
      f.write(f'build_type = "{build_type}"\n')
  else:
    print(f'{dst_args_gn_file} already exists.' +
          ' Running ninja will regenerate build files automatically.')

  gn_command = [
      'gn', '--script-executable={}'.format(sys.executable), 'gen',
      out_directory
  ] + gn_gen_args
  print(' '.join(gn_command))
  subprocess.check_call(gn_command)


if __name__ == '__main__':
  parser = argparse.ArgumentParser()

  builds_directory_group = parser.add_mutually_exclusive_group()
  builds_directory_group.add_argument(
      'out_directory',
      type=str,
      nargs='?',
      help='Path to the directory to build in.')
  builds_directory_group.add_argument(
      '-b',
      '--builds_directory',
      type=str,
      help='Path to the directory a named build folder, <platform>_<config>/, '
      'will be created in.')
  parser.add_argument(
      '-p',
      '--platform',
      required=True,
      choices=list(PLATFORMS),
      help='The platform to build.')
  parser.add_argument(
      '-c',
      '-C',
      '--build_type',
      default='devel',
      choices=_BUILD_TYPES,
      help='The build_type (configuration) to build with.')
  parser.add_argument(
      '--overwrite_args',
      default=False,
      action='store_true',
      help='Whether or not to overwrite an existing args.gn file if one exists '
      'in the out directory. In general, if the file exists, you should run '
      '`gn args <out_directory>` to edit it instead.')
  known_args, unknown_args = parser.parse_known_args()

  if known_args.out_directory:
    builds_out_directory = known_args.out_directory
  else:
    builds_directory = os.getenv('COBALT_BUILDS_DIRECTORY',
                                 known_args.builds_directory or 'out')
    builds_out_directory = os.path.join(
        builds_directory, f'{known_args.platform}_{known_args.build_type}')

  main(builds_out_directory, known_args.platform, known_args.build_type,
       known_args.overwrite_args, unknown_args)
