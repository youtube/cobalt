#!/usr/bin/env python3
# Copyright 2026 The Cobalt Authors. All Rights Reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for filter_platform.py."""

import json
import tempfile
import unittest
from pathlib import Path
from unittest import mock

from cobalt.build import filter_platform


class FilterPlatformTest(unittest.TestCase):

  def test_infra_prefixes_never_skipped(self):
    infra_files = [
        '.github/workflows/main.yaml',
        'build/config/BUILD.gn',
        'DEPS',
        'cobalt/build/gn.py',
    ]
    for infra_file in infra_files:
      with self.subTest(infra_file=infra_file):
        self.assertFalse(
            filter_platform.should_skip('out/dummy', [infra_file], ['cobalt']))
        with tempfile.NamedTemporaryFile('w', encoding='utf-8') as f:
          json.dump(['some/unrelated/file.cc'], f)
          f.flush()
          self.assertFalse(
              filter_platform.should_skip_from_deps(f.name, [infra_file]))

  def test_empty_files_never_skipped(self):
    self.assertFalse(filter_platform.should_skip('out/dummy', [], ['cobalt']))
    with tempfile.NamedTemporaryFile('w', encoding='utf-8') as f:
      json.dump(['some/file.cc'], f)
      f.flush()
      self.assertFalse(filter_platform.should_skip_from_deps(f.name, []))

  def test_should_skip_from_deps_list(self):
    with tempfile.NamedTemporaryFile('w', encoding='utf-8') as f:
      json.dump(['//cobalt/browser/main.cc', 'starboard/common/log.cc'], f)
      f.flush()
      # Unrelated file -> should skip
      self.assertTrue(
          filter_platform.should_skip_from_deps(
              f.name, ['starboard/android/shared/audio.cc']))
      # Matching file -> should not skip
      self.assertFalse(
          filter_platform.should_skip_from_deps(f.name,
                                                ['cobalt/browser/main.cc']))
      # Leading // match -> should not skip
      self.assertFalse(
          filter_platform.should_skip_from_deps(f.name,
                                                ['//starboard/common/log.cc']))

  def test_should_skip_from_deps_dict(self):
    with tempfile.NamedTemporaryFile('w', encoding='utf-8') as f:
      json.dump(
          {
              'target1': ['cobalt/browser/main.cc'],
              'target2': ['//starboard/tvos/player.mm']
          }, f)
      f.flush()
      self.assertTrue(
          filter_platform.should_skip_from_deps(
              f.name, ['cobalt/android/apk/app.java']))
      self.assertFalse(
          filter_platform.should_skip_from_deps(f.name,
                                                ['starboard/tvos/player.mm']))

  @mock.patch('subprocess.check_call')
  @mock.patch('subprocess.check_output')
  def test_should_skip_gn_analyze_no_dependency(self, mock_output, mock_call):
    mock_output.return_value = '//cobalt:cobalt\n//starboard:nplb\n'
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
  @mock.patch('subprocess.check_output')
  def test_should_skip_gn_analyze_found_dependency(self, mock_output,
                                                   mock_call):
    mock_output.return_value = '//cobalt:cobalt\n//starboard:nplb\n'
    with tempfile.TemporaryDirectory() as tmp_dir:
      out_dir = Path(tmp_dir)

      def fake_analyze(cmd):
        out_json = Path(cmd[4])
        out_json.write_text(
            json.dumps({'status': 'Found dependency'}), encoding='utf-8')

      mock_call.side_effect = fake_analyze
      self.assertFalse(
          filter_platform.should_skip(
              str(out_dir), ['cobalt/browser/main.cc'], ['cobalt']))


if __name__ == '__main__':
  unittest.main()
