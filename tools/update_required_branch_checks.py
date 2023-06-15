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

from github import Github

BRANCH_NAME = 'main'

YOUR_TOKEN = ''
g = Github(YOUR_TOKEN)

repo = g.get_repo('youtube/cobalt')
latest_open_pr = repo.get_pulls(
    state='open', sort='updated', base=BRANCH_NAME, direction='desc')[0]
latest_pr_commit = repo.get_commit(latest_open_pr.head)
check_runs = latest_pr_commit.get_check_runs()


# Filter out check runs that have '_on_device_' in the name
def should_include_run(check_run):
  if '_on_device_' in check_run.name:
    return False
  return True


filtered_check_runs = [run for run in check_runs if should_include_run(run)]
check_names = [run.name for run in filtered_check_runs]

branch = repo.get_branch(BRANCH_NAME)
protection = branch.get_protection()
check_names += protection.required_status_checks.contexts
checks = set(check_names)

branch.edit_protection(contexts=check_names)
