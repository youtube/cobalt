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
"""Validates a specified Starboard ABI JSON file."""

import _env  # pylint: disable=unused-import

import argparse
import json
import os
import sys

from starboard.sabi import sabi_utils
from starboard.tools import paths

# Starboard ABI files are considered invalid unless 'jsonschema' is installed
# and used to verify them.
try:
  import jsonschema
except ImportError:
  jsonschema = None

_STARBOARD_ABI_JSON_SCHEMA_PATH = os.path.join(paths.STARBOARD_ROOT, 'sabi',
                                               'sabi.schema.json')


def ValidateSabi(sabi_json):
  """Validates the provided Starboard ABI JSON with a jsonschema schema.

  Args:
    sabi_json: The Starboard ABI JSON to be validated.

  Returns:
    |True| if the provided Starboard ABI JSON is valid. |False| if jsonschema
    could not be imported, or if the Starboard ABI JSON is invalid.
  """
  with open(_STARBOARD_ABI_JSON_SCHEMA_PATH) as f:
    schema = json.load(f)
  try:
    if jsonschema:
      jsonschema.validate(sabi_json, schema=schema)
      return True
    print("Please install 'jsonschema' to use this tool.")
  except (jsonschema.SchemaError, jsonschema.ValidationError) as e:
    print(e.message)
  return False


def main():
  arg_parser = argparse.ArgumentParser()
  sabi_utils.AddSabiArguments(arg_parser)
  args, _ = arg_parser.parse_known_args()
  return ValidateSabi(sabi_utils.LoadSabi(args.filename, args.platform))


if __name__ == '__main__':
  sys.exit(main())
