#!/usr/bin/env python3
"""Library for autoroller scripts."""
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


def run(cmd, cwd=None):
  subprocess.run(cmd, check=True, stdout=sys.stderr, cwd=cwd)


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


def apply_and_commit(action, sha, metadata, first_commit, autoroll_metadata):
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
  autoroll_file, autoroll_sha = autoroll_metadata
  with open(autoroll_file, 'w', encoding='utf-8') as f:
    if result == CommitStatus.CONFLICTED:
      f.write(f'CONFLICTED:{autoroll_sha}\n')
    else:
      f.write(f'{autoroll_sha}\n')
  run(['git', 'add', '--', autoroll_file])

  # Commit
  run([
      'git', 'commit', '--no-verify', f'--date={date}', f'--author={author}',
      '-m', msg
  ])
  return result, unmerged_files
