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
"""Tests the generate_sabi_id module."""

import _env  # pylint: disable=unused-import

import os
import tempfile
import unittest

from starboard.sabi import generate_sabi_id

_EXAMPLE_SABI = """
{
  "variables": {
    "target_arch": "toaster",
    "word_size": 1
  }
}"""
_EXAMPLE_SABI_ID = '"{\\"target_arch\\":\\"toaster\\",\\"word_size\\":1}"'
_EXAMPLE_SABI_ID_OMAHA = '"\\\\{\\"target_arch\\":\\"toaster\\",\\"word_size\\":1\\\\}"'


class GenerateSabiIdTest(unittest.TestCase):

  def setUp(self):
    self.sabi_json = tempfile.NamedTemporaryFile()
    self.sabi_json.write(_EXAMPLE_SABI)
    self.sabi_json.flush()

  def tearDown(self):
    self.sabi_json.close()

  def testRainyDayNoFileNoPlatform(self):
    with self.assertRaises(SystemExit):
      generate_sabi_id.DoMain([])

  def testRainyDayNonexistentFile(self):
    with self.assertRaises(IOError):
      generate_sabi_id.DoMain(['-f', 'invalid_filename'])

  def testRainyDayBadFile(self):
    bad_sabi_json = tempfile.NamedTemporaryFile()
    bad_sabi_json.write('{}')
    with self.assertRaises(ValueError):
      generate_sabi_id.DoMain(['-f', bad_sabi_json.name])

  def testRainyDayBadPlatform(self):
    with self.assertRaises(ValueError):
      generate_sabi_id.DoMain(['-p', 'ms-dos'])

  def testSunnyDayNotOmaha(self):
    sabi_id = generate_sabi_id.DoMain(['-f', self.sabi_json.name])
    self.assertEqual(_EXAMPLE_SABI_ID, sabi_id)

  def testSunnyDayOmaha(self):
    sabi_id = generate_sabi_id.DoMain(['-f', self.sabi_json.name, '-o'])
    self.assertEqual(_EXAMPLE_SABI_ID_OMAHA, sabi_id)


if __name__ == '__main__':
  unittest.main()
