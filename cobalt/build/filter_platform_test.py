#!/usr/bin/env python3
# Copyright 2026 The Cobalt Authors. All Rights Reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for filter_platform.py."""

import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock

from cobalt.build import filter_platform


class FilterPlatformTest(unittest.TestCase):

  def test_infra_prefixes_never_skipped(self):
    infra_files = [
        '.github/workflows/main.yaml',
        'build/config/BUILD.gn',
        'build_overrides/build.gni',
        'DEPS',
        'cobalt/build/gn.py',
    ]
    for infra_file in infra_files:
      with self.subTest(infra_file=infra_file):
        self.assertFalse(
            filter_platform.should_skip('out/dummy', [infra_file], ['cobalt']))

  def test_empty_files_never_skipped(self):
    self.assertFalse(filter_platform.should_skip('out/dummy', [], ['cobalt']))

  @mock.patch('subprocess.check_call')
  def test_should_skip_gn_analyze_no_dependency(self, mock_call):
    with tempfile.TemporaryDirectory() as tmp_dir:
      out_dir = Path(tmp_dir)

      # simulate gn analyze writing analyze_out.json
      def fake_analyze(cmd):
        out_json = Path(cmd[4])
        out_json.write_text(
            json.dumps({'status': 'No dependency'}), encoding='utf-8')

      mock_call.side_effect = fake_analyze
      self.assertTrue(
          filter_platform.should_skip(
              str(out_dir), ['starboard/android/audio.cc'], ['cobalt']))

  @mock.patch('subprocess.check_call')
  def test_should_skip_gn_analyze_found_dependency(self, mock_call):
    with tempfile.TemporaryDirectory() as tmp_dir:
      out_dir = Path(tmp_dir)

      def fake_analyze(cmd):
        out_json = Path(cmd[4])
        out_json.write_text(
            json.dumps({'status': 'Found dependency'}), encoding='utf-8')

      mock_call.side_effect = fake_analyze
      self.assertFalse(
          filter_platform.should_skip(
              str(out_dir), ['cobalt/browser/main.cc'], ['//cobalt:cobalt']))

  @mock.patch('subprocess.check_call')
  def test_target_formatting(self, mock_call):
    with tempfile.TemporaryDirectory() as tmp_dir:
      out_dir = Path(tmp_dir)

      def fake_analyze(cmd):
        out_json = Path(cmd[4])
        out_json.write_text(
            json.dumps({'status': 'No dependency'}), encoding='utf-8')

      mock_call.side_effect = fake_analyze
      filter_platform.should_skip(
          str(out_dir), ['some/file.cc'],
          ['cobalt:gn_all', '//starboard:nplb', '', 'null'])
      in_json = json.loads(
          (out_dir / 'analyze_in.json').read_text(encoding='utf-8'))
      self.assertEqual(in_json['additional_compile_targets'],
                       ['//cobalt:gn_all', '//starboard:nplb'])
      self.assertEqual(in_json['files'], ['//some/file.cc'])

  @mock.patch('subprocess.check_call')
  def test_gn_analyze_error_raises(self, mock_call):
    with tempfile.TemporaryDirectory() as tmp_dir:
      out_dir = Path(tmp_dir)

      def fake_analyze(cmd):
        out_json = Path(cmd[4])
        out_json.write_text(
            json.dumps({'error': 'Target not found'}), encoding='utf-8')

      mock_call.side_effect = fake_analyze
      with self.assertRaises(RuntimeError):
        filter_platform.should_skip(
            str(out_dir), ['some/file.cc'], ['//cobalt'])


if __name__ == '__main__':
  unittest.main()
