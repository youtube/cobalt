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
import github
import os

_GITHUB_TOKEN = os.getenv('GITHUB_TOKEN', '')
assert _GITHUB_TOKEN != '', (
    'GITHUB_TOKEN must be set. Use `gh auth token` or issue a Personal Access'
    'Token with \'repo\' permission on https://github.com/settings/tokens.')

TARGET_REPO = 'youtube/cobalt'

EXCLUDED_CHECK_PATTERNS = [
    # Not ready yet/temporary excludes.
    '_yts_wpt_',
    'browser_tests_on_host',

    # Excludes docker jobs. Build jobs depend on docker jobs and are required.
    'docker-',

    # Excludes non build/test checks.
    'feedback/copybara',
    'prepare_branch_list',
    'cherry_pick',
    'assign-reviewer',
    'import/copybara',
    'upload-release-artifacts',
    'generate_commit_message',

    # Excludes coverage, test logs and other report jobs.
    'linux-coverage',
    'codecov',
    'on-host-unit-test-report',
    'Test Report',

    # Excludes the actual test jobs. The requiredness
    # is determined by the tests_passing job.
    '_on_device_',
    '_on_host_',

    # Excludes slow and flaky evergreen tests.
    'evergreen-as-blackbox_test',
    'evergreen_test',

    # Excludes templated check names.
    '${{',
    'matrix.test_target',

    # Old compiler versions have started failing due to node/glibc
    # incompatibilities.
    'linux-clang-3-9',
    'linux-gcc-6-3',
]

# Exclude rc_11 and COBALT_9 releases.
MINIMUM_LTS_RELEASE = 19
LATEST_LTS_RELEASE = 25


def get_protected_branches() -> list[str]:
  branches = ['main']
  for i in range(MINIMUM_LTS_RELEASE, LATEST_LTS_RELEASE + 1)[:-1]:
    branches.append(f'{i}.lts.1+')
  return branches


def initialize_repo_connection():
  g = github.Github(auth=github.Auth.Token(_GITHUB_TOKEN))
  return g.get_repo(TARGET_REPO)


def get_required_checks_for_branch(repo, branch: str) -> list[str]:
  # The 'merged' sort order is not listed in public docs but still works.
  # If this functionality is removed the alternative is to loop through all
  # PRs and use the 'merged_at' property to determine which is the latest one.
  # https://docs.github.com/en/rest/pulls/pulls#list-pull-requests.
  # The equivalent query in the UI is: state:merged base:{branch} sort:desc.
  prs = repo.get_pulls(
      state='closed', sort='merged', base=branch, direction='desc')

  for pr in prs[:20]:
    if pr.merged:
      print(f'Checking #{pr.number}')
      latest_pr_commit = repo.get_commit(pr.head.sha)
      checks = latest_pr_commit.get_check_runs()
      req_checks = [c for c in checks if should_include_run(c)]
      # Ensure all required jobs passed as downstream jobs of
      # failed/cancelled jobs don't appear in the list.
      if len(req_checks) and all(c.conclusion == 'success' for c in req_checks):
        return list({run.name for run in req_checks})
      else:
        print('\n'.join(f'  {c.name}: {c.conclusion}' for c in req_checks
                        if c.conclusion != 'success'))

  raise RuntimeError(f'Could not find any completed checks for branch {branch}')


def should_include_run(check_run) -> bool:
  return not any(
      pattern in check_run.name for pattern in EXCLUDED_CHECK_PATTERNS)


def print_checks(repo, branch_name: str, new_checks: list[str],
                 print_unchanged: bool) -> None:
  branch = repo.get_branch(branch_name)
  current_checks = branch.get_required_status_checks().contexts

  def print_check_list(checks):
    for check_name in sorted(checks):
      print(check_name)
    print()

  new_checks_set = set(new_checks)
  current_checks_set = set(current_checks)

  added_checks = new_checks_set - current_checks_set
  if added_checks:
    print(f'Required checks to be ADDED for {branch_name}:')
    print_check_list(added_checks)

  removed_checks = current_checks_set - new_checks_set
  if removed_checks:
    print(f'Required checks to be REMOVED for {branch_name}:')
    print_check_list(removed_checks)

  if print_unchanged:
    unchanged_checks = current_checks_set.intersection(new_checks_set)
    print(f'Required checks that will REMAIN for {branch_name}:')
    print_check_list(unchanged_checks)


def update_protection_for_branch(repo, branch: str,
                                 check_names: list[str]) -> None:
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

  if args.apply:
    print('Applying changes to required branch checks.\n')
  else:
    print('This is a dry-run, printing pending changes only.\n')

  for branch in args.branch:
    required_checks = get_required_checks_for_branch(repo, branch)
    print_checks(repo, branch, required_checks, args.print_unchanged)
    if args.apply:
      update_protection_for_branch(repo, branch, required_checks)

  if not args.apply:
    print('Re-run with --apply to apply the changes.')


if __name__ == '__main__':
  main()
