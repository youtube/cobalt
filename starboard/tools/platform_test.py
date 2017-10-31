#!/usr/bin/env python
#
# Copyright 2017 Google Inc. All Rights Reserved.
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
"""Tests the platform module."""

import platform
import unittest


class PlatformTest(unittest.TestCase):

  def testGetAll(self):
    platform_names = platform.GetAll()
    self.assertNotEqual(0, len(platform_names))

  def testGet(self):
    platform_names = platform.GetAll()
    self.assertNotEqual(0, len(platform_names))
    platform_name = platform_names[0]
    platform_info = platform.Get(platform_name)
    self.assertTrue(platform_info)
    self.assertEqual(platform_name, platform_info.name)
    self.assertTrue(platform_info.path)

  def testIsValid(self):
    platform_names = platform.GetAll()
    for platform_name in platform_names:
      self.assertTrue(platform.IsValid(platform_name))
      self.assertTrue(platform.IsValid(platform_name.lower()))
      self.assertFalse(platform.IsValid(platform_name.upper()))
    self.assertFalse(platform.IsValid('invalidplatform'))


if __name__ == '__main__':
  unittest.main()
