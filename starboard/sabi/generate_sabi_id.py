#!/usr/bin/env python

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
"""Generates the ID of the Starboard ABI JSON for a given platform."""

import _env  # pylint: disable=unused-import

import argparse
import json
import os
import sys

from starboard.tools import build
from starboard.tools import paths


def _GenerateSabiId(filename, platform, omaha):
  """Returns the Starboard ABI ID for |platform|.

  This function will generate the sorted, whitespace-stripped id of the
  Starboard ABI JSON file for the specified platform.

  When generating an ID to be used with Omaha, it will be stringified, and will
  have all double quotes escaped and curly braces double-escaped. The resulting
  value will be ready to be copied directly into Omaha as-is.

  Args:
    filename: The Starboard ABI JSON that will be used.
    platform: The platform whose Starboard ABI JSON will be used.
    omaha: Whether or not this string will be used for Omaha.

  Returns:
    The final, processed id of the Starboard ABI JSON for this platform.
  """
  if platform:
    platform_configuration = build.GetPlatformConfig(platform)
    if not platform_configuration:
      raise ValueError('Failed to get platform configuration.')
    filename = platform_configuration.GetPathToSabiJsonFile()
    filename = os.path.join(paths.REPOSITORY_ROOT, filename)
  with open(filename) as f:
    sabi_json = json.load(f)
    if 'variables' not in sabi_json:
      raise ValueError("'variables' entry not found in Starboard ABI file.")
    sabi_id = json.dumps(sabi_json['variables'], sort_keys=True)
    if omaha:
      sabi_id = '\\\\' + sabi_id[:-1] + '\\\\}'
      sabi_id = sabi_id.replace('"', '\\"')
      sabi_id = '"{}"'.format(sabi_id)
    return ''.join(sabi_id.split())
  return ''


def DoMain(argv=None):
  """Function to allow the use of this script from GYP's pymod_do_main."""
  arg_parser = argparse.ArgumentParser()
  arg_parser.add_argument(
      '-o',
      '--omaha',
      action='store_true',
      default=False,
      help='Whether or not this string will be used for Omaha.',
  )
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
  args, _ = arg_parser.parse_known_args(argv)
  return _GenerateSabiId(
      filename=args.filename, platform=args.platform, omaha=args.omaha)


def main():
  print(DoMain())
  return 0


if __name__ == '__main__':
  sys.exit(main())
