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
"""Tests the generate_sabi_id module."""

import json
import os
import sys
import tempfile
import unittest

sys.path.append(os.path.join(os.path.dirname(__file__), os.pardir, os.pardir))

# pylint: disable=wrong-import-position
from starboard.sabi import generate_sabi_id
from starboard.tools import paths

_TEST_SABI = os.path.join(paths.STARBOARD_ROOT, 'sabi', 'test', 'sabi.json')
_TEST_SABI_ID = '{"alignment_char":1,"alignment_double":8,"alignment_float":4,"alignment_int":4,"alignment_llong":8,"alignment_long":8,"alignment_pointer":8,"alignment_short":2,"calling_convention":"sysv","endianness":"little","floating_point_abi":"","floating_point_fpu":"","sb_api_version":12,"signedness_of_char":"signed","signedness_of_enum":"signed","size_of_char":1,"size_of_double":8,"size_of_enum":4,"size_of_float":4,"size_of_int":4,"size_of_llong":8,"size_of_long":8,"size_of_pointer":8,"size_of_short":2,"target_arch":"x64","target_arch_sub":"","word_size":64}'
_TEST_SABI_ID_OMAHA = '"\\\\{\\"alignment_char\\":1,\\"alignment_double\\":8,\\"alignment_float\\":4,\\"alignment_int\\":4,\\"alignment_llong\\":8,\\"alignment_long\\":8,\\"alignment_pointer\\":8,\\"alignment_short\\":2,\\"calling_convention\\":\\"sysv\\",\\"endianness\\":\\"little\\",\\"floating_point_abi\\":\\"\\",\\"floating_point_fpu\\":\\"\\",\\"sb_api_version\\":12,\\"signedness_of_char\\":\\"signed\\",\\"signedness_of_enum\\":\\"signed\\",\\"size_of_char\\":1,\\"size_of_double\\":8,\\"size_of_enum\\":4,\\"size_of_float\\":4,\\"size_of_int\\":4,\\"size_of_llong\\":8,\\"size_of_long\\":8,\\"size_of_pointer\\":8,\\"size_of_short\\":2,\\"target_arch\\":\\"x64\\",\\"target_arch_sub\\":\\"\\",\\"word_size\\":64\\\\}"'


class GenerateSabiIdTest(unittest.TestCase):

  def testRainyDayNoFileNoPlatform(self):
    with self.assertRaises(TypeError):
      generate_sabi_id.DoMain([])

  def testRainyDayNonexistentFile(self):
    with self.assertRaises(IOError):
      generate_sabi_id.DoMain(['-f', 'invalid_filename'])

  def testRainyDayBadFile(self):
    bad_sabi_json = tempfile.NamedTemporaryFile(mode='w')
    bad_sabi_json.write('{}')
    with self.assertRaises(json.decoder.JSONDecodeError):
      generate_sabi_id.DoMain(['-f', bad_sabi_json.name])

  def testSunnyDayNotOmaha(self):
    sabi_id = generate_sabi_id.DoMain(['-f', _TEST_SABI])
    self.assertEqual(_TEST_SABI_ID, sabi_id)

  def testSunnyDayOmaha(self):
    sabi_id = generate_sabi_id.DoMain(['-f', _TEST_SABI, '-o'])
    self.assertEqual(_TEST_SABI_ID_OMAHA, sabi_id)


if __name__ == '__main__':
  unittest.main()
