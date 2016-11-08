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
"""Unit tests for tv_testcase.py."""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import os
import sys
import unittest

# This directory is a package
sys.path.insert(0, os.path.abspath('.'))
# pylint: disable=C6203,C6203
import tv_testcase


class MedianTest(unittest.TestCase):

  def test_empty_case(self):
    self.assertEqual(tv_testcase._median([]), None)

  def test_one_item(self):
    self.assertEqual(tv_testcase._median([1]), 1)

  def test_two_items(self):
    self.assertAlmostEqual(tv_testcase._median([4, 1]), 2.5)

  def test_three_items(self):
    self.assertAlmostEqual(tv_testcase._median([4, -4, -2]), -2)

  def test_four_items(self):
    self.assertAlmostEqual(tv_testcase._median([4, 0, 1, -2]), 0.5)


class PercentileTest(unittest.TestCase):

  def test_one_item(self):
    self.assertEqual(tv_testcase._percentile(0.1, [1]), 1)
    self.assertEqual(tv_testcase._percentile(0.5, [2]), 2)
    self.assertEqual(tv_testcase._percentile(0.9, [3]), 3)

  def test_two_items(self):
    self.assertEqual(tv_testcase._percentile(0.1, [2, 1]), 1)
    self.assertEqual(tv_testcase._percentile(0.5, [2, 3]), 2.5)
    self.assertEqual(tv_testcase._percentile(0.9, [3, 4]), 3)
    self.assertEqual(tv_testcase._percentile(1, [3, 4]), 4)

  def test_five_items(self):
    self.assertEqual(tv_testcase._percentile(0.1, [2, 1, 3, 4, 5]), 1)
    self.assertEqual(tv_testcase._percentile(0.5, [2, 1, 3, 4, 5]), 3)
    self.assertEqual(tv_testcase._percentile(0.9, [2, 1, 3, 4, 5]), 4)
    self.assertEqual(tv_testcase._percentile(1, [2, 1, 3, 4, 5]), 5)

if __name__ == '__main__':
  sys.exit(unittest.main())
