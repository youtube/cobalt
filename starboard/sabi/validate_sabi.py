#!/usr/bin/env python3

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

import argparse
import os
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), os.pardir, os.pardir))

from starboard.sabi import sabi_utils  # pylint:disable=wrong-import-position

# Starboard ABI files are considered invalid unless 'jsonschema' is installed
# and used to verify them.
try:
  import jsonschema
except ImportError:
  jsonschema = None


def ValidateSabi(sabi_json, schema_json):
  """Validates the provided Starboard ABI JSON with a jsonschema schema.

  Args:
    sabi_json: The Starboard ABI JSON to be validated.
    schema_json: The Starboard ABI schema JSON to validate with.

  Returns:
    |True| if the provided Starboard ABI JSON is valid. |False| if jsonschema
    could not be imported, or if the Starboard ABI JSON is invalid.
  """
  try:
    if jsonschema:
      jsonschema.validate(sabi_json, schema=schema_json)
      return True
    print("Please install 'jsonschema' to use this tool.")
  except (jsonschema.SchemaError, jsonschema.ValidationError) as e:
    print(e.message)
  return False


def main():
  arg_parser = argparse.ArgumentParser()
  sabi_utils.AddSabiArguments(arg_parser)
  args, _ = arg_parser.parse_known_args()
  return ValidateSabi(
      sabi_utils.LoadSabi(args.filename),
      sabi_utils.LoadSabiSchema(args.filename))


if __name__ == '__main__':
  sys.exit(main())
