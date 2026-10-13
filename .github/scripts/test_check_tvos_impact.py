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
"""Tests for check_tvos_impact."""

import json
import pathlib
import tempfile
import unittest
from unittest import mock

import check_tvos_impact


class CheckTvosImpactTest(unittest.TestCase):

  def test_matches_always_trigger_exact_and_prefix(self):
    self.assertEqual(
        check_tvos_impact.matches_always_trigger(['README.md', 'DEPS']),
        'DEPS',
    )
    self.assertEqual(
        check_tvos_impact.matches_always_trigger(
            ['cobalt/devinfra/kokoro/bin/presubmit_build_mac.sh']),
        'cobalt/devinfra/kokoro/bin/presubmit_build_mac.sh',
    )
    self.assertIsNone(
        check_tvos_impact.matches_always_trigger([
            'README.md',
            '.github/workflows/linux.yaml',
            'starboard/android/shared/player.cc',
        ]))

  def test_expand_gn_files_includes_sources_referencing_header(self):
    with tempfile.TemporaryDirectory() as tmp:
      root = pathlib.Path(tmp)
      pkg_dir = root / 'cobalt/app/tvos'
      pkg_dir.mkdir(parents=True)
      (pkg_dir / 'helper.h').write_text('// header\n', encoding='utf-8')
      (pkg_dir / 'main.mm').write_text(
          '#import "cobalt/app/tvos/helper.h"\n',
          encoding='utf-8',
      )
      (pkg_dir / 'other.cc').write_text(
          '#include "unrelated.h"\n',
          encoding='utf-8',
      )

      expanded = check_tvos_impact.expand_gn_files(
          root, ['cobalt/app/tvos/helper.h'])
      self.assertEqual(
          expanded,
          [
              '//cobalt/app/tvos/helper.h',
              '//cobalt/app/tvos/main.mm',
          ],
      )

  def test_load_test_targets_reads_json_and_cli_arg(self):
    with tempfile.TemporaryDirectory() as tmp:
      root = pathlib.Path(tmp)
      target_json = root / check_tvos_impact.TEST_TARGETS_JSON_RELPATH
      target_json.parent.mkdir(parents=True)
      target_json.write_text(
          json.dumps([{'target': 'cobalt:cobalt_unittests'}]),
          encoding='utf-8',
      )
      self.assertEqual(
          check_tvos_impact.load_test_targets(root),
          ['//cobalt:cobalt_unittests'],
      )
      self.assertEqual(
          check_tvos_impact.load_test_targets(
              root, json.dumps(['base:base_unittests'])),
          ['//base:base_unittests'],
      )

  def test_check_tvos_impact_empty_files(self):
    with tempfile.TemporaryDirectory() as tmp:
      self.assertFalse(
          check_tvos_impact.check_tvos_impact(pathlib.Path(tmp), []))

  @mock.patch('check_tvos_impact._find_gn_binary', return_value='/usr/bin/gn')
  @mock.patch('subprocess.run')
  def test_check_tvos_impact_parses_gn_analyze_status(
      self, mock_run, unused_find_gn):
    with tempfile.TemporaryDirectory() as tmp:
      root = pathlib.Path(tmp)
      out_dir = root / 'out'

      def fake_run(cmd, cwd, check, **kwargs):
        del cwd, check, kwargs
        if cmd[0] == '/usr/bin/gn' and cmd[1] == 'analyze':
          out_path = pathlib.Path(cmd[4])
          out_path.write_text(
              json.dumps({'status': 'No dependency'}), encoding='utf-8')

      mock_run.side_effect = fake_run
      self.assertFalse(
          check_tvos_impact.check_tvos_impact(
              root,
              ['starboard/android/shared/foo.cc'],
              out_dir=out_dir,
              test_targets=['base:base_unittests'],
              compile_targets=['//cobalt:cobalt'],
          ))

      def fake_run_dep(cmd, cwd, check, **kwargs):
        del cwd, check, kwargs
        if cmd[0] == '/usr/bin/gn' and cmd[1] == 'analyze':
          out_path = pathlib.Path(cmd[4])
          out_path.write_text(
              json.dumps({'status': 'Found dependency'}), encoding='utf-8')

      mock_run.side_effect = fake_run_dep
      self.assertTrue(
          check_tvos_impact.check_tvos_impact(
              root,
              ['starboard/tvos/shared/foo.cc'],
              out_dir=out_dir,
              test_targets=['base:base_unittests'],
              compile_targets=['//cobalt:cobalt'],
          ))


if __name__ == '__main__':
  unittest.main()
