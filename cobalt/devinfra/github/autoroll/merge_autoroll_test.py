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
"""Tests for merge_autoroll.py."""

import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
import urllib.error
from unittest.mock import MagicMock, patch

# Add the current directory to sys.path to import merge_autoroll
sys.path.append(os.path.dirname(__file__))
# pylint: disable=wrong-import-position
import merge_autoroll
# pylint: enable=wrong-import-position


class TestMergeAutoroll(unittest.TestCase):
  """Test cases for merge_autoroll main execution flows."""

  def setUp(self):
    # Mock get_installation_access_token to return fake token
    self.token_patcher = patch(
        'merge_autoroll.get_installation_access_token',
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
            'merge_autoroll.py',
            '--source-branch',
            'main',
            '--target-branch',
            'target',
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
          'title': 'Autoroll from main to target',
          'headRefName': 'autoroll-main-to-target',
          'baseRefName': 'target',
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
      merge_autoroll.main()

    # Verify gh pr list call
    mock_run.assert_any_call([
        'gh', 'pr', 'list', '--repo', merge_autoroll.REPO_OWNER_PATH,
        '--state', 'open', '--head', 'autoroll-main-to-target', '--json',
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
        merge_autoroll.REPO_URL, '+target:refs/remotes/origin/target',
        '+autoroll-main-to-target:refs/remotes/origin/autoroll-main-to-target'
    ],
                             check=True,
                             env=unittest.mock.ANY)
    mock_run.assert_any_call([
        'git', 'worktree', 'add', '--no-checkout', unittest.mock.ANY,
        'origin/autoroll-main-to-target'
    ],
                             check=True)
    mock_run.assert_any_call(['git', 'sparse-checkout', 'init', '--cone'],
                             check=True)
    mock_run.assert_any_call(['git', 'sparse-checkout', 'set', '.github'],
                             check=True)
    mock_run.assert_any_call(['git', 'checkout'], check=True)
    mock_run.assert_any_call(['git', 'rebase', 'origin/target'], check=True)
    mock_run.assert_any_call([
        'git', '-c', 'credential.helper=', '-c',
        'credential.helper=!gh auth git-credential', 'push',
        merge_autoroll.REPO_URL, 'HEAD:target'
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
        merge_autoroll.main()
    self.assertEqual(cm.exception.code, 0)
    mock_run.assert_called_once_with([
        'gh', 'pr', 'list', '--repo', merge_autoroll.REPO_OWNER_PATH,
        '--state', 'open', '--head', 'autoroll-main-to-target', '--json',
        'number,headRefName,baseRefName,title'
    ],
                                     capture_output=True,
                                     text=True,
                                     check=True,
                                     env=unittest.mock.ANY)

  @patch('subprocess.run')
  def test_main_conflicted_pr(self, mock_run):
    # Mock gh pr list returning a conflicted PR (using the actual format from workflow)
    self._setup_mock_run(
        mock_run,
        prs_list=[{
            'number': 1,
            'title': 'CONFLICTED Cherry pick PR #123: Some commit',
            'headRefName': 'autoroll-main-to-target',
            'baseRefName': 'target',
        }])

    with patch('sys.stdout'), patch('sys.stderr'):
      with self.assertRaises(SystemExit) as cm:
        merge_autoroll.main()

    self.assertEqual(cm.exception.code, 1)
    mock_run.assert_called_once_with([
        'gh', 'pr', 'list', '--repo', merge_autoroll.REPO_OWNER_PATH,
        '--state', 'open', '--head', 'autoroll-main-to-target', '--json',
        'number,headRefName,baseRefName,title'
    ],
                                     capture_output=True,
                                     text=True,
                                     check=True,
                                     env=unittest.mock.ANY)

  @patch('subprocess.run')
  def test_main_conflicted_autoroll_file(self, mock_run):
    for conflict_content in ['CONFLICTED:12345\n', '<<<<<<< HEAD\n']:
      # Call real mkdtemp before mocking it
      test_tmpdir = tempfile.mkdtemp()

      with patch('merge_autoroll.tempfile.mkdtemp') as mock_mkdtemp:
        mock_mkdtemp.return_value = test_tmpdir

        os.makedirs(os.path.join(test_tmpdir, '.github'))
        autoroll_path = os.path.join(test_tmpdir, '.github/AUTOROLL')
        with open(autoroll_path, 'w') as f:
          f.write(conflict_content)

        # Reset mock_run call history for each iteration
        mock_run.reset_mock()
        self._setup_mock_run(mock_run)

        try:
          with patch('sys.stdout'), patch('sys.stderr'):
            with self.assertRaises(SystemExit) as cm:
              merge_autoroll.main()
          self.assertEqual(cm.exception.code, 1)
        finally:
          shutil.rmtree(test_tmpdir, ignore_errors=True)

  @patch('subprocess.run')
  def test_main_list_prs_fails(self, mock_run):
    self._setup_mock_run(mock_run, list_fail=True)

    with patch('sys.stdout'), patch('sys.stderr'):
      with self.assertRaises(subprocess.CalledProcessError):
        merge_autoroll.main()
    mock_run.assert_called_once_with([
        'gh', 'pr', 'list', '--repo', merge_autoroll.REPO_OWNER_PATH,
        '--state', 'open', '--head', 'autoroll-main-to-target', '--json',
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

  @patch('merge_autoroll.openssl')
  def test_generate_jwt_success(self, mock_openssl):
    mock_openssl.return_value = b'fake_signature'

    with patch('time.time', return_value=100000):
      jwt = merge_autoroll.generate_jwt('12345', '/tmp/fake.pem')

    self.assertTrue(jwt.startswith('eyJhbGciOiAiUlMyNTYiLCAidHlwIjogIkpXVCJ9.'))
    mock_openssl.assert_called_once_with(
        'dgst',
        '-sha256',
        '-sign',
        '/tmp/fake.pem',
        stdin=unittest.mock.ANY,
        stdout=subprocess.PIPE)

  @patch('merge_autoroll.openssl')
  def test_generate_jwt_openssl_not_found(self, mock_openssl):
    mock_openssl.side_effect = FileNotFoundError('openssl not found')

    with patch('sys.stdout'), patch('sys.stderr'):
      with self.assertRaises(FileNotFoundError):
        merge_autoroll.generate_jwt('12345', '/tmp/fake.pem')

  @patch('merge_autoroll.openssl')
  def test_generate_jwt_openssl_error(self, mock_openssl):
    mock_openssl.side_effect = subprocess.CalledProcessError(
        returncode=1, cmd='openssl')

    with patch('sys.stdout'), patch('sys.stderr'):
      with self.assertRaises(subprocess.CalledProcessError):
        merge_autoroll.generate_jwt('12345', '/tmp/fake.pem')

  @patch('urllib.request.urlopen')
  @patch('merge_autoroll.generate_jwt', return_value='fake_jwt')
  def test_get_installation_access_token_success(self, mock_generate_jwt,
                                                 mock_urlopen):
    mock_res_tok = MagicMock()
    mock_res_tok.__enter__.return_value = mock_res_tok
    mock_res_tok.read.return_value = b'{"token": "app_installed_token"}'

    mock_urlopen.return_value = mock_res_tok

    with patch('sys.stdout'), patch('sys.stderr'):
      token = merge_autoroll.get_installation_access_token(
          '12345', '/tmp/fake.pem')

    self.assertEqual(token, 'app_installed_token')
    mock_generate_jwt.assert_called_once_with('12345', '/tmp/fake.pem')

  @patch('urllib.request.urlopen')
  @patch('merge_autoroll.generate_jwt', return_value='fake_jwt')
  def test_get_installation_access_token_token_error(self, mock_generate_jwt,
                                                     mock_urlopen):
    mock_urlopen.side_effect = urllib.error.HTTPError(
        'https://api.github.com/app/installations/119484904/access_tokens', 400,
        'Bad Request', None, None)

    with patch('sys.stdout'), patch('sys.stderr'):
      with self.assertRaises(urllib.error.HTTPError):
        merge_autoroll.get_installation_access_token(
            '12345', '/tmp/fake.pem')

  # pylint: enable=unused-argument


class TestWrappers(unittest.TestCase):
  """Test cases for wrapper functions."""

  @patch('subprocess.run')
  def test_git_wrapper_default_check(self, mock_run):
    merge_autoroll.git('status')
    mock_run.assert_called_once_with(['git', 'status'], check=True)

  @patch('subprocess.run')
  def test_git_wrapper_custom_check(self, mock_run):
    merge_autoroll.git('status', check=False)
    mock_run.assert_called_once_with(['git', 'status'], check=False)

  @patch('subprocess.run')
  def test_gh_wrapper_default_check(self, mock_run):
    merge_autoroll.gh('auth', 'status')
    mock_run.assert_called_once_with(['gh', 'auth', 'status'], check=True)

  @patch('subprocess.run')
  def test_gh_wrapper_custom_check(self, mock_run):
    merge_autoroll.gh('auth', 'status', check=False)
    mock_run.assert_called_once_with(['gh', 'auth', 'status'], check=False)

  @patch('subprocess.run')
  def test_openssl_wrapper_default_check(self, mock_run):
    merge_autoroll.openssl('status')
    mock_run.assert_called_once_with(['openssl', 'status'],
                                     input=None,
                                     check=True)

  @patch('subprocess.run')
  def test_openssl_wrapper_custom_stdin_and_check(self, mock_run):
    merge_autoroll.openssl('status', stdin=b'data', check=False)
    mock_run.assert_called_once_with(['openssl', 'status'],
                                     input=b'data',
                                     check=False)


if __name__ == '__main__':
  unittest.main()
