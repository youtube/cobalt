#!/usr/bin/env python3
# Copyright 2026 The Cobalt Authors. All Rights Reserved.
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
"""Baseline unit and regression tests for asan_symbolize.py."""

import base64
import io
import json
import os
import shutil
import sys
import tempfile
import unittest
from unittest import mock

_SRC_DIR = os.path.abspath(
    os.path.join(os.path.dirname(__file__), '..', '..', '..'))
if _SRC_DIR not in sys.path:
  sys.path.insert(0, _SRC_DIR)

from tools.valgrind.asan import asan_symbolize
from tools.valgrind.asan.third_party import asan_symbolize as tp_asan_symbolize

_TESTDATA_DIR = os.path.join(os.path.dirname(__file__), 'testdata')


class LineBufferedTest(unittest.TestCase):
  """Tests for LineBuffered stream wrapper."""

  def test_flushes_on_newline(self):
    mock_stream = mock.MagicMock()
    buffered = asan_symbolize.LineBuffered(mock_stream)
    buffered.write('line without newline')
    mock_stream.write.assert_called_once_with('line without newline')
    mock_stream.flush.assert_not_called()

    buffered.write(' and now newline\n')
    mock_stream.flush.assert_called_once()

  def test_delegates_attributes(self):
    mock_stream = mock.MagicMock()
    mock_stream.custom_attribute = 'test_value'
    buffered = asan_symbolize.LineBuffered(mock_stream)
    self.assertEqual(buffered.custom_attribute, 'test_value')


class CheckUTF8Test(unittest.TestCase):
  """Tests for CheckUTF8 decode error tolerance."""

  def test_valid_utf8(self):
    data = b'line 1\nline 2\n'
    stream = io.BytesIO(data)
    mock_text_stream = mock.MagicMock()
    mock_text_stream.buffer = stream

    checker = asan_symbolize.CheckUTF8(mock_text_stream)
    lines = list(checker)
    self.assertEqual(lines, ['line 1\n', 'line 2\n'])

  def test_invalid_utf8_produces_warning_and_continues(self):
    # \x80 is an invalid UTF-8 start byte
    data = b'valid line\n\x80\x81invalid\nanother valid\n'
    stream = io.BytesIO(data)
    mock_text_stream = mock.MagicMock()
    mock_text_stream.buffer = stream

    checker = asan_symbolize.CheckUTF8(mock_text_stream)
    captured_stdout = io.StringIO()
    with mock.patch('sys.stdout', captured_stdout):
      lines = list(checker)

    self.assertEqual(lines, ['valid line\n', '', 'another valid\n'])
    self.assertIn('WARNING: asan_symbolize.py failed to decode',
                  captured_stdout.getvalue())


class PathPrefixStrippingTest(unittest.TestCase):
  """Tests path prefix stripping via fix_filename."""

  def tearDown(self):
    tp_asan_symbolize.fix_filename_patterns = None

  def test_strip_path_prefix(self):
    tp_asan_symbolize.fix_filename_patterns = [
        'Release/../../', 'custom/prefix/'
    ]
    path1 = '/home/user/out/Release/../../base/logging.cc:100'
    path2 = '/prefix/path/custom/prefix/starboard/file.cc:42'
    path3 = '/unrelated/path/main.cc:10'

    self.assertEqual(tp_asan_symbolize.fix_filename(path1),
                     'base/logging.cc:100')
    self.assertEqual(tp_asan_symbolize.fix_filename(path2),
                     'starboard/file.cc:42')
    self.assertEqual(tp_asan_symbolize.fix_filename(path3), path3)


class MacPluginsTest(unittest.TestCase):
  """Tests for macOSBinaryNameFilterPlugin and path helpers."""

  def test_is_hash_name(self):
    self.assertTrue(asan_symbolize.is_hash_name('a1b2c3d4e5f6071829'))
    self.assertFalse(asan_symbolize.is_hash_name('libcobalt.so'))
    self.assertFalse(asan_symbolize.is_hash_name('not_a_hash!'))

  def test_chrome_product_dir_path_app_bundle(self):
    app_path = '/Applications/Chromium.app/Contents/MacOS/Chromium'
    product_dir = asan_symbolize.chrome_product_dir_path(app_path)
    self.assertEqual(product_dir, '/Applications')

  def test_chrome_product_dir_path_standalone(self):
    exe_path = '/out/Default/cobalt_bin'
    product_dir = asan_symbolize.chrome_product_dir_path(exe_path)
    self.assertEqual(product_dir, '/out/Default')

  def test_chrome_dsym_hints_standalone(self):
    binary = '/out/Default/libcobalt.so'
    self.assertEqual(asan_symbolize.chrome_dsym_hints(binary), [])

  def test_chrome_dsym_hints_app(self):
    binary = '/out/Default/Chromium.app/Contents/MacOS/Chromium'
    self.assertEqual(asan_symbolize.chrome_dsym_hints(binary),
                     ['/out/Default/Chromium.app.dSYM'])


class JSONTestRunSymbolizerTest(unittest.TestCase):
  """Tests for JSONTestRunSymbolizer."""

  def setUp(self):
    self.mock_loop = mock.MagicMock()

  def test_passing_test_unchanged(self):
    snippet = 'Test ran and passed.\n'
    test_run = {
        'output_snippet': snippet,
        'output_snippet_base64': base64.b64encode(snippet.encode()).decode()
    }
    self.mock_loop.process_line.side_effect = lambda l: [l]
    symbolizer = asan_symbolize.JSONTestRunSymbolizer(self.mock_loop)
    symbolizer.symbolize(test_run)

    self.assertNotIn('original_output_snippet', test_run)
    self.assertNotIn('snippet_processed_by', test_run)
    self.assertEqual(test_run['output_snippet'], snippet)

  def test_crashing_test_symbolized(self):
    orig = '    #0 0x1000 (<unknown module>)\n'
    sym = '    #0 0x1000 in MyFunc() file.cc:10\n'
    test_run = {
        'output_snippet': orig,
        'output_snippet_base64': base64.b64encode(orig.encode()).decode()
    }
    self.mock_loop.process_line.side_effect = lambda l: [sym.rstrip(
    )] if '#0' in l else [l]
    symbolizer = asan_symbolize.JSONTestRunSymbolizer(self.mock_loop)
    symbolizer.symbolize(test_run)

    self.assertEqual(test_run['original_output_snippet'], orig)
    self.assertEqual(test_run['original_output_snippet_base64'],
                     base64.b64encode(orig.encode()).decode())
    self.assertIn('MyFunc()', test_run['output_snippet'])
    self.assertEqual(test_run['snippet_processed_by'], 'asan_symbolize.py')
    decoded = base64.b64decode(test_run['output_snippet_base64']).decode()
    self.assertIn('MyFunc()', decoded)

  def test_non_ascii_characters_replaced(self):
    orig = 'Crash at \xff\xfe address\n'
    test_run = {
        'output_snippet': orig,
        'output_snippet_base64':
        base64.b64encode(orig.encode('latin1')).decode()
    }
    captured_lines = []

    def record_line(l):
      captured_lines.append(l)
      return ['symbolized line']

    self.mock_loop.process_line.side_effect = record_line
    symbolizer = asan_symbolize.JSONTestRunSymbolizer(self.mock_loop)
    symbolizer.symbolize(test_run)

    self.assertTrue(any('??' in l for l in captured_lines))


class CobaltEvergreenCustomizationsTest(unittest.TestCase):
  """Tests for Cobalt-specific Evergreen base address and module handling."""

  def test_base_address_detection(self):
    loop = tp_asan_symbolize.SymbolizationLoop(
        plugin_proxy=mock.MagicMock(),
        dsym_hint_producer=mock.MagicMock(),
        extra_binary_path='/path/to/libcobalt.so')
    self.assertIsNone(loop.base_addr)

    loop.check_for_base_addr(
        'Load start=0x7f48e7a44000 base_memory_address=0x7f48e7a44000')
    self.assertEqual(loop.base_addr, 0x7f48e7a44000)

  def test_unknown_module_regex_matching(self):
    loop = tp_asan_symbolize.SymbolizationLoop(
        plugin_proxy=mock.MagicMock(),
        dsym_hint_producer=mock.MagicMock(),
        extra_binary_path='/path/to/libcobalt.so')
    line = '    #0 0x7f48e7a45000  (<unknown module>)'
    match = loop.unknown_module_re.match(line)
    self.assertIsNotNone(match)
    frameno_str = match.group(2)
    addrstr = match.group(3)
    self.assertEqual(frameno_str, '0')
    self.assertEqual(addrstr, '0x7f48e7a45000')

  def test_missing_extra_binary_raises(self):
    test_args = ['asan_symbolize.py', '--extra-binary', '/nonexistent/lib.so']
    with mock.patch('sys.argv', test_args):
      with self.assertRaises(ValueError) as ctx:
        asan_symbolize.main()
      self.assertIn('Extra binary not found', str(ctx.exception))


class GoldenFixturesParityTest(unittest.TestCase):
  """Tests end-to-end processing against golden fixtures."""

  def test_symbolize_snippets_in_json(self):
    json_path = os.path.join(_TESTDATA_DIR, 'test_summary.json')
    if not os.path.exists(json_path):
      self.skipTest('test_summary.json fixture not found')

    with tempfile.TemporaryDirectory() as tmp_dir:
      test_json = os.path.join(tmp_dir, 'test_summary.json')
      shutil.copyfile(json_path, test_json)

      mock_loop = mock.MagicMock()

      def mock_process_line(line):
        if '(<unknown module>)' in line and '0x7f48e7a45000' in line:
          return ['    #0 0x7f48e7a45000 in CobaltCrashFunc() cobalt.cc:123']
        if '(<unknown module>)' in line and '0x7f48e7a46000' in line:
          return ['    #1 0x7f48e7a46000 in CobaltParentFunc() cobalt.cc:456']
        return [line]

      mock_loop.process_line.side_effect = mock_process_line

      asan_symbolize.symbolize_snippets_in_json(test_json, mock_loop)

      with open(test_json, 'r', encoding='utf-8') as f:
        data = json.load(f)

      iteration = data['per_iteration_data'][0]
      pass_run = iteration['TestPassing'][0]
      self.assertNotIn('original_output_snippet', pass_run)

      crash_run = iteration['TestWithSanitizerReport'][0]
      self.assertIn('original_output_snippet', crash_run)
      self.assertIn('CobaltCrashFunc()', crash_run['output_snippet'])
      self.assertIn('CobaltParentFunc()', crash_run['output_snippet'])
      self.assertEqual(crash_run['snippet_processed_by'], 'asan_symbolize.py')


if __name__ == '__main__':
  unittest.main()
