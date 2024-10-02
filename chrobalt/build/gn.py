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


# Parsing code Lifted from tools/code_coverage/coverage.py
def get_build_args(build_args_path):
    """Parses args.gn file and returns results as original contents, and dictionary."""
    assert os.path.exists(build_args_path), (
        f'{build_args_path} is not a build directory, '
        'missing args.gn file.')
    dict_settings = {}
    with open(build_args_path, encoding='utf-8') as build_args_file:
        build_args_lines = build_args_file.readlines()

    for build_arg_line in build_args_lines:
        build_arg_without_comments = build_arg_line.split('#')[0]
        key_value_pair = build_arg_without_comments.split('=')
        if len(key_value_pair) == 2:
            key = key_value_pair[0].strip()
            # Values are wrapped within a pair of double-quotes, so remove the leading
            # and trailing double-quotes.
            value = key_value_pair[1].strip().strip('"')
            dict_settings[key] = value

    return build_args_lines, dict_settings


_BUILD_TYPES = {
    'debug': {
        'symbol_level': 2,
        'is_debug': 'false',
        'is_asan': 'false',
        'is_lsan': 'false',
        'is_msan': 'false',
    },
    'devel': {
        'symbol_level': 1,
        'is_debug': 'false',
        'is_asan': 'false',
        'is_lsan': 'false',
        'is_msan': 'false',
    },
    'qa': {
        'symbol_level': 1,
        'is_debug': 'false'
    },
    'gold': {
        'symbol_level': 0,
        'is_official_build': 'true'
    }
}

CC_WRAPPER = "sccache"
CONTROLLED_ARGS = [
    'cc_wrapper',  # See build/toolschain/cc_wrapper.gni
    'is_debug',  # See build/config/BUILDCONFIG.GN
    'is_official_build',  # mutually exclusive with is_debug
    'symbol_level',  # See build/config/compiler/compiler.gni
    'is_asan',  # See build/config/sanitizers/sanitizers.gni
    'is_lsan',  # -"-
    'is_msan'  # -"-
]


def write_build_args(build_args_path, original_lines, dict_settings,
                     build_type):
    """ Write args file, modifying settings for config"""
    controlled_args = [(k, dict_settings[k]) for k in CONTROLLED_ARGS
                       if k in dict_settings]
    if controlled_args:
        raise RuntimeError(
            f'The following args cannot be set in configs: {controlled_args}')
    gen_comment = '# Set by gn.py'
    with open(build_args_path, 'w', encoding='utf-8') as f:
        f.write(f'cc_wrapper = "{CC_WRAPPER}" {gen_comment}\n')
        f.write(f'build_type = "{build_type}" {gen_comment}\n')
        for key, value in _BUILD_TYPES[build_type].items():
            f.write(f'{key} = {value} {gen_comment}\n')
        for line in original_lines:
            f.write(line)


def main(out_directory: str, platform: str, build_type: str,
         overwrite_args: bool, gn_gen_args: List[str]):
    platform_path = f'chrobalt/build/configs/{platform}'
    dst_args_gn_file = os.path.join(out_directory, 'args.gn')
    src_args_gn_file = os.path.join(platform_path, 'args.gn')
    Path(out_directory).mkdir(parents=True, exist_ok=True)

    if overwrite_args or not os.path.exists(dst_args_gn_file):
        build_args = get_build_args(src_args_gn_file)
        write_build_args(dst_args_gn_file, build_args[0], build_args[1],
                         build_type)
    else:
        print(f'{dst_args_gn_file} already exists.' +
              ' Running ninja will regenerate build files automatically.')

    gn_command = ['gn', 'gen', out_directory] + gn_gen_args
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

    parser.add_argument(
        '-p',
        '--platform',
        default='linux',
        choices=['linux-x64x11',
                 'android-arm', 
                 'android-arm64',
                 'android-x86', 
                 'linux'],
        help='The platform to build.')
    parser.add_argument('-c',
                        '-C',
                        '--build_type',
                        default='devel',
                        choices=_BUILD_TYPES.keys(),
                        help='The build_type (configuration) to build with.')
    parser.add_argument(
        '--overwrite_args',
        default=False,
        action='store_true',
        help=
        'Whether or not to overwrite an existing args.gn file if one exists '
        'in the out directory. In general, if the file exists, you should run '
        '`gn args <out_directory>` to edit it instead.')
    parser.add_argument(
        '--no-check',
        default=False,
        action='store_true',
        help='Pass this flag to disable the header dependency gn check.')
    script_args, gen_args = parser.parse_known_args()

    if script_args.platform == 'linux':
        script_args.platform = 'linux-x64x11'

    if not script_args.no_check:
        gen_args.append('--check')

    if script_args.out_directory:
        builds_out_directory = script_args.out_directory
    else:
        BUILDS_DIRECTORY = 'out'
        builds_out_directory = os.path.join(
            BUILDS_DIRECTORY,
            f'{script_args.platform}_{script_args.build_type}')
    main(builds_out_directory, script_args.platform, script_args.build_type,
         script_args.overwrite_args, gen_args)
