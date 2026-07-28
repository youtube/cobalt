"""Unit tests for autoroll scripts."""

import io
import os
import subprocess
import sys
import unittest
from unittest.mock import MagicMock, mock_open, patch

# Add current directory to path
sys.path.append(os.path.dirname(__file__))
# pylint: disable=wrong-import-position
import autoroll
import autoroll_chromium
import autoroll_lib as lib
# pylint: enable=wrong-import-position


class TestGitHelper(unittest.TestCase):
  """Test cases for the git() helper function in autoroll_lib."""

  @patch('subprocess.run')
  def test_git_default(self, mock_run):
    mock_run.return_value = MagicMock(stdout='some output\n')
    res = lib.git('status')
    mock_run.assert_called_once_with(['git', 'status'],
                                     check=True,
                                     stdout=subprocess.PIPE,
                                     text=True)
    self.assertEqual(res, 'some output\n')

  @patch('subprocess.run')
  def test_git_stdout_redirection(self, mock_run):
    mock_run.return_value = MagicMock(stdout='output')
    res = lib.git('status', stdout=sys.stderr)
    mock_run.assert_called_once_with(['git', 'status'],
                                     check=True,
                                     stdout=sys.stderr,
                                     text=True)
    self.assertEqual(res, 'output')


class TestGetStartSha(unittest.TestCase):
  """Test cases for get_start_sha()."""

  @patch('autoroll_lib.git')
  def test_get_start_sha_normal(self, mock_git):
    mock_git.return_value = '  abc123def  \n'
    res = lib.get_start_sha('my-branch', 'roll_file')
    mock_git.assert_called_once_with('show', 'my-branch:roll_file')
    self.assertEqual(res, 'abc123def')

  @patch('autoroll_lib.git')
  def test_get_start_sha_conflicted(self, mock_git):
    mock_git.return_value = 'CONFLICTED:some_sha\n'
    res = lib.get_start_sha('my-branch', 'roll_file')
    self.assertIsNone(res)


class TestGetCommits(unittest.TestCase):
  """Test cases for get_commits()."""

  @patch('autoroll_lib.git')
  def test_get_commits_parsing(self, mock_git):
    mock_git.return_value = ('sha1 Commit Title 1 (#100)\n'
                             'sha2 Commit Title 2\n'
                             'sha3 Commit Title 3 (#200)\n')
    res = lib.get_commits('main', 'start_sha')
    mock_git.assert_called_once_with('rev-list', '--oneline',
                                     '--no-abbrev-commit', '--reverse',
                                     'start_sha..main')
    self.assertEqual(res, [('sha1', 'Commit Title 1', '100'),
                           ('sha2', 'Commit Title 2', None),
                           ('sha3', 'Commit Title 3', '200')])


class TestGetUnmergedFiles(unittest.TestCase):
  """Test cases for get_unmerged_files()."""

  @patch('autoroll_lib.git')
  def test_get_unmerged_files(self, mock_git):
    mock_git.return_value = ('100644 1234567 2\tfile1.txt\n'
                             '100644 abcdefg 3\tfile1.txt\n'
                             '100644 2345678 1\tfile2.txt\n')
    res = lib.get_unmerged_files()
    mock_git.assert_called_once_with('ls-files', '-u')
    self.assertEqual(res, {
        'file1.txt': {'ours', 'theirs'},
        'file2.txt': {'ancestor'}
    })

  @patch('autoroll_lib.git')
  def test_get_unmerged_files_malformed(self, mock_git):
    mock_git.return_value = 'malformed_line_no_tab\n'
    res = lib.get_unmerged_files()
    self.assertEqual(res, {})


class TestResolveConflicts(unittest.TestCase):
  """Test cases for resolve_conflicts()."""

  @patch('autoroll_lib.git')
  @patch('shutil.move')
  def test_resolve_conflicts_gitmodules(self, mock_move, mock_git):
    unmerged = {'.gitmodules': {'ours', 'theirs'}}
    mock_git.return_value = ''
    resolved = lib.resolve_conflicts(unmerged)
    self.assertTrue(resolved)
    mock_move.assert_called_once_with('.gitmodules', '.gitmodules_conflict')
    mock_git.assert_any_call('checkout', '--ours', '--', '.gitmodules')
    mock_git.assert_any_call('add', '--', '.gitmodules', '.gitmodules_conflict')
    self.assertNotIn('.gitmodules', unmerged)

  @patch('autoroll_lib.git')
  def test_resolve_conflicts_deleted_by_us(self, mock_git):
    unmerged = {'file1.txt': {'theirs'}}
    mock_git.return_value = ''
    resolved = lib.resolve_conflicts(unmerged)
    self.assertTrue(resolved)
    mock_git.assert_any_call('rm', '--ignore-unmatch', '--', 'file1.txt')
    self.assertEqual(unmerged, {})

  @patch('autoroll_lib.git')
  def test_resolve_conflicts_deleted_by_them(self, mock_git):
    unmerged = {'file1.txt': {'ours'}}
    mock_git.return_value = ''
    resolved = lib.resolve_conflicts(unmerged)
    self.assertTrue(resolved)
    mock_git.assert_any_call('rm', '--ignore-unmatch', '--', 'file1.txt')
    self.assertEqual(unmerged, {})

  @patch('autoroll_lib.git')
  def test_resolve_conflicts_submodule(self, mock_git):
    unmerged = {'submod': {'ours', 'theirs'}}

    def git_side_effect(*args, **_kwargs):
      if args[:3] == ('ls-files', '-u', '--'):
        return ('160000 0000000000000000000000000000000000000000 1\tsubmod\n'
                '160000 abc123def456 3\tsubmod\n')
      return ''

    mock_git.side_effect = git_side_effect

    resolved = lib.resolve_conflicts(unmerged)
    self.assertTrue(resolved)
    mock_git.assert_any_call('update-index', '--add', '--cacheinfo',
                             '160000,abc123def456,submod')
    self.assertEqual(unmerged, {})

  @patch('autoroll_lib.git')
  def test_resolve_conflicts_unresolvable(self, mock_git):
    unmerged = {'file1.txt': {'ours', 'theirs'}}
    mock_git.return_value = '100644 abc123def 2\tfile1.txt\n'
    resolved = lib.resolve_conflicts(unmerged)
    self.assertFalse(resolved)
    self.assertEqual(unmerged, {'file1.txt': {'ours', 'theirs'}})


class TestGetCherryPickMetadata(unittest.TestCase):
  """Test cases for get_cherry_pick_metadata()."""

  @patch('autoroll_lib.git')
  def test_get_metadata_with_pr(self, mock_git):
    mock_git.return_value = (
        'Mon July 10\x00John Doe <john@doe.com>\x00Commit body message')
    date, author, msg = lib.get_cherry_pick_metadata('sha123', 'My Title',
                                                     '456')
    self.assertEqual(date, 'Mon July 10')
    self.assertEqual(author, 'John Doe <john@doe.com>')
    self.assertIn('Cherry pick PR #456: My Title', msg)
    self.assertIn('Refer to original PR: #456', msg)
    self.assertIn('Commit body message', msg)
    self.assertIn('(cherry picked from commit sha123)', msg)

  @patch('autoroll_lib.git')
  def test_get_metadata_no_pr(self, mock_git):
    mock_git.return_value = (
        'Mon July 10\x00John Doe <john@doe.com>\x00Commit body message')
    date, author, msg = lib.get_cherry_pick_metadata('sha123', 'My Title',
                                                     None)
    self.assertEqual(date, 'Mon July 10')
    self.assertEqual(author, 'John Doe <john@doe.com>')
    self.assertIn('Cherry pick commit sha123: My Title', msg)
    self.assertIn('Refer to original commit: sha123', msg)
    self.assertIn('Commit body message', msg)


class TestApplyAndCommit(unittest.TestCase):
  """Test cases for apply_and_commit()."""

  @patch('autoroll_lib.git')
  def test_apply_and_commit_success(self, mock_git):

    def git_side_effect(*args, **_kwargs):
      if args[:3] == ('diff', '--cached', '--name-only'):
        return 'file1.txt\n'
      return ''

    mock_git.side_effect = git_side_effect

    with patch('builtins.open', mock_open()) as mock_file:
      status, unmerged = lib.apply_and_commit(
          'cherry-pick', 'sha123', ('date', 'author', 'msg'), True,
          ('roll_file', 'sha123'))

    self.assertEqual(status, lib.CommitStatus.SUCCESS)
    self.assertIsNone(unmerged)
    mock_file.assert_called_once_with('roll_file', 'w', encoding='utf-8')
    mock_file().write.assert_called_once_with('sha123\n')
    mock_git.assert_any_call('cherry-pick', '--no-commit', 'sha123')
    mock_git.assert_any_call('add', '--', 'roll_file')
    mock_git.assert_any_call('commit', '--no-verify', '--date=date',
                             '--author=author', '-m', 'msg')

  @patch('autoroll_lib.git')
  def test_apply_and_commit_skipped(self, mock_git):

    def git_side_effect(*args, **_kwargs):
      if args[:3] == ('diff', '--cached', '--name-only'):
        return ''
      return ''

    mock_git.side_effect = git_side_effect

    status, unmerged = lib.apply_and_commit('cherry-pick', 'sha123',
                                            ('date', 'author', 'msg'), True,
                                            ('roll_file', 'sha123'))
    self.assertEqual(status, lib.CommitStatus.SKIPPED)
    self.assertIsNone(unmerged)

  @patch('autoroll_lib.git')
  @patch('autoroll_lib.get_unmerged_files')
  @patch('autoroll_lib.resolve_conflicts')
  def test_apply_and_commit_conflicted_resolved(self, mock_resolve,
                                                mock_unmerged, mock_git):
    mock_git.side_effect = [
        subprocess.CalledProcessError(1, 'git cherry-pick'), 'file1.txt\n', '',
        ''
    ]
    mock_unmerged.return_value = {'file1.txt': {'ours', 'theirs'}}
    mock_resolve.return_value = True

    with patch('builtins.open', mock_open()) as mock_file:
      status, unmerged = lib.apply_and_commit(
          'cherry-pick', 'sha123', ('date', 'author', 'msg'), True,
          ('roll_file', 'sha123'))

    self.assertEqual(status, lib.CommitStatus.SUCCESS)
    self.assertIsNone(unmerged)
    mock_resolve.assert_called_once_with({'file1.txt': {'ours', 'theirs'}})
    mock_file().write.assert_called_once_with('sha123\n')

  @patch('autoroll_lib.git')
  @patch('autoroll_lib.get_unmerged_files')
  @patch('autoroll_lib.resolve_conflicts')
  def test_apply_and_commit_conflicted_unresolved_first_commit(
      self, mock_resolve, mock_unmerged, mock_git):
    mock_git.side_effect = [
        subprocess.CalledProcessError(1, 'git cherry-pick'), '', 'file1.txt\n',
        '', ''
    ]
    mock_unmerged.return_value = {'file1.txt': {'ours', 'theirs'}}
    mock_resolve.return_value = False

    with patch('builtins.open', mock_open()) as mock_file:
      status, unmerged = lib.apply_and_commit(
          'cherry-pick', 'sha123', ('date', 'author', 'msg'), True,
          ('roll_file', 'sha123'))

    self.assertEqual(status, lib.CommitStatus.CONFLICTED)
    self.assertEqual(unmerged, ['file1.txt'])
    mock_file().write.assert_called_once_with('CONFLICTED:sha123\n')
    mock_git.assert_any_call('add', '--', 'file1.txt')
    mock_git.assert_any_call('commit', '--no-verify', '--date=date',
                             '--author=author', '-m', 'CONFLICTED msg')

  @patch('autoroll_lib.git')
  @patch('autoroll_lib.get_unmerged_files')
  @patch('autoroll_lib.resolve_conflicts')
  def test_apply_and_commit_conflicted_unresolved_not_first_commit(
      self, mock_resolve, mock_unmerged, mock_git):
    mock_git.side_effect = [
        subprocess.CalledProcessError(1, 'git cherry-pick'), ''
    ]
    mock_unmerged.return_value = {'file1.txt': {'ours', 'theirs'}}
    mock_resolve.return_value = False

    status, unmerged = lib.apply_and_commit('cherry-pick', 'sha123',
                                            ('date', 'author', 'msg'), False,
                                            ('roll_file', 'sha123'))

    self.assertEqual(status, lib.CommitStatus.FAILED)
    self.assertEqual(unmerged, ['file1.txt'])
    mock_git.assert_any_call('reset', '--hard', 'HEAD')


class TestAutorollMainFlow(unittest.TestCase):
  """Test cases for autoroll.main()."""

  @patch('autoroll_lib.get_start_sha')
  @patch('autoroll_lib.get_commits')
  @patch('autoroll_lib.git')
  @patch('autoroll.cherry_pick')
  @patch('autoroll_lib.get_cherry_pick_metadata')
  # pylint: disable=too-many-positional-arguments
  def test_main_success_flow(self, mock_metadata, mock_cherry_pick, _,
                             mock_get_commits, mock_get_start_sha):
    mock_get_start_sha.side_effect = (lambda branch, file: 'start_sha'
                                      if branch != 'HEAD' else 'roll_sha')

    mock_get_commits.side_effect = [[('sha1', 'Title 1', '101'),
                                     ('sha2', 'Title 2', '102')],
                                    [('sha2', 'Title 2', '102')]]

    mock_metadata.return_value = ('date', 'author', 'msg')
    mock_cherry_pick.return_value = (lib.CommitStatus.SUCCESS, None)

    test_args = [
        'autoroll.py', '--source-branch', 'main', '--target-branch', '27.lts',
        '--autoroll-file', 'roll_file', '--max-commits', '5',
        '--existing-pr-sha', ''
    ]

    with patch('sys.argv', test_args), patch(
        'sys.stdout', new_callable=io.StringIO) as mock_stdout:
      autoroll.main()

      mock_cherry_pick.assert_called_once_with('sha2',
                                               ('date', 'author', 'msg'), False,
                                               ('roll_file', 'sha2'))
      self.assertEqual('- #101\n- #102', mock_stdout.getvalue().strip())


class TestChromiumSubmoduleAndCheckout(unittest.TestCase):
  """Test cases for Chromium submodule and checkout handling."""

  @patch('autoroll_lib.git')
  def test_get_submodule_root_dirs(self, mock_git):
    mock_git.return_value = (
        'submodule.cobalt.path cobalt\n'
        'submodule.third_party/starboard.path third_party/starboard\n')
    res = autoroll_chromium.get_submodule_root_dirs()
    self.assertEqual(res, ['cobalt', 'third_party'])

  @patch('autoroll_chromium.get_submodule_root_dirs', return_value=['dir1'])
  @patch('os.path.isdir', return_value=True)
  @patch('os.path.islink', return_value=False)
  @patch('shutil.rmtree')
  @patch('autoroll_lib.git')
  def test_remove_local_checkout(self, mock_git, mock_rmtree, _mock_islink,
                                 _mock_isdir, _mock_submodules):
    autoroll_chromium.remove_local_checkout()
    mock_rmtree.assert_called_once_with('dir1', ignore_errors=True)
    mock_git.assert_any_call('rm', '-qrf', '--', '.')
    mock_git.assert_any_call('clean', '-qffdx')


class TestChromiumVerification(unittest.TestCase):
  """Test cases for Chromium commit verification functions."""

  @patch('urllib.request.urlopen')
  def test_fetch_chromium_tree(self, mock_urlopen):
    mock_resp = MagicMock()
    mock_resp.read.return_value = (
        b")]}\'\n{\"commit\": \"sha123\", \"tree\": \"tree_sha_gitiles\"}")
    mock_resp.__enter__.return_value = mock_resp
    mock_urlopen.return_value = mock_resp

    tree = autoroll_chromium.fetch_chromium_tree('sha123')
    self.assertEqual(tree, 'tree_sha_gitiles')

  @patch('autoroll_lib.git')
  def test_get_upstream_chromium_sha(self, mock_git):
    mock_git.return_value = (
        'Update to m139 branch point.\n\n'
        'Update to commit f600d0656fd5b5fe4a82981f533d31ed6939e2e4.\n')
    sha = autoroll_chromium.get_upstream_chromium_sha('cobalt_sha')
    self.assertEqual(sha, 'f600d0656fd5b5fe4a82981f533d31ed6939e2e4')
    mock_git.assert_called_once_with('log', '-1', '--format=%B', 'cobalt_sha')

  @patch(
      'autoroll_chromium.fetch_chromium_tree',
      return_value='tree_sha_match',
  )
  @patch(
      'autoroll_chromium.get_upstream_chromium_sha',
      return_value='upstream_sha_123',
  )
  @patch('autoroll_lib.git')
  def test_verify_chromium_commit_with_upstream_success(
      self,
      mock_git,
      unused_mock_upstream,
      unused_mock_fetch,
  ):
    mock_git.return_value = 'tree_sha_match\n'
    with patch('sys.stderr'):
      result = autoroll_chromium.verify_chromium_commit('cobalt_sha')
    self.assertTrue(result)

  @patch(
      'autoroll_chromium.get_upstream_chromium_sha',
      return_value=None,
  )
  def test_verify_chromium_commit_missing_upstream_sha_fails(
      self,
      unused_mock_upstream,
  ):
    with patch('sys.stderr'):
      result = autoroll_chromium.verify_chromium_commit('cobalt_sha')
    self.assertFalse(result)

  @patch(
      'autoroll_chromium.fetch_chromium_tree',
      side_effect=RuntimeError('Network error'),
  )
  @patch(
      'autoroll_chromium.get_upstream_chromium_sha',
      return_value='upstream_sha_123',
  )
  def test_verify_chromium_commit_gitiles_error_fails(
      self,
      unused_mock_upstream,
      unused_mock_fetch,
  ):
    with patch('sys.stderr'):
      result = autoroll_chromium.verify_chromium_commit('cobalt_sha')
    self.assertFalse(result)

  @patch(
      'autoroll_chromium.fetch_chromium_tree',
      return_value='tree_sha_expected',
  )
  @patch(
      'autoroll_chromium.get_upstream_chromium_sha',
      return_value='upstream_sha_123',
  )
  @patch('autoroll_lib.git')
  def test_verify_chromium_commit_failure(
      self,
      mock_git,
      unused_mock_upstream,
      unused_mock_fetch,
  ):
    mock_git.side_effect = [
        'tree_sha_different\n',
        'M path/to/file.cc\n',
    ]
    with patch('sys.stderr'):
      result = autoroll_chromium.verify_chromium_commit('cobalt_sha')
    self.assertFalse(result)


class TestChromiumCherryPick(unittest.TestCase):
  """Test cases for chromium_cherry_pick()."""

  @patch('autoroll_chromium.verify_chromium_commit', return_value=True)
  @patch('autoroll_chromium.remove_local_checkout')
  @patch('autoroll_chromium.replace_submodules_with_dirs')
  @patch('autoroll_lib.git')
  @patch('autoroll_lib.apply_and_commit')
  def test_chromium_cherry_pick(self, mock_apply_and_commit, mock_git,
                                mock_replace_submodules, mock_remove_checkout,
                                mock_verify):
    mock_git.side_effect = lambda *args, **kwargs: 'revert_sha' if args[
        0] == 'rev-parse' else ''
    mock_apply_and_commit.return_value = (lib.CommitStatus.CONFLICTED,
                                          ['file1.txt'])

    res = autoroll_chromium.chromium_cherry_pick('prev_sha', ['sha123'],
                                                 ('date', 'auth', 'msg'),
                                                 ('roll_file', 'sha123'))

    self.assertEqual(mock_remove_checkout.call_count, 2)
    self.assertEqual(mock_replace_submodules.call_count, 2)

    mock_git.assert_any_call('checkout', 'prev_sha', '--', '.')
    mock_git.assert_any_call('checkout', '-f', 'prev_sha', '--', '.')
    mock_git.assert_any_call('commit', '--no-verify', '-qm',
                             'CONFLICTED Chromium Cherry pick: Revert Cobalt.')
    mock_git.assert_any_call('commit', '--no-verify', '-qm',
                             'Restore submodules.')
    mock_git.assert_any_call('commit', '--no-verify', '-qm',
                             'Remove submodules.')
    mock_git.assert_any_call('cherry-pick', 'sha123')
    mock_apply_and_commit.assert_called_once_with('revert', 'revert_sha',
                                                  ('date', 'auth', 'msg'), True,
                                                  ('roll_file', 'sha123'))
    self.assertEqual(res, (lib.CommitStatus.CONFLICTED, ['file1.txt']))


if __name__ == '__main__':
  unittest.main()
