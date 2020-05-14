#!/usr/bin/env python

# Copyright 2020 The Cobalt Authors. All Rights Reserved.
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
"""Utility functions for interacting with Starboard ABI JSON files."""

import _env  # pylint: disable=unused-import

import json
import os
import re

from starboard.sabi import sabi
from starboard.tools import build
from starboard.tools import paths

SABI_SCHEMA_PATH = os.path.join(paths.STARBOARD_ROOT, 'sabi', 'schema')
SB_API_VERSION_FROM_SABI_RE = re.compile('sabi-v([0-9]+).json')


def _PlatformToSabiFile(platform):
  """Returns the Starboard ABI file for the given platform.

  Args:
    platform: The platform of the desired Starboard ABI file.

  Raises:
    ValueError: When |platform| is not provided, or when |platform| is provided
      and it fails to load the platform configuration.

  Returns:
    The path to the Starboard ABI file associated with the provided platform.
  """
  if not platform:
    raise ValueError('A platform must be specified.')
  platform_configuration = build.GetPlatformConfig(platform)
  if not platform_configuration:
    raise ValueError('Failed to get platform configuration.')
  filename = platform_configuration.GetPathToSabiJsonFile().format(
      sb_api_version=sabi.SB_API_VERSION)
  return os.path.join(paths.REPOSITORY_ROOT, filename)


def AddSabiArguments(arg_parser):
  group = arg_parser.add_mutually_exclusive_group(required=True)
  group.add_argument(
      '-f',
      '--filename',
      default=None,
      help='The Starboard ABI JSON file that should be used.',
  )
  group.add_argument(
      '-p',
      '--platform',
      default=None,
      help='The platform whose Starboard ABI JSON should be used.',
  )


def LoadSabi(filename=None, platform=None):
  """Returns the contents of the desired Starboard ABI file.

  This function will use either the provided |filename| or the provided
  |platform| to locate the desired Starboard ABI file.

  Args:
    filename: The path, can be relative or absolute, to the desired Starboard
      ABI file.
    platform: The platform of the desired Starboard ABI file.

  Raises:
    ValueError: When both |filename| and |platform| are provided.

  Returns:
    The contents of the desired Starboard ABI file.
  """
  if (filename and platform) or (not filename and not platform):
    raise ValueError('Either |filename| or |platform| must be provided.')
  if platform:
    filename = _PlatformToSabiFile(platform)
  with open(filename) as f:
    return json.load(f)['variables']


def LoadSabiSchema(filename=None, platform=None):
  """Returns the contents of the schema associated with the Starboard ABI file.

  Args:
    filename: The path, can be relative or absolute, to the desired Starboard
      ABI schema file.
    platform: A platform whose Starboard ABI file can be validated with the
      desired Starboard ABI schema file.

  Raises:
    ValueError: When both |filename| and |platform| are provided, or when
      |platform| is provided and Starboard API version could not be parsed
      from the associated Starboard ABI filename.

  Returns:
    The contents of the schema associated with the provided Starboard ABI file.
  """
  if (filename and platform) or (not filename and not platform):
    raise ValueError('Either |filename| or |platform| must be provided.')
  if platform:
    filename = _PlatformToSabiFile(platform)
    filename = os.path.basename(filename)
    match = SB_API_VERSION_FROM_SABI_RE.search(filename)
    if not match:
      raise ValueError(
          'The Starboard API version could not be parsed from the filename: {}'
          .format(filename))
    filename = os.path.join(
        SABI_SCHEMA_PATH, 'sabi-v{sb_api_version}.schema.json'.format(
            sb_api_version=match.group(1)))
  with open(filename) as f:
    return json.load(f)
