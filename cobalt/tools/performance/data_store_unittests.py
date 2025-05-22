#!/usr/bin/env python3
#
# Copyright 2025 The Cobalt Authors. All Rights Reserved.
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
"""Tests the collection of monitoring data."""

import unittest

from data_store import MonitoringDataStore


class MonitoringDataTest(unittest.TestCase):

  def testAddSnapshotSingle(self):
    store = MonitoringDataStore()
    test_data = {}
    test_data['key1'] = 'val1'
    test_data['key2'] = 'val2'
    test_data['key3'] = ['val3', 'val4']

    store.add_snapshot(test_data)
    self.assertEqual(test_data, store.get_all_data()[0])

  def testAddSnapshotMultiple(self):
    store = MonitoringDataStore()
    for i in range(10):
      test_data = {}
      test_data['key' + str(i)] = i
      test_data[str(i) + 'key'] = [i, i + 1]

    for data_entry in store.get_all_data():
      self.assertTrue(data_entry in test_data)

  def testSnapshotNotModifiable(self):
    store = MonitoringDataStore()
    test_data = {}
    test_data['key1'] = 'val1'
    test_data['key2'] = 'val2'
    test_data['key3'] = ['val3', 'val4']

    store.add_snapshot(test_data)
    data_copy = store.get_all_data()[0]
    self.assertEqual(test_data, data_copy)

    data_copy['key1'] = 1234
    self.assertNotEqual(test_data, data_copy)
