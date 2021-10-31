# Copyright (c) 2009, 2010, 2011 Google Inc. All rights reserved.
# Copyright (c) 2009 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import datetime
import logging
import re

from webkitpy.common.memoized import memoized
from webkitpy.common.system.executive import Executive, ScriptError
from webkitpy.common.system.filesystem import FileSystem

_log = logging.getLogger(__name__)


class Git(object):
    # Unless otherwise specified, methods are expected to return paths relative
    # to self.checkout_root.

    # Git doesn't appear to document error codes, but seems to return
    # 1 or 128, mostly.
    ERROR_FILE_IS_MISSING = 128

    executable_name = 'git'

    def __init__(self, cwd=None, executive=None, filesystem=None):
        self._executive = executive or Executive()
        self._filesystem = filesystem or FileSystem()

        self.cwd = cwd or self._filesystem.abspath(self._filesystem.getcwd())
        if not Git.in_working_directory(self.cwd, executive=self._executive):
            module_directory = self._filesystem.abspath(
                self._filesystem.dirname(self._filesystem.path_to_module(self.__module__)))
            _log.info('The current directory (%s) is not in a git repo, trying directory %s.',
                      cwd, module_directory)
            if Git.in_working_directory(module_directory, executive=self._executive):
                self.cwd = module_directory
            _log.error('Failed to find Git repo for %s or %s', cwd, module_directory)

        self._init_executable_name()
        self.checkout_root = self.find_checkout_root(self.cwd)

    def _init_executable_name(self):
        # FIXME: This is a hack and should be removed.
        try:
            self._executive.run_command(['git', 'help'])
        except OSError:
            try:
                self._executive.run_command(['git.bat', 'help'])
                # The Win port uses the depot_tools package, which contains a number
                # of development tools, including Python and git. Instead of using a
                # real git executable, depot_tools indirects via a batch file, called
                # "git.bat". This batch file allows depot_tools to auto-update the real
                # git executable, which is contained in a subdirectory.
                _log.debug('Engaging git.bat Windows hack.')
                self.executable_name = 'git.bat'
            except OSError:
                _log.debug('Failed to engage git.bat Windows hack.')

    def _run_git(self,
                 command_args,
                 cwd=None,
                 input=None,  # pylint: disable=redefined-builtin
                 timeout_seconds=None,
                 decode_output=True,
                 return_exit_code=False):
        full_command_args = [self.executable_name] + command_args
        cwd = cwd or self.checkout_root
        return self._executive.run_command(
            full_command_args,
            cwd=cwd,
            input=input,
            timeout_seconds=timeout_seconds,
            return_exit_code=return_exit_code,
            decode_output=decode_output)

    # SCM always returns repository relative path, but sometimes we need
    # absolute paths to pass to rm, etc.
    def absolute_path(self, repository_relative_path):
        return self._filesystem.join(self.checkout_root, repository_relative_path)

    @classmethod
    def in_working_directory(cls, path, executive=None):
        try:
            executive = executive or Executive()
            return executive.run_command([cls.executable_name, 'rev-parse', '--is-inside-work-tree'],
                                         cwd=path, error_handler=Executive.ignore_error).rstrip() == 'true'
        except OSError:
            # The Windows bots seem to through a WindowsError when git isn't installed.
            return False

    def find_checkout_root(self, path):
        # "git rev-parse --show-cdup" would be another way to get to the root
        checkout_root = self._run_git(['rev-parse', '--show-toplevel'], cwd=(path or './')).strip()
        if not self._filesystem.isabs(checkout_root):  # Sometimes git returns relative paths
            checkout_root = self._filesystem.join(path, checkout_root)
        return checkout_root

    @classmethod
    def read_git_config(cls, key, cwd=None, executive=None):
        # FIXME: This should probably use cwd=self.checkout_root.
        # Pass --get-all for cases where the config has multiple values
        # Pass the cwd if provided so that we can handle the case of running webkit-patch outside of the working directory.
        # FIXME: This should use an Executive.
        executive = executive or Executive()
        return executive.run_command(
            [cls.executable_name, 'config', '--get-all', key], error_handler=Executive.ignore_error, cwd=cwd).rstrip('\n')

    def _discard_local_commits(self):
        self._run_git(['reset', '--hard', self._remote_branch_ref()])

    def _local_commits(self, ref='HEAD'):
        return self._run_git(['log', '--pretty=oneline', ref + '...' + self._remote_branch_ref()]).splitlines()

    def _rebase_in_progress(self):
        return self._filesystem.exists(self.absolute_path(self._filesystem.join('.git', 'rebase-apply')))

    def has_working_directory_changes(self, pathspec=None):
        """Checks whether there are uncommitted changes."""
        command = ['diff', 'HEAD', '--no-renames', '--name-only']
        if pathspec:
            command.extend(['--', pathspec])
        return self._run_git(command) != ''

    def _discard_working_directory_changes(self):
        # Could run git clean here too, but that wouldn't match subversion
        self._run_git(['reset', 'HEAD', '--hard'])
        # Aborting rebase even though this does not match subversion
        if self._rebase_in_progress():
            self._run_git(['rebase', '--abort'])

    def unstaged_changes(self):
        """Lists files with unstaged changes, including untracked files.

        Returns a dict mapping modified file paths (relative to checkout root)
        to one-character codes identifying the change, e.g. 'M' for modified,
        'D' for deleted, '?' for untracked.
        """
        # `git status -z` is a version of `git status -s`, that's recommended
        # for machine parsing. Lines are terminated with NUL rather than LF.
        change_lines = self._run_git(['status', '-z', '--untracked-files=all']).rstrip('\x00')
        if not change_lines:
            return {}  # No changes.
        unstaged_changes = {}
        for line in change_lines.split('\x00'):
            assert len(line) >= 4, 'Unexpected change line format %s' % line
            if line[1] == ' ':
                continue  # Already staged for commit.
            path = line[3:]
            unstaged_changes[path] = line[1]
        return unstaged_changes

    def add_list(self, paths, return_exit_code=False):
        return self._run_git(['add'] + paths, return_exit_code=return_exit_code)

    def delete_list(self, paths):
        return self._run_git(['rm', '-f'] + paths)

    def move(self, origin, destination):
        return self._run_git(['mv', '-f', origin, destination])

    def exists(self, path):
        return_code = self._run_git(['show', 'HEAD:%s' % path], return_exit_code=True, decode_output=False)
        return return_code != self.ERROR_FILE_IS_MISSING

    def _branch_from_ref(self, ref):
        return ref.replace('refs/heads/', '')

    def current_branch(self):
        """Returns the name of the current branch, or empty string if HEAD is detached."""
        ref = self._run_git(['rev-parse', '--symbolic-full-name', 'HEAD']).strip()
        if ref == 'HEAD':
            # HEAD is detached; return an empty string.
            return ''
        return self._branch_from_ref(ref)

    def current_branch_or_ref(self):
        """Returns the name of the current branch, or the commit hash if HEAD is detached."""
        branch_name = self.current_branch()
        if not branch_name:
            # HEAD is detached; use commit SHA instead.
            return self._run_git(['rev-parse', 'HEAD']).strip()
        return branch_name

    def _upstream_branch(self):
        current_branch = self.current_branch()
        return self._branch_from_ref(self.read_git_config(
            'branch.%s.merge' % current_branch, cwd=self.checkout_root, executive=self._executive).strip())

    def _merge_base(self, git_commit=None):
        if git_commit:
            # Rewrite UPSTREAM to the upstream branch
            if 'UPSTREAM' in git_commit:
                upstream = self._upstream_branch()
                if not upstream:
                    raise ScriptError(message='No upstream/tracking branch set.')
                git_commit = git_commit.replace('UPSTREAM', upstream)

            # Special-case <refname>.. to include working copy changes, e.g., 'HEAD....' shows only the diffs from HEAD.
            if git_commit.endswith('....'):
                return git_commit[:-4]

            if '..' not in git_commit:
                git_commit = git_commit + '^..' + git_commit
            return git_commit

        return self._remote_merge_base()

    def changed_files(self, git_commit=None, diff_filter='ADM'):
        # FIXME: --diff-filter could be used to avoid the "extract_filenames" step.
        status_command = ['diff', '-r', '--name-status',
                          '--no-renames', '--no-ext-diff', '--full-index', self._merge_base(git_commit)]
        # FIXME: I'm not sure we're returning the same set of files that SVN.changed_files is.
        # Added (A), Copied (C), Deleted (D), Modified (M), Renamed (R)
        return self._run_status_and_extract_filenames(status_command, self._status_regexp(diff_filter))

    def added_files(self):
        return self._run_status_and_extract_filenames(self.status_command(), self._status_regexp('A'))

    def _run_status_and_extract_filenames(self, status_command, status_regexp):
        filenames = []
        # We run with cwd=self.checkout_root so that returned-paths are root-relative.
        for line in self._run_git(status_command, cwd=self.checkout_root).splitlines():
            match = re.search(status_regexp, line)
            if not match:
                continue
            # status = match.group('status')
            filename = match.group('filename')
            filenames.append(filename)
        return filenames

    def status_command(self):
        # git status returns non-zero when there are changes, so we use git diff name --name-status HEAD instead.
        # No file contents printed, thus utf-8 autodecoding in self.run is fine.
        return ['diff', '--name-status', '--no-renames', 'HEAD']

    def _status_regexp(self, expected_types):
        return '^(?P<status>[%s])\t(?P<filename>.+)$' % expected_types

    @staticmethod
    def supports_local_commits():
        return True

    def display_name(self):
        return 'git'

    def most_recent_log_matching(self, grep_str, path):
        # We use '--grep=' + foo rather than '--grep', foo because
        # git 1.7.0.4 (and earlier) didn't support the separate arg.
        return self._run_git(['log', '-1', '--grep=' + grep_str, '--date=iso', self.find_checkout_root(path)])

    def _commit_position_from_git_log(self, git_log):
        match = re.search(r"^\s*Cr-Commit-Position:.*@\{#(?P<commit_position>\d+)\}", git_log, re.MULTILINE)
        if not match:
            return ''
        return int(match.group('commit_position'))

    def commit_position(self, path):
        """Returns the latest chromium commit position found in the checkout."""
        git_log = self.most_recent_log_matching('Cr-Commit-Position:', path)
        return self._commit_position_from_git_log(git_log)

    def _commit_position_regex_for_timestamp(self):
        return 'Cr-Commit-Position:.*@{#%s}'

    def timestamp_of_revision(self, path, revision):
        git_log = self.most_recent_log_matching(self._commit_position_regex_for_timestamp() % revision, path)
        match = re.search(r"^Date:\s*(\d{4})-(\d{2})-(\d{2}) (\d{2}):(\d{2}):(\d{2}) ([+-])(\d{2})(\d{2})$", git_log, re.MULTILINE)
        if not match:
            return ''

        # Manually modify the timezone since Git doesn't have an option to show it in UTC.
        # Git also truncates milliseconds but we're going to ignore that for now.
        time_with_timezone = datetime.datetime(int(match.group(1)), int(match.group(2)), int(match.group(3)),
                                               int(match.group(4)), int(match.group(5)), int(match.group(6)), 0)

        sign = 1 if match.group(7) == '+' else -1
        time_without_timezone = time_with_timezone - \
            datetime.timedelta(hours=sign * int(match.group(8)), minutes=int(match.group(9)))
        return time_without_timezone.strftime('%Y-%m-%dT%H:%M:%SZ')

    def create_patch(self, git_commit=None, changed_files=None):
        """Returns a byte array (str()) representing the patch file.
        Patch files are effectively binary since they may contain
        files of multiple different encodings.
        """
        order = self._patch_order()
        command = [
            'diff',
            '--binary',
            '--no-color',
            '--no-ext-diff',
            '--full-index',
            '--no-renames',
            '--src-prefix=a/',
            '--dst-prefix=b/',

        ]
        if order:
            command.append(order)
        command += [self._merge_base(git_commit), '--']
        if changed_files:
            command += changed_files
        return self._run_git(command, decode_output=False, cwd=self.checkout_root)

    def _patch_order(self):
        # Put code changes at the top of the patch and layout tests
        # at the bottom, this makes for easier reviewing.
        config_path = self._filesystem.dirname(self._filesystem.path_to_module('webkitpy.common.config'))
        order_file = self._filesystem.join(config_path, 'orderfile')
        if self._filesystem.exists(order_file):
            return '-O%s' % order_file
        return ''

    @memoized
    def commit_position_from_git_commit(self, git_commit):
        git_log = self.git_commit_detail(git_commit)
        return self._commit_position_from_git_log(git_log)

    def checkout_branch(self, name):
        self._run_git(['checkout', '-q', name])

    def create_clean_branch(self, name):
        self._run_git(['checkout', '-q', '-b', name, self._remote_branch_ref()])

    def blame(self, path):
        return self._run_git(['blame', '--show-email', path])

    # Git-specific methods:
    def _branch_ref_exists(self, branch_ref):
        return self._run_git(['show-ref', '--quiet', '--verify', branch_ref], return_exit_code=True) == 0

    def delete_branch(self, branch_name):
        if self._branch_ref_exists('refs/heads/' + branch_name):
            self._run_git(['branch', '-D', branch_name])

    def _remote_merge_base(self):
        return self._run_git(['merge-base', self._remote_branch_ref(), 'HEAD']).strip()

    def _remote_branch_ref(self):
        # Use references so that we can avoid collisions, e.g. we don't want to operate on refs/heads/trunk if it exists.
        remote_master_ref = 'refs/remotes/origin/master'
        if not self._branch_ref_exists(remote_master_ref):
            raise ScriptError(message="Can't find a branch to diff against. %s does not exist" % remote_master_ref)
        return remote_master_ref

    def commit_locally_with_message(self, message):
        command = ['commit', '--all', '-F', '-']
        self._run_git(command, input=message)

    def pull(self, timeout_seconds=None):
        self._run_git(['pull'], timeout_seconds=timeout_seconds)

    def latest_git_commit(self):
        return self._run_git(['log', '-1', '--format=%H']).strip()

    def git_commits_since(self, commit):
        return self._run_git(['log', commit + '..master', '--format=%H', '--reverse']).split()

    def git_commit_detail(self, commit, format=None):  # pylint: disable=redefined-builtin
        args = ['log', '-1', commit]
        if format:
            args.append('--format=' + format)
        return self._run_git(args)

    def affected_files(self, commit):
        output = self._run_git(['log', '-1', '--format=', '--name-only', commit])
        return output.strip().split('\n')

    def _branch_tracking_remote_master(self):
        origin_info = self._run_git(['remote', 'show', 'origin', '-n'])
        match = re.search(r"^\s*(?P<branch_name>\S+)\s+merges with remote master$", origin_info, re.MULTILINE)
        if not match:
            raise ScriptError(message='Unable to find local branch tracking origin/master.')
        branch = str(match.group('branch_name'))
        return self._branch_from_ref(self._run_git(['rev-parse', '--symbolic-full-name', branch]).strip())

    def is_cleanly_tracking_remote_master(self):
        if self.has_working_directory_changes():
            return False
        if self.current_branch() != self._branch_tracking_remote_master():
            return False
        if len(self._local_commits(self._branch_tracking_remote_master())) > 0:
            return False
        return True

    def ensure_cleanly_tracking_remote_master(self):
        self._discard_working_directory_changes()
        self._run_git(['checkout', '-q', self._branch_tracking_remote_master()])
        self._discard_local_commits()
