# Copyright 2023 The Cobalt Authors. All Rights Reserved.
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
"""Updates the requires status checks for a branch.

Requires PyGithub to run:

  $ pip install PyGithub
"""

import argparse
from github import Github
from typing import List

# Issue a Personal Access Token with 'repo' permission on
# https://github.com/settings/tokens.
YOUR_GITHUB_TOKEN = ''
assert YOUR_GITHUB_TOKEN != '', 'YOUR_GITHUB_TOKEN must be set.'

TARGET_REPO = 'youtube/cobalt'

EXCLUDED_CHECK_PATTERNS = [
    # Excludes non build/test checks.
    'feedback/copybara',
    'prepare_branch_list',
    'cherry_pick',
    'assign-reviewer',

    # Excludes coverage and test reports.
    'linux-coverage',
    'codecov',
    'on-host-unit-test-report',

    # Excludes blackbox, web platform, and unit tests run on-device.
    '_on_device_',

    # Excludes slow and flaky evergreen tests.
    'evergreen-as-blackbox_test',
    'evergreen_test',

    # Excludes templated check names.
    '${{'
]

# Exclude rc_11 and COBALT_9 releases.
MINIMUM_LTS_RELEASE_NUMBER = 19
LATEST_LTS_RELEASE_NUMBER = 24


def get_protected_branches() -> List[str]:
  branches = ['main']
  for i in range(MINIMUM_LTS_RELEASE_NUMBER, LATEST_LTS_RELEASE_NUMBER + 1):
    branches.append(f'{i}.lts.1+')
  return branches


def initialize_repo_connection():
  g = Github(YOUR_GITHUB_TOKEN)
  return g.get_repo(TARGET_REPO)


def get_checks_for_branch(repo, branch: str) -> None:
  # The 'merged' sort order is not listed in public docs but still works.
  # If this functionality is removed the alternative is to loop through all
  # PRs and use the 'merged_at' property to determine which is the latest one.
  # https://docs.github.com/en/rest/pulls/pulls#list-pull-requests
  prs = repo.get_pulls(
      state='closed', sort='merged', base=branch, direction='desc')

  latest_pr = None
  for pr in prs:
    if pr.merged:
      latest_pr = pr
      break

  latest_pr_commit = repo.get_commit(latest_pr.head.sha)
  checks = latest_pr_commit.get_check_runs()
  return checks


def should_include_run(check_run) -> bool:
  for pattern in EXCLUDED_CHECK_PATTERNS:
    if pattern in check_run.name:
      return False
  return True


def get_required_checks_for_branch(repo, branch: str) -> List[str]:
  checks = get_checks_for_branch(repo, branch)
  filtered_check_runs = [run for run in checks if should_include_run(run)]
  check_names = set(run.name for run in filtered_check_runs)
  return list(check_names)


def print_checks(repo, branch_name: str, new_checks: List[str],
                 print_unchanged: bool) -> None:
  branch = repo.get_branch(branch_name)
  current_checks = branch.get_required_status_checks().contexts

  def print_check_list(checks):
    for check_name in sorted(checks):
      print(check_name)
    print()

  added_checks = set(new_checks) - set(current_checks)
  if added_checks:
    print(f'Required checks to be ADDED for {branch_name}:')
    print_check_list(added_checks)

  removed_checks = set(current_checks) - set(new_checks)
  if removed_checks:
    print(f'Required checks to be REMOVED for {branch_name}:')
    print_check_list(removed_checks)

  if print_unchanged:
    unchanged_checks = set(current_checks).intersection(set(new_checks))
    print(f'Required checks that will REMAIN for {branch_name}:')
    print_check_list(unchanged_checks)


def update_protection_for_branch(repo, branch: str,
                                 check_names: List[str]) -> None:
  branch = repo.get_branch(branch)
  branch.edit_required_status_checks(contexts=check_names)


def parse_args() -> None:
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '-b',
      '--branch',
      action='append',
      help='Branch to update. Can be repeated to update multiple branches.'
      ' Defaults to all protected branches.')
  parser.add_argument(
      '--apply', action='store_true', help='Apply required checks updates.')
  parser.add_argument(
      '--print_unchanged',
      action='store_true',
      help='Also print the checks that will be left unchanged.'
      ' Is a no-op with --apply.')
  args = parser.parse_args()

  if not args.branch:
    args.branch = get_protected_branches()

  return args


def main() -> None:
  args = parse_args()
  repo = initialize_repo_connection()

  if not args.apply:
    print('This is a dry-run, printing pending changes only.')

  for branch in args.branch:
    required_checks = get_required_checks_for_branch(repo, branch)
    if args.apply:
      update_protection_for_branch(repo, branch, required_checks)
    else:
      print_checks(repo, branch, required_checks, args.print_unchanged)

  if not args.apply:
    print('Re-run with --apply to apply the changes.')


if __name__ == '__main__':
  main()
