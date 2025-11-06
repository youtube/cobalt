#!/usr/bin/env python
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
"""Tests for update_build_gn."""

import os
import shutil
import tempfile
import unittest

import update_build_gn

import json


class UpdateBuildGnTest(unittest.TestCase):

  def setUp(self):
    self.temp_dir = tempfile.mkdtemp()
    self.content_test_data = os.path.join(self.temp_dir, 'content', 'test',
                                          'data')
    os.makedirs(self.content_test_data)

    with open(
        os.path.join(self.content_test_data, 'file1.txt'),
        'w',
        encoding='utf-8') as f:
      f.write('test')
    with open(
        os.path.join(self.content_test_data, 'file2.html'),
        'w',
        encoding='utf-8') as f:
      f.write('test')

  def tearDown(self):
    shutil.rmtree(self.temp_dir)

  def test_create_json_data_file(self):
    output_path = os.path.join(self.temp_dir, 'output.json')
    update_build_gn.create_json_data_file(self.temp_dir, output_path)
    self.assertTrue(os.path.exists(output_path))

    with open(output_path, 'r', encoding='utf-8') as f:
      content = json.load(f)

    expected_content = [
        '//content/test/data/file1.txt',
        '//content/test/data/file2.html',
    ]
    self.assertEqual(content, expected_content)


if __name__ == '__main__':
  unittest.main()
