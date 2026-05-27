"""Tests for autoroll_lts.py."""

import os
import subprocess
import sys
import unittest
from unittest.mock import MagicMock, patch

# Add the current directory to sys.path to import autoroll_lts
sys.path.append(os.path.dirname(__file__))
# pylint: disable=wrong-import-position
import autoroll_lts
# pylint: enable=wrong-import-position


class TestAutorollLts(unittest.TestCase):
  """Test cases for autoroll_lts."""

  @patch('subprocess.run')
  def test_get_unmerged_files(self, mock_run):
    """Test get_unmerged_files with mock output."""
    # Simulate output of 'git ls-files -u'
    # Format: <mode> <object> <stage>\t<file>
    mock_output = (
        '100644 1234567890abcdef1234567890abcdef12345678 2\tfile1.txt\n'
        '100644 abcdef1234567890abcdef1234567890abcdef 3\tfile1.txt\n')
    mock_run.return_value = MagicMock(stdout=mock_output, returncode=0)

    unmerged = autoroll_lts.get_unmerged_files()
    self.assertEqual(unmerged, {'file1.txt': {'ours', 'theirs'}})

  @patch('subprocess.run')
  def test_get_unmerged_files_with_spaces(self, mock_run):
    """Test get_unmerged_files with filenames containing spaces."""
    mock_output = ('100644 abcdef 2\tfile with spaces.txt\n'
                   '100644 abcdef 3\tfile with spaces.txt\n')
    mock_run.return_value = MagicMock(stdout=mock_output, returncode=0)

    unmerged = autoroll_lts.get_unmerged_files()
    self.assertEqual(unmerged, {'file with spaces.txt': {'ours', 'theirs'}})

  @patch('subprocess.run')
  def test_get_unmerged_files_with_leading_spaces(self, mock_run):
    """Test get_unmerged_files with filenames containing leading spaces."""
    mock_output = ('100644 abcdef 2\t  file with leading spaces.txt\n'
                   '100644 abcdef 3\t  file with leading spaces.txt\n')
    mock_run.return_value = MagicMock(stdout=mock_output, returncode=0)

    unmerged = autoroll_lts.get_unmerged_files()
    self.assertEqual(unmerged,
                     {'  file with leading spaces.txt': {'ours', 'theirs'}})

  @patch('subprocess.run')
  def test_get_unmerged_files_with_tabs(self, mock_run):
    """Test get_unmerged_files with filenames containing tabs."""
    mock_output = ('100644 abcdef 2\tfile_with\ttab.txt\n'
                   '100644 abcdef 3\tfile_with\ttab.txt\n')
    mock_run.return_value = MagicMock(stdout=mock_output, returncode=0)

    unmerged = autoroll_lts.get_unmerged_files()
    self.assertEqual(unmerged, {'file_with\ttab.txt': {'ours', 'theirs'}})

  @patch('subprocess.run')
  def test_get_unmerged_files_empty(self, mock_run):
    """Test get_unmerged_files with empty output."""
    mock_run.return_value = MagicMock(stdout='', returncode=0)
    unmerged = autoroll_lts.get_unmerged_files()
    self.assertEqual(unmerged, {})

  @patch('subprocess.run')
  def test_resolve_conflicts_deleted_by_us(self, mock_run):
    """Test resolve_conflicts with 'deleted by us' conflict."""
    unmerged = {'file1.txt': {'theirs'}}
    mock_run.return_value = MagicMock(returncode=0)

    with patch('builtins.print') as mock_print:
      resolved = autoroll_lts.resolve_conflicts(unmerged)

    self.assertTrue(resolved)
    mock_run.assert_called_with(['git', 'rm', '--', 'file1.txt'],
                                check=True,
                                stdout=sys.stderr)
    mock_print.assert_called_once()
    self.assertEqual(mock_print.call_args[1]['file'], sys.stderr)

  @patch('subprocess.run')
  def test_resolve_conflicts_other(self, mock_run):
    """Test resolve_conflicts with other conflicts."""
    unmerged = {'file1.txt': {'ours', 'theirs'}}

    with patch('sys.stderr'):
      resolved = autoroll_lts.resolve_conflicts(unmerged)

    self.assertFalse(resolved)
    mock_run.assert_not_called()

  @patch('subprocess.run')
  def test_cherry_pick_conflict_deleted_by_us(self, mock_run):
    """Test cherry_pick handles 'deleted by us' conflicts."""

    # pylint: disable=unused-argument
    def side_effect(cmd, *args, **kwargs):
      if 'log' in cmd:
        return MagicMock(stdout='date\x00author\x00body', returncode=0)
      if 'show' in cmd and '-s' in cmd:
        return MagicMock(stdout='parent1', returncode=0)
      if 'cherry-pick' in cmd and '--no-commit' in cmd:
        raise subprocess.CalledProcessError(1, cmd)
      if 'ls-files' in cmd:
        return MagicMock(
            stdout=('100644 1234567890abcdef1234567890abcdef12345678 '
                    '3\tfile1.txt\n'),
            returncode=0,
        )
      if 'rm' in cmd:
        return MagicMock(returncode=0)
      if 'diff' in cmd:
        return MagicMock(returncode=1)  # Has changes
      if 'commit' in cmd:
        return MagicMock(returncode=0)
      return MagicMock(returncode=0)

    mock_run.side_effect = side_effect

    with patch('sys.stderr'):
      result = autoroll_lts.cherry_pick('sha', '123', 'title')

    self.assertTrue(result)

  @patch('subprocess.run')
  def test_cherry_pick_raises_on_conflict_failure(self, mock_run):
    """Test cherry_pick raises CalledProcessError on conflict failure."""

    def side_effect(cmd, *args, **kwargs):
      # pylint: disable=unused-argument
      if 'cherry-pick' in cmd:
        raise subprocess.CalledProcessError(1, cmd)
      if 'ls-files' in cmd:
        return MagicMock(
            stdout='100644 abcdef 2\tfile1.txt\n100644 abcdef 3\tfile1.txt\n',
            returncode=0)
      return MagicMock(returncode=0)

    mock_run.side_effect = side_effect

    with patch('sys.stderr'):
      with self.assertRaises(subprocess.CalledProcessError):
        autoroll_lts.cherry_pick('sha', '123', 'title')


class TestAutorollLtsMain(unittest.TestCase):
  """Test cases for main() function argument parsing and defaults."""

  @patch('autoroll_lts.get_pr_set')
  @patch('autoroll_lts.get_commits')
  @patch('sys.argv', ['autoroll_lts.py', '--target-branch', '27.lts'])
  def test_main_defaults(self, mock_get_commits, mock_get_pr_set):
    """Test main() with default arguments."""
    mock_get_pr_set.return_value = set()
    mock_get_commits.return_value = []

    with patch('sys.stderr'):
      autoroll_lts.main()

    mock_get_commits.assert_called_once_with(
        'main', '27.lts', '2079b05a9fee4de18abd188fa4a6aceb01a77d7e')

  @patch('autoroll_lts.get_pr_set')
  @patch('autoroll_lts.get_commits')
  @patch('sys.argv',
         ['autoroll_lts.py', '--target-branch', '27.lts', '--start-commit', ''])
  def test_main_empty_start_commit(self, mock_get_commits, mock_get_pr_set):
    """Test main() with empty string start commit."""
    mock_get_pr_set.return_value = set()
    mock_get_commits.return_value = []

    with patch('sys.stderr'):
      autoroll_lts.main()

    mock_get_commits.assert_called_once_with('main', '27.lts', '')

  @patch('autoroll_lts.get_pr_set')
  @patch('autoroll_lts.get_commits')
  @patch('sys.argv', [
      'autoroll_lts.py', '--target-branch', '27.lts', '--start-commit',
      'custom_commit'
  ])
  def test_main_custom_start_commit(self, mock_get_commits, mock_get_pr_set):
    """Test main() with custom start commit."""
    mock_get_pr_set.return_value = set()
    mock_get_commits.return_value = []

    with patch('sys.stderr'):
      autoroll_lts.main()

    mock_get_commits.assert_called_once_with('main', '27.lts', 'custom_commit')


class TestAutorollLtsGetPrSetAndCommits(unittest.TestCase):
  """Test cases for get_pr_set and get_commits."""

  def test_get_pr_set(self):
    subjects = [
        'Cherry pick PR #101: First cherry-pick',
        'Revert "Cherry pick PR #101: First cherry-pick"',
        'Cherry pick PR #102: Second cherry-pick',
        'Some random commit that is not a cherry pick',
        'Revert Cherry pick PR #102: Second cherry-pick',
        'Cherry pick PR #103: Third cherry-pick',
        'Cherry pick PR #104: Fourth cherry-pick',
        'Revert "Cherry pick PR #104: Fourth cherry-pick"',
        'Cherry pick PR #105: Fifth cherry-pick',
        "Revert 'Cherry pick PR #105: Fifth cherry-pick'",
    ]
    with patch('autoroll_lts.get_out') as mock_get_out:
      mock_get_out.return_value = '\n'.join(subjects) + '\n'
      prs = autoroll_lts.get_pr_set('target-branch', 'main')
      self.assertEqual(prs, {'103'})

  @patch('autoroll_lts.get_out')
  def test_get_commits_with_start(self, mock_get_out):
    mock_get_out.return_value = 'sha1 Commit 1\nsha2 Commit 2\n'
    commits = autoroll_lts.get_commits('main', '27.lts', 'start_sha')
    mock_get_out.assert_called_once_with([
        'git', 'rev-list', '--oneline', '--reverse', 'main', '^27.lts',
        'start_sha^..main'
    ])
    self.assertEqual(commits, ['sha1 Commit 1', 'sha2 Commit 2'])

  @patch('autoroll_lts.get_out')
  def test_get_commits_without_start(self, mock_get_out):
    mock_get_out.return_value = 'sha1 Commit 1\n'
    commits = autoroll_lts.get_commits('main', '27.lts', '')
    mock_get_out.assert_called_once_with(
        ['git', 'rev-list', '--oneline', '--reverse', 'main', '^27.lts'])
    self.assertEqual(commits, ['sha1 Commit 1'])


class TestAutorollLtsMainLoop(unittest.TestCase):
  """Test cases for main loop processing."""

  @patch('autoroll_lts.get_pr_set')
  @patch('autoroll_lts.get_commits')
  @patch('autoroll_lts.cherry_pick')
  @patch('sys.argv',
         ['autoroll_lts.py', '--target-branch', '27.lts', '--max-commits', '2'])
  def test_main_loop_processing(self, mock_cherry_pick, mock_get_commits,
                                mock_get_pr_set):
    """Test cases for main loop processing.

    Note: This test passes --max-commits 2. Commits that are skipped (already
    on branch or in skip list) do not increment the commits_added counter.
    Thus, the loop continues until 2 NEW commits are cherry-picked.
    PR #300 is already on the branch, so it is added to links but not counted.
    PR #9476 is in the skip list, so it is skipped.
    """
    mock_get_pr_set.side_effect = [
        {'100', '200'},
        {'100', '300'},
    ]
    mock_get_commits.return_value = [
        'sha1 PR title (#300)',
        'sha2 PR title (#200)',
        '7e6524981fdd6 Reordered build status (#9476)',
        'sha4 Valid new PR (#400)',
        'sha5 Another new PR (#500)',
        'sha6 One more PR (#600)',
    ]
    mock_cherry_pick.return_value = True
    with patch('sys.stderr'), patch('builtins.print') as mock_print:
      autoroll_lts.main()
      mock_cherry_pick.assert_called_once_with('sha4', '400', 'Valid new PR')
      printed_calls = mock_print.call_args_list
      stdout_calls = [call for call in printed_calls if 'file' not in call[1]]
      self.assertEqual(len(stdout_calls), 1)
      self.assertEqual(stdout_calls[0][0][0], '- #300\n- #400')


if __name__ == '__main__':
  unittest.main()
