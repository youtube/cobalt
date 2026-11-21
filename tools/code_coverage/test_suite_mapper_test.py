#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for test_suite_mapper.py."""

import contextlib
import io
import pathlib
import unittest
from unittest.mock import MagicMock, patch

from pyfakefs import fake_filesystem_unittest

import test_suite_mapper

MOCK_ISOLATE_MAP = {
    '//chrome/test:browser_tests': 'browser_tests',
    '//chrome/test:unit_tests': 'unit_tests',
    '//media/midi:midi_unittests': 'midi_unittests',
    '//media:media_unittests': 'media_unittests',
    '//chrome/browser:foo_tests': 'foo_tests',
    '//content/test:bar_tests': 'bar_tests',
    '//foo/bar:baz_tests': 'baz_tests',
    '//base:base_unittests': 'base_unittests',
    '//:blink_wpt_tests': 'blink_wpt_tests',
}


class TestSuiteMapperUnit(fake_filesystem_unittest.TestCase):

  def setUp(self):
    self.setUpPyfakefs()
    self.subprocess_patcher = patch('subprocess.run')
    self.mock_run = self.subprocess_patcher.start()
    self.addCleanup(self.subprocess_patcher.stop)

  def test_common_dir_prefix_len(self):
    self.assertEqual(test_suite_mapper.common_dir_prefix_len('a/b/c', 'a/b/c'),
                     3)
    self.assertEqual(test_suite_mapper.common_dir_prefix_len('a/b/c', 'a/b/d'),
                     2)
    self.assertEqual(test_suite_mapper.common_dir_prefix_len('a/b/c', 'x/y/z'),
                     0)
    self.assertEqual(test_suite_mapper.common_dir_prefix_len('a/b/c', ''), 0)
    self.assertEqual(test_suite_mapper.common_dir_prefix_len('a/b', 'a/b/c'), 2)

  def test_parse_isolate_map_valid(self):
    content = ("{'browser_tests': {'label': '//chrome/test:browser_tests'},"
               " 'unit_tests': {'label': '//chrome/test:unit_tests'}}")
    expected = {
        '//chrome/test:browser_tests': 'browser_tests',
        '//chrome/test:unit_tests': 'unit_tests',
    }
    self.assertEqual(test_suite_mapper.parse_isolate_map(content, 'test'),
                     expected)

  def test_parse_isolate_map_missing_label(self):
    content = "{'suite1': {'type': 'additional_compile_target'}}"
    self.assertEqual(test_suite_mapper.parse_isolate_map(content, 'test'), {})

  def test_parse_isolate_map_invalid(self):
    content = 'invalid dict'
    f = io.StringIO()
    with contextlib.redirect_stderr(f):
      self.assertIsNone(test_suite_mapper.parse_isolate_map(content, 'test'))
    self.assertTrue(len(f.getvalue()) > 0)

  def test_parse_isolate_map_not_dict(self):
    content = '[1, 2, 3]'
    f = io.StringIO()
    with contextlib.redirect_stderr(f):
      self.assertIsNone(test_suite_mapper.parse_isolate_map(content, 'test'))
    self.assertIn('Expected a dictionary', f.getvalue())

  def test_load_isolate_map_success(self):
    self.fs.create_dir('/src')
    pyl_path = pathlib.Path('/src') / test_suite_mapper.ISOLATE_MAP_PATH
    self.fs.create_file(
        pyl_path,
        contents="{'browser_tests': {'label': '//chrome/test:browser_tests'}}",
    )
    isolate_map = test_suite_mapper.load_isolate_map('/src')
    self.assertEqual(isolate_map,
                     {'//chrome/test:browser_tests': 'browser_tests'})

  def test_load_isolate_map_missing_file(self):
    self.fs.create_dir('/src')
    f = io.StringIO()
    with contextlib.redirect_stderr(f):
      self.assertIsNone(test_suite_mapper.load_isolate_map('/src'))
    self.assertIn('Isolate map not found', f.getvalue())

  def test_load_isolate_map_read_error(self):
    self.fs.create_dir('/src')
    pyl_path = pathlib.Path('/src') / test_suite_mapper.ISOLATE_MAP_PATH
    self.fs.create_dir(pyl_path)
    f = io.StringIO()
    with contextlib.redirect_stderr(f):
      self.assertIsNone(test_suite_mapper.load_isolate_map('/src'))
    self.assertIn('Error reading', f.getvalue())

  def test_extract_test_suites_basic(self):
    refs = [
        '//chrome/browser:foo_tests',
        '//content/test:bar_tests',
        '//foo/bar:baz_tests',
    ]
    suites = test_suite_mapper.extract_test_suites(
        refs, pathlib.Path('chrome/browser/foo.cc'), MOCK_ISOLATE_MAP)
    self.assertEqual(suites, ['foo_tests'])

  def test_extract_test_suites_proximity(self):
    refs = [
        '//chrome/test:browser_tests',
        '//media/midi:midi_unittests',
    ]
    suites = test_suite_mapper.extract_test_suites(
        refs, pathlib.Path('media/midi/midi_manager.cc'), MOCK_ISOLATE_MAP)
    self.assertEqual(suites, ['midi_unittests'])

  def test_extract_test_suites_filter_non_isolate(self):
    refs = [
        '//media/base:unit_tests',
        '//media:media_unittests',
    ]
    suites = test_suite_mapper.extract_test_suites(
        refs,
        pathlib.Path('media/base/audio_bus_unittest.cc'),
        MOCK_ISOLATE_MAP,
    )
    self.assertEqual(suites, ['media_unittests'])

  def test_extract_test_suites_empty_target(self):
    refs = ['//chrome/browser:foo_tests', '', '  ']
    suites = test_suite_mapper.extract_test_suites(
        refs, pathlib.Path('chrome/browser/foo.cc'), MOCK_ISOLATE_MAP)
    self.assertEqual(suites, ['foo_tests'])

  def test_extract_test_suites_no_match(self):
    refs = ['//unrelated:target']
    suites = test_suite_mapper.extract_test_suites(
        refs, pathlib.Path('chrome/browser/foo.cc'), MOCK_ISOLATE_MAP)
    self.assertEqual(suites, [])

  def test_extract_test_suites_all_zero_score(self):
    refs = ['//media/midi:midi_unittests', '//foo/bar:baz_tests']
    suites = test_suite_mapper.extract_test_suites(
        refs, pathlib.Path('chrome/browser/foo.cc'), MOCK_ISOLATE_MAP)
    self.assertEqual(sorted(suites), ['baz_tests', 'midi_unittests'])

  def test_verify_build_dir_valid(self):
    self.fs.create_file(
        '/src/out/Default/args.gn',
        contents=('# This is a comment\n'
                  'target_os = "linux"\n'
                  '\n'
                  'use_goma = true\n'),
    )
    success, err = test_suite_mapper.verify_build_dir('/src', 'out/Default')
    self.assertTrue(success)
    self.assertIsNone(err)

  def test_verify_build_dir_missing_file(self):
    self.fs.create_dir('/src')
    success, err = test_suite_mapper.verify_build_dir('/src', 'out/Default')
    self.assertFalse(success)
    self.assertIn('args.gn not found', err)

  def test_verify_build_dir_invalid_os(self):
    self.fs.create_file('/src/out/Default/args.gn',
                        contents='target_os = "android"\n')
    success, err = test_suite_mapper.verify_build_dir('/src', 'out/Default')
    self.assertFalse(success)
    self.assertIn('Unsupported target_os: android', err)

  @patch('sys.platform', 'linux')
  def test_verify_build_dir_no_target_os_on_linux(self):
    self.fs.create_file('/src/out/Default/args.gn',
                        contents='is_debug = true\n')
    success, err = test_suite_mapper.verify_build_dir('/src', 'out/Default')
    self.assertTrue(success)
    self.assertIsNone(err)

  @patch('sys.platform', 'darwin')
  def test_verify_build_dir_no_target_os_on_mac(self):
    self.fs.create_file('/src/out/Default/args.gn',
                        contents='is_debug = true\n')
    success, err = test_suite_mapper.verify_build_dir('/src', 'out/Default')
    self.assertFalse(success)
    self.assertIn('Host platform darwin is not Linux', err)

  def test_run_gn_refs_calls_gn(self):
    self.mock_run.return_value = MagicMock(returncode=0,
                                           stdout='//foo:foo_unittests\n',
                                           stderr='')
    suites, err = test_suite_mapper.run_gn_refs('/src', 'out/Default',
                                                pathlib.Path('foo/bar.cc'))
    self.assertEqual(suites, ['//foo:foo_unittests'])
    self.assertIsNone(err)
    self.mock_run.assert_called_once_with(
        [
            'gn',
            'refs',
            'out/Default',
            '--all',
            '//foo/bar.cc',
        ],
        capture_output=True,
        text=True,
        cwd='/src',
        check=False,
        encoding='utf-8',
    )

  def test_run_gn_refs_failure(self):
    self.mock_run.return_value = MagicMock(returncode=1,
                                           stdout='',
                                           stderr='GN error message\n')
    suites, err = test_suite_mapper.run_gn_refs('/src', 'out/Default',
                                                pathlib.Path('foo/bar.cc'))
    self.assertIsNone(suites)
    self.assertEqual(err, 'GN error message')

  def test_run_gn_refs_exception(self):
    self.mock_run.side_effect = OSError('Command not found')
    suites, err = test_suite_mapper.run_gn_refs('/src', 'out/Default',
                                                pathlib.Path('foo/bar.cc'))
    self.assertIsNone(suites)
    self.assertEqual(err, 'Command not found')


if __name__ == '__main__':
  unittest.main()
