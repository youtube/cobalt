#!/usr/bin/env python3
#
# Copyright 2023 The Cobalt Authors. All Rights Reserved.
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
"""Unit tests for api leak detector."""

import sys
import unittest
from unittest.mock import MagicMock, patch
import api_leak_detector


class TestAPILeakDetector(unittest.TestCase):
  """unittest class for api leak detector."""

  def test_diff_with_manifest(self):
    manifest = {'leak_1', 'leak_2', 'leak_3'}
    leaks = {'leak_1', 'leak_2', 'leak_4'}

    with patch('api_leak_detector.LoadManifest') as mock_load_manifest:
      mock_load_manifest.return_value = manifest

      result = api_leak_detector.DiffWithManifest(leaks, 'dummy/path')

      expected_result = {'leak_4'}, {'leak_3'}
      self.assertEqual(result, expected_result)
      mock_load_manifest.assert_called_once_with('dummy/path')

  def test_process_nm_output(self):
    nm_output = b'U symbol1@@version1\n U symbol2\n U symbol3@@version2'
    expected_symbols = ['symbol1', 'symbol2', 'symbol3']
    symbols = list(api_leak_detector.ProcessNmOutput(nm_output))

    self.assertEqual(symbols, expected_symbols)

  def test_inversed_keys(self):
    nested_dict = {
        'a': {
            '1': 'x',
            '2': 'y'
        },
        'b': {
            '3': 'z',
            '1': 'w'
        },
    }
    expected_inversed = {
        '1': {
            'a': 'x',
            'b': 'w'
        },
        '2': {
            'a': 'y'
        },
        '3': {
            'b': 'z'
        },
    }
    inversed = api_leak_detector.InversedKeys(nested_dict)

    self.assertEqual(inversed, expected_inversed)

  @patch('builtins.open', new_callable=MagicMock)
  def test_load_allowed_c99_symbols(self, mock_open):
    mock_open.return_value.__enter__.return_value = [
        '# Comment line\n', '* allowed_symbol_1\n', '* allowed_symbol_2\n',
        '  # Another comment\n', '* allowed_symbol_3\n'
    ]

    allowed_c99_symbols = api_leak_detector.LoadAllowedC99Symbols()
    expected_symbols = {
        'allowed_symbol_1', 'allowed_symbol_2', 'allowed_symbol_3'
    }
    self.assertEqual(allowed_c99_symbols, expected_symbols)

  @patch('builtins.open', new_callable=MagicMock)
  def test_load_manifest(self, mock_open):
    mock_open.return_value.__enter__.return_value = [
        '# Manifest of Leaking Files\n', '\n',
        "# This file was auto-generated using 'api_leak_detector.py'.\n",
        'leak_1\n', 'leak_2\n', 'leak_3\n'
    ]

    manifest = api_leak_detector.LoadManifest('fake_manifest_path')
    expected_manifest = {'leak_1', 'leak_2', 'leak_3'}
    self.assertEqual(manifest, expected_manifest)

  @patch('builtins.open', new_callable=MagicMock)
  def test_load_libraries_to_ignore(self, mock_open):
    mock_open.return_value.__enter__.return_value = [
        '# Comment line\n', 'lib_ignore_1.a\n', 'lib_ignore_2.a\n',
        '  # Another comment\n', 'lib_ignore_3.a\n'
    ]

    libraries_to_ignore = api_leak_detector.LoadLibrariesToIgnore()
    expected_libraries = {
        'libstarboard_platform.a', 'lib_ignore_1.a', 'lib_ignore_2.a',
        'lib_ignore_3.a'
    }
    self.assertEqual(libraries_to_ignore, expected_libraries)

  @patch('builtins.print')
  def test_pretty_print_dict(self, mock_print):
    test_dict = {
        'key1': {
            'inner_key1': 'inner_value1',
            'inner_key2': 'inner_value2'
        },
        'key2': {
            'inner_key3': 'inner_value3',
            'inner_key4': 'inner_value4'
        }
    }
    api_leak_detector.PrettyPrint(test_dict, indent=0)
    mock_print.assert_any_call('\nkey1', file=sys.stderr)
    mock_print.assert_any_call('  inner_key1', file=sys.stderr)
    mock_print.assert_any_call('    inner_value1', file=sys.stderr)
    mock_print.assert_any_call('  inner_key2', file=sys.stderr)
    mock_print.assert_any_call('    inner_value2', file=sys.stderr)
    mock_print.assert_any_call('\nkey2', file=sys.stderr)
    mock_print.assert_any_call('  inner_key3', file=sys.stderr)
    mock_print.assert_any_call('    inner_value3', file=sys.stderr)
    mock_print.assert_any_call('  inner_key4', file=sys.stderr)
    mock_print.assert_any_call('    inner_value4', file=sys.stderr)


if __name__ == '__main__':
  unittest.main()
