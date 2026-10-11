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
"""Integration and unit tests for autoroll.py and autoroll_lib.py.

Critical User Journeys (CUJs) Covered:
- CUJ 1: Clean Roll (Full Mode)
  Rolls eligible commits chronologically from source to target. On success,
  updates the autoroll cursor file (.github/AUTOROLL) to the latest rolled SHA.
- CUJ 2: Clean Roll (Label Mode)
  Filters commits strictly by the `cp-<target_branch>` label. On success,
  the AUTOROLL marker file SHA is NEVER updated and remains stickied to the
  last full mode SHA.
- CUJ 3: Target Branch History Deduplication
  Candidate PRs already merged into target branch history (via cherry-pick
  commit metadata or PR references) are detected and skipped.
- CUJ 4: Incremental Stacking on Open PR
  Candidate PRs already cherry-picked onto the active autoroll PR branch
  (target..HEAD) are preserved in the PR description, and new candidate commits
  are stacked incrementally.
- CUJ 5: Conflict on First Commit (Full Mode)
  When the first cherry-pick produces unresolvable conflicts, the unmerged files
  are staged, the commit is committed with 'CONFLICTED ' prefix, the autoroll
  cursor is updated with 'CONFLICTED:<sha>', and the script halts with a
  conflict markdown block.
- CUJ 6: Conflict on First Commit (Label Mode)
  Behaves identically to full mode for conflict detection, staging, and PR
  titling, EXCEPT the AUTOROLL marker file SHA is NOT updated to the candidate
  commit SHA; it remains stickied to the last full mode SHA as
  'CONFLICTED:<last_full_sha>'.
- CUJ 7: Conflict on Subsequent Commit
  If a conflict occurs on a commit after prior clean commits have already been
  applied on the autoroll branch, a hard reset reverts the conflicted commit,
  preserving the prior clean commits, and the script halts cleanly without
  marking the PR as conflicted.
- CUJ 8: Auto-Resolvable Conflicts
  Resolves expected conflicts such as submodule pointer updates and deleted
  files via `resolve_conflicts` without user intervention.
- CUJ 9: Halt on Unresolved Conflicted PR Branch
  If .github/AUTOROLL on HEAD contains 'CONFLICTED:<sha>', the autoroller aborts
  immediately (exit code 1) until resolved.
- CUJ 10: Halt on Resolved Conflicted PR Branch
  If an existing open PR branch contains a commit starting with 'CONFLICTED',
  the autoroller halts (exit code 1) until the PR is squashed and merged.
- CUJ 11: Respects Max Commits Limit
  Halts cherry-picking once the configured `--max-commits` limit is reached.
- CUJ 12: Missing `--prs-json` Validation in Label Mode
  Exits with error (exit code 1) if `--mode label` is invoked without providing
  pre-fetched PR labels via `--prs-json`.
- CUJ 13: Out-of-Order PR Migration in Label Mode
  Because the roll cursor remains stickied to the last full mode SHA, any older
  PRs labeled later are discovered and migrated even if newer PRs have already
  been merged. The AUTOROLL file SHA remains stickied to the last full mode SHA.
"""

import io
import json
import os
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

# Add current directory to path
sys.path.append(os.path.dirname(__file__))

import autoroll  # pylint: disable=wrong-import-position
import autoroll_lib as lib  # pylint: disable=wrong-import-position


class TestAutorollLib(unittest.TestCase):
  """Unit tests for autoroll_lib helper functions."""

  def setUp(self):
    lib._PR_LABELS_CACHE.clear()  # pylint: disable=protected-access

  def tearDown(self):
    lib._PR_LABELS_CACHE.clear()  # pylint: disable=protected-access

  def test_load_pr_labels_from_file(self):
    prs_data = [
        {
            'number': 123,
            'labels': [{
                'name': 'cp-27.lts'
            }, {
                'name': 'feature'
            }]
        },
        {
            'number': 456,
            'labels': []
        },
    ]
    with tempfile.NamedTemporaryFile(
        mode='w', encoding='utf-8', delete=False) as f:
      json.dump(prs_data, f)
      temp_path = f.name

    try:
      lib.load_pr_labels_from_file(temp_path)
      self.assertEqual(lib.get_pr_labels(123), {'cp-27.lts', 'feature'})
      self.assertEqual(lib.get_pr_labels(456), set())
      self.assertEqual(lib.get_pr_labels(999), set())
    finally:
      os.remove(temp_path)

  @patch('autoroll_lib.get_out')
  def test_get_rolled_source_items(self, mock_get_out):
    mock_get_out.return_value = (
        'Cherry pick PR #12799: content: Fail gracefully\n'
        'Refer to original PR: #12799\n\n'
        '(cherry picked from commit 1111111111111111111111111111111111111111)\n'
        'Cherry pick PR #12800: some feature\n'
        '(cherry picked from commit 2222222222222222222222222222222222222222)\n'
    )
    shas, prs = lib.get_rolled_source_items('27.lts')
    self.assertEqual(
        shas, {
            '1111111111111111111111111111111111111111',
            '2222222222222222222222222222222222222222',
        })
    self.assertEqual(prs, {12799, 12800})

  @patch('autoroll_lib.get_out')
  def test_get_cherry_pick_metadata(self, mock_get_out):
    mock_get_out.return_value = (
        'Mon Oct 6 00:00:00 2026\x00Author <a@b.com>\x00Body line')
    date, author, msg = lib.get_cherry_pick_metadata('sha123', 'My Title',
                                                     '101')
    self.assertEqual(date, 'Mon Oct 6 00:00:00 2026')
    self.assertEqual(author, 'Author <a@b.com>')
    self.assertIn('Cherry pick PR #101: My Title', msg)
    self.assertIn('Refer to original PR: #101', msg)
    self.assertIn('(cherry picked from commit sha123)', msg)

    _, _, msg_no_pr = lib.get_cherry_pick_metadata('sha456', 'Direct Commit',
                                                   None)
    self.assertIn('Cherry pick commit sha456: Direct Commit', msg_no_pr)
    self.assertIn('Refer to original commit: sha456', msg_no_pr)


class TestAutorollIntegration(unittest.TestCase):
  """End-to-end integration tests for autoroll CUJs using Git repositories."""

  def setUp(self):
    lib._PR_LABELS_CACHE.clear()  # pylint: disable=protected-access
    self.temp_dir_obj = (
        tempfile.TemporaryDirectory()  # pylint: disable=consider-using-with
    )
    self.repo_dir = self.temp_dir_obj.name
    self.old_cwd = os.getcwd()
    os.chdir(self.repo_dir)

    self.git_env = dict(
        os.environ,
        GIT_AUTHOR_NAME='Test Author',
        GIT_AUTHOR_EMAIL='author@example.com',
        GIT_COMMITTER_NAME='Test Committer',
        GIT_COMMITTER_EMAIL='committer@example.com',
    )

    self.git('init', '-b', 'main')
    self.git('config', 'user.name', 'Test Committer')
    self.git('config', 'user.email', 'committer@example.com')
    self.git('config', 'commit.gpgsign', 'false')

    os.makedirs('.github', exist_ok=True)
    with open('initial.txt', 'w', encoding='utf-8') as f:
      f.write('initial repo content\n')
    with open('.github/AUTOROLL', 'w', encoding='utf-8') as f:
      f.write('root\n')
    self.git('add', '.')
    self.git('commit', '-m', 'Initial root commit')

    # Commit A on main: base commit where target branch will start
    with open('base.txt', 'w', encoding='utf-8') as f:
      f.write('base\n')
    self.git('add', 'base.txt')
    self.git('commit', '-m', 'Base commit on main')
    self.start_sha = self.get_head_sha()

    # Remote origin pointing to self so fetch origin works
    self.git('remote', 'add', 'origin', self.repo_dir)
    self.prs_files = []

  def tearDown(self):
    for prs_path in self.prs_files:
      if os.path.exists(prs_path):
        os.remove(prs_path)
    os.chdir(self.old_cwd)
    self.temp_dir_obj.cleanup()
    lib._PR_LABELS_CACHE.clear()  # pylint: disable=protected-access

  def git(self, *args):
    """Runs a git command in the test repository."""
    cmd = ['git'] + list(args)
    return subprocess.run(
        cmd,
        cwd=self.repo_dir,
        check=True,
        capture_output=True,
        text=True,
        env=self.git_env)

  def get_head_sha(self):
    """Returns the current HEAD SHA."""
    return self.git('rev-parse', 'HEAD').stdout.strip()

  def commit_file(self, filepath, content, message):
    """Creates or overwrites a file, stages it, and commits."""
    dirname = os.path.dirname(filepath)
    if dirname:
      os.makedirs(dirname, exist_ok=True)
    with open(filepath, 'w', encoding='utf-8') as f:
      f.write(content)
    self.git('add', filepath)
    self.git('commit', '-m', message)
    return self.get_head_sha()

  def init_target_branch(self, branch_name):
    """Creates target branch from start_sha with AUTOROLL initialized."""
    self.git('checkout', '-b', branch_name, self.start_sha)
    with open('.github/AUTOROLL', 'w', encoding='utf-8') as f:
      f.write(f'{self.start_sha}\n')
    self.git('add', '.github/AUTOROLL')
    self.git('commit', '-m', f'Set autoroll cursor on {branch_name}')
    self.git('checkout', 'main')

  def create_prs_json(self, pr_label_map):
    """Writes candidate PRs JSON from a mapping of {pr_num: [labels]}."""
    prs_data = [{
        'number': int(pr_num),
        'labels': [{
            'name': label
        } for label in labels]
    } for pr_num, labels in pr_label_map.items()]
    with tempfile.NamedTemporaryFile(
        mode='w', encoding='utf-8', suffix='.json', delete=False) as temp_file:
      json.dump(prs_data, temp_file)
      prs_path = temp_file.name
    self.prs_files.append(prs_path)
    return prs_path

  def run_autoroll(self, args_list):
    """Runs autoroll.main() capturing stdout, stderr, and exit code."""
    test_args = ['autoroll.py'] + args_list
    captured_stdout = io.StringIO()
    exit_code = 0
    with tempfile.TemporaryFile(mode='w+') as temp_err:
      with patch('sys.argv', test_args):
        with patch('sys.stdout', captured_stdout):
          with patch('sys.stderr', temp_err):
            try:
              autoroll.main()
            except SystemExit as e:
              exit_code = e.code if isinstance(e.code, int) else 1
      temp_err.seek(0)
      stderr_val = temp_err.read()
    return exit_code, captured_stdout.getvalue(), stderr_val

  def test_cuj1_clean_roll_full_mode(self):
    """CUJ 1: Clean Roll (Full Mode) rolls all commits and updates cursor."""
    self.init_target_branch('staging')
    self.commit_file('a.txt', 'content a\n', 'Add feature A (#101)')
    sha2 = self.commit_file('b.txt', 'content b\n', 'Direct commit without PR')

    self.git('checkout', 'staging')
    self.git('checkout', '-b', 'autoroll-main-to-staging')

    exit_code, stdout, _ = self.run_autoroll([
        '--source-branch',
        'main',
        '--target-branch',
        'staging',
        '--autoroll-file',
        '.github/AUTOROLL',
        '--max-commits',
        '10',
        '--existing-pr-sha',
        '',
        '--mode',
        'full',
    ])

    self.assertEqual(exit_code, 0)
    self.assertEqual(stdout.strip(), f'- #101\n- {sha2}')
    self.assertTrue(os.path.exists('a.txt'))
    self.assertTrue(os.path.exists('b.txt'))

    # .github/AUTOROLL on HEAD must be updated to the last rolled commit
    autoroll_content = self.git('show', 'HEAD:.github/AUTOROLL').stdout.strip()
    self.assertEqual(autoroll_content, sha2)

  def test_cuj2_clean_roll_label_mode(self):
    """CUJ 2: Label mode rolls only labeled PRs without moving cursor."""
    self.init_target_branch('27.lts')

    self.commit_file('a.txt', 'content a\n', 'Feature 101 (#101)')
    self.commit_file('b.txt', 'content b\n', 'Feature 102 (#102)')
    self.commit_file('c.txt', 'content c\n', 'Feature 103 (#103)')

    prs_json = self.create_prs_json({
        101: ['cp-27.lts'],
        102: ['unrelated-label'],
        103: ['cp-27.lts'],
    })

    self.git('checkout', '27.lts')
    self.git('checkout', '-b', 'autoroll-main-to-27.lts')

    exit_code, stdout, _ = self.run_autoroll([
        '--source-branch',
        'main',
        '--target-branch',
        '27.lts',
        '--autoroll-file',
        '.github/AUTOROLL',
        '--max-commits',
        '10',
        '--existing-pr-sha',
        '',
        '--mode',
        'label',
        '--prs-json',
        prs_json,
    ])

    self.assertEqual(exit_code, 0)
    self.assertEqual(stdout.strip(), '- #101\n- #103')
    self.assertTrue(os.path.exists('a.txt'))
    self.assertFalse(os.path.exists('b.txt'))
    self.assertTrue(os.path.exists('c.txt'))

    # Cursor must remain pinned to start_sha in label mode on success
    autoroll_content = self.git('show', 'HEAD:.github/AUTOROLL').stdout.strip()
    self.assertEqual(autoroll_content, self.start_sha)

  def test_cuj3_deduplication_against_merged_target_history(self):
    """CUJ 3: Deduplication recognizes CPs and squashed autoroll commits."""
    self.init_target_branch('27.lts')

    sha1 = self.commit_file('a.txt', 'content a\n', 'Feature 101 (#101)')
    sha2 = self.commit_file('b.txt', 'content b\n', 'Feature 102 (#102)')
    self.commit_file('c.txt', 'content c\n', 'Feature 103 (#103)')

    # On 27.lts: PR 101 merged via individual CP, 102 merged via CP
    self.git('checkout', '27.lts')
    with open('a.txt', 'w', encoding='utf-8') as f:
      f.write('content a\n')
    self.git('add', 'a.txt')
    msg_cp = (f'Cherry pick PR #101: Feature 101\n\n'
              f'Refer to original PR: #101\n\n'
              f'(cherry picked from commit {sha1})')
    self.git('commit', '-m', msg_cp)

    with open('b.txt', 'w', encoding='utf-8') as f:
      f.write('content b\n')
    self.git('add', 'b.txt')
    msg_cp2 = (f'Cherry pick PR #102: Feature 102\n\n'
               f'Refer to original PR: #102\n\n'
               f'(cherry picked from commit {sha2})')
    self.git('commit', '-m', msg_cp2)

    self.git('checkout', '-b', 'autoroll-main-to-27.lts')

    prs_json = self.create_prs_json({
        101: ['cp-27.lts'],
        102: ['cp-27.lts'],
        103: ['cp-27.lts'],
    })

    exit_code, stdout, _ = self.run_autoroll([
        '--source-branch',
        'main',
        '--target-branch',
        '27.lts',
        '--autoroll-file',
        '.github/AUTOROLL',
        '--max-commits',
        '10',
        '--existing-pr-sha',
        '',
        '--mode',
        'label',
        '--prs-json',
        prs_json,
    ])

    self.assertEqual(exit_code, 0)
    # Both 101 and 102 must be skipped as already merged; only 103 rolled
    self.assertEqual(stdout.strip(), '- #103')
    self.assertTrue(os.path.exists('c.txt'))

  def test_cuj4_incremental_stacking_on_open_pr(self):
    """CUJ 4: Incremental stacking preserves commits in target..HEAD."""
    self.init_target_branch('27.lts')

    sha1 = self.commit_file('a.txt', 'content a\n', 'Feature 101 (#101)')
    self.commit_file('b.txt', 'content b\n', 'Feature 102 (#102)')

    # Simulate existing open PR branch that already has 101
    self.git('checkout', '27.lts')
    self.git('checkout', '-b', 'autoroll-main-to-27.lts')
    with open('a.txt', 'w', encoding='utf-8') as f:
      f.write('content a\n')
    self.git('add', 'a.txt')
    msg_open_pr = (f'Cherry pick PR #101: Feature 101\n\n'
                   f'Refer to original PR: #101\n\n'
                   f'(cherry picked from commit {sha1})')
    self.git('commit', '-m', msg_open_pr)

    prs_json = self.create_prs_json({
        101: ['cp-27.lts'],
        102: ['cp-27.lts'],
    })

    exit_code, stdout, _ = self.run_autoroll([
        '--source-branch',
        'main',
        '--target-branch',
        '27.lts',
        '--autoroll-file',
        '.github/AUTOROLL',
        '--max-commits',
        '10',
        '--existing-pr-sha',
        '',
        '--mode',
        'label',
        '--prs-json',
        prs_json,
    ])

    self.assertEqual(exit_code, 0)
    self.assertEqual(stdout.strip(), '- #101\n- #102')
    self.assertTrue(os.path.exists('b.txt'))

  def test_cuj5_conflict_on_first_commit_full_mode(self):
    """CUJ 5: Conflict on first commit in full mode stages conflict."""
    self.commit_file('conflict.txt', 'line 1\nline 2\n',
                     'Add base conflict file')
    base_sha = self.get_head_sha()

    # Target branch modifies line 2
    self.git('checkout', '-b', 'staging', base_sha)
    with open('.github/AUTOROLL', 'w', encoding='utf-8') as f:
      f.write(f'{base_sha}\n')
    self.git('add', '.github/AUTOROLL')
    self.commit_file('conflict.txt', 'line 1\nstaging edit\n', 'Staging edit')

    # Main modifies line 2 differently
    self.git('checkout', 'main')
    sha1 = self.commit_file('conflict.txt', 'line 1\nmain edit\n',
                            'Main edit (#101)')
    self.commit_file('other.txt', 'other\n', 'Other commit (#102)')

    self.git('checkout', 'staging')
    self.git('checkout', '-b', 'autoroll-main-to-staging')

    exit_code, stdout, _ = self.run_autoroll([
        '--source-branch',
        'main',
        '--target-branch',
        'staging',
        '--autoroll-file',
        '.github/AUTOROLL',
        '--max-commits',
        '10',
        '--existing-pr-sha',
        '',
        '--mode',
        'full',
    ])

    self.assertEqual(exit_code, 0)
    expected_output = ('- #101\n\n'
                       'CONFLICTED files:\n'
                       '```\n'
                       'conflict.txt\n'
                       '```')
    self.assertEqual(stdout.strip(), expected_output)

    last_title = self.git('log', '-1', '--format=%s').stdout.strip()
    self.assertTrue(last_title.startswith('CONFLICTED Cherry pick PR #101:'))

    autoroll_content = self.git('show', 'HEAD:.github/AUTOROLL').stdout.strip()
    self.assertEqual(autoroll_content, f'CONFLICTED:{sha1}')

  def test_cuj6_conflict_on_first_commit_label_mode(self):
    """CUJ 6: Conflict on first commit in label mode behaves like full mode."""
    self.commit_file('conflict.txt', 'line 1\nline 2\n',
                     'Add base conflict file')
    base_sha = self.get_head_sha()

    # Target branch modifies line 2
    self.git('checkout', '-b', '27.lts', base_sha)
    with open('.github/AUTOROLL', 'w', encoding='utf-8') as f:
      f.write(f'{base_sha}\n')
    self.git('add', '.github/AUTOROLL')
    self.commit_file('conflict.txt', 'line 1\n27.lts edit\n', '27.lts edit')

    # Main modifies line 2 differently
    self.git('checkout', 'main')
    self.commit_file('conflict.txt', 'line 1\nmain edit\n', 'Main edit (#101)')
    self.commit_file('other.txt', 'other\n', 'Other commit (#102)')

    self.git('checkout', '27.lts')
    self.git('checkout', '-b', 'autoroll-main-to-27.lts')

    prs_json = self.create_prs_json({
        101: ['cp-27.lts'],
        102: ['cp-27.lts'],
    })

    exit_code, stdout, _ = self.run_autoroll([
        '--source-branch',
        'main',
        '--target-branch',
        '27.lts',
        '--autoroll-file',
        '.github/AUTOROLL',
        '--max-commits',
        '10',
        '--existing-pr-sha',
        '',
        '--mode',
        'label',
        '--prs-json',
        prs_json,
    ])

    self.assertEqual(exit_code, 0)
    expected_output = ('- #101\n\n'
                       'CONFLICTED files:\n'
                       '```\n'
                       'conflict.txt\n'
                       '```')
    self.assertEqual(stdout.strip(), expected_output)

    last_title = self.git('log', '-1', '--format=%s').stdout.strip()
    self.assertTrue(last_title.startswith('CONFLICTED Cherry pick PR #101:'))

    # In label mode, AUTOROLL marker file SHA must not be updated to candidate
    # SHA; it must remain stickied to the last full mode SHA (base_sha).
    autoroll_content = self.git('show', 'HEAD:.github/AUTOROLL').stdout.strip()
    self.assertEqual(autoroll_content, f'CONFLICTED:{base_sha}')

  def test_cuj7_conflict_on_subsequent_commit(self):
    """CUJ 7: Conflict on subsequent commit executes hard reset."""
    self.commit_file('conflict.txt', 'line 1\nline 2\n',
                     'Add base conflict file')
    base_sha = self.get_head_sha()

    self.git('checkout', '-b', '27.lts', base_sha)
    with open('.github/AUTOROLL', 'w', encoding='utf-8') as f:
      f.write(f'{base_sha}\n')
    self.git('add', '.github/AUTOROLL')
    self.commit_file('conflict.txt', 'line 1\n27.lts edit\n', '27.lts edit')

    self.git('checkout', 'main')
    self.commit_file('clean.txt', 'clean content\n', 'Clean feature (#101)')
    self.commit_file('conflict.txt', 'line 1\nmain edit\n', 'Conflict (#102)')

    self.git('checkout', '27.lts')
    self.git('checkout', '-b', 'autoroll-main-to-27.lts')

    prs_json = self.create_prs_json({
        101: ['cp-27.lts'],
        102: ['cp-27.lts'],
    })

    exit_code, stdout, _ = self.run_autoroll([
        '--source-branch',
        'main',
        '--target-branch',
        '27.lts',
        '--autoroll-file',
        '.github/AUTOROLL',
        '--max-commits',
        '10',
        '--existing-pr-sha',
        '',
        '--mode',
        'label',
        '--prs-json',
        prs_json,
    ])

    self.assertEqual(exit_code, 0)
    # Output contains only the successful commit 101 (no CONFLICTED files block)
    self.assertEqual(stdout.strip(), '- #101')
    self.assertTrue(os.path.exists('clean.txt'))

    # Working tree and index must be clean
    status = self.git('status', '--porcelain').stdout.strip()
    self.assertEqual(status, '')

  def test_cuj8_auto_resolvable_deleted_by_them_conflict(self):
    """CUJ 8: Auto-resolves deleted file conflict without failure."""
    self.commit_file('to_delete.txt', 'content\n', 'Add file to delete')
    base_sha = self.get_head_sha()

    self.git('checkout', '-b', '27.lts', base_sha)
    with open('.github/AUTOROLL', 'w', encoding='utf-8') as f:
      f.write(f'{base_sha}\n')
    self.git('add', '.github/AUTOROLL')
    self.git('rm', 'to_delete.txt')
    self.git('commit', '-m', 'Delete file on 27.lts')

    self.git('checkout', 'main')
    with open('to_delete.txt', 'w', encoding='utf-8') as f:
      f.write('modified content\n')
    self.git('add', 'to_delete.txt')
    with open('added_file.txt', 'w', encoding='utf-8') as f:
      f.write('added feature\n')
    self.git('add', 'added_file.txt')
    self.git('commit', '-m', 'Modify to_delete and add file (#101)')

    self.git('checkout', '27.lts')
    self.git('checkout', '-b', 'autoroll-main-to-27.lts')

    prs_json = self.create_prs_json({101: ['cp-27.lts']})

    exit_code, stdout, _ = self.run_autoroll([
        '--source-branch',
        'main',
        '--target-branch',
        '27.lts',
        '--autoroll-file',
        '.github/AUTOROLL',
        '--max-commits',
        '10',
        '--existing-pr-sha',
        '',
        '--mode',
        'label',
        '--prs-json',
        prs_json,
    ])

    self.assertEqual(exit_code, 0)
    self.assertEqual(stdout.strip(), '- #101')
    last_title = self.git('log', '-1', '--format=%s').stdout.strip()
    self.assertFalse(last_title.startswith('CONFLICTED'))
    self.assertTrue(os.path.exists('added_file.txt'))
    self.assertFalse(os.path.exists('to_delete.txt'))

  def test_cuj9_halts_on_unresolved_conflicted_pr_branch(self):
    """CUJ 9: Exits with code 1 if HEAD has an unresolved CONFLICTED commit."""
    self.init_target_branch('27.lts')
    self.git('checkout', '27.lts')
    self.git('checkout', '-b', 'autoroll-main-to-27.lts')

    with open('.github/AUTOROLL', 'w', encoding='utf-8') as f:
      f.write('CONFLICTED:some_sha_1234\n')
    self.git('add', '.github/AUTOROLL')
    self.git('commit', '-m', 'Conflicted commit')

    prs_json = self.create_prs_json({101: ['cp-27.lts']})

    exit_code, _, stderr = self.run_autoroll([
        '--source-branch',
        'main',
        '--target-branch',
        '27.lts',
        '--autoroll-file',
        '.github/AUTOROLL',
        '--max-commits',
        '10',
        '--existing-pr-sha',
        '',
        '--mode',
        'label',
        '--prs-json',
        prs_json,
    ])

    self.assertEqual(exit_code, 1)
    self.assertIn('Autoroll branch has an unresolved CONFLICTED commit.',
                  stderr)

  def test_cuj10_halts_on_resolved_conflicted_pr_branch(self):
    """CUJ 10: Halts if existing PR was resolved until squashed & merged."""
    self.init_target_branch('27.lts')
    self.git('checkout', '27.lts')
    self.git('checkout', '-b', 'autoroll-main-to-27.lts')

    # Commit 1: CONFLICTED
    with open('file.txt', 'w', encoding='utf-8') as f:
      f.write('conflicted\n')
    self.git('add', 'file.txt')
    self.git('commit', '-m', 'CONFLICTED Cherry pick PR #101: Feature 101')
    conflicted_pr_sha = self.get_head_sha()

    # Commit 2: RESOLVED
    with open('.github/AUTOROLL', 'w', encoding='utf-8') as f:
      f.write(f'{conflicted_pr_sha}\n')
    with open('file.txt', 'w', encoding='utf-8') as f:
      f.write('resolved\n')
    self.git('add', '.')
    self.git('commit', '-m', 'RESOLVED Cherry pick PR #101: Feature 101')

    prs_json = self.create_prs_json({101: ['cp-27.lts']})

    exit_code, _, stderr = self.run_autoroll([
        '--source-branch',
        'main',
        '--target-branch',
        '27.lts',
        '--autoroll-file',
        '.github/AUTOROLL',
        '--max-commits',
        '10',
        '--existing-pr-sha',
        conflicted_pr_sha,
        '--mode',
        'label',
        '--prs-json',
        prs_json,
    ])

    self.assertEqual(exit_code, 1)
    self.assertIn(
        'Autoroll branch has a resolved CONFLICTED commit. '
        'Squash and merge before autoroll will continue.', stderr)

  def test_cuj11_respects_max_commits(self):
    """CUJ 11: Autoroller stops rolling once max commits limit is reached."""
    self.init_target_branch('staging')

    for i in range(1, 6):
      self.commit_file(f'f{i}.txt', f'content {i}\n', f'Feature {i} (#{100+i})')

    self.git('checkout', 'staging')
    self.git('checkout', '-b', 'autoroll-main-to-staging')

    exit_code, stdout, stderr = self.run_autoroll([
        '--source-branch',
        'main',
        '--target-branch',
        'staging',
        '--autoroll-file',
        '.github/AUTOROLL',
        '--max-commits',
        '2',
        '--existing-pr-sha',
        '',
        '--mode',
        'full',
    ])

    self.assertEqual(exit_code, 0)
    self.assertEqual(stdout.strip(), '- #101\n- #102')
    self.assertIn('Reached commit limit (2).', stderr)

  def test_cuj12_label_mode_requires_prs_json(self):
    """CUJ 12: Exits with code 1 if label mode is invoked without --prs-json."""
    self.init_target_branch('27.lts')
    self.git('checkout', '27.lts')
    self.git('checkout', '-b', 'autoroll-main-to-27.lts')

    exit_code, _, stderr = self.run_autoroll([
        '--source-branch',
        'main',
        '--target-branch',
        '27.lts',
        '--autoroll-file',
        '.github/AUTOROLL',
        '--max-commits',
        '10',
        '--existing-pr-sha',
        '',
        '--mode',
        'label',
    ])

    self.assertEqual(exit_code, 1)
    self.assertIn('Error: --prs-json is required in label mode.', stderr)

  def test_cuj13_out_of_order_prs_in_label_mode(self):
    """CUJ 13: Picks unrolled older PR when newer PR is already merged."""
    self.init_target_branch('27.lts')

    self.commit_file('a.txt', 'content a\n', 'Older PR (#101)')
    sha2 = self.commit_file('b.txt', 'content b\n', 'Newer PR (#102)')

    # On 27.lts: PR 102 was already merged earlier out of order
    self.git('checkout', '27.lts')
    with open('b.txt', 'w', encoding='utf-8') as f:
      f.write('content b\n')
    self.git('add', 'b.txt')
    msg_newer = (f'Cherry pick PR #102: Newer PR\n\n'
                 f'Refer to original PR: #102\n\n'
                 f'(cherry picked from commit {sha2})')
    self.git('commit', '-m', msg_newer)

    self.git('checkout', '-b', 'autoroll-main-to-27.lts')

    prs_json = self.create_prs_json({
        101: ['cp-27.lts'],
        102: ['cp-27.lts'],
    })

    exit_code, stdout, _ = self.run_autoroll([
        '--source-branch',
        'main',
        '--target-branch',
        '27.lts',
        '--autoroll-file',
        '.github/AUTOROLL',
        '--max-commits',
        '10',
        '--existing-pr-sha',
        '',
        '--mode',
        'label',
        '--prs-json',
        prs_json,
    ])

    self.assertEqual(exit_code, 0)
    self.assertEqual(stdout.strip(), '- #101')
    self.assertTrue(os.path.exists('a.txt'))
    # Ensure AUTOROLL marker file SHA remains stickied to last full mode SHA
    autoroll_content = self.git('show', 'HEAD:.github/AUTOROLL').stdout.strip()
    self.assertEqual(autoroll_content, self.start_sha)


if __name__ == '__main__':
  unittest.main()
