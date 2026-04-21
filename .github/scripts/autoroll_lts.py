#!/usr/bin/env python3
"""Script to automatically roll LTS branch."""
import argparse
import re
import subprocess
import sys

_SKIP_LIST = {
    '27.lts': [
        # Reorders deleted BUILD_STATUS.md file (#9476, #9508).
        '7e6524981fdd6ab3c87bc55785343d40116a05e5',
        'adda40a0d3b08b9302f441e76eef0391c70e0462',
        # Modifies deleted workflow trigger files (#9473).
        'b24037232cbc7a74bf01dbc4c93dbe9701328b5e',
        # Skia import commits, already applied in #9625 (#9624).
        'b77e86a96022541455c239778a4a62462d790c73',
        '8ed51696a04da8b51b82d6540b3b314347c43794',
        # Change to deleted workflow file (#9670, #9934, #10080).
        'f69b1d1e21f3340d9c963846ed4e1cbef8fa2fb9',
        '478e5c52cf4872407ed855a100165e93b02d9eee',
        '00531389e019a835f49fb3bf56364b244a0d3acd',
    ],
}


def get_out(cmd):
  res = subprocess.run(cmd, capture_output=True, text=True, check=True)
  return res.stdout


def get_commits(origin, target, start):
  cmd = ['git', 'rev-list', '--oneline', '--reverse', origin, f'^{target}']
  if start:
    cmd.append(f'{start}^..{origin}')
  return get_out(cmd).splitlines()


def get_pr_set(branch, exclude_branch):
  prs = set()
  cmd = ['git', 'log', '--reverse', '--format=%s', branch, f'^{exclude_branch}']
  subjects = get_out(cmd).splitlines()
  for subject in subjects:
    match = re.match(r'^(Revert\s+"?)?Cherry pick PR #(\d+):', subject)
    if match:
      revert, pr_num = match.groups()
      if revert:
        prs.discard(pr_num)
      else:
        prs.add(pr_num)
  return prs


def cherry_pick(sha, num, title):
  log_output = get_out(
      ['git', 'log', '-1', '--format=%ad%x00%an <%ae>%x00%b', sha])
  parts = log_output.split('\x00', 2)
  date = parts[0]
  author = parts[1]
  body = parts[2] if len(parts) > 2 else ''

  msg = f'Cherry pick PR #{num}: {title}\n\n'
  msg += f'Refer to original PR: #{num}\n\n'
  if body:
    msg += f'{body}\n\n'
  msg += f'(cherry picked from commit {sha})'

  cmd = ['git', 'cherry-pick', '--no-commit']
  ps = get_out(['git', 'show', '-s', '--format=%P', sha]).strip().split()
  if len(ps) > 1:
    cmd.append('--mainline=1')
  subprocess.run(cmd + [sha], check=True, stdout=sys.stderr)

  cmd = [
      'git', 'commit', '--no-verify', f'--author={author}', f'--date={date}',
      '-m', msg
  ]
  subprocess.run(cmd, check=True, stdout=sys.stderr)


def main():
  p = argparse.ArgumentParser()
  p.add_argument('--target-branch', required=True)
  p.add_argument('--start-commit')
  p.add_argument('--origin-branch', default='main')
  p.add_argument('--max-commits', type=int, default=1000)
  args = p.parse_args()

  links = []
  target_prs = get_pr_set(args.target_branch, args.origin_branch)
  autoroll_prs = get_pr_set('HEAD', args.origin_branch)

  # Get the number of unmerged commits on the autoroll branch.
  commits_added = len(autoroll_prs - target_prs)

  for line in get_commits(args.origin_branch, args.target_branch,
                          args.start_commit):
    if commits_added >= args.max_commits:
      print(f"Reached commit limit ({args.max_commits}).", file=sys.stderr)
      break

    match = re.match(r'^(\w+) (.*) \(#(\d+)\)$', line)
    if match:
      sha, title, pr_num = match.groups()
      if any(
          skip_sha.startswith(sha)
          for skip_sha in _SKIP_LIST.get(args.target_branch, [])):
        continue

      # Skip if the PR is already in the target branch.
      if pr_num in target_prs:
        continue

      # If the PR is not on the current (autoroll) branch, cherry-pick it.
      if pr_num not in autoroll_prs:
        cherry_pick(sha, pr_num, title)
        autoroll_prs.add(pr_num)
        commits_added += 1

      links.append(f'- #{pr_num}')

  if links:
    print('\n'.join(links))


if __name__ == '__main__':
  main()
