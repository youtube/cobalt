#!/usr/bin/env python3
"""Script to automatically roll LTS branch."""
import argparse
from collections import defaultdict
import enum
import re
import subprocess
import sys

_AUTOROLL_FILE = '.github/AUTOROLL'


@enum.unique
class CherryPickStatus(enum.Enum):
  """Represents the outcome of a cherry-pick attempt."""
  SUCCESS = 'success'  # Successfully cherry-picked and committed.
  CONFLICTED = 'conflicted'  # Cherry-picked and committed with conflicts.
  SKIPPED = 'skipped'  # The commit was already present or no action was needed.
  FAILED = 'failed'  # The cherry-pick failed due to conflicts or other errors.


def log(msg):
  print(msg, file=sys.stderr)


def run(cmd):
  subprocess.run(cmd, check=True, stdout=sys.stderr)


def get_out(cmd):
  res = subprocess.run(cmd, capture_output=True, text=True, check=True)
  return res.stdout


def get_start_sha(branch):
  """Returns an autoroll start SHA or None if CONFLICTED."""
  start = get_out(['git', 'show', f'{branch}:{_AUTOROLL_FILE}']).strip()

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
      f'{start}^..{branch}'
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
  deleted_by_us = []
  other_conflicts = []
  for path, stages in unmerged_files.items():
    if 'theirs' in stages and 'ours' not in stages:
      deleted_by_us.append(path)
    else:
      other_conflicts.append(path)

  if other_conflicts:
    log(f'Cannot resolve conflicts: {other_conflicts}')
    return False

  if deleted_by_us:
    log(f'Resolving \'deleted by us\' conflicts: {deleted_by_us}')
    run(['git', 'rm', '--'] + deleted_by_us)
    return True

  return False


def cherry_pick(sha, title, pr_num, first_cherry_pick):
  """Attempts to cherry-pick a single commit.

  Returns:
    CherryPickStatus: Enum indicating the outcome of the operation.
      - SUCCESS: Successfully cherry-picked and committed.
      - CONFLICTED: Cherry-picked and committed with conflicts.
      - SKIPPED: The commit was already present or no action was needed.
      - FAILED: The cherry-pick failed due to conflicts or other errors.
    unmerged_files: List of files with conflicts.
  """
  log_output = get_out(
      ['git', 'log', '-1', '--format=%ad%x00%an <%ae>%x00%b', sha])
  parts = log_output.split('\x00', 2)
  date = parts[0]
  author = parts[1]
  body = parts[2] if len(parts) > 2 else ''

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

  cmd = ['git', 'cherry-pick', '--no-commit']
  ps = get_out(['git', 'show', '-s', '--format=%P', sha]).strip().split()
  if len(ps) > 1:
    cmd.append('--mainline=1')

  result = CherryPickStatus.SUCCESS
  unmerged_files = None
  try:
    run(cmd + [sha])
  except subprocess.CalledProcessError:
    unmerged_files = get_unmerged_files()
    if resolve_conflicts(unmerged_files):
      unmerged_files = None
    else:
      unmerged_files = list(unmerged_files)

      if not first_cherry_pick:
        run(['git', 'reset', '--hard', 'HEAD'])
        return CherryPickStatus.FAILED, unmerged_files

      run(['git', 'add', '--sparse'] + unmerged_files)
      msg = f'CONFLICTED {msg}'
      result = CherryPickStatus.CONFLICTED

  # Check if there are changes to commit
  cmd = ['git', 'diff', '--quiet', '--cached']
  res = subprocess.run(cmd, check=False)
  if res.returncode == 0:
    log('Cherry pick skipped.')
    return CherryPickStatus.SKIPPED, unmerged_files

  # Update autoroll file
  with open(_AUTOROLL_FILE, 'w', encoding='utf-8') as f:
    if result == CherryPickStatus.CONFLICTED:
      f.write(f'CONFLICTED:{sha}\n')
    else:
      f.write(f'{sha}\n')
  run(['git', 'add', '--sparse', _AUTOROLL_FILE])

  cmd = [
      'git', 'commit', '--no-verify', f'--author={author}', f'--date={date}',
      '-m', msg
  ]
  run(cmd)
  return result, unmerged_files


def main():
  p = argparse.ArgumentParser()
  p.add_argument('--source-branch', required=True)
  p.add_argument('--target-branch', required=True)
  p.add_argument('--max-commits', type=int, required=True)
  args = p.parse_args()

  target_start = get_start_sha(args.target_branch)
  autoroll_start = get_start_sha('HEAD')
  if autoroll_start is None:
    log('Autoroll branch has an unresolved CONFLICTED cherry pick.')
    return

  # Commits in source but not in target
  commits_to_target = get_commits(args.source_branch, target_start)
  # Commits in source but not in autoroll
  commits_to_autoroll = get_commits(args.source_branch, autoroll_start)
  # SHAs in source but not in autoroll
  shas_to_autoroll = {sha for sha, _, _ in commits_to_autoroll}

  commits_added = []

  for sha, title, pr_num in commits_to_target:
    if len(commits_added) >= args.max_commits:
      log(f'Reached commit limit ({args.max_commits}).')
      break

    identifier = f'- #{pr_num}' if pr_num else f'- {sha}'

    # Skip if already in autoroll
    if sha not in shas_to_autoroll:
      commits_added.append(identifier)
      continue

    # Cherry pick PR
    first_cherry_pick = not commits_added
    result, unmerged_files = cherry_pick(sha, title, pr_num, first_cherry_pick)

    if result in (CherryPickStatus.SUCCESS, CherryPickStatus.CONFLICTED):
      commits_added.append(identifier)
      if result == CherryPickStatus.CONFLICTED:
        commits_added.append('')
        commits_added.append('CONFLICTED files:')
        commits_added.append('```')
        commits_added.extend(unmerged_files)
        commits_added.append('```')

    if result in (CherryPickStatus.FAILED, CherryPickStatus.CONFLICTED):
      log(f'Reached CONFLICTED cherry pick ({sha}).')
      break

  if commits_added:
    print('\n'.join(commits_added))


if __name__ == '__main__':
  main()
