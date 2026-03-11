#!/usr/bin/env python3
# Copyright 2026 The Cobalt Authors. All Rights Reserved.
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
"""Tests for expand_filelist.py."""

import io
import os
import shutil
import tempfile
import unittest
from unittest.mock import patch
import sys

# Add the current directory to sys.path to import expand_filelist
sys.path.append(os.path.dirname(os.path.abspath(__file__)))
# pylint: disable=wrong-import-position
import expand_filelist


class ExpandFilelistTest(unittest.TestCase):

  def setUp(self):
    self.test_dir = tempfile.mkdtemp()
    self.root_dir = os.path.join(self.test_dir, 'root')
    os.makedirs(self.root_dir)

    # Create some dummy files
    self.create_file('a.txt')
    self.create_file('b.txt')
    self.create_file('dir1/c.txt')
    self.create_file('dir1/d.txt')
    self.create_file('dir1/subdir/e.txt')
    self.create_file('dir2/f.txt')

  def tearDown(self):
    shutil.rmtree(self.test_dir)

  def create_file(self, rel_path):
    full_path = os.path.join(self.root_dir, rel_path)
    os.makedirs(os.path.dirname(full_path), exist_ok=True)
    with open(full_path, 'w', encoding='utf-8') as f:
      f.write('dummy content')

  def run_expand(self, lines):
    filelist_path = os.path.join(self.test_dir, 'test.filelist')
    with open(filelist_path, 'w', encoding='utf-8') as f:
      for line in lines:
        f.write(line + '\n')

    with patch('sys.stdout', new=io.StringIO()) as mock_stdout:
      with patch(
          'sys.argv',
          ['expand_filelist.py', filelist_path, '--root', self.root_dir]):
        expand_filelist.main()
        output = mock_stdout.getvalue().splitlines()
    return output

  def test_basic_inclusion(self):
    lines = ['//a.txt', '//dir1/c.txt']
    output = self.run_expand(lines)
    self.assertEqual(sorted(output), ['//a.txt', '//dir1/c.txt'])

  def test_directory_inclusion_no_wildcard(self):
    lines = ['//dir1']
    output = self.run_expand(lines)
    # Traditionally, including a dir includes all its files recursively
    expected = ['//dir1/c.txt', '//dir1/d.txt', '//dir1/subdir/e.txt']
    self.assertEqual(sorted(output), sorted(expected))

  def test_recursive_inclusion(self):
    lines = ['//dir1/**']
    output = self.run_expand(lines)
    expected = ['//dir1/c.txt', '//dir1/d.txt', '//dir1/subdir/e.txt']
    self.assertEqual(sorted(output), sorted(expected))

  def test_basic_exclusion(self):
    lines = ['//dir1/**', '-//dir1/d.txt']
    output = self.run_expand(lines)
    expected = ['//dir1/c.txt', '//dir1/subdir/e.txt']
    self.assertEqual(sorted(output), sorted(expected))

  def test_recursive_exclusion(self):
    self.create_file('dir1/subdir/deep/g.txt')
    lines = ['//dir1/**', '-//dir1/subdir/**']
    output = self.run_expand(lines)
    # Correct behavior: everything under dir1/subdir/ is excluded.
    expected = ['//dir1/c.txt', '//dir1/d.txt']
    self.assertEqual(sorted(output), sorted(expected))

  def test_single_level_wildcard(self):
    lines = ['//dir1/*']
    output = self.run_expand(lines)
    # glob: * matches only one level. subdir/e.txt should be excluded.
    expected = ['//dir1/c.txt', '//dir1/d.txt']
    self.assertEqual(sorted(output), sorted(expected))

  def test_exclusion_patterns(self):
    lines = ['//**', '-//dir1/**', '-//dir2/f.txt']
    output = self.run_expand(lines)
    # Correct behavior: dir1/ and dir2/f.txt are excluded.
    expected = ['//a.txt', '//b.txt']
    self.assertEqual(sorted(output), sorted(expected))


if __name__ == '__main__':
  unittest.main()
