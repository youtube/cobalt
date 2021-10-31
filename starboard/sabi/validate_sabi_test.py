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
#
"""Tests the validate_sabi module."""

import copy
import os
import sys
import unittest

sys.path.append(os.path.join(os.path.dirname(__file__), os.pardir, os.pardir))

# pylint: disable=wrong-import-position
from starboard.sabi import sabi_utils
from starboard.sabi import validate_sabi
from starboard.tools import paths

_TEST_SABI_JSON = sabi_utils.LoadSabi(
    os.path.join(paths.STARBOARD_ROOT, 'sabi', 'test', 'sabi.json'))
_TEST_SABI_SCHEMA = sabi_utils.LoadSabiSchema(
    os.path.join(paths.STARBOARD_ROOT, 'sabi', 'test', 'sabi.schema.json'))


class ValidateSabiTest(unittest.TestCase):

  def testSunnyDayValidateSabi(self):
    self.assertTrue(
        validate_sabi.ValidateSabi(_TEST_SABI_JSON, _TEST_SABI_SCHEMA))

  def testSunnyDayValidateSabiAllSabiFiles(self):
    for root, _, files in os.walk(os.path.join(paths.STARBOARD_ROOT, 'sabi')):
      if os.path.basename(root) == 'default':
        continue
      for f in files:
        match = sabi_utils.SB_API_VERSION_FROM_SABI_RE.search(f)
        if match:
          sabi_schema_path = os.path.join(
              sabi_utils.SABI_SCHEMA_PATH,
              'sabi-v{}.schema.json'.format(match.group(1)))
          sabi_json = sabi_utils.LoadSabi(os.path.join(root, f))
          sabi_schema = sabi_utils.LoadSabiSchema(sabi_schema_path)
          self.assertTrue(validate_sabi.ValidateSabi(sabi_json, sabi_schema))

  def testRainyDayValidateSabiSabiJsonWithMissingEntry(self):
    sabi_json = copy.deepcopy(_TEST_SABI_JSON)
    _, _ = sabi_json.popitem()
    self.assertFalse(validate_sabi.ValidateSabi(sabi_json, _TEST_SABI_SCHEMA))

  def testRainyDayValidateSabiSabiJsonWithExtraEntry(self):
    sabi_json = copy.deepcopy(_TEST_SABI_JSON)
    sabi_json['invalid_key'] = 'invalid_value'
    self.assertFalse(validate_sabi.ValidateSabi(sabi_json, _TEST_SABI_SCHEMA))

  def testRainyDayValidateSabiSabiJsonWithInvalidEntry(self):
    sabi_json = copy.deepcopy(_TEST_SABI_JSON)
    sabi_json[list(sabi_json.keys())[0]] = 'invalid_value'
    self.assertFalse(validate_sabi.ValidateSabi(sabi_json, _TEST_SABI_SCHEMA))


if __name__ == '__main__':
  unittest.main()
