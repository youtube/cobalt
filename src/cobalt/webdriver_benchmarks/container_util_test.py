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
# pylint: disable=g-import-not-at-top
import container_util


class MeanTest(unittest.TestCase):

  def test_empty_case(self):
    self.assertEqual(container_util.mean([]), None)

  def test_one_item(self):
    self.assertEqual(container_util.mean([1]), 1)

  def test_two_items(self):
    self.assertAlmostEqual(container_util.mean([4, 1]), 2.5)

  def test_three_items(self):
    self.assertAlmostEqual(container_util.mean([4, -4, 9]), 3)

  def test_four_items(self):
    self.assertAlmostEqual(container_util.mean([6, 0, 1, -2]), 1.25)


class MedianPercentileTest(unittest.TestCase):

  def test_empty_case(self):
    self.assertEqual(container_util.percentile([], 50), None)

  def test_one_item(self):
    self.assertEqual(container_util.percentile([1], 50), 1)

  def test_two_items(self):
    self.assertAlmostEqual(container_util.percentile([4, 1], 50), 2.5)

  def test_three_items(self):
    self.assertAlmostEqual(container_util.percentile([4, -4, -2], 50), -2)

  def test_four_items(self):
    self.assertAlmostEqual(container_util.percentile([4, 0, 1, -2], 50), 0.5)


class PercentileTest(unittest.TestCase):

  def test_one_item(self):
    self.assertEqual(container_util.percentile([1], 10), 1)
    self.assertEqual(container_util.percentile([2], 50), 2)
    self.assertEqual(container_util.percentile([3], 90), 3)

  def test_two_items(self):
    self.assertEqual(container_util.percentile([2, 1], 10), 1.1)
    self.assertEqual(container_util.percentile([2, 3], 50), 2.5)
    self.assertEqual(container_util.percentile([3, 4], 90), 3.9)
    self.assertEqual(container_util.percentile([3, 4], 100), 4)

  def test_five_items(self):
    self.assertEqual(container_util.percentile([2, 1, 3, 4, 5], 10), 1.4)
    self.assertEqual(container_util.percentile([2, 1, 3, 4, 5], 50), 3)
    self.assertEqual(container_util.percentile([2, 1, 3, 4, 5], 90), 4.6)
    self.assertEqual(container_util.percentile([2, 1, 3, 4, 5], 100), 5)


if __name__ == '__main__':
  sys.exit(unittest.main())
