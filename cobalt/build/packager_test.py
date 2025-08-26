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
#
"""Tests the packager module."""

import os
import tempfile
import unittest

from cobalt.build import packager


# pylint: disable=consider-using-with
class TestLayout(unittest.TestCase):
  """Tests packager.layout."""

  def setUp(self):
    self.archive_data = {
        'files': ['a.txt', 'b.txt'],
        'rename_files': [{
            'from_file': 'b.txt',
            'to_file': 'c.txt'
        }],
        'dirs': ['dir_a', 'dir_b'],
        'rename_dirs': [{
            'from_dir': 'dir_b',
            'to_dir': 'dir_c'
        }]
    }

    self.out_dir = tempfile.TemporaryDirectory()
    with open(
        os.path.join(self.out_dir.name, 'a.txt'), 'w', encoding='utf-8') as f:
      f.write('a')
    with open(
        os.path.join(self.out_dir.name, 'b.txt'), 'w', encoding='utf-8') as f:
      f.write('b')
    os.makedirs(os.path.join(self.out_dir.name, 'dir_a'))
    with open(
        os.path.join(self.out_dir.name, 'dir_a', 'dir_a.txt'),
        'w',
        encoding='utf-8') as f:
      f.write('dir_a')
    os.makedirs(os.path.join(self.out_dir.name, 'dir_b'))
    with open(
        os.path.join(self.out_dir.name, 'dir_b', 'dir_b.txt'),
        'w',
        encoding='utf-8') as f:
      f.write('dir_b')

    self.base_dir = tempfile.TemporaryDirectory()

  def tearDown(self):
    self.out_dir.cleanup()
    self.base_dir.cleanup()

  def test_layout(self):
    packager.layout(self.archive_data, self.out_dir.name, self.base_dir.name)
    self.assertTrue(os.path.isfile(os.path.join(self.base_dir.name, 'a.txt')))
    self.assertTrue(os.path.isfile(os.path.join(self.base_dir.name, 'c.txt')))
    self.assertTrue(
        os.path.isfile(os.path.join(self.base_dir.name, 'dir_a', 'dir_a.txt')))
    self.assertTrue(
        os.path.isfile(os.path.join(self.base_dir.name, 'dir_c', 'dir_b.txt')))


if __name__ == '__main__':
  unittest.main()
