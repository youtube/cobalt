#!/usr/bin/env python3

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
"""Generates the ID of a specified Starboard ABI JSON file."""

import argparse
import json
import os
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), os.pardir, os.pardir))

from starboard.sabi import sabi_utils  # pylint:disable=wrong-import-position


def _GenerateSabiId(sabi_json, omaha):
  """Returns the Starboard ABI ID for |platform|.

  This function will generate the sorted, whitespace-stripped id of the
  Starboard ABI JSON file for the specified platform.

  When generating an ID to be used with Omaha, it will be stringified, and will
  have all double quotes escaped and curly braces double-escaped. The resulting
  value will be ready to be copied directly into Omaha as-is.

  Args:
    sabi_json: The contents of the Starboard ABI JSON that will be used.
    omaha: Whether or not this string will be used for Omaha.

  Returns:
    The final, processed id of the Starboard ABI JSON for this platform.
  """
  sabi_id = json.dumps(sabi_json, sort_keys=True)
  if omaha:
    sabi_id = '\\\\' + sabi_id[:-1] + '\\\\}'
    sabi_id = sabi_id.replace('"', '\\"')
    sabi_id = '"{}"'.format(sabi_id)
  return ''.join(sabi_id.split())


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
  sabi_utils.AddSabiArguments(arg_parser)
  args, _ = arg_parser.parse_known_args(argv)
  sabi_json = sabi_utils.LoadSabi(args.filename)
  return _GenerateSabiId(sabi_json, args.omaha)


def main():
  print(DoMain())
  return 0


if __name__ == '__main__':
  sys.exit(main())
