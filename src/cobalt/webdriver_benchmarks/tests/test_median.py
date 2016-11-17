#!/usr/bin/python2
# Copyright 2016 Google Inc. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ==============================================================================
"""Tests the functionality median function."""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import os
import sys
# The parent directory is a module
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.realpath(__file__))))

# pylint: disable=C6204,C6203
from tv_testcase import _median as median
import unittest


class MedianTest(unittest.TestCase):

  def test_empty_case(self):
    self.assertEqual(median([]), None)

  def test_one_item(self):
    self.assertEqual(median([1]), 1)

  def test_two_items(self):
    self.assertAlmostEqual(median([4, 1]), 2.5)

  def test_three_items(self):
    self.assertAlmostEqual(median([4, -4, -2]), -2)

  def test_four_items(self):
    self.assertAlmostEqual(median([4, 0, 1, -2]), 0.5)


if __name__ == '__main__':
  sys.exit(unittest.main())
