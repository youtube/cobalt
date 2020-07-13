#!/usr/bin/python
#
# Copyright 2017 The Cobalt Authors. All Rights Reserved.
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
"""Adds common command-line arguments to a provided argument parser."""

import _env  # pylint: disable=unused-import


def AddPlatformConfigArguments(arg_parser):
  """Adds the platform configuration arguments required for building."""
  arg_parser.add_argument(
      '-p',
      '--platform',
      required=True,
      help="Device platform, eg 'linux-x64x11'. Requires that you have "
      'already run gyp_cobalt for the desired platform.')
  arg_parser.add_argument(
      '-c',
      '--config',
      required=True,
      help="Build config (eg, 'qa' or 'devel')")
  arg_parser.add_argument(
      '-P',
      '--loader_platform',
      default=None,
      help='Specifies the platform to build the loader with. This flag is only '
      'relevant for Evergreen builds, and should be the platform you intend to '
      "run your tests on (eg 'linux-x64x11', or 'raspi-2'). Requires that "
      '--loader_config be given, and that you have already run gyp_cobalt for '
      'the desired loader platform.')
  arg_parser.add_argument(
      '-C',
      '--loader_config',
      default=None,
      help="Specifies the config to build the loader with (eg 'qa' or 'devel')."
      'This flag is only relevant for Evergreen builds, and requires that '
      '--loader_platform be given.')
