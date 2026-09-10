#!/usr/bin/env python3
#
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
"""Tests for cobalt.precommit.filter_format_check."""

import io
import os
import shutil
import sys
import tempfile
import unittest
from unittest import mock

_REPO_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), '..', '..'))
if _REPO_ROOT not in sys.path:
  sys.path.insert(0, _REPO_ROOT)

# pylint: disable=wrong-import-position
from cobalt.precommit.filter_format_check import check_filter_file
from cobalt.precommit.filter_format_check import check_filter_files
from cobalt.precommit.filter_format_check import main
# pylint: enable=wrong-import-position


class TestFilterFormatCheck(unittest.TestCase):
  """Tests for check_filter_file and check_filter_files."""

  def setUp(self):
    self.temp_dir = tempfile.mkdtemp()

  def tearDown(self):
    shutil.rmtree(self.temp_dir)

  def _create_file(self, filename: str, content: str) -> str:
    path = os.path.join(self.temp_dir, filename)
    with open(path, 'w', encoding='utf-8') as f:
      f.write(content)
    return path

  def test_valid_filter_file(self):
    content = """# Header comment
Suite.PositiveTest1
+Suite.PositiveTest2
-Suite.NegativeTest3
-All/ParameterizedTest.Case/*
"""
    path = self._create_file('valid.filter', content)
    errors = check_filter_file(path)
    self.assertEqual(errors, [])

  def test_double_slash_comment_error(self):
    content = """// Invalid comment
-Suite.NegativeTest
"""
    path = self._create_file('invalid_comment.filter', content)
    errors = check_filter_file(path)
    self.assertEqual(len(errors), 1)
    self.assertIn('Starts with //, use # for comments.', errors[0])

  def test_invalid_negative_pattern(self):
    content = """-
"""
    path = self._create_file('invalid_negative.filter', content)
    errors = check_filter_file(path)
    self.assertEqual(len(errors), 1)
    self.assertIn('Invalid negative filter pattern', errors[0])

  def test_invalid_positive_pattern(self):
    content = """+
"""
    path = self._create_file('invalid_positive.filter', content)
    errors = check_filter_file(path)
    self.assertEqual(len(errors), 1)
    self.assertIn('Invalid positive filter pattern', errors[0])

  def test_unrecognized_line_format(self):
    content = """@UnrecognizedPattern
"""
    path = self._create_file('unrecognized.filter', content)
    errors = check_filter_file(path)
    self.assertEqual(len(errors), 1)
    self.assertIn('Unrecognized line format', errors[0])

  def test_nonexistent_file(self):
    errors = check_filter_file(os.path.join(self.temp_dir, 'missing.filter'))
    self.assertEqual(len(errors), 1)
    self.assertIn('is not a regular file', errors[0])

  def test_check_filter_files_returns_success(self):
    path = self._create_file('good.filter', 'Suite.Test1\n-Suite.Test2\n')
    self.assertEqual(check_filter_files([path]), 0)

  def test_check_filter_files_returns_failure(self):
    path = self._create_file('bad.filter', '// bad\n')
    with mock.patch('sys.stdout', new_callable=io.StringIO):
      self.assertEqual(check_filter_files([path]), 1)

  def test_main_cli_empty_args(self):
    self.assertEqual(main([]), 0)

  def test_main_cli_with_file(self):
    path = self._create_file('good.filter', 'Suite.Test1\n')
    self.assertEqual(main([path]), 0)


if __name__ == '__main__':
  unittest.main()
