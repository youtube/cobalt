#!/usr/bin/env python3
#
# Copyright 2024 The Cobalt Authors. All Rights Reserved.
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
"""Warns if commit message does not include a tracking bug."""

import os
import re
import subprocess
import sys

RE_BUG_DESCRIPTION = re.compile(r'(\s|^)b/[0-9]+(\s|$)')


def check_commits_for_bug_in_description() -> bool:
    """Checks commits for tracking bug in description."""
    from_ref = os.environ.get('PRE_COMMIT_FROM_REF')
    to_ref = os.environ.get('PRE_COMMIT_TO_REF')
    if not from_ref or not to_ref:
        print('Invalid from ref or to ref, exiting...')
        sys.exit(0)

    commits_with_errors = []
    git_hashes = subprocess.check_output(
        ['git', 'log', f'{from_ref}..{to_ref}', '--format=%h'])
    git_hashes = git_hashes.strip().decode('utf-8').split()

    for git_hash in git_hashes:
        commit_message = subprocess.check_output(
            ['git', 'show', git_hash, '--format=%b', '--quiet'])
        commit_message = commit_message.strip().decode('utf-8')
        if not RE_BUG_DESCRIPTION.search(commit_message):
            commits_with_errors.append(git_hash)

    if commits_with_errors:
        print('Some changes do not reference a bug in the form b/########')
        for commits_with_error in commits_with_errors:
            subprocess.call(
                ['git', 'show', commits_with_error, '-s', '--oneline'])
        return True

    return False


if __name__ == '__main__':
    sys.exit(check_commits_for_bug_in_description())
