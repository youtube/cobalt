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
#
"""Tests the validate_sabi module."""

import _env  # pylint: disable=unused-import

import copy
import os
import unittest

from starboard.sabi import sabi_utils
from starboard.sabi import validate_sabi
from starboard.tools import paths

_TEST_SABI_PATH = os.path.join(paths.STARBOARD_ROOT, 'sabi', 'test',
                               'sabi.json')
_TEST_SABI_JSON = sabi_utils.LoadSabi(_TEST_SABI_PATH, None)


class ValidateSabiTest(unittest.TestCase):

  def testSunnyDayValidateSabi(self):
    self.assertTrue(validate_sabi.ValidateSabi(_TEST_SABI_JSON))

  def testSunnyDayValidateSabiAllSabiFiles(self):
    for root, _, files in os.walk(os.path.join(paths.STARBOARD_ROOT, 'sabi')):
      if os.path.basename(root) == 'default':
        continue
      if 'sabi.json' in files:
        sabi_json = sabi_utils.LoadSabi(os.path.join(root, 'sabi.json'), None)
        self.assertTrue(validate_sabi.ValidateSabi(sabi_json))

  def testRainyDayValidateSabiSabiJsonWithMissingEntry(self):
    sabi_json = copy.deepcopy(_TEST_SABI_JSON)
    _, _ = sabi_json.popitem()
    self.assertFalse(validate_sabi.ValidateSabi(sabi_json))

  def testRainyDayValidateSabiSabiJsonWithExtraEntry(self):
    sabi_json = copy.deepcopy(_TEST_SABI_JSON)
    sabi_json['invalid_key'] = 'invalid_value'
    self.assertFalse(validate_sabi.ValidateSabi(sabi_json))

  def testRainyDayValidateSabiSabiJsonWithInvalidEntry(self):
    sabi_json = copy.deepcopy(_TEST_SABI_JSON)
    sabi_json[sabi_json.keys()[0]] = 'invalid_value'
    self.assertFalse(validate_sabi.ValidateSabi(sabi_json))


if __name__ == '__main__':
  unittest.main()
