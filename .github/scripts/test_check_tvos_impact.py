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
import os
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

  def test_expand_gn_files_includes_nearest_build_gn_for_unlisted_header(self):
    with tempfile.TemporaryDirectory() as tmp:
      root = pathlib.Path(tmp)
      pkg_dir = root / 'starboard/shared/uikit'
      sub_dir = pkg_dir / 'detail'
      sub_dir.mkdir(parents=True)
      (pkg_dir / 'BUILD.gn').write_text(
          'source_set("uikit") { sources = [ "listed.h" ] }\n',
          encoding='utf-8',
      )
      (pkg_dir / 'listed.h').write_text('// listed\n', encoding='utf-8')
      (sub_dir / 'unlisted.h').write_text('// unlisted\n', encoding='utf-8')

      expanded_listed = check_tvos_impact.expand_gn_files(
          root, ['starboard/shared/uikit/listed.h'])
      self.assertEqual(expanded_listed, ['//starboard/shared/uikit/listed.h'])

      expanded_unlisted = check_tvos_impact.expand_gn_files(
          root, ['starboard/shared/uikit/detail/unlisted.h'])
      self.assertEqual(
          expanded_unlisted,
          [
              '//starboard/shared/uikit/BUILD.gn',
              '//starboard/shared/uikit/detail/unlisted.h',
          ],
      )

  def test_load_test_targets_reads_json_or_falls_back(self):
    with tempfile.TemporaryDirectory() as tmp:
      root = pathlib.Path(tmp)
      self.assertEqual(
          check_tvos_impact.load_test_targets(root),
          list(check_tvos_impact.DEFAULT_TEST_TARGETS),
      )

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
              root, ['starboard/android/shared/foo.cc'], out_dir=out_dir))

      def fake_run_dep(cmd, cwd, check, **kwargs):
        del cwd, check, kwargs
        if cmd[0] == '/usr/bin/gn' and cmd[1] == 'analyze':
          out_path = pathlib.Path(cmd[4])
          out_path.write_text(
              json.dumps({'status': 'Found dependency'}), encoding='utf-8')

      mock_run.side_effect = fake_run_dep
      self.assertTrue(
          check_tvos_impact.check_tvos_impact(
              root, ['starboard/tvos/shared/foo.cc'], out_dir=out_dir))

  @mock.patch('check_tvos_impact._github_api_request')
  def test_cmd_precheck_draft_removes_label(self, mock_api):
    mock_api.side_effect = [
        {'draft': True, 'labels': [{'name': 'kokoro:run'}]},
        None,
    ]
    with tempfile.TemporaryDirectory() as tmp:
      out_file = pathlib.Path(tmp) / 'github_output.txt'
      env = {
          'REPO': 'youtube/cobalt',
          'PR_NUMBER': '42',
          'IS_DRAFT': 'true',
          'EVENT_ACTION': 'converted_to_draft',
          'AUTHOR_ASSOC': 'MEMBER',
          'PR_AUTHOR': 'alice',
      }
      with mock.patch.dict(os.environ, env, clear=False):
        rc = check_tvos_impact.cmd_precheck(out_file)
      self.assertEqual(rc, 0)
      self.assertIn(
          'should_analyze=false', out_file.read_text(encoding='utf-8'))
      mock_api.assert_any_call(
          'DELETE', 'repos/youtube/cobalt/issues/42/labels/kokoro%3Arun')

  @mock.patch('check_tvos_impact._github_api_request')
  def test_cmd_precheck_outputs_changed_files_for_analyze(self, mock_api):
    mock_api.side_effect = [
        {'draft': False, 'labels': []},
        [{'filename': 'starboard/android/shared/player.cc'}],
    ]
    with tempfile.TemporaryDirectory() as tmp:
      out_file = pathlib.Path(tmp) / 'github_output.txt'
      env = {
          'REPO': 'youtube/cobalt',
          'PR_NUMBER': '42',
          'IS_DRAFT': 'false',
          'EVENT_ACTION': 'synchronize',
          'AUTHOR_ASSOC': 'MEMBER',
          'PR_AUTHOR': 'alice',
      }
      with mock.patch.dict(os.environ, env, clear=False):
        rc = check_tvos_impact.cmd_precheck(out_file)
      self.assertEqual(rc, 0)
      output = out_file.read_text(encoding='utf-8')
      self.assertIn('should_analyze=true', output)
      self.assertIn(
          'changed_files=["starboard/android/shared/player.cc"]', output)

  @mock.patch('check_tvos_impact._github_api_request')
  def test_cmd_wait_for_label_checks_workflow_and_label(self, mock_api):
    mock_api.side_effect = [
        {
            'workflow_runs': [{
                'display_title': 'Auto Label Kokoro (#42 @ deadbeef)',
                'status': 'completed',
                'conclusion': 'success',
            }]
        },
        {'draft': False, 'labels': [{'name': 'kokoro:run'}]},
    ]
    with tempfile.TemporaryDirectory() as tmp:
      out_file = pathlib.Path(tmp) / 'github_output.txt'
      env = {
          'REPO': 'youtube/cobalt',
          'PR_NUMBER': '42',
          'COMMIT_SHA': 'deadbeef',
          'IS_DRAFT': 'false',
      }
      with mock.patch.dict(os.environ, env, clear=False):
        rc = check_tvos_impact.cmd_wait_for_label(
            out_file, max_attempts=1, sleep_seconds=0)
      self.assertEqual(rc, 0)
      self.assertIn('skipped=false', out_file.read_text(encoding='utf-8'))


if __name__ == '__main__':
  unittest.main()
