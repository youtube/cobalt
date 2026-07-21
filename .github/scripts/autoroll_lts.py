#!/usr/bin/env python3
"""Script to automatically roll LTS branch."""
import argparse
from collections import defaultdict
import enum
import re
import shutil
import subprocess
import sys


@enum.unique
class CommitStatus(enum.Enum):
  """Represents the outcome of a commit attempt."""
  SUCCESS = 'success'  # Successfully committed.
  CONFLICTED = 'conflicted'  # Committed with conflicts.
  SKIPPED = 'skipped'  # The commit was already present or no action was needed.
  FAILED = 'failed'  # The commit failed due to conflicts or other errors.


def log(msg):
  print(msg, file=sys.stderr)


def run(cmd):
  subprocess.run(cmd, check=True, stdout=sys.stderr)


def get_out(cmd):
  res = subprocess.run(cmd, capture_output=True, text=True, check=True)
  return res.stdout


def get_start_sha(branch, autoroll_file):
  """Returns an autoroll start SHA or None if CONFLICTED."""
  start = get_out(['git', 'show', f'{branch}:{autoroll_file}']).strip()

  if start.startswith('CONFLICTED:'):
    return None
  return start


def get_commits(branch, start):
  """Returns a list of commits in chronological order.

  Starting from the non-inclusive start, the commits are represented as a
  (sha, title, pr_num) tuple.
  """
  cmd = [
      'git', 'rev-list', '--oneline', '--no-abbrev-commit', '--reverse',
      f'{start}..{branch}'
  ]
  lines = get_out(cmd).splitlines()

  commits = []
  for line in lines:
    # match.groups() returns (sha, title, pr_num)
    # If no PR number is found, pr_num will be None
    match = re.search(r'^(\w+) (.*?)(?: \(#(\d+)\))?$', line)
    if match:
      commits.append(match.groups())
  return commits


def get_unmerged_files():
  """Returns a dict of files with conflicts mapping to named stages.

  Stages are mapped as follows:
  - '1': 'ancestor'
  - '2': 'ours'
  - '3': 'theirs'
  """
  lines = get_out(['git', 'ls-files', '-u']).splitlines()
  files = defaultdict(set)
  stage_map = {'1': 'ancestor', '2': 'ours', '3': 'theirs'}
  for line in lines:
    parts = line.split('\t', 1)
    if len(parts) < 2:
      log(f'Warning: Malformed line (missing tab): {line}')
      continue
    metadata, path = parts
    meta_parts = metadata.split()
    if len(meta_parts) < 3:
      log(f'Warning: Malformed metadata: {metadata}')
      continue
    _, _, stage = meta_parts[:3]
    stage_name = stage_map.get(stage, stage)
    files[path].add(stage_name)
  return files


def resolve_conflicts(unmerged_files):
  """Attempts to resolve conflicts automatically.

  Returns:
    bool: True if all conflicts were resolved, False otherwise.
  """
  # Special handling for .gitmodules to prevent "bad config" fatal errors
  if '.gitmodules' in unmerged_files:
    shutil.move('.gitmodules', '.gitmodules_conflict')
    run(['git', 'checkout', '--ours', '--', '.gitmodules'])
    run(['git', 'add', '--', '.gitmodules', '.gitmodules_conflict'])
    unmerged_files.pop('.gitmodules', None)

  deleted_by_us = []
  deleted_by_them = []
  submodule_conflicts = []
  other_conflicts = []

  for path, stages in unmerged_files.items():
    # Check if this path is a submodule (mode 160000)
    file_info = get_out(['git', 'ls-files', '-u', '--', path])
    is_submodule = '160000' in file_info

    if 'theirs' in stages and 'ours' not in stages:
      deleted_by_us.append(path)
    elif 'theirs' not in stages and 'ours' in stages:
      deleted_by_them.append(path)
    elif is_submodule:
      submodule_conflicts.append(path)
    else:
      other_conflicts.append(path)

  if deleted_by_us:
    log(f'Resolving \'deleted by us\' conflicts: {deleted_by_us}')
    run(['git', 'rm', '--ignore-unmatch', '--'] + deleted_by_us)
    for path in deleted_by_us:
      unmerged_files.pop(path, None)

  if deleted_by_them:
    log(f'Resolving \'deleted by them\' conflicts: {deleted_by_them}')
    run(['git', 'rm', '--ignore-unmatch', '--'] + deleted_by_them)
    for path in deleted_by_them:
      unmerged_files.pop(path, None)

  if submodule_conflicts:
    log(f'Resolving submodule conflicts: {submodule_conflicts}')
    for path in submodule_conflicts:
      ls_files_out = get_out(['git', 'ls-files', '-u', '--', path])
      match = re.search(r'160000 ([a-f0-9]+) 3', ls_files_out)
      theirs_sha = match.group(1)
      run([
          'git', 'update-index', '--add', '--cacheinfo',
          f'160000,{theirs_sha},{path}'
      ])
      unmerged_files.pop(path, None)

  if other_conflicts:
    log(f'Cannot resolve conflicts: {other_conflicts}')
    return False

  return True


def get_cherry_pick_metadata(sha, title, pr_num):
  log_output = get_out(
      ['git', 'log', '-1', '--format=%ad%x00%an <%ae>%x00%b', sha])
  parts = log_output.split('\x00', 2)
  date = parts[0]
  author = parts[1]
  body = ''.join(parts[2:])
  body_section = f'{body}\n\n' if body else ''

  if pr_num is not None:
    msg = (f'Cherry pick PR #{pr_num}: {title}\n\n'
           f'Refer to original PR: #{pr_num}\n\n'
           f'{body_section}'
           f'(cherry picked from commit {sha})')
  else:
    msg = (f'Cherry pick commit {sha}: {title}\n\n'
           f'Refer to original commit: {sha}\n\n'
           f'{body_section}'
           f'(cherry picked from commit {sha})')

  return date, author, msg


def get_submodule_root_dirs():
  paths = get_out(
      ['git', 'config', '--file', '.gitmodules', '--get-regexp', 'path'])

  return sorted(
      {line.split(' ', 1)[1].split('/')[0] for line in paths.splitlines()})


def chromium_cherry_pick(previous_sha, sha, metadata, first_commit,
                         autoroll_file):
  """Temporarily reverts Cobalt changes to apply a Chromium cherry-pick.

  This function performs a "clean slate" cherry-pick by wiping the current
  working directory, checking out the pure Chromium state at `previous_sha`,
  and committing that as a temporary revert. It then applies the desired
  Chromium `sha` and re-applies Cobalt's modifications over the result.

  Args:
    previous_sha: The SHA of the clean Chromium base before Cobalt changes were
      applied.
    sha: The SHA of the Chromium commit to be cherry-picked.
    metadata: Metadata associated with the cherry-pick, passed to the final
      conflicting revert call.
    first_commit: boolean flag indicating if it's the first commit in the PR.
    autoroll_file: Path to the file that tracks autoroll progress.

  Returns:
    CommitStatus and unmerged_files.
  """
  log('Deleting everything...')
  roots = get_submodule_root_dirs()
  if roots:
    run(['rm', '-rf', '--'] + roots)
  run(['git', 'rm', '-rf', '--', '.'])
  run(['git', 'clean', '-ffdx'])

  log(f'Checking out clean Chromium state: {previous_sha}')
  run(['git', 'checkout', previous_sha, '--', '.'])

  log('Committing Cobalt revert...')
  run(['git', 'add', '--', '.'])
  run([
      'git', 'commit', '--no-verify', '-m',
      'CONFLICTED Chromium Cherry pick: Reverting Cobalt.'
  ])
  cobalt_revert_sha = get_out(['git', 'rev-parse', 'HEAD']).strip()

  log('Cherry picking Chromium...')
  run(['git', 'cherry-pick', sha])

  log('Reverting Cobalt revert...')
  return revert(cobalt_revert_sha, metadata, first_commit, autoroll_file)


def cherry_pick(sha, metadata, first_commit, autoroll_file):
  return apply_and_commit('cherry-pick', sha, metadata, first_commit,
                          autoroll_file)


def revert(sha, metadata, first_commit, autoroll_file):
  return apply_and_commit('revert', sha, metadata, first_commit, autoroll_file)


def apply_and_commit(action, sha, metadata, first_commit, autoroll_file):
  """Attempts to apply a single commit.

  Returns:
    CommitStatus: Enum indicating the outcome of the operation.
      - SUCCESS: Successfully committed.
      - CONFLICTED: Committed with conflicts.
      - SKIPPED: The commit was already present or no action was needed.
      - FAILED: The commit failed due to conflicts or other errors.
    unmerged_files: List of files with conflicts.
  """
  date, author, msg = metadata
  result = CommitStatus.SUCCESS
  unmerged_files = None

  # Apply
  try:
    run(['git', action, '--no-commit', sha])
  except subprocess.CalledProcessError:
    unmerged_files = get_unmerged_files()
    if resolve_conflicts(unmerged_files):
      unmerged_files = None
    else:
      unmerged_files = list(unmerged_files)

      if not first_commit:
        run(['git', 'reset', '--hard', 'HEAD'])
        return CommitStatus.FAILED, unmerged_files

      run(['git', 'add', '--'] + unmerged_files)
      msg = f'CONFLICTED {msg}'
      result = CommitStatus.CONFLICTED

  # Check if there are changes to commit
  if not get_out(['git', 'diff', '--cached', '--name-only']).strip():
    log('Commit skipped.')
    return CommitStatus.SKIPPED, unmerged_files

  # Update autoroll file
  with open(autoroll_file, 'w', encoding='utf-8') as f:
    if result == CommitStatus.CONFLICTED:
      f.write(f'CONFLICTED:{sha}\n')
    else:
      f.write(f'{sha}\n')
  run(['git', 'add', '--', autoroll_file])

  # Commit
  run([
      'git', 'commit', '--no-verify', f'--date={date}', f'--author={author}',
      '-m', msg
  ])
  return result, unmerged_files


def main():
  p = argparse.ArgumentParser()
  p.add_argument('--source-branch', required=True)
  p.add_argument('--target-branch', required=True)
  p.add_argument('--autoroll-file', required=True)
  p.add_argument('--max-commits', type=int, required=True)
  p.add_argument('--existing-pr-sha', required=True)
  args = p.parse_args()

  target_start = get_start_sha(args.target_branch, args.autoroll_file)
  autoroll_start = get_start_sha('HEAD', args.autoroll_file)

  if autoroll_start is None:
    log('Autoroll branch has an unresolved CONFLICTED commit.')
    return

  if args.existing_pr_sha:
    run(['git', 'fetch', 'origin', args.existing_pr_sha])
    commit_title = get_out(
        ['git', 'log', '-1', args.existing_pr_sha, '--format=%s']).strip()
    if commit_title.startswith('CONFLICTED'):
      log('Autoroll branch has a resolved CONFLICTED commit. '
          'Squash and merge before autoroll will continue.')
      return

  # Commits in source but not in target
  commits_to_target = get_commits(args.source_branch, target_start)
  # Commits in source but not in autoroll
  commits_to_autoroll = get_commits(args.source_branch, autoroll_start)
  # SHAs in source but not in autoroll
  shas_to_autoroll = {sha for sha, _, _ in commits_to_autoroll}

  commits_added = []
  previous_sha = target_start

  for sha, title, pr_num in commits_to_target:
    if len(commits_added) >= args.max_commits:
      log(f'Reached commit limit ({args.max_commits}).')
      break

    identifier = f'- #{pr_num}' if pr_num else f'- {sha}'

    # Skip if already in autoroll
    if sha not in shas_to_autoroll:
      commits_added.append(identifier)
      previous_sha = sha
      continue

    # Commit PR
    metadata = get_cherry_pick_metadata(sha, title, pr_num)
    first_commit = not commits_added

    if args.source_branch.startswith('chromium/'):
      result, unmerged_files = chromium_cherry_pick(previous_sha, sha, metadata,
                                                    first_commit,
                                                    args.autoroll_file)
    else:
      result, unmerged_files = cherry_pick(sha, metadata, first_commit,
                                           args.autoroll_file)

    if result in (CommitStatus.SUCCESS, CommitStatus.CONFLICTED):
      commits_added.append(identifier)
      if result == CommitStatus.CONFLICTED:
        commits_added.append('')
        commits_added.append('CONFLICTED files:')
        commits_added.append('```')
        commits_added.extend(unmerged_files)
        commits_added.append('```')

    if result in (CommitStatus.FAILED, CommitStatus.CONFLICTED):
      log(f'Reached CONFLICTED commit ({sha}).')
      break

    previous_sha = sha

  if commits_added:
    print('\n'.join(commits_added))


if __name__ == '__main__':
  main()
