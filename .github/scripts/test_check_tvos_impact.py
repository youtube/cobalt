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
import subprocess
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

      expanded = check_tvos_impact.expand_gn_files(root,
                                                   ['cobalt/app/tvos/helper.h'])
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
          json.dumps([{
              'target': 'cobalt:cobalt_unittests'
          }]),
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
  def test_check_tvos_impact_parses_gn_analyze_status(self, mock_run,
                                                      unused_find_gn):
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


def _git(cwd, *args):
  return subprocess.run(
      [
          'git', '-c', 'user.name=test', '-c', 'user.email=test@example.com',
          '-c', 'init.defaultBranch=main', *args
      ],
      cwd=cwd,
      capture_output=True,
      text=True,
      check=True,
  ).stdout.strip()


class MergeCommitTest(unittest.TestCase):
  """Tests against a local "origin" repo that mimics GitHub's PR refs."""

  def setUp(self):
    super().setUp()
    tmp = tempfile.TemporaryDirectory()  # pylint: disable=consider-using-with
    self.addCleanup(tmp.cleanup)
    root = pathlib.Path(tmp.name)
    self.origin = root / 'origin'
    self.origin.mkdir()
    _git(self.origin, 'init')
    self._commit('base.cc', 'base')
    self._commit('old_name.h', 'header')

    # The PR branches off here, while main moves on.
    _git(self.origin, 'checkout', '-b', 'pr')
    _git(self.origin, 'mv', 'old_name.h', 'new_name.h')
    self.head_sha = self._commit('pr.cc', 'pr')
    _git(self.origin, 'checkout', 'main')
    self._commit('landed_on_main.cc', 'main')

    _git(self.origin, 'checkout', '--detach', 'main')
    _git(self.origin, 'merge', '--no-ff', '-m', 'merge', 'pr')
    self.merge_sha = _git(self.origin, 'rev-parse', 'HEAD')
    _git(self.origin, 'update-ref', 'refs/pull/1/merge', self.merge_sha)
    _git(self.origin, 'checkout', 'main')

    self.clone = root / 'clone'
    _git(root, 'clone', '--depth=1', f'file://{self.origin}', str(self.clone))

  def _commit(self, filename, content):
    (self.origin / filename).write_text(content, encoding='utf-8')
    _git(self.origin, 'add', '-A')
    _git(self.origin, 'commit', '-m', filename)
    return _git(self.origin, 'rev-parse', 'HEAD')

  def test_fetches_merge_commit_and_lists_only_pr_changes(self):
    merge_sha = check_tvos_impact.fetch_pr_merge_commit(
        self.clone, '1', self.head_sha, attempts=1)
    self.assertEqual(merge_sha, self.merge_sha)
    # Doesn't include landed_on_main.cc, which landed after the PR branched.
    self.assertEqual(
        check_tvos_impact.get_merge_commit_changed_files(self.clone, merge_sha),
        ['new_name.h', 'old_name.h', 'pr.cc'])

  @mock.patch('time.sleep')
  def test_retries_until_merge_commit_is_up_to_date(self, mock_sleep):
    with self.assertRaises(check_tvos_impact.MergeCommitUnavailableError):
      check_tvos_impact.fetch_pr_merge_commit(
          self.clone, '1', 'f' * 40, attempts=3, retry_delay_seconds=5)
    self.assertEqual(mock_sleep.call_args_list, [mock.call(5), mock.call(5)])

  @mock.patch('time.sleep')
  def test_raises_without_merge_ref(self, unused_sleep):
    # E.g., the PR has merge conflicts.
    with self.assertRaises(check_tvos_impact.MergeCommitUnavailableError):
      check_tvos_impact.fetch_pr_merge_commit(
          self.clone, '2', self.head_sha, attempts=2)

  @mock.patch('time.sleep')
  def test_main_defaults_to_affected_without_merge_commit(self, unused_sleep):
    output = self.clone / 'github_output'
    check_tvos_impact.main([
        '--src-dir',
        str(self.clone),
        '--pr-number',
        '2',
        '--github-output',
        str(output),
    ])
    self.assertEqual(output.read_text(encoding='utf-8'), 'affects_tvos=true\n')

  @mock.patch('check_tvos_impact.check_tvos_impact', return_value=False)
  def test_main_analyzes_merge_commit(self, mock_check):
    output = self.clone / 'github_output'
    check_tvos_impact.main([
        '--src-dir',
        str(self.clone),
        '--pr-number',
        '1',
        '--pr-head-sha',
        self.head_sha,
        '--github-output',
        str(output),
    ])
    self.assertEqual(output.read_text(encoding='utf-8'), 'affects_tvos=false\n')
    self.assertEqual(mock_check.call_args.args[1],
                     ['new_name.h', 'old_name.h', 'pr.cc'])
    # The analysis runs on the merge commit.
    self.assertEqual(_git(self.clone, 'rev-parse', 'HEAD'), self.merge_sha)


if __name__ == '__main__':
  unittest.main()
