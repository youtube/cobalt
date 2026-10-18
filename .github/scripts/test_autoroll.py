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
"""Integration/E2E blackbox tests for autoroll.py.

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
  When the first cherry-pick produces unresolvable conflicts, it is isolated
  on its own conflict branch (`autoroll-conflict-<src>-to-<tgt>-<id>`), leaving
  the clean branch empty.
- CUJ 6: Conflict in Label Mode Uses a Conflict Branch
  The clean autoroll branch never receives a CONFLICTED commit. The conflicting
  PR is skipped on the clean branch and committed as the single CONFLICTED
  commit of its own `autoroll-conflict-<src>-to-<tgt>-<pr>` branch built from
  the target branch. The AUTOROLL marker file is never touched in label mode.
  The manifest describes both branches for the workflow.
- CUJ 7: Conflict on Subsequent Commit
  If a conflict occurs on a commit after prior clean commits have already been
  applied on the autoroll branch, a hard reset reverts the conflicted commit,
  preserving the prior clean commits. In both modes the conflicting commit is
  isolated on its own conflict branch.
- CUJ 8: Auto-Resolvable Conflicts
  Resolves expected conflicts such as submodule pointer updates and deleted
  files via `resolve_conflicts` without user intervention.
- CUJ 9: Legacy Conflicted Clean Branch Is Rebuilt (Label Mode)
  A clean branch holding a roller CONFLICTED commit (from before conflicts
  moved to their own branches) is rebuilt from the target; the conflict gets
  its own branch. A stray 'CONFLICTED:' cursor on the target is tolerated.
- CUJ 10: Human Commits on the Clean Branch Are Left Alone (Label Mode)
  Any commit not written by the roller freezes the branch: nothing is rolled,
  rebuilt, or deleted (exit code 0, empty output).
- CUJ 10b: Full Mode Stops at Conflict and Respects Resolutions
  In full mode, a conflict stops rolling further commits to preserve linear
  history, isolates the conflict to its own branch, and respects human
  resolution commits.
- CUJ 11: Respects Max Commits Limit
  Halts cherry-picking once the configured `--max-commits` limit is reached.
- CUJ 12: Missing `--prs-json` Validation in Label Mode
  Exits with error (exit code 1) if `--mode label` is invoked without providing
  pre-fetched PR labels via `--prs-json`.
- CUJ 13: Out-of-Order PR Migration in Label Mode
  Because the roll cursor remains stickied to the last full mode SHA, any older
  PRs labeled later are discovered and migrated even if newer PRs have already
  been merged. The AUTOROLL file SHA remains stickied to the last full mode SHA.
- CUJ 14: Older Conflicting PR Never Stacks on Rolled Commits
  An older PR labeled after newer PRs were already rolled onto the open autoroll
  PR is isolated on a conflict branch; the clean branch is left untouched.
- CUJ 15: Label Removal Deletes the Conflict Branch
  Removing `cp-<target>` from a conflicting PR schedules its conflict branch
  for deletion (closing the PR) while the clean branch keeps rolling.
- CUJ 16: Dependency Labeled Later Fixes the Conflict
  Candidates are applied in source order, so labeling the older PR a
  conflicting pick depends on makes both apply cleanly; the conflict branch is
  deleted.
- CUJ 17: Unchanged Conflict Is a No-op
  Rebuilding a conflict branch with an identical patch keeps the existing SHA
  so the workflow does not push or comment.
- CUJ 18: Target Change Rebuilds the Conflict Branch
  When the target branch changes the conflicted result, the conflict branch is
  rebuilt and flagged as changed.
- CUJ 19: Resolved Conflict Branch Is Left Alone
  A conflict branch with human (RESOLVED) commits is never rebuilt or deleted,
  even after the label is removed.
- CUJ 20: Single Conflict Branch Limit
  At most one conflict branch exists at a time per target; existing branches
  keep their slot and additional conflicts are reported as pending.
- CUJ 21: Label Removal From a Rolled PR Rebuilds the Clean Branch
  When a PR already on the clean branch loses its label, the clean branch is
  rebuilt from the target without it; an empty result is reported so the
  workflow can delete the branch.
- CUJ 22: Full Mode Never Stacks CONFLICTED on a Non-Empty Branch
  Whether a conflict may be committed depends on the branch state, not on the
  candidate iteration order.
- CUJ 23: Unrelated Target Move Keeps the Conflict Branch
  The conflict commit's patch id, not the whole tree, decides whether a
  conflict branch is rebuilt, so unrelated target changes cause no churn.
- CUJ 24: Hands-off Conflict Branch Occupies the Slot
  A conflict branch a human is resolving occupies the single conflict slot;
  further conflicts are reported as pending until the slot frees up.
- CUJ 25: Direct Commit Conflict in Full Mode
  A commit without a PR number that conflicts in full mode is committed on an
  autoroll-conflict-<src>-to-<tgt>-<sha[:10]> branch.

Integration Test Guidelines:
- All integration tests MUST verify the branch state after the SUT finishes:
  1. Verify working tree and index cleanliness via `assert_working_tree_clean`.
  2. Verify applied commits on the branch match expected counts and titles via
     `get_branch_commit_titles`, or verify HEAD remains unchanged on aborts.
  3. Verify working tree files and conflict markers match expected state.
  4. Verify `.github/AUTOROLL` cursor file content matches expected state.
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

  def git(self, *args, check=True):
    """Runs a git command in the test repository."""
    cmd = ['git'] + list(args)
    return subprocess.run(
        cmd,
        cwd=self.repo_dir,
        check=check,
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

  def run_label_autoroll(self,
                         pr_label_map,
                         *,
                         target='27.lts',
                         max_commits=10):
    """Runs autoroll in label mode and returns (exit_code, stdout, manifest).
    """
    prs_json = self.create_prs_json(pr_label_map)
    manifest_path = os.path.join(self.repo_dir, 'manifest.json')
    exit_code, stdout, _ = self.run_autoroll([
        '--source-branch',
        'main',
        '--target-branch',
        target,
        '--autoroll-file',
        '.github/AUTOROLL',
        '--max-commits',
        str(max_commits),
        '--mode',
        'label',
        '--prs-json',
        prs_json,
        '--manifest',
        manifest_path,
    ])
    manifest = None
    if os.path.exists(manifest_path):
      with open(manifest_path, 'r', encoding='utf-8') as f:
        manifest = json.load(f)
      os.remove(manifest_path)
    return exit_code, stdout, manifest

  def setup_conflict_scenario(self, target='27.lts'):
    """Creates target with an edit to conflict.txt that main edits differently.

    Returns the SHA of the conflicting main commit (#101). Leaves HEAD on a
    fresh autoroll branch based on target.
    """
    self.commit_file('conflict.txt', 'line 1\nline 2\n',
                     'Add base conflict file')
    base_sha = self.get_head_sha()

    self.git('checkout', '-b', target, base_sha)
    with open('.github/AUTOROLL', 'w', encoding='utf-8') as f:
      f.write(f'{base_sha}\n')
    self.git('add', '.github/AUTOROLL')
    self.commit_file('conflict.txt', f'line 1\n{target} edit\n',
                     f'{target} edit')

    self.git('checkout', 'main')
    conflict_sha = self.commit_file('conflict.txt', 'line 1\nmain edit\n',
                                    'Main edit (#101)')

    self.git('checkout', target)
    self.git('checkout', '-b', f'autoroll-main-to-{target}')
    self.base_sha = base_sha
    return conflict_sha

  def commit_cherry_pick(self, filepath, content, *, pr_num, title, sha):
    """Commits a bot-style cherry-pick commit on the current branch."""
    with open(filepath, 'w', encoding='utf-8') as f:
      f.write(content)
    self.git('add', filepath)
    msg = (f'Cherry pick PR #{pr_num}: {title}\n\n'
           f'Refer to original PR: #{pr_num}\n\n'
           f'(cherry picked from commit {sha})')
    self.git('commit', '-m', msg)
    return self.get_head_sha()

  def branch_sha(self, branch):
    """Returns the SHA of a local branch or None if it does not exist."""
    res = subprocess.run(['git', 'rev-parse', '--verify', '--quiet', branch],
                         cwd=self.repo_dir,
                         check=False,
                         capture_output=True,
                         text=True,
                         env=self.git_env)
    return res.stdout.strip() if res.returncode == 0 else None

  def get_commit_titles(self, base_ref, ref):
    output = self.git('log', '--reverse', f'{base_ref}..{ref}',
                      '--format=%s').stdout.strip()
    return output.splitlines() if output else []

  def assert_conflict_branch(self, branch, pr_num, target='27.lts'):
    """Asserts branch holds one CONFLICTED commit for pr_num on top of target.
    """
    titles = self.get_commit_titles(target, branch)
    self.assertEqual(len(titles), 1, titles)
    self.assertTrue(
        titles[0].startswith(f'CONFLICTED Cherry pick PR #{pr_num}:'),
        titles[0])
    conflict_content = self.git('show', f'{branch}:conflict.txt').stdout
    self.assertIn('<<<<<<< HEAD', conflict_content)
    # Label-mode conflict commits never touch the AUTOROLL cursor.
    autoroll_content = self.git('show',
                                f'{branch}:.github/AUTOROLL').stdout.strip()
    self.assertEqual(autoroll_content, self.base_sha)

  def assert_working_tree_clean(self):
    """Asserts that git status shows a clean working tree and index."""
    status = self.git('status', '--porcelain').stdout.strip()
    self.assertEqual(status, '')

  def get_branch_commit_titles(self, base_ref):
    """Returns commit titles between base_ref and HEAD in chronological order.
    """
    output = self.git('log', '--reverse', f'{base_ref}..HEAD',
                      '--format=%s').stdout.strip()
    return output.splitlines() if output else []

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
        '--mode',
        'full',
    ])

    self.assertEqual(exit_code, 0)
    self.assertEqual(stdout.strip(), f'- #101\n- {sha2}')
    self.assertTrue(os.path.exists('a.txt'))
    self.assertTrue(os.path.exists('b.txt'))

    # Verify branch state after SUT finishes
    self.assert_working_tree_clean()
    commit_titles = self.get_branch_commit_titles('staging')
    self.assertEqual(len(commit_titles), 2)
    self.assertTrue(commit_titles[0].startswith('Cherry pick PR #101:'))
    self.assertEqual(commit_titles[1],
                     f'Cherry pick commit {sha2}: Direct commit without PR')
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

    # Verify branch state after SUT finishes
    self.assert_working_tree_clean()
    commit_titles = self.get_branch_commit_titles('27.lts')
    self.assertEqual(len(commit_titles), 2)
    self.assertTrue(commit_titles[0].startswith('Cherry pick PR #101:'))
    self.assertTrue(commit_titles[1].startswith('Cherry pick PR #103:'))
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
        '--mode',
        'label',
        '--prs-json',
        prs_json,
    ])

    self.assertEqual(exit_code, 0)
    # Both 101 and 102 must be skipped as already merged; only 103 rolled
    self.assertEqual(stdout.strip(), '- #103')
    self.assertTrue(os.path.exists('c.txt'))

    # Verify branch state after SUT finishes
    self.assert_working_tree_clean()
    commit_titles = self.get_branch_commit_titles('27.lts')
    self.assertEqual(len(commit_titles), 1)
    self.assertTrue(commit_titles[0].startswith('Cherry pick PR #103:'))
    autoroll_content = self.git('show', 'HEAD:.github/AUTOROLL').stdout.strip()
    self.assertEqual(autoroll_content, self.start_sha)

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
        '--mode',
        'label',
        '--prs-json',
        prs_json,
    ])

    self.assertEqual(exit_code, 0)
    self.assertEqual(stdout.strip(), '- #101\n- #102')
    self.assertTrue(os.path.exists('b.txt'))

    # Verify branch state after SUT finishes
    self.assert_working_tree_clean()
    commit_titles = self.get_branch_commit_titles('27.lts')
    self.assertEqual(len(commit_titles), 2)
    self.assertTrue(commit_titles[0].startswith('Cherry pick PR #101:'))
    self.assertTrue(commit_titles[1].startswith('Cherry pick PR #102:'))
    autoroll_content = self.git('show', 'HEAD:.github/AUTOROLL').stdout.strip()
    self.assertEqual(autoroll_content, self.start_sha)

  def test_cuj5_conflict_on_first_commit_full_mode(self):
    """CUJ 5: Conflict on first commit in full mode isolates conflict to branch.
    """
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

    with tempfile.NamedTemporaryFile('w+', encoding='utf-8', delete=False) as f:
      manifest_path = f.name

    try:
      exit_code, stdout, _ = self.run_autoroll([
          '--source-branch',
          'main',
          '--target-branch',
          'staging',
          '--autoroll-file',
          '.github/AUTOROLL',
          '--max-commits',
          '10',
          '--mode',
          'full',
          '--manifest',
          manifest_path,
      ])

      self.assertEqual(exit_code, 0)
      self.assertEqual(stdout.strip(), '')

      # Clean branch remains empty on top of staging.
      self.assert_working_tree_clean()
      commit_titles = self.get_branch_commit_titles('staging')
      self.assertEqual(len(commit_titles), 0)

      # Conflict branch holds the single CONFLICTED cherry-pick.
      branch = 'autoroll-conflict-main-to-staging-101'
      conflict_titles = self.get_commit_titles('staging', branch)
      self.assertEqual(len(conflict_titles), 1)
      self.assertTrue(
          conflict_titles[0].startswith('CONFLICTED Cherry pick PR #101:'))
      conflict_content = self.git('show', f'{branch}:conflict.txt').stdout
      self.assertIn('<<<<<<< HEAD', conflict_content)
      autoroll_content = self.git('show',
                                  f'{branch}:.github/AUTOROLL').stdout.strip()
      self.assertEqual(autoroll_content, f'CONFLICTED:{sha1}')

      with open(manifest_path, 'r', encoding='utf-8') as f:
        manifest = json.load(f)
      self.assertEqual(manifest['clean']['items'], [])
      self.assertEqual(len(manifest['conflicts']), 1)
      self.assertEqual(manifest['conflicts'][0]['pr'], 101)
      self.assertEqual(manifest['conflicts'][0]['branch'], branch)
    finally:
      if os.path.exists(manifest_path):
        os.unlink(manifest_path)

  def test_cuj6_conflict_in_label_mode_uses_conflict_branch(self):
    """CUJ 6: Label mode keeps the clean branch clean; conflict gets own branch.
    """
    conflict_sha = self.setup_conflict_scenario()
    self.git('checkout', 'main')
    self.commit_file('other.txt', 'other\n', 'Other commit (#102)')
    self.git('checkout', 'autoroll-main-to-27.lts')

    exit_code, stdout, manifest = self.run_label_autoroll({
        101: ['cp-27.lts'],
        102: ['cp-27.lts'],
    })

    self.assertEqual(exit_code, 0)
    # Only the clean pick is reported for the autoroll PR.
    self.assertEqual(stdout.strip(), '- #102')

    # Clean branch: one clean commit, no conflict markers, cursor untouched.
    self.assert_working_tree_clean()
    self.assertEqual(
        self.git('rev-parse', '--abbrev-ref', 'HEAD').stdout.strip(),
        'autoroll-main-to-27.lts')
    commit_titles = self.get_branch_commit_titles('27.lts')
    self.assertEqual(len(commit_titles), 1)
    self.assertTrue(commit_titles[0].startswith('Cherry pick PR #102:'))
    with open('conflict.txt', 'r', encoding='utf-8') as f:
      self.assertEqual(f.read(), 'line 1\n27.lts edit\n')
    autoroll_content = self.git('show', 'HEAD:.github/AUTOROLL').stdout.strip()
    self.assertEqual(autoroll_content, self.base_sha)

    # Conflict branch: a single CONFLICTED commit on top of the target.
    branch = 'autoroll-conflict-main-to-27.lts-101'
    self.assert_conflict_branch(branch, 101)

    self.assertEqual(manifest['clean']['items'], ['- #102'])
    self.assertTrue(manifest['clean']['changed'])
    self.assertFalse(manifest['clean']['rebuilt'])
    self.assertEqual(len(manifest['conflicts']), 1)
    conflict = manifest['conflicts'][0]
    self.assertEqual(conflict['pr'], 101)
    self.assertEqual(conflict['branch'], branch)
    self.assertEqual(conflict['old_sha'], '')
    self.assertTrue(conflict['changed'])
    self.assertTrue(
        conflict['title'].startswith('CONFLICTED Cherry pick PR #101:'))
    self.assertIn('Refer to original PR: #101', conflict['body'])
    self.assertIn(f'(cherry picked from commit {conflict_sha})',
                  conflict['body'])
    self.assertIn('CONFLICTED files:\n```\nconflict.txt\n```', conflict['body'])
    self.assertEqual(manifest['pending'], [])
    self.assertEqual(manifest['delete'], [])
    self.assertEqual(manifest['hands_off'], [])

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
        '--mode',
        'label',
        '--prs-json',
        prs_json,
    ])

    self.assertEqual(exit_code, 0)
    # Output contains only the successful commit 101 (no CONFLICTED files block)
    self.assertEqual(stdout.strip(), '- #101')
    self.assertTrue(os.path.exists('clean.txt'))

    # Verify branch state after SUT finishes
    self.assert_working_tree_clean()
    commit_titles = self.get_branch_commit_titles('27.lts')
    self.assertEqual(len(commit_titles), 1)
    self.assertTrue(commit_titles[0].startswith('Cherry pick PR #101:'))
    with open('conflict.txt', 'r', encoding='utf-8') as f:
      self.assertEqual(f.read(), 'line 1\n27.lts edit\n')
    autoroll_content = self.git('show', 'HEAD:.github/AUTOROLL').stdout.strip()
    self.assertEqual(autoroll_content, base_sha)
    # The conflicting pick is isolated on its own branch.
    conflict_titles = self.get_commit_titles(
        '27.lts', 'autoroll-conflict-main-to-27.lts-102')
    self.assertEqual(len(conflict_titles), 1)
    self.assertTrue(
        conflict_titles[0].startswith('CONFLICTED Cherry pick PR #102:'))

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
        '--mode',
        'label',
        '--prs-json',
        prs_json,
    ])

    self.assertEqual(exit_code, 0)
    self.assertEqual(stdout.strip(), '- #101')
    self.assertTrue(os.path.exists('added_file.txt'))
    self.assertFalse(os.path.exists('to_delete.txt'))

    # Verify branch state after SUT finishes
    self.assert_working_tree_clean()
    commit_titles = self.get_branch_commit_titles('27.lts')
    self.assertEqual(len(commit_titles), 1)
    self.assertTrue(commit_titles[0].startswith('Cherry pick PR #101:'))
    self.assertFalse(commit_titles[0].startswith('CONFLICTED'))
    autoroll_content = self.git('show', 'HEAD:.github/AUTOROLL').stdout.strip()
    self.assertEqual(autoroll_content, base_sha)

  def test_cuj9_legacy_conflicted_clean_branch_is_rebuilt(self):
    """CUJ 9: Label mode rebuilds a clean branch holding a bot CONFLICTED
    commit (and a stray CONFLICTED: cursor on the target does not crash).
    """
    self.setup_conflict_scenario()
    self.git('checkout', 'main')
    self.commit_file('other.txt', 'other\n', 'Other commit (#102)')

    # Legacy state: the clean branch holds the bot's CONFLICTED commit for #101
    # and the target carries a stray CONFLICTED: cursor from a squash merge.
    self.git('checkout', '27.lts')
    with open('.github/AUTOROLL', 'w', encoding='utf-8') as f:
      f.write(f'CONFLICTED:{self.base_sha}\n')
    self.git('add', '.github/AUTOROLL')
    self.git('commit', '-m', 'Squash merge that forgot the cursor')
    self.git('checkout', 'autoroll-main-to-27.lts')
    self.git('reset', '--hard', '27.lts')
    main_sha = self.git('rev-parse', 'main~1').stdout.strip()
    self.git('cherry-pick', '--no-commit', main_sha, check=False)
    self.git('add', 'conflict.txt')
    self.git(
        'commit', '-m', 'CONFLICTED Cherry pick PR #101: Main edit\n\n'
        'Refer to original PR: #101\n\n'
        f'(cherry picked from commit {main_sha})')
    legacy_sha = self.get_head_sha()

    exit_code, stdout, manifest = self.run_label_autoroll({
        101: ['cp-27.lts'],
        102: ['cp-27.lts'],
    })

    self.assertEqual(exit_code, 0)
    self.assertEqual(stdout.strip(), '- #102')
    self.assert_working_tree_clean()
    self.assertTrue(manifest['clean']['rebuilt'])
    self.assertEqual(manifest['clean']['old_sha'], legacy_sha)
    commit_titles = self.get_branch_commit_titles('27.lts')
    self.assertEqual(len(commit_titles), 1)
    self.assertTrue(commit_titles[0].startswith('Cherry pick PR #102:'))
    # The conflict moved to its own branch.
    self.assertEqual([c['pr'] for c in manifest['conflicts']], [101])

  def test_cuj10_human_commits_on_clean_branch_are_left_alone(self):
    """CUJ 10: A clean branch with non-roller commits is never touched."""
    self.init_target_branch('27.lts')
    sha1 = self.commit_file('a.txt', 'content a\n', 'Feature 101 (#101)')
    self.commit_file('b.txt', 'content b\n', 'Feature 102 (#102)')

    self.git('checkout', '27.lts')
    self.git('checkout', '-b', 'autoroll-main-to-27.lts')
    self.commit_cherry_pick(
        'a.txt', 'content a\n', pr_num=101, title='Feature 101', sha=sha1)
    self.commit_file('fix.txt', 'fix\n', 'Fix build on the roll branch')
    head_before = self.get_head_sha()

    # Even with label changes that would otherwise rebuild the branch.
    exit_code, stdout, manifest = self.run_label_autoroll({102: ['cp-27.lts']})

    self.assertEqual(exit_code, 0)
    self.assertEqual(stdout.strip(), '')
    self.assertEqual(self.get_head_sha(), head_before)
    self.assert_working_tree_clean()
    self.assertTrue(manifest['clean']['hands_off'])
    self.assertFalse(manifest['clean']['rebuilt'])
    self.assertNotIn('conflicts', manifest)
    self.assertEqual(manifest['delete'], [])

  def test_cuj10b_full_mode_stops_at_conflict_and_respects_resolutions(self):
    """CUJ 10b: Full mode stops at conflict and respects human resolutions."""
    self.setup_conflict_scenario(target='staging')
    self.git('checkout', 'main')
    self.commit_file('clean.txt', 'clean\n', 'Clean commit (#102)')
    self.git('checkout', 'autoroll-main-to-staging')

    with tempfile.NamedTemporaryFile('w+', encoding='utf-8', delete=False) as f:
      manifest_path = f.name

    try:
      args = [
          '--source-branch', 'main', '--target-branch', 'staging',
          '--autoroll-file', '.github/AUTOROLL', '--max-commits', '10',
          '--mode', 'full', '--manifest', manifest_path
      ]
      # 1. Run 1: Candidate #101 conflicts. Clean branch is stopped,
      # conflict isolated.
      exit_code, stdout, _ = self.run_autoroll(args)
      self.assertEqual(exit_code, 0)
      self.assertEqual(stdout.strip(), '')
      conflict_branch = 'autoroll-conflict-main-to-staging-101'
      conflicted_sha = self.branch_sha(conflict_branch)
      self.assertIsNotNone(conflicted_sha)

      with open(manifest_path, 'r', encoding='utf-8') as f:
        m1 = json.load(f)
      self.assertEqual(m1['clean']['items'], [])
      self.assertEqual([c['pr'] for c in m1['conflicts']], [101])
      self.assertTrue(m1['conflicts'][0]['changed'])

      # 2. Run 2: Unresolved conflict branch still exists. No change.
      exit_code, stdout, _ = self.run_autoroll(args)
      self.assertEqual(exit_code, 0)
      self.assertEqual(self.branch_sha(conflict_branch), conflicted_sha)
      with open(manifest_path, 'r', encoding='utf-8') as f:
        m2 = json.load(f)
      self.assertFalse(m2['conflicts'][0]['changed'])

      # 3. Run 3: Human resolves conflict on the conflict branch.
      self.git('checkout', conflict_branch)
      self.commit_file('conflict.txt', 'line 1\nresolved\n',
                       'RESOLVED Cherry pick PR #101: Main edit')
      resolved_sha = self.get_head_sha()
      self.git('checkout', 'autoroll-main-to-staging')

      exit_code, stdout, _ = self.run_autoroll(args)
      self.assertEqual(exit_code, 0)
      self.assertEqual(self.branch_sha(conflict_branch), resolved_sha)
      with open(manifest_path, 'r', encoding='utf-8') as f:
        m3 = json.load(f)
      self.assertEqual(m3['hands_off'], [{'branch': conflict_branch}])
      self.assertEqual(m3['conflicts'], [])

      # 4. Human squash-merges resolved #101 into staging.
      self.git('checkout', 'staging')
      with open('.github/AUTOROLL', 'w', encoding='utf-8') as f:
        main_sha1 = self.git('rev-parse', 'main~1').stdout.strip()
        f.write(f'{main_sha1}\n')
      self.git('add', '.github/AUTOROLL')
      self.commit_cherry_pick(
          'conflict.txt',
          'line 1\nresolved\n',
          pr_num=101,
          title='Main edit',
          sha=main_sha1)
      self.git('checkout', 'autoroll-main-to-staging')
      self.git('reset', '--hard', 'staging')

      # 5. Run 4: Now #101 is landed, roller resumes rolling #102,
      # and deletes conflict branch.
      exit_code, stdout, _ = self.run_autoroll(args)
      self.assertEqual(exit_code, 0)
      self.assertEqual(stdout.strip(), '- #102')
      with open(manifest_path, 'r', encoding='utf-8') as f:
        m4 = json.load(f)
      self.assertEqual(m4['clean']['items'], ['- #102'])
      self.assertEqual(m4['hands_off'], [{'branch': conflict_branch}])
      self.assertEqual(m4['delete'], [])
    finally:
      if os.path.exists(manifest_path):
        os.unlink(manifest_path)

  def test_cuj11_respects_max_commits(self):
    """CUJ 11: Autoroller stops rolling once max commits limit is reached."""
    self.init_target_branch('staging')

    shas = []
    for i in range(1, 6):
      shas.append(
          self.commit_file(f'f{i}.txt', f'content {i}\n',
                           f'Feature {i} (#{100+i})'))

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
        '--mode',
        'full',
    ])

    self.assertEqual(exit_code, 0)
    self.assertEqual(stdout.strip(), '- #101\n- #102')
    self.assertIn('Reached commit limit (2).', stderr)

    # Verify branch state after SUT finishes
    self.assert_working_tree_clean()
    commit_titles = self.get_branch_commit_titles('staging')
    self.assertEqual(len(commit_titles), 2)
    self.assertTrue(commit_titles[0].startswith('Cherry pick PR #101:'))
    self.assertTrue(commit_titles[1].startswith('Cherry pick PR #102:'))
    self.assertTrue(os.path.exists('f1.txt'))
    self.assertTrue(os.path.exists('f2.txt'))
    self.assertFalse(os.path.exists('f3.txt'))
    self.assertFalse(os.path.exists('f4.txt'))
    self.assertFalse(os.path.exists('f5.txt'))
    autoroll_content = self.git('show', 'HEAD:.github/AUTOROLL').stdout.strip()
    self.assertEqual(autoroll_content, shas[1])

  def test_cuj12_label_mode_requires_prs_json(self):
    """CUJ 12: Exits with code 1 if label mode is invoked without --prs-json."""
    self.init_target_branch('27.lts')
    self.git('checkout', '27.lts')
    self.git('checkout', '-b', 'autoroll-main-to-27.lts')

    head_before = self.get_head_sha()

    exit_code, _, stderr = self.run_autoroll([
        '--source-branch',
        'main',
        '--target-branch',
        '27.lts',
        '--autoroll-file',
        '.github/AUTOROLL',
        '--max-commits',
        '10',
        '--mode',
        'label',
    ])

    self.assertEqual(exit_code, 1)
    self.assertIn('Error: --prs-json is required in label mode.', stderr)

    # Verify branch state after SUT finishes (exited with zero state changes)
    self.assertEqual(self.get_head_sha(), head_before)
    self.assert_working_tree_clean()

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
        '--mode',
        'label',
        '--prs-json',
        prs_json,
    ])

    self.assertEqual(exit_code, 0)
    self.assertEqual(stdout.strip(), '- #101')
    self.assertTrue(os.path.exists('a.txt'))

    # Verify branch state after SUT finishes
    self.assert_working_tree_clean()
    commit_titles = self.get_branch_commit_titles('27.lts')
    self.assertEqual(len(commit_titles), 1)
    self.assertTrue(commit_titles[0].startswith('Cherry pick PR #101:'))
    autoroll_content = self.git('show', 'HEAD:.github/AUTOROLL').stdout.strip()
    self.assertEqual(autoroll_content, self.start_sha)

  def test_cuj14_older_conflict_never_stacks_on_rolled_commits(self):
    """CUJ 14: Older conflicting PR labeled late never lands on clean branch.
    """
    self.setup_conflict_scenario()
    self.git('checkout', 'main')
    sha2 = self.commit_file('b.txt', 'content b\n', 'Newer PR (#102)')

    # The open autoroll PR already rolled the newer PR #102.
    self.git('checkout', 'autoroll-main-to-27.lts')
    rolled_sha = self.commit_cherry_pick(
        'b.txt', 'content b\n', pr_num=102, title='Newer PR', sha=sha2)

    # Now the older PR #101 (which conflicts) gets labeled.
    exit_code, stdout, manifest = self.run_label_autoroll({
        101: ['cp-27.lts'],
        102: ['cp-27.lts'],
    })

    self.assertEqual(exit_code, 0)
    self.assertEqual(stdout.strip(), '- #102')

    # Clean branch untouched: no CONFLICTED commit stacked on top.
    self.assert_working_tree_clean()
    self.assertEqual(self.get_head_sha(), rolled_sha)
    self.assertFalse(manifest['clean']['changed'])
    self.assertFalse(manifest['clean']['rebuilt'])

    self.assert_conflict_branch('autoroll-conflict-main-to-27.lts-101', 101)
    self.assertEqual([c['pr'] for c in manifest['conflicts']], [101])

  def test_cuj15_label_removed_deletes_conflict_branch(self):
    """CUJ 15: Removing the label from a conflicting PR deletes its branch."""
    self.setup_conflict_scenario()
    self.git('checkout', 'main')
    self.commit_file('other.txt', 'other\n', 'Other commit (#102)')
    self.git('checkout', 'autoroll-main-to-27.lts')

    # First run creates the conflict branch.
    exit_code, _, manifest = self.run_label_autoroll({
        101: ['cp-27.lts'],
        102: ['cp-27.lts'],
    })
    self.assertEqual(exit_code, 0)
    branch = 'autoroll-conflict-main-to-27.lts-101'
    old_sha = self.branch_sha(branch)
    self.assertIsNotNone(old_sha)

    # Label removed from #101.
    exit_code, stdout, manifest = self.run_label_autoroll({
        102: ['cp-27.lts'],
    })

    self.assertEqual(exit_code, 0)
    self.assertEqual(stdout.strip(), '- #102')
    self.assert_working_tree_clean()
    self.assertEqual(manifest['conflicts'], [])
    self.assertEqual(manifest['delete'], [{
        'branch': branch,
        'old_sha': old_sha,
    }])
    self.assertEqual(manifest['hands_off'], [])
    # The clean branch kept its commit (no rebuild needed).
    self.assertFalse(manifest['clean']['changed'])
    self.assertFalse(manifest['clean']['rebuilt'])

  def test_cuj16_dependency_labeled_later_fixes_conflict(self):
    """CUJ 16: Labeling the older PR a conflict depends on resolves it."""
    self.init_target_branch('27.lts')
    self.commit_file('conflict.txt', 'line 1\nline 2\n', 'Add file')
    # #101 depends on #100: both edit line 2 in sequence.
    self.git('checkout', '27.lts')
    with open('conflict.txt', 'w', encoding='utf-8') as f:
      f.write('line 1\nline 2\n')
    self.git('add', 'conflict.txt')
    self.git('commit', '-m', 'Add file on 27.lts')
    self.git('checkout', 'main')
    self.commit_file('conflict.txt', 'line 1\nstep 1\n', 'Step 1 (#100)')
    self.commit_file('conflict.txt', 'line 1\nstep 2\n', 'Step 2 (#101)')
    self.git('checkout', '27.lts')
    self.git('checkout', '-b', 'autoroll-main-to-27.lts')
    self.base_sha = self.start_sha

    # First run: only #101 is labeled and conflicts without #100.
    exit_code, stdout, manifest = self.run_label_autoroll({101: ['cp-27.lts']})
    self.assertEqual(exit_code, 0)
    self.assertEqual(stdout.strip(), '')
    branch = 'autoroll-conflict-main-to-27.lts-101'
    old_sha = self.branch_sha(branch)
    self.assert_conflict_branch(branch, 101)
    self.assertEqual(manifest['clean']['items'], [])

    # The dependency #100 gets labeled: both now roll cleanly in order.
    exit_code, stdout, manifest = self.run_label_autoroll({
        100: ['cp-27.lts'],
        101: ['cp-27.lts'],
    })

    self.assertEqual(exit_code, 0)
    self.assertEqual(stdout.strip(), '- #100\n- #101')
    self.assert_working_tree_clean()
    commit_titles = self.get_branch_commit_titles('27.lts')
    self.assertEqual(len(commit_titles), 2)
    self.assertTrue(commit_titles[0].startswith('Cherry pick PR #100:'))
    self.assertTrue(commit_titles[1].startswith('Cherry pick PR #101:'))
    with open('conflict.txt', 'r', encoding='utf-8') as f:
      self.assertEqual(f.read(), 'line 1\nstep 2\n')
    self.assertEqual(manifest['conflicts'], [])
    self.assertEqual([d['branch'] for d in manifest['delete']], [branch])
    self.assertEqual(manifest['delete'][0]['old_sha'], old_sha)

  def test_cuj17_unchanged_conflict_is_a_noop(self):
    """CUJ 17: An unchanged conflict keeps the existing branch SHA."""
    self.setup_conflict_scenario()
    exit_code, _, manifest = self.run_label_autoroll({101: ['cp-27.lts']})
    self.assertEqual(exit_code, 0)
    branch = 'autoroll-conflict-main-to-27.lts-101'
    old_sha = self.branch_sha(branch)
    self.assertTrue(manifest['conflicts'][0]['changed'])

    exit_code, _, manifest = self.run_label_autoroll({101: ['cp-27.lts']})

    self.assertEqual(exit_code, 0)
    self.assert_working_tree_clean()
    self.assertEqual(self.branch_sha(branch), old_sha)
    conflict = manifest['conflicts'][0]
    self.assertFalse(conflict['changed'])
    self.assertEqual(conflict['old_sha'], old_sha)
    self.assertEqual(manifest['delete'], [])

  def test_cuj18_target_moved_rebuilds_conflict_branch(self):
    """CUJ 18: Conflict branch is rebuilt when target changes the conflict."""
    self.setup_conflict_scenario()
    exit_code, _, _ = self.run_label_autoroll({101: ['cp-27.lts']})
    self.assertEqual(exit_code, 0)
    branch = 'autoroll-conflict-main-to-27.lts-101'
    old_sha = self.branch_sha(branch)

    # Target moves in a way that changes the conflicted tree.
    self.git('checkout', '27.lts')
    self.commit_file('conflict.txt', 'line 1\n27.lts edit v2\n', '27.lts v2')
    self.git('checkout', 'autoroll-main-to-27.lts')
    self.git('reset', '--hard', '27.lts')

    exit_code, _, manifest = self.run_label_autoroll({101: ['cp-27.lts']})

    self.assertEqual(exit_code, 0)
    self.assert_working_tree_clean()
    conflict = manifest['conflicts'][0]
    self.assertTrue(conflict['changed'])
    self.assertEqual(conflict['old_sha'], old_sha)
    self.assertNotEqual(self.branch_sha(branch), old_sha)
    self.assert_conflict_branch(branch, 101)
    self.assertIn('27.lts edit v2',
                  self.git('show', f'{branch}:conflict.txt').stdout)

  def test_cuj19_resolved_conflict_branch_is_left_alone(self):
    """CUJ 19: A conflict branch with human commits is never touched."""
    self.setup_conflict_scenario()
    exit_code, _, _ = self.run_label_autoroll({101: ['cp-27.lts']})
    self.assertEqual(exit_code, 0)
    branch = 'autoroll-conflict-main-to-27.lts-101'

    # Human resolves the conflict in a separate commit.
    self.git('checkout', branch)
    self.commit_file('conflict.txt', 'line 1\nresolved\n',
                     'RESOLVED Cherry pick PR #101: Main edit')
    resolved_sha = self.get_head_sha()
    self.git('checkout', 'autoroll-main-to-27.lts')

    # Still conflicting: hands off.
    exit_code, _, manifest = self.run_label_autoroll({101: ['cp-27.lts']})
    self.assertEqual(exit_code, 0)
    self.assert_working_tree_clean()
    self.assertEqual(self.branch_sha(branch), resolved_sha)
    self.assertEqual(manifest['conflicts'], [])
    self.assertEqual(manifest['hands_off'], [{'branch': branch}])
    self.assertEqual(manifest['delete'], [])

    # Label removed: still hands off, not deleted.
    exit_code, _, manifest = self.run_label_autoroll({})
    self.assertEqual(exit_code, 0)
    self.assertEqual(self.branch_sha(branch), resolved_sha)
    self.assertEqual(manifest['hands_off'], [{'branch': branch}])
    self.assertEqual(manifest['delete'], [])

  def test_cuj20_conflict_branch_cap(self):
    """CUJ 20: At most one conflict branch exists; existing keeps its slot."""
    self.setup_conflict_scenario()
    self.git('checkout', 'main')
    for pr in (102, 103, 104):
      self.commit_file('conflict.txt', f'line 1\nmain edit {pr}\n',
                       f'Main edit (#{pr})')
    self.git('checkout', 'autoroll-main-to-27.lts')

    # #103 already has a conflict branch from an earlier run.
    labels = {pr: ['cp-27.lts'] for pr in (101, 102, 103, 104)}
    exit_code, _, manifest = self.run_label_autoroll({103: ['cp-27.lts']})
    self.assertEqual(exit_code, 0)
    self.assertEqual([c['pr'] for c in manifest['conflicts']], [103])

    exit_code, stdout, manifest = self.run_label_autoroll(labels)

    self.assertEqual(exit_code, 0)
    self.assertEqual(stdout.strip(), '')
    self.assert_working_tree_clean()
    self.assertEqual([c['pr'] for c in manifest['conflicts']], [103])
    self.assertEqual([p['pr'] for p in manifest['pending']], [101, 102, 104])
    self.assertIn('waiting for existing conflict',
                  manifest['pending'][0]['reason'])
    self.assertIsNone(self.branch_sha('autoroll-conflict-main-to-27.lts-101'))
    self.assertIsNone(self.branch_sha('autoroll-conflict-main-to-27.lts-102'))
    self.assertIsNone(self.branch_sha('autoroll-conflict-main-to-27.lts-104'))
    self.assertEqual(manifest['delete'], [])

  def test_cuj21_label_removed_from_rolled_pr_rebuilds_clean_branch(self):
    """CUJ 21: Clean branch is rebuilt when a rolled PR loses its label."""
    self.init_target_branch('27.lts')
    sha1 = self.commit_file('a.txt', 'content a\n', 'Feature 101 (#101)')
    sha2 = self.commit_file('b.txt', 'content b\n', 'Feature 102 (#102)')

    self.git('checkout', '27.lts')
    self.git('checkout', '-b', 'autoroll-main-to-27.lts')
    self.commit_cherry_pick(
        'a.txt', 'content a\n', pr_num=101, title='Feature 101', sha=sha1)
    self.commit_cherry_pick(
        'b.txt', 'content b\n', pr_num=102, title='Feature 102', sha=sha2)

    exit_code, stdout, manifest = self.run_label_autoroll({
        102: ['cp-27.lts'],
    })

    self.assertEqual(exit_code, 0)
    self.assertEqual(stdout.strip(), '- #102')
    self.assert_working_tree_clean()
    self.assertTrue(manifest['clean']['rebuilt'])
    self.assertTrue(manifest['clean']['changed'])
    commit_titles = self.get_branch_commit_titles('27.lts')
    self.assertEqual(len(commit_titles), 1)
    self.assertTrue(commit_titles[0].startswith('Cherry pick PR #102:'))
    self.assertFalse(os.path.exists('a.txt'))

    # All labels removed: the branch ends up empty.
    exit_code, stdout, manifest = self.run_label_autoroll({})
    self.assertEqual(exit_code, 0)
    self.assertEqual(stdout.strip(), '')
    self.assertTrue(manifest['clean']['rebuilt'])
    self.assertEqual(manifest['clean']['items'], [])
    self.assertEqual(self.get_head_sha(),
                     self.git('rev-parse', '27.lts').stdout.strip())
    self.assertEqual(manifest['delete'], [{
        'branch': 'autoroll-main-to-27.lts',
        'old_sha': manifest['clean']['old_sha'],
    }])

  def test_cuj22_full_mode_older_conflict_does_not_stack(self):
    """CUJ 22: Full mode never commits CONFLICTED onto a non-empty branch."""
    self.setup_conflict_scenario(target='staging')
    self.git('checkout', 'main')
    sha2 = self.commit_file('b.txt', 'content b\n', 'Newer commit (#102)')

    # The open autoroll PR already holds the newer commit.
    self.git('checkout', 'autoroll-main-to-staging')
    rolled_sha = self.commit_cherry_pick(
        'b.txt', 'content b\n', pr_num=102, title='Newer commit', sha=sha2)

    with tempfile.NamedTemporaryFile('w+', encoding='utf-8', delete=False) as f:
      manifest_path = f.name

    try:
      exit_code, stdout, _ = self.run_autoroll([
          '--source-branch',
          'main',
          '--target-branch',
          'staging',
          '--autoroll-file',
          '.github/AUTOROLL',
          '--max-commits',
          '10',
          '--mode',
          'full',
          '--manifest',
          manifest_path,
      ])

      self.assertEqual(exit_code, 0)
      self.assertEqual(stdout.strip(), '- #102')
      self.assert_working_tree_clean()
      self.assertEqual(self.get_head_sha(), rolled_sha)
      autoroll_content = self.git('show',
                                  'HEAD:.github/AUTOROLL').stdout.strip()
      self.assertEqual(autoroll_content, self.base_sha)

      # Conflict branch holds #101 instead of polluting the clean branch
      branch = 'autoroll-conflict-main-to-staging-101'
      self.assertIsNotNone(self.branch_sha(branch))
      with open(manifest_path, 'r', encoding='utf-8') as f:
        manifest = json.load(f)
      self.assertEqual(manifest['clean']['items'], ['- #102'])
      self.assertEqual([c['pr'] for c in manifest['conflicts']], [101])
    finally:
      if os.path.exists(manifest_path):
        os.unlink(manifest_path)

  def test_cuj23_unrelated_target_move_keeps_conflict_branch(self):
    """CUJ 23: A target change that leaves the conflict intact is a no-op."""
    self.setup_conflict_scenario()
    exit_code, _, _ = self.run_label_autoroll({101: ['cp-27.lts']})
    self.assertEqual(exit_code, 0)
    branch = 'autoroll-conflict-main-to-27.lts-101'
    old_sha = self.branch_sha(branch)

    self.git('checkout', '27.lts')
    self.commit_file('unrelated.txt', 'unrelated\n', 'Unrelated 27.lts change')
    self.git('checkout', 'autoroll-main-to-27.lts')
    self.git('reset', '--hard', '27.lts')

    exit_code, _, manifest = self.run_label_autoroll({101: ['cp-27.lts']})

    self.assertEqual(exit_code, 0)
    self.assert_working_tree_clean()
    self.assertEqual(self.branch_sha(branch), old_sha)
    self.assertFalse(manifest['conflicts'][0]['changed'])

  def test_cuj24_cap_counts_hands_off_branches(self):
    """CUJ 24: Hands-off conflict branch occupies the slot; new ones wait."""
    self.setup_conflict_scenario()
    self.git('checkout', 'main')
    self.commit_file('conflict.txt', 'line 1\nmain edit 2\n',
                     'Main edit 2 (#102)')
    self.git('checkout', 'autoroll-main-to-27.lts')

    exit_code, _, manifest = self.run_label_autoroll({102: ['cp-27.lts']})
    self.assertEqual(exit_code, 0)
    self.assertEqual([c['pr'] for c in manifest['conflicts']], [102])
    branch_102 = 'autoroll-conflict-main-to-27.lts-102'

    # A human starts resolving #102.
    self.git('checkout', branch_102)
    self.commit_file('conflict.txt', 'line 1\nresolved\n',
                     'RESOLVED Cherry pick PR #102: Main edit 2')
    self.git('checkout', 'autoroll-main-to-27.lts')

    # #101 is labeled too, but the slot is taken by the hands-off branch.
    labels = {101: ['cp-27.lts'], 102: ['cp-27.lts']}
    exit_code, _, manifest = self.run_label_autoroll(labels)
    self.assertEqual(exit_code, 0)
    self.assert_working_tree_clean()
    self.assertEqual(manifest['conflicts'], [])
    self.assertEqual(manifest['hands_off'], [{'branch': branch_102}])
    self.assertEqual([p['pr'] for p in manifest['pending']], [101])
    self.assertIn('waiting for existing conflict',
                  manifest['pending'][0]['reason'])
    self.assertIsNone(self.branch_sha('autoroll-conflict-main-to-27.lts-101'))

    # Once #102 lands in target, #101 gets the slot.
    self.git('checkout', '27.lts')
    main_sha2 = self.git('rev-parse', 'main').stdout.strip()
    self.commit_cherry_pick(
        'conflict.txt',
        'line 1\nresolved\n',
        pr_num=102,
        title='Main edit 2',
        sha=main_sha2)
    self.git('checkout', 'autoroll-main-to-27.lts')
    self.git('reset', '--hard', '27.lts')

    exit_code, _, manifest = self.run_label_autoroll({101: ['cp-27.lts']})
    self.assertEqual(exit_code, 0)
    self.assertEqual([c['pr'] for c in manifest['conflicts']], [101])
    self.assertEqual(manifest['pending'], [])
    self.assert_conflict_branch('autoroll-conflict-main-to-27.lts-101', 101)

  def test_cuj25_full_mode_direct_commit_without_pr_conflict(self):
    """CUJ 25: Direct commit without PR gets conflict branch named by SHA."""
    self.commit_file('conflict.txt', 'line 1\nline 2\n',
                     'Add base conflict file')
    base_sha = self.get_head_sha()

    # Target branch modifies line 2
    self.git('checkout', '-b', 'staging', base_sha)
    with open('.github/AUTOROLL', 'w', encoding='utf-8') as f:
      f.write(f'{base_sha}\n')
    self.git('add', '.github/AUTOROLL')
    self.commit_file('conflict.txt', 'line 1\nstaging edit\n', 'Staging edit')

    # Main modifies line 2 with direct commit (no PR number)
    self.git('checkout', 'main')
    sha1 = self.commit_file('conflict.txt', 'line 1\nmain direct edit\n',
                            'Direct commit without PR')

    self.git('checkout', 'staging')
    self.git('checkout', '-b', 'autoroll-main-to-staging')

    with tempfile.NamedTemporaryFile('w+', encoding='utf-8', delete=False) as f:
      manifest_path = f.name

    try:
      exit_code, stdout, _ = self.run_autoroll([
          '--source-branch',
          'main',
          '--target-branch',
          'staging',
          '--autoroll-file',
          '.github/AUTOROLL',
          '--max-commits',
          '10',
          '--mode',
          'full',
          '--manifest',
          manifest_path,
      ])

      self.assertEqual(exit_code, 0)
      self.assertEqual(stdout.strip(), '')
      self.assert_working_tree_clean()

      # Conflict branch is named using short SHA
      branch = f'autoroll-conflict-main-to-staging-{sha1[:10]}'
      self.assertIsNotNone(self.branch_sha(branch))
      titles = self.get_commit_titles('staging', branch)
      self.assertEqual(len(titles), 1)
      self.assertTrue(
          titles[0].startswith(f'CONFLICTED Cherry pick commit {sha1}:'))

      with open(manifest_path, 'r', encoding='utf-8') as f:
        manifest = json.load(f)
      self.assertEqual(len(manifest['conflicts']), 1)
      conflict = manifest['conflicts'][0]
      self.assertEqual(conflict['id'], sha1[:10])
      self.assertIsNone(conflict['pr'])
      self.assertEqual(conflict['branch'], branch)
    finally:
      if os.path.exists(manifest_path):
        os.unlink(manifest_path)


if __name__ == '__main__':
  unittest.main()
