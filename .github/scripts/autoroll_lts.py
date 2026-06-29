#!/usr/bin/env python3
"""Script to automatically roll LTS branch."""
import argparse
from collections import defaultdict
import enum
import re
import subprocess
import sys

_SKIP_LIST = {
    '27.lts': [],
    'staging': [],
}


@enum.unique
class CherryPickStatus(enum.Enum):
  """Represents the outcome of a cherry-pick attempt."""
  SUCCESS = 'success'  # Successfully cherry-picked and committed.
  CONFLICTED = 'conflicted'  # Cherry-picked and committed with conflicts.
  SKIPPED = 'skipped'  # The commit was already present or no action was needed.
  FAILED = 'failed'  # The cherry-pick failed due to conflicts or other errors.


def run(cmd):
  subprocess.run(cmd, check=True, stdout=sys.stderr)


def get_out(cmd):
  res = subprocess.run(cmd, capture_output=True, text=True, check=True)
  return res.stdout


def get_commits(source, target, start):
  """Returns a list of commit lines in chronological order.

  Retrieves commits that are present on the source branch but not on the target
  branch, optionally starting from a specific commit.
  """
  cmd = [
      'git', 'rev-list', '--oneline', '--no-abbrev-commit', '--reverse', source,
      f'^{target}'
  ]
  if start:
    cmd.append(f'{start}^..{source}')
  return get_out(cmd).splitlines()


def get_change_id_set(branch, exclude_branch, identifier_type):
  """Returns a set of change IDs that have been merged into the branch.

  Excludes any changes that exist on the exclude_branch. Automatically accounts
  for reverted cherry-picks.
  """
  if identifier_type == 'pr':
    pattern = r'Cherry pick PR #(\d+)'
  else:
    pattern = r'Cherry pick commit ([0-9a-fA-F]{4,40})'

  change_ids = set()
  conflicted_change_ids = set()
  cmd = ['git', 'log', '--reverse', '--format=%s', branch, f'^{exclude_branch}']
  subjects = get_out(cmd).splitlines()
  for subject in subjects:
    match = re.search(fr'^(Revert\s+[\'"]?)?(Conflicted )?{pattern}:', subject)
    if match:
      revert, conflicted, change_id = match.groups()
      if revert:
        if conflicted:
          conflicted_change_ids.discard(change_id)
        else:
          change_ids.discard(change_id)
      elif conflicted:
        conflicted_change_ids.add(change_id)
      else:
        change_ids.add(change_id)
  return change_ids, conflicted_change_ids


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
      print(f'Warning: Malformed line (missing tab): {line}', file=sys.stderr)
      continue
    metadata, path = parts
    meta_parts = metadata.split()
    if len(meta_parts) < 3:
      print(f'Warning: Malformed metadata: {metadata}', file=sys.stderr)
      continue
    _, _, stage = meta_parts[:3]
    stage_name = stage_map.get(stage, stage)
    files[path].add(stage_name)
  return files


def resolve_conflicts(unmerged):
  """Attempts to resolve conflicts automatically.

  Returns:
    bool: True if all conflicts were resolved, False otherwise.
  """
  deleted_by_us = []
  other_conflicts = []
  for path, stages in unmerged.items():
    if 'theirs' in stages and 'ours' not in stages:
      deleted_by_us.append(path)
    else:
      other_conflicts.append(path)

  if other_conflicts:
    print(
        f'Cannot resolve conflicts autonomously. Other conflicts: '
        f'{other_conflicts}',
        file=sys.stderr)
    return False

  if deleted_by_us:
    print(
        f"Resolving 'deleted by us' conflicts: {deleted_by_us}",
        file=sys.stderr)
    run(['git', 'rm', '--'] + deleted_by_us)
    return True

  return False


def cherry_pick(sha, num, title, first_cherry_pick):
  """Attempts to cherry-pick a single commit.

  Returns:
    CherryPickStatus: Enum indicating the outcome of the operation.
      - SUCCESS: Successfully cherry-picked and committed.
      - CONFLICTED: Cherry-picked and committed with conflicts.
      - SKIPPED: The commit was already present or no action was needed.
      - FAILED: The cherry-pick failed due to conflicts or other errors.
  """
  log_output = get_out(
      ['git', 'log', '-1', '--format=%ad%x00%an <%ae>%x00%b', sha])
  parts = log_output.split('\x00', 2)
  date = parts[0]
  author = parts[1]
  body = parts[2] if len(parts) > 2 else ''

  body_section = f'{body}\n\n' if body else ''
  if num is not None:
    msg = (f'Cherry pick PR #{num}: {title}\n\n'
           f'Refer to original PR: #{num}\n\n'
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
  try:
    run(cmd + [sha])
  except subprocess.CalledProcessError:
    unmerged = get_unmerged_files()
    if not resolve_conflicts(unmerged):
      if not first_cherry_pick:
        run(['git', 'reset', '--hard', 'HEAD'])
        return CherryPickStatus.FAILED

      msg = f'Conflicted {msg}'
      run(['git', 'add', '--sparse'] + list(unmerged))
      result = CherryPickStatus.CONFLICTED

  # Check if there are changes to commit.
  res = subprocess.run(['git', 'diff', '--quiet', '--cached'], check=False)
  if res.returncode == 0:
    print('Cherry pick skipped.', file=sys.stderr)
    return CherryPickStatus.SKIPPED

  cmd = [
      'git', 'commit', '--no-verify', f'--author={author}', f'--date={date}',
      '-m', msg
  ]
  run(cmd)
  return result


def main():
  p = argparse.ArgumentParser()
  p.add_argument('--source-branch', required=True)
  p.add_argument('--target-branch', required=True)
  p.add_argument('--start-commit')
  p.add_argument('--max-commits', type=int, default=1000)
  p.add_argument('--identifier-type', required=True)
  args = p.parse_args()

  # All cherry picked changes in target branch since the branch point.
  target_change_ids, _ = get_change_id_set(args.target_branch,
                                           args.source_branch,
                                           args.identifier_type)
  # All cherry picked changes in autoroll branch since the branch point.
  autoroll_change_ids, autoroll_conflicted_change_ids = get_change_id_set(
      'HEAD', args.source_branch, args.identifier_type)
  commits_added = []

  # All commits in source branch and not in target branch (all commits
  # since the branch point).
  for line in get_commits(args.source_branch, args.target_branch,
                          args.start_commit):
    if len(commits_added) >= args.max_commits:
      print(f"Reached commit limit ({args.max_commits}).", file=sys.stderr)
      break

    match = re.search(r'^(\w+) (.*?)(?: \(#(\d+)\))?$', line)
    if match:
      sha, title, pr_num = match.groups()

      # Skip if in skip list.
      if sha in _SKIP_LIST.get(args.target_branch, []):
        continue

      if args.identifier_type == 'pr':
        change_id = pr_num
        prefix = '#'
      else:
        change_id = sha
        prefix = ''

      # Skip if in target branch.
      if change_id in target_change_ids:
        continue

      # Skip if in autoroll branch.
      if change_id in autoroll_change_ids:
        commits_added.append(f'- {prefix}{change_id}')
        continue

      # Break if in autoroll branch as conflicted.
      if change_id in autoroll_conflicted_change_ids:
        print(f"Reached conflicted cherry pick ({sha}).", file=sys.stderr)
        break

      # Cherry pick PR.
      first_cherry_pick = not commits_added
      result = cherry_pick(sha, pr_num, title, first_cherry_pick)
      if result == CherryPickStatus.CONFLICTED:
        commits_added.append('CONFLICTED:')
      if result in (CherryPickStatus.SUCCESS, CherryPickStatus.CONFLICTED):
        autoroll_change_ids.add(change_id)
        commits_added.append(f'- {prefix}{change_id}')
      if result in (CherryPickStatus.FAILED, CherryPickStatus.CONFLICTED):
        print(f"Reached conflicted cherry pick ({sha}).", file=sys.stderr)
        break

  if commits_added:
    print('\n'.join(commits_added))


if __name__ == '__main__':
  main()
