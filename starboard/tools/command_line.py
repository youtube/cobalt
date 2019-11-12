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
"""Common command line parser for all Starboard tools.

Arguments can be added to the parser returned from CreateParser() on a
per-tool basis.
"""

import argparse

import _env  # pylint: disable=unused-import
from starboard.tools import build
import starboard.tools.config
import starboard.tools.platform


def CreateParser():
  """Returns an argparse.ArgumentParser object set up for Starboard tools."""
  arg_parser = CreatePlatformConfigParser(
      description='Runs application/tool executables.')
  arg_parser.add_argument(
      '-d', '--device_id', help='Devkit or IP address for the target device.')
  arg_parser.add_argument(
      '--target_params',
      help='Command line arguments to pass to the executable.'
      ' Because different executables could have differing command'
      ' line syntax, list all arguments exactly as you would to the'
      ' executable between a set of double quotation marks.')
  arg_parser.add_argument(
      '-o',
      '--out_directory',
      help='Directory containing tool binaries or their components.'
      ' Automatically derived if absent.')
  return arg_parser


def CreatePlatformConfigParser(description=None, **kwargs):
  """An arg parser suitable for building.

  Args:
    description: Description passed onto argument parser, if none then a default
      is assigned.
    **kwargs: are constructor arguments for the argparser.

  Returns:
    An arg parser ready to parse a command argument string.
  """
  if description is None:
    description = 'Runs build related tool.'
  arg_parser = argparse.ArgumentParser(description=description, **kwargs)
  default_config, default_platform = build.GetDefaultConfigAndPlatform()
  arg_parser.add_argument(
      '-p',
      '--platform',
      choices=starboard.tools.platform.GetAll(),
      default=default_platform,
      required=not default_platform,
      help="Device platform, eg 'linux-x64x11'. Requires that you have "
      'already run gyp_cobalt for the desired platform.')
  arg_parser.add_argument(
      '-c',
      '--config',
      choices=starboard.tools.config.GetAll(),
      default=default_config,
      required=not default_config,
      help="Build config (eg, 'qa' or 'devel')")
  arg_parser.add_argument(
      '-P',
      '--loader_platform',
      help='Specifies the platform to build the loader with. This flag is only '
      'relevant for Evergreen builds, and should be the platform you intend to '
      "run your tests on (eg 'linux-x64x11', or 'raspi-2'). Requires that "
      '--loader_config be given, and that you have already run gyp_cobalt for '
      'the desired loader platform.')
  arg_parser.add_argument(
      '-C',
      '--loader_config',
      help="Specifies the config to build the loader with (eg 'qa' or 'devel'). This flag is only "
      'relevant for Evergreen builds, and requires that --loader_platform be '
      'given.')
  return arg_parser
