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
"""Integration test suite for autoroll_lib git plumbing operations."""

import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(__file__))
import autoroll_lib as lib  # pylint: disable=wrong-import-position

TRACK = '.github/AUTOROLL'


class AutorollPlumbingScenarioTest(unittest.TestCase):
  """Validates autoroll plumbing against the 7 core edge scenarios."""

  def setUp(self):
    self.tmpdir = tempfile.mkdtemp(prefix='autoroll_test_')
    self.old_cwd = os.getcwd()
    os.chdir(self.tmpdir)

    self._git('init', '-q', '-b', 'main')
    self._git('config', 'user.name', 'Cobalt Bot')
    self._git('config', 'user.email', 'bot@cobalt.foo')
    self._git('config', 'submodule.recurse', 'false')

    # Seed base state
    self._write('a.txt', '1\n2\n3\n')
    self._write('b.txt', 'b\n')
    self._write('c.txt', 'c\n')
    self._write('.gitmodules',
                '[submodule "sub"]\n\tpath = sub\n\turl = https://foo/sub\n')
    self._write(TRACK, 'BASE_SHA\n')
    self._git('add', '-A')
    self._gitlink('sub', '1' * 40)
    self.base_sha = self._commit_all('base commit', stage_all=False)

  def tearDown(self):
    os.chdir(self.old_cwd)
    shutil.rmtree(self.tmpdir, ignore_errors=True)

  def _git(self, *cmd, stdin=None):
    res = subprocess.run(['git', *cmd], check=True, capture_output=True,
                         text=True, input=stdin)
    return res.stdout.strip()

  def _raw_git(self, *cmd, stdin=None):
    res = subprocess.run(['git', *cmd], check=True, capture_output=True,
                         text=True, input=stdin)
    return res.stdout

  def _write(self, path, content):
    os.makedirs(os.path.dirname(path) or '.', exist_ok=True)
    with open(path, 'w', encoding='utf-8') as f:
      f.write(content)

  def _gitlink(self, path, oid):
    self._git('update-index', '--add', '--cacheinfo', f'160000,{oid},{path}')

  def _commit_all(self, msg, date='2026-01-01T12:00:00+0000', stage_all=True):
    if stage_all:
      self._git('add', '-A')
    env = dict(os.environ, GIT_AUTHOR_DATE=date, GIT_COMMITTER_DATE=date)
    subprocess.run(['git', 'commit', '-qm', msg, '--allow-empty'], check=True,
                   env=env)
    return self._git('rev-parse', 'HEAD')

  def _create_source_commit(self, file_updates, msg='source commit',
                            author='Upstream <up@src.foo>',
                            date='2026-01-02T12:00:00+0000'):
    self._git('checkout', '-q', '-B', 'source', self.base_sha)
    sub_oid = None
    for path, content in file_updates.items():
      if content is None:
        self._git('rm', '-q', path)
      elif path == 'sub':
        sub_oid = content
      else:
        self._write(path, content)
        self._git('add', path)
    if sub_oid is not None:
      self._gitlink('sub', sub_oid)
    name, email = re.match(r'^(.*) <(.*)>$', author).groups()
    env = dict(os.environ, GIT_AUTHOR_NAME=name, GIT_AUTHOR_EMAIL=email,
               GIT_AUTHOR_DATE=date, GIT_COMMITTER_DATE=date)
    subprocess.run(['git', 'commit', '-qm', msg, '--allow-empty'], check=True,
                   env=env)
    return self._git('rev-parse', 'HEAD')

  def test_scenario1_clean_single_cherry_pick(self):
    """Scenario 1: Clean Single Cherry-Pick (Fast Path)."""
    # Target modifies b.txt, source modifies a.txt
    self._git('checkout', '-q', '-B', 'target', self.base_sha)
    self._write('b.txt', 'target-b\n')
    target_head = self._commit_all('target update b')

    src_sha = self._create_source_commit({'a.txt': '1\n2\nsrc-3\n'})

    self._git('checkout', '-q', '-B', 'auto', target_head)
    metadata = ('2026-01-02T12:00:00+0000', 'Upstream <up@src.foo>',
                'Cherry pick: Clean change')

    status, unmerged = lib.apply_and_commit('cherry-pick', src_sha, metadata,
                                            True, (TRACK, src_sha))
    self.assertEqual(status, lib.CommitStatus.SUCCESS)
    self.assertIsNone(unmerged)

    # Check that HEAD advanced and files merged correctly
    self.assertEqual(self._raw_git('cat-file', 'blob', 'HEAD:a.txt'),
                     '1\n2\nsrc-3\n')
    self.assertEqual(self._raw_git('cat-file', 'blob', 'HEAD:b.txt'),
                     'target-b\n')
    self.assertEqual(self._raw_git('cat-file', 'blob', f'HEAD:{TRACK}'),
                     f'{src_sha}\n')

  def test_scenario2_batch_cherry_picks(self):
    """Scenario 2: Batch Cherry-Picks (Multi-Commit Roll)."""
    self._git('checkout', '-q', '-B', 'target', self.base_sha)
    self._write('target.txt', 'tgt\n')
    target_head = self._commit_all('target initial')

    shas = []
    for i in range(5):
      sha = self._create_source_commit({f'f{i}.txt': f'content {i}\n'},
                                       msg=f'src commit {i}')
      shas.append(sha)

    self._git('checkout', '-q', '-B', 'auto', target_head)
    for i, sha in enumerate(shas):
      metadata = ('2026-01-02T12:00:00+0000', 'Upstream <up@src.foo>',
                  f'Cherry pick: commit {i}')
      status, unmerged = lib.apply_and_commit('cherry-pick', sha, metadata,
                                              i == 0, (TRACK, sha))
      self.assertEqual(status, lib.CommitStatus.SUCCESS)
      self.assertIsNone(unmerged)
      self.assertEqual(self._raw_git('cat-file', 'blob', f'HEAD:f{i}.txt'),
                       f'content {i}\n')

    self.assertEqual(self._raw_git('cat-file', 'blob', f'HEAD:{TRACK}'),
                     f'{shas[-1]}\n')

  def test_scenario3_real_code_conflict_first_commit(self):
    """Scenario 3: Real Code Conflict on first commit becomes CONFLICTED."""
    self._git('checkout', '-q', '-B', 'target', self.base_sha)
    self._write('a.txt', '1\ntarget-conflict\n3\n')
    target_head = self._commit_all('target conflict')

    src_sha = self._create_source_commit(
        {'a.txt': '1\nsource-conflict\n3\n'})

    self._git('checkout', '-q', '-B', 'auto', target_head)
    metadata = ('2026-01-02T12:00:00+0000', 'Upstream <up@src.foo>',
                'Conflict test')

    status, unmerged = lib.apply_and_commit('cherry-pick', src_sha, metadata,
                                            True, (TRACK, src_sha))
    self.assertEqual(status, lib.CommitStatus.CONFLICTED)
    self.assertEqual(unmerged, ['a.txt'])

    commit_title = self._git('log', '-1', '--format=%s')
    self.assertTrue(commit_title.startswith('CONFLICTED'))
    blob_content = self._raw_git('cat-file', 'blob', 'HEAD:a.txt')
    self.assertIn('<<<<<<<', blob_content)
    self.assertIn('=======', blob_content)
    self.assertIn('>>>>>>>', blob_content)
    self.assertEqual(self._raw_git('cat-file', 'blob', f'HEAD:{TRACK}'),
                     f'CONFLICTED:{src_sha}\n')

  def test_scenario3b_conflict_on_subsequent_commit_fails(self):
    """Scenario 3b: Conflict on 2nd commit returns FAILED and does not advance."""
    self._git('checkout', '-q', '-B', 'target', self.base_sha)
    self._write('a.txt', '1\ntarget-conflict\n3\n')
    target_head = self._commit_all('target conflict')

    clean_sha = self._create_source_commit({'clean.txt': 'clean\n'})
    conflict_sha = self._create_source_commit(
        {'a.txt': '1\nsource-conflict\n3\n'})

    self._git('checkout', '-q', '-B', 'auto', target_head)

    # 1st commit is clean
    meta1 = ('2026-01-02T12:00:00+0000', 'Upstream <up@src.foo>', 'Clean 1')
    status, unmerged = lib.apply_and_commit('cherry-pick', clean_sha, meta1,
                                            True, (TRACK, clean_sha))
    self.assertEqual(status, lib.CommitStatus.SUCCESS)
    head_after_clean = self._git('rev-parse', 'HEAD')

    # 2nd commit conflicts -> should fail and NOT advance HEAD
    meta2 = ('2026-01-02T12:00:00+0000', 'Upstream <up@src.foo>', 'Conflicted 2')
    status, unmerged = lib.apply_and_commit('cherry-pick', conflict_sha, meta2,
                                            False, (TRACK, conflict_sha))
    self.assertEqual(status, lib.CommitStatus.FAILED)
    self.assertEqual(unmerged, ['a.txt'])
    self.assertEqual(self._git('rev-parse', 'HEAD'), head_after_clean)

  def test_scenario4_deleted_by_us(self):
    """Scenario 4: Deleted by Us (Ours deleted, theirs modified -> delete)."""
    self._git('checkout', '-q', '-B', 'target', self.base_sha)
    self._git('rm', '-q', 'b.txt')
    target_head = self._commit_all('target deletes b', stage_all=False)

    src_sha = self._create_source_commit({'b.txt': 'b-modified-by-src\n'})

    self._git('checkout', '-q', '-B', 'auto', target_head)
    metadata = ('2026-01-02T12:00:00+0000', 'Upstream <up@src.foo>',
                'Deleted by us test')

    status, unmerged = lib.apply_and_commit('cherry-pick', src_sha, metadata,
                                            True, (TRACK, src_sha))
    self.assertEqual(status, lib.CommitStatus.SUCCESS)
    self.assertIsNone(unmerged)

    # b.txt should remain deleted
    ls_out = self._git('ls-tree', 'HEAD', 'b.txt')
    self.assertEqual(ls_out, '')

  def test_scenario5_deleted_by_them(self):
    """Scenario 5: Deleted by Them (Ours modified, theirs deleted -> delete)."""
    self._git('checkout', '-q', '-B', 'target', self.base_sha)
    self._write('b.txt', 'b-modified-by-target\n')
    target_head = self._commit_all('target modifies b')

    src_sha = self._create_source_commit({'b.txt': None})

    self._git('checkout', '-q', '-B', 'auto', target_head)
    metadata = ('2026-01-02T12:00:00+0000', 'Upstream <up@src.foo>',
                'Deleted by them test')

    status, unmerged = lib.apply_and_commit('cherry-pick', src_sha, metadata,
                                            True, (TRACK, src_sha))
    self.assertEqual(status, lib.CommitStatus.SUCCESS)
    self.assertIsNone(unmerged)

    # b.txt should be deleted
    ls_out = self._git('ls-tree', 'HEAD', 'b.txt')
    self.assertEqual(ls_out, '')

  def test_scenario6_submodule_mode_conflict(self):
    """Scenario 6: Submodule mode 160000 conflict takes theirs SHA."""
    self._git('checkout', '-q', '-B', 'target', self.base_sha)
    self._gitlink('sub', '2' * 40)
    target_head = self._commit_all('target bump sub to 2', stage_all=False)

    src_sha = self._create_source_commit({'sub': '3' * 40})

    self._git('checkout', '-q', '-B', 'auto', target_head)
    metadata = ('2026-01-02T12:00:00+0000', 'Upstream <up@src.foo>',
                'Submodule conflict test')

    status, unmerged = lib.apply_and_commit('cherry-pick', src_sha, metadata,
                                            True, (TRACK, src_sha))
    self.assertEqual(status, lib.CommitStatus.SUCCESS)
    self.assertIsNone(unmerged)

    sub_entry = self._git('ls-tree', 'HEAD', 'sub')
    self.assertIn('160000 commit ' + ('3' * 40), sub_entry)

  def test_scenario7_gitmodules_conflict(self):
    """Scenario 7: .gitmodules conflict moves conflicted to .gitmodules_conflict."""
    self._git('checkout', '-q', '-B', 'target', self.base_sha)
    self._write('.gitmodules',
                '[submodule "sub"]\n\tpath = sub\n\turl = https://target/sub\n')
    target_head = self._commit_all('target update .gitmodules')

    src_sha = self._create_source_commit({
        '.gitmodules':
            '[submodule "sub"]\n\tpath = sub\n\turl = https://source/sub\n'
    })

    self._git('checkout', '-q', '-B', 'auto', target_head)
    metadata = ('2026-01-02T12:00:00+0000', 'Upstream <up@src.foo>',
                '.gitmodules conflict test')

    status, unmerged = lib.apply_and_commit('cherry-pick', src_sha, metadata,
                                            True, (TRACK, src_sha))
    self.assertEqual(status, lib.CommitStatus.SUCCESS)
    self.assertIsNone(unmerged)

    # .gitmodules has target (ours), .gitmodules_conflict has conflict markers
    gitmodules_content = self._raw_git('cat-file', 'blob', 'HEAD:.gitmodules')
    self.assertEqual(
        gitmodules_content,
        '[submodule "sub"]\n\tpath = sub\n\turl = https://target/sub\n')

    conflict_content = self._raw_git('cat-file', 'blob',
                                     'HEAD:.gitmodules_conflict')
    self.assertIn('<<<<<<<', conflict_content)


if __name__ == '__main__':
  unittest.main()
