#!/usr/bin/env python3
"""Tests for merge_autoroll_lts.py."""

import json
import os
import sys
import unittest
from unittest.mock import MagicMock, patch

# Add the current directory to sys.path to import merge_autoroll_lts
sys.path.append(os.path.dirname(__file__))
# pylint: disable=wrong-import-position
import merge_autoroll_lts
# pylint: enable=wrong-import-position


class TestMergeAutorollLts(unittest.TestCase):
  """Test cases for merge_autoroll_lts main execution flows."""

  # pylint: disable=unused-argument

  @patch('subprocess.run')
  @patch('subprocess.check_output')
  @patch('merge_autoroll_lts.get_github_token', return_value='fake_token')
  @patch('sys.argv', ['merge_autoroll_lts.py', '27.lts'])
  def test_main_success(self, mock_get_token, mock_check_output, mock_run):
    # Mock gh pr list
    mock_check_output.return_value = json.dumps([{
        'number': 1,
        'title': 'Autoroll from main to 27.lts',
        'headRefName': 'autoroll-main-to-27.lts',
        'baseRefName': '27.lts',
    }])

    # Mock git operations
    mock_run.return_value = MagicMock(returncode=0)

    with patch('sys.stdout'), patch('sys.stderr'):
      merge_autoroll_lts.main()

    # Verify gh pr list call
    mock_check_output.assert_called_once_with([
        'gh', 'pr', 'list', '--repo', merge_autoroll_lts.REPO_OWNER_PATH,
        '--state', 'open', '--json', 'number,headRefName,baseRefName,title'
    ],
                                              env=unittest.mock.ANY,
                                              text=True)

    # Verify key subprocess calls
    mock_run.assert_any_call([
        'git', 'fetch', merge_autoroll_lts.REPO_URL,
        '+27.lts:refs/remotes/origin/27.lts',
        '+autoroll-main-to-27.lts:refs/remotes/origin/autoroll-main-to-27.lts'
    ],
                             check=True)
    mock_run.assert_any_call([
        'git', 'worktree', 'add', '--no-checkout', unittest.mock.ANY,
        'origin/autoroll-main-to-27.lts'
    ],
                             check=True)
    mock_run.assert_any_call(
        ['git', '-C', unittest.mock.ANY, 'sparse-checkout', 'init', '--cone'],
        check=True)
    mock_run.assert_any_call(
        ['git', '-C', unittest.mock.ANY, 'sparse-checkout', 'set', '.github'],
        check=True)
    mock_run.assert_any_call(
        ['git', '-C', unittest.mock.ANY, 'rebase', 'origin/27.lts'],
        check=False)
    mock_run.assert_any_call([
        'git', '-C', unittest.mock.ANY, '-c', 'credential.helper=', '-c',
        'credential.helper=!gh auth git-credential', 'push',
        merge_autoroll_lts.REPO_URL, 'HEAD:27.lts'
    ],
                             env=unittest.mock.ANY,
                             check=True)
    mock_run.assert_any_call(
        ['git', 'worktree', 'remove', '--force', unittest.mock.ANY],
        check=False)

    close_comment = (
        'Closing this PR because the changes have been rebased and pushed '
        'directly to 27.lts.')
    mock_run.assert_any_call([
        'gh', 'pr', 'close', '1', '--repo', merge_autoroll_lts.REPO_OWNER_PATH,
        '--comment', close_comment, '--delete-branch'
    ],
                             env=unittest.mock.ANY,
                             check=True)

  @patch('subprocess.run')
  @patch('subprocess.check_output')
  @patch('merge_autoroll_lts.get_github_token', return_value='fake_token')
  @patch('sys.argv', ['merge_autoroll_lts.py', '27.lts'])
  def test_main_no_pr_found(self, mock_get_token, mock_check_output, mock_run):
    mock_check_output.return_value = json.dumps([])
    with patch('sys.stdout'), patch('sys.stderr'):
      with self.assertRaises(SystemExit) as cm:
        merge_autoroll_lts.main()
    self.assertEqual(cm.exception.code, 1)
    mock_run.assert_not_called()

  @patch('subprocess.run')
  @patch('subprocess.check_output')
  @patch('merge_autoroll_lts.get_github_token', return_value='fake_token')
  @patch('sys.argv', ['merge_autoroll_lts.py', '27.lts'])
  def test_main_rebase_conflict(self, mock_get_token, mock_check_output,
                                mock_run):
    mock_check_output.return_value = json.dumps([{
        'number': 1,
        'title': 'Autoroll from main to 27.lts',
        'headRefName': 'autoroll-main-to-27.lts',
        'baseRefName': '27.lts',
    }])

    # Mock git operations
    def run_side_effect(cmd, *args, **kwargs):
      res = MagicMock()
      res.returncode = 0
      if 'rebase' in cmd:
        res.returncode = 1
      return res

    mock_run.side_effect = run_side_effect

    with patch('sys.stdout'), patch('sys.stderr'):
      with self.assertRaises(SystemExit) as cm:
        merge_autoroll_lts.main()
    self.assertEqual(cm.exception.code, 1)
    # Check that worktree cleanup was still called
    mock_run.assert_any_call(
        ['git', 'worktree', 'remove', '--force', unittest.mock.ANY],
        check=False)

  @patch('subprocess.run')
  @patch('subprocess.check_output')
  @patch('merge_autoroll_lts.get_github_token', return_value='fake_token')
  @patch('sys.argv', ['merge_autoroll_lts.py', '27.lts'])
  def test_main_conflicted_pr(self, mock_get_token, mock_check_output,
                              mock_run):
    # Mock gh pr list returning a conflicted PR
    mock_check_output.return_value = json.dumps([{
        'number': 1,
        'title': 'CONFLICTED: Autoroll from main to 27.lts',
        'headRefName': 'autoroll-main-to-27.lts',
        'baseRefName': '27.lts',
    }])

    with patch('sys.stdout'), patch('sys.stderr'):
      with self.assertRaises(SystemExit) as cm:
        merge_autoroll_lts.main()

    self.assertEqual(cm.exception.code, 1)
    mock_run.assert_not_called()

  # pylint: enable=unused-argument


class TestGetGithubToken(unittest.TestCase):
  """Test cases for get_github_token authorization function."""

  # pylint: disable=unused-argument

  @patch.dict('os.environ', {'GITHUB_TOKEN': 'env_token'})
  def test_token_from_env(self):
    token = merge_autoroll_lts.get_github_token()
    self.assertEqual(token, 'env_token')

  @patch('builtins.input', side_effect=['12345', '/tmp/fake_key.pem'])
  @patch('os.path.exists', return_value=True)
  @patch(
      'merge_autoroll_lts.get_installation_access_token',
      return_value='app_token')
  @patch.dict('os.environ', {}, clear=True)
  def test_token_from_app_inputs(self, mock_get_app_token, mock_path_exists,
                                 mock_input):
    with patch('sys.stdout'), patch('sys.stderr'):
      token = merge_autoroll_lts.get_github_token()
    self.assertEqual(token, 'app_token')
    mock_get_app_token.assert_called_once_with('12345', '/tmp/fake_key.pem')

  # pylint: enable=unused-argument


if __name__ == '__main__':
  unittest.main()
