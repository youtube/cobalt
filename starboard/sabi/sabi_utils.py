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
"""Utility functions for interacting with Starboard ABI JSON files."""

import json
import os
import re
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), os.pardir, os.pardir))

from starboard.tools import paths  # pylint:disable=wrong-import-position

SABI_SCHEMA_PATH = os.path.join(paths.STARBOARD_ROOT, 'sabi', 'schema')
SB_API_VERSION_FROM_SABI_RE = re.compile('sabi-v([0-9]+).json')


def AddSabiArguments(arg_parser):
  arg_parser.add_argument(
      '-f',
      '--filename',
      default=None,
      help='The Starboard ABI JSON file that should be used.',
  )


def LoadSabi(filename):
  """Returns the contents of the desired Starboard ABI file.

  This function will use the provided |filename| to locate the desired
  Starboard ABI file.

  Args:
    filename: The path, can be relative or absolute, to the desired Starboard
      ABI file.

  Returns:
    The contents of the desired Starboard ABI file.
  """
  with open(filename) as f:
    return json.load(f)['variables']


def LoadSabiSchema(filename):
  """Returns the contents of the schema associated with the Starboard ABI file.

  Args:
    filename: The path, can be relative or absolute, to the desired Starboard
      ABI schema file.

  Returns:
    The contents of the schema associated with the provided Starboard ABI file.
  """
  with open(filename) as f:
    return json.load(f)
