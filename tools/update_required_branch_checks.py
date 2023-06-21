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
"""Updates the requires status checks for a branch."""

import argparse
from github import Github
from typing import List

YOUR_GITHUB_TOKEN = ''

TARGET_REPO = 'youtube/cobalt'

EXCLUDED_CHECK_PATTERNS = ['feedback/copybara', '_on_device_', r'${{']

# Exclude rc_11 and COBALT_9 releases.
MINIMUM_LTS_RELEASE_NUMBER = 19
LATEST_LTS_RELEASE_NUMBER = 24


def get_protected_branches() -> List[str]:
  branches = ['main']
  for i in range(MINIMUM_LTS_RELEASE_NUMBER, LATEST_LTS_RELEASE_NUMBER + 1):
    branches.append(f'{i}.lts.1+')
  return branches


def parse_args():
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '-b',
      '--branch',
      action='append',
      help='Branch to update. Can be repeated to update multiple branches.'
      ' Defaults to all protected branches.')
  parser.add_argument(
      '--dry_run',
      action='store_true',
      default=False,
      help='Only print protection updates.')
  args = parser.parse_args()

  if not args.branch:
    args.branch = get_protected_branches()

  return args


def initialize_repo_connection():
  g = Github(YOUR_GITHUB_TOKEN)
  return g.get_repo(TARGET_REPO)


def get_checks_for_branch(repo, branch: str):
  prs = repo.get_pulls(
      state='open', sort='updated', base=branch, direction='desc')
  try:
    latest_pr = prs[0]
  except IndexError:
    prs = repo.get_pulls(
        state='closed', sort='updated', base=branch, direction='desc')
    latest_pr = prs[0]
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
  check_names = [run.name for run in filtered_check_runs]
  return check_names


def print_checks(branch: str, check_names: List[str]):
  print(f'Checks for {branch}:')
  for check_name in check_names:
    print(check_name)
  print()


def update_protection_for_branch(repo, branch: str, check_names: List[str]):
  branch = repo.get_branch(branch)
  branch.edit_protection(contexts=check_names)


def main():
  args = parse_args()
  repo = initialize_repo_connection()
  for branch in args.branch:
    required_checks = get_required_checks_for_branch(repo, branch)
    if args.dry_run:
      print_checks(branch, required_checks)
    else:
      update_protection_for_branch(repo, branch, required_checks)


if __name__ == '__main__':
  main()
