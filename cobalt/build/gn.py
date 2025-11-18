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

# pylint: disable=missing-module-docstring,missing-function-docstring

import argparse
import os
import subprocess
from pathlib import Path
from typing import List

_BUILDS_DIRECTORY = 'out'

_BUILD_TYPES = {
    'debug': {
        'symbol_level': 2,
        'is_debug': 'true'
    },
    'devel': {
        'symbol_level': 1,
        'is_debug': 'false'
    },
    'qa': {
        'symbol_level': 1,
        'is_official_build': 'true',
        # Enable stack traces, disabled by is_official_build
        'exclude_unwind_tables': 'false'
    },
    'gold': {
        'symbol_level': 1,
        'is_official_build': 'true',
        'cobalt_is_release_build': 'true'
    }
}

_CHROMIUM_PLATFORMS = [
    'chromium_linux-x64x11',
    'chromium_android-arm',
    'chromium_android-arm64',
    'chromium_android-x86',
]
_COBALT_LINUX_PLATFORMS = [
    'linux-x64x11',
    'linux-x64x11-evergreen',
]
_COBALT_ANDROID_PLATFORMS = [
    'android-arm',
    'android-arm64',
    'android-x86',
]


# Build flags should be set more broadly. Do not add conditions or build flags
# to this function.
def write_targeted_build_args(f, platform, build_type):
  """Write very targeted build arguments. Do not add to this function."""
  if platform in _COBALT_LINUX_PLATFORMS and build_type in ['devel', 'debug']:
    f.write('angle_use_x11 = true\n')


def write_build_args(build_args_path, platform_args_path, build_type, use_rbe,
                     platform):
  """ Write args file, modifying settings for config"""
  gen_comment = '# Set by gn.py'
  with open(build_args_path, 'w', encoding='utf-8') as f:
    f.write(f'use_siso = false {gen_comment}\n')
    if use_rbe:
      f.write(f'use_remoteexec = true {gen_comment}\n')
    f.write(
        f'rbe_cfg_dir = rebase_path("//cobalt/reclient_cfgs") {gen_comment}\n')
    f.write(f'build_type = "{build_type}" {gen_comment}\n')
    write_targeted_build_args(f, platform, build_type)
    for key, value in _BUILD_TYPES[build_type].items():
      f.write(f'{key} = {value} {gen_comment}\n')
    f.write(f'import("//{platform_args_path}")\n')


def configure_out_directory(out_directory: str, platform: str, build_type: str,
                            use_rbe: bool, gn_gen_args: List[str]):
  Path(out_directory).mkdir(parents=True, exist_ok=True)
  platform_path = f'cobalt/build/configs/{platform}'
  dst_args_gn_file = os.path.join(out_directory, 'args.gn')
  src_args_gn_file = os.path.join(platform_path, 'args.gn')

  if os.path.exists(dst_args_gn_file):
    # Copy the stale args.gn into stale_args.gn
    stale_dst_args_gn_file = dst_args_gn_file.replace('args', 'stale_args')
    os.rename(dst_args_gn_file, stale_dst_args_gn_file)
    print(f' Warning: {dst_args_gn_file} is rewritten.'
          f' Old file is copied to {stale_dst_args_gn_file}.'
          'In general, if the file exists, you should run'
          ' `gn args <out_directory>` to edit it instead.')

  write_build_args(dst_args_gn_file, src_args_gn_file, build_type, use_rbe,
                   platform)

  gn_command = ['gn', 'gen', out_directory] + gn_gen_args
  print(' '.join(gn_command))
  subprocess.check_call(gn_command)


def parse_args():
  parser = argparse.ArgumentParser()
  builds_directory_group = parser.add_mutually_exclusive_group()
  builds_directory_group.add_argument(
      'out_directory',
      type=str,
      nargs='?',
      help='Path to the directory to build in.')

  parser.add_argument(
      '-p',
      '--platform',
      default='linux-x64x11',
      choices=_CHROMIUM_PLATFORMS + _COBALT_LINUX_PLATFORMS +
      _COBALT_ANDROID_PLATFORMS,
      help='The platform to build.')
  parser.add_argument(
      '-c',
      '-C',
      '--build_type',
      default='devel',
      choices=_BUILD_TYPES.keys(),
      help='The build_type (configuration) to build with.')
  parser.add_argument(
      '--no-check',
      default=False,
      action='store_true',
      help='Pass this flag to disable the header dependency gn check.')
  parser.add_argument(
      '--no-rbe',
      default=False,
      action='store_true',
      help='Pass this flag to disable Remote Build Execution.')
  script_args, gen_args = parser.parse_known_args()

  if script_args.platform == 'linux':
    script_args.platform = 'linux-x64x11'

  if not script_args.no_check:
    gen_args.append('--check')

  return script_args, gen_args


def main():
  script_args, gen_args = parse_args()
  if script_args.out_directory:
    builds_out_directory = script_args.out_directory
  else:
    builds_out_directory = os.path.join(
        _BUILDS_DIRECTORY, f'{script_args.platform}_{script_args.build_type}')
  configure_out_directory(builds_out_directory, script_args.platform,
                          script_args.build_type, not script_args.no_rbe,
                          gen_args)


if __name__ == '__main__':
  main()
