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
"""Tests for merge_autoroll_lts.py."""

import json
import os
import subprocess
import sys
import unittest
import urllib.error
from unittest.mock import MagicMock, patch

# Add the current directory to sys.path to import merge_autoroll_lts
sys.path.append(os.path.dirname(__file__))
# pylint: disable=wrong-import-position
import merge_autoroll_lts
# pylint: enable=wrong-import-position


class TestMergeAutorollLts(unittest.TestCase):
  """Test cases for merge_autoroll_lts main execution flows."""

  def setUp(self):
    # Mock get_installation_access_token to return fake token
    self.token_patcher = patch(
        'merge_autoroll_lts.get_installation_access_token',
        return_value='fake_token')
    self.mock_get_token = self.token_patcher.start()

    # Mock exists to return True for our fake key file
    orig_exists = os.path.exists
    self.exists_patcher = patch(
        'os.path.exists',
        side_effect=lambda path: True
        if 'fake_key.pem' in path else orig_exists(path))
    self.mock_exists = self.exists_patcher.start()

    # Mock sys.argv for all tests to use argparse arguments
    self.argv_patcher = patch(
        'sys.argv',
        [
            'merge_autoroll_lts.py',
            '--source-branch',
            'main',
            '--target-branch',
            '27.lts',
            '--key-file',
            '/tmp/fake_key.pem',
        ],
    )
    self.mock_argv = self.argv_patcher.start()

  def tearDown(self):
    self.token_patcher.stop()
    self.exists_patcher.stop()
    self.argv_patcher.stop()

  # pylint: disable=unused-argument

  def _setup_mock_run(self,
                      mock_run,
                      *,
                      prs_list=None,
                      rebase_fail=False,
                      close_fail=None,
                      list_fail=False):
    if prs_list is None:
      prs_list = [{
          'number': 1,
          'title': 'Autoroll from main to 27.lts',
          'headRefName': 'autoroll-main-to-27.lts',
          'baseRefName': '27.lts',
      }]

    def run_side_effect(args, **kwargs):
      cmd = args[0]
      if cmd == 'gh':
        subcmd = args[1]
        if subcmd == 'pr':
          pr_action = args[2]
          if pr_action == 'list':
            if list_fail:
              raise subprocess.CalledProcessError(
                  returncode=1, cmd='gh pr list')
            mock_res = MagicMock()
            mock_res.returncode = 0
            mock_res.stdout = json.dumps(prs_list)
            return mock_res
          elif pr_action == 'close':
            if close_fail == 'warning':
              raise subprocess.CalledProcessError(
                  returncode=1,
                  cmd=args,
                  output='✓ Closed pull request\n',
                  stderr='failed to delete remote branch: HTTP 404')
            elif close_fail == 'error':
              raise subprocess.CalledProcessError(
                  returncode=1,
                  cmd=args,
                  output='',
                  stderr='HTTP 403: Forbidden')
            mock_res = MagicMock()
            mock_res.returncode = 0
            mock_res.stdout = '✓ Closed pull request'
            return mock_res
      elif cmd == 'git':
        git_subcmd = args[1]
        if git_subcmd == 'rebase' and rebase_fail:
          mock_res = MagicMock()
          mock_res.returncode = 1
          return mock_res

      mock_res = MagicMock()
      mock_res.returncode = 0
      return mock_res

    mock_run.side_effect = run_side_effect

  @patch('subprocess.run')
  def test_main_success(self, mock_run):
    self._setup_mock_run(mock_run)

    with patch('sys.stdout'), patch('sys.stderr'):
      merge_autoroll_lts.main()

    # Verify gh pr list call
    mock_run.assert_any_call([
        'gh', 'pr', 'list', '--repo', merge_autoroll_lts.REPO_OWNER_PATH,
        '--state', 'open', '--head', 'autoroll-main-to-27.lts', '--json',
        'number,headRefName,baseRefName,title'
    ],
                             capture_output=True,
                             text=True,
                             check=True,
                             env=unittest.mock.ANY)

    # Verify key subprocess calls
    mock_run.assert_any_call([
        'git', '-c', 'credential.helper=', '-c',
        'credential.helper=!gh auth git-credential', 'fetch',
        merge_autoroll_lts.REPO_URL, '+27.lts:refs/remotes/origin/27.lts',
        '+autoroll-main-to-27.lts:refs/remotes/origin/autoroll-main-to-27.lts'
    ],
                             check=True,
                             env=unittest.mock.ANY)
    mock_run.assert_any_call([
        'git', 'worktree', 'add', '--no-checkout', unittest.mock.ANY,
        'origin/autoroll-main-to-27.lts'
    ],
                             check=True)
    mock_run.assert_any_call(['git', 'sparse-checkout', 'init', '--cone'],
                             check=True)
    mock_run.assert_any_call(['git', 'sparse-checkout', 'set', '.github'],
                             check=True)
    mock_run.assert_any_call(['git', 'checkout'], check=True)
    mock_run.assert_any_call(['git', 'rebase', 'origin/27.lts'], check=False)
    mock_run.assert_any_call([
        'git', '-c', 'credential.helper=', '-c',
        'credential.helper=!gh auth git-credential', 'push',
        merge_autoroll_lts.REPO_URL, 'HEAD:27.lts'
    ],
                             check=True,
                             env=unittest.mock.ANY)
    mock_run.assert_any_call(
        ['git', 'worktree', 'remove', '--force', unittest.mock.ANY],
        check=False)

    self.mock_get_token.assert_called_once_with('3203510', '/tmp/fake_key.pem')

  @patch('subprocess.run')
  def test_main_no_pr_found(self, mock_run):
    self._setup_mock_run(mock_run, prs_list=[])
    with patch('sys.stdout'), patch('sys.stderr'):
      with self.assertRaises(SystemExit) as cm:
        merge_autoroll_lts.main()
    self.assertEqual(cm.exception.code, 0)
    mock_run.assert_called_once_with([
        'gh', 'pr', 'list', '--repo', merge_autoroll_lts.REPO_OWNER_PATH,
        '--state', 'open', '--head', 'autoroll-main-to-27.lts', '--json',
        'number,headRefName,baseRefName,title'
    ],
                                     capture_output=True,
                                     text=True,
                                     check=True,
                                     env=unittest.mock.ANY)

  @patch('subprocess.run')
  def test_main_conflicted_pr(self, mock_run):
    # Mock gh pr list returning a conflicted PR
    self._setup_mock_run(
        mock_run,
        prs_list=[{
            'number': 1,
            'title': 'CONFLICTED: Autoroll from main to 27.lts',
            'headRefName': 'autoroll-main-to-27.lts',
            'baseRefName': '27.lts',
        }])

    with patch('sys.stdout'), patch('sys.stderr'):
      with self.assertRaises(SystemExit) as cm:
        merge_autoroll_lts.main()

    self.assertEqual(cm.exception.code, 1)
    mock_run.assert_called_once_with([
        'gh', 'pr', 'list', '--repo', merge_autoroll_lts.REPO_OWNER_PATH,
        '--state', 'open', '--head', 'autoroll-main-to-27.lts', '--json',
        'number,headRefName,baseRefName,title'
    ],
                                     capture_output=True,
                                     text=True,
                                     check=True,
                                     env=unittest.mock.ANY)

  @patch('subprocess.run')
  def test_main_list_prs_fails(self, mock_run):
    self._setup_mock_run(mock_run, list_fail=True)

    with patch('sys.stdout'), patch('sys.stderr'):
      with self.assertRaises(subprocess.CalledProcessError):
        merge_autoroll_lts.main()
    mock_run.assert_called_once_with([
        'gh', 'pr', 'list', '--repo', merge_autoroll_lts.REPO_OWNER_PATH,
        '--state', 'open', '--head', 'autoroll-main-to-27.lts', '--json',
        'number,headRefName,baseRefName,title'
    ],
                                     capture_output=True,
                                     text=True,
                                     check=True,
                                     env=unittest.mock.ANY)

  # pylint: enable=unused-argument


class TestAppCredentialExchange(unittest.TestCase):
  """Test cases for generating JWT and fetching App access tokens."""

  # pylint: disable=unused-argument

  @patch('merge_autoroll_lts.openssl')
  def test_generate_jwt_success(self, mock_openssl):
    mock_openssl.return_value = b'fake_signature'

    with patch('time.time', return_value=100000):
      jwt = merge_autoroll_lts.generate_jwt('12345', '/tmp/fake.pem')

    self.assertTrue(jwt.startswith('eyJhbGciOiAiUlMyNTYiLCAidHlwIjogIkpXVCJ9.'))
    mock_openssl.assert_called_once_with(
        'dgst',
        '-sha256',
        '-sign',
        '/tmp/fake.pem',
        stdin=unittest.mock.ANY,
        stdout=subprocess.PIPE)

  @patch('merge_autoroll_lts.openssl')
  def test_generate_jwt_openssl_not_found(self, mock_openssl):
    mock_openssl.side_effect = FileNotFoundError('openssl not found')

    with patch('sys.stdout'), patch('sys.stderr'):
      with self.assertRaises(FileNotFoundError):
        merge_autoroll_lts.generate_jwt('12345', '/tmp/fake.pem')

  @patch('merge_autoroll_lts.openssl')
  def test_generate_jwt_openssl_error(self, mock_openssl):
    mock_openssl.side_effect = subprocess.CalledProcessError(
        returncode=1, cmd='openssl')

    with patch('sys.stdout'), patch('sys.stderr'):
      with self.assertRaises(subprocess.CalledProcessError):
        merge_autoroll_lts.generate_jwt('12345', '/tmp/fake.pem')

  @patch('urllib.request.urlopen')
  @patch('merge_autoroll_lts.generate_jwt', return_value='fake_jwt')
  def test_get_installation_access_token_success(self, mock_generate_jwt,
                                                 mock_urlopen):
    mock_res_tok = MagicMock()
    mock_res_tok.__enter__.return_value = mock_res_tok
    mock_res_tok.read.return_value = b'{"token": "app_installed_token"}'

    mock_urlopen.return_value = mock_res_tok

    with patch('sys.stdout'), patch('sys.stderr'):
      token = merge_autoroll_lts.get_installation_access_token(
          '12345', '/tmp/fake.pem')

    self.assertEqual(token, 'app_installed_token')
    mock_generate_jwt.assert_called_once_with('12345', '/tmp/fake.pem')

  @patch('urllib.request.urlopen')
  @patch('merge_autoroll_lts.generate_jwt', return_value='fake_jwt')
  def test_get_installation_access_token_token_error(self, mock_generate_jwt,
                                                     mock_urlopen):
    mock_urlopen.side_effect = urllib.error.HTTPError(
        'https://api.github.com/app/installations/119484904/access_tokens', 400,
        'Bad Request', None, None)

    with patch('sys.stdout'), patch('sys.stderr'):
      with self.assertRaises(urllib.error.HTTPError):
        merge_autoroll_lts.get_installation_access_token(
            '12345', '/tmp/fake.pem')

  # pylint: enable=unused-argument


class TestWrappers(unittest.TestCase):
  """Test cases for wrapper functions."""

  @patch('subprocess.run')
  def test_git_wrapper_default_check(self, mock_run):
    merge_autoroll_lts.git('status')
    mock_run.assert_called_once_with(['git', 'status'], check=True)

  @patch('subprocess.run')
  def test_git_wrapper_custom_check(self, mock_run):
    merge_autoroll_lts.git('status', check=False)
    mock_run.assert_called_once_with(['git', 'status'], check=False)

  @patch('subprocess.run')
  def test_gh_wrapper_default_check(self, mock_run):
    merge_autoroll_lts.gh('auth', 'status')
    mock_run.assert_called_once_with(['gh', 'auth', 'status'], check=True)

  @patch('subprocess.run')
  def test_gh_wrapper_custom_check(self, mock_run):
    merge_autoroll_lts.gh('auth', 'status', check=False)
    mock_run.assert_called_once_with(['gh', 'auth', 'status'], check=False)

  @patch('subprocess.run')
  def test_openssl_wrapper_default_check(self, mock_run):
    merge_autoroll_lts.openssl('status')
    mock_run.assert_called_once_with(['openssl', 'status'],
                                     input=None,
                                     check=True)

  @patch('subprocess.run')
  def test_openssl_wrapper_custom_stdin_and_check(self, mock_run):
    merge_autoroll_lts.openssl('status', stdin=b'data', check=False)
    mock_run.assert_called_once_with(['openssl', 'status'],
                                     input=b'data',
                                     check=False)


if __name__ == '__main__':
  unittest.main()
