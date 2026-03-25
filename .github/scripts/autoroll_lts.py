#!/usr/bin/env python3
"""Script to automatically roll LTS branch."""
import argparse
import re
import subprocess

_SKIP_LIST = {
    '27.lts': [
        # Reorders deleted BUILD_STATUS.md file (#9476).
        '7e6524981fdd6ab3c87bc55785343d40116a05e5',
        # Reorders deleted BUILD_STATUS.md file (#9508).
        'adda40a0d3b08b9302f441e76eef0391c70e0462',
        # Modifies deleted workflow trigger files (#9473).
        'b24037232cbc7a74bf01dbc4c93dbe9701328b5e',
        # Skia import commits, already applied in #9625 (#9624).
        'b77e86a96022541455c239778a4a62462d790c73',
        '8ed51696a04da8b51b82d6540b3b314347c43794',
    ],
}


def get_out(cmd):
  res = subprocess.run(cmd, capture_output=True, text=True, check=True)
  return res.stdout.strip()


def get_commits(origin, target, start):
  cmd = ['git', 'rev-list', '--oneline', '--reverse', origin, f'^{target}']
  if start:
    cmd.append(f'{start}^..{origin}')
  return get_out(cmd).splitlines()


def cherry_pick(sha):
  ps = get_out(['git', 'show', '-s', '--format=%P', sha]).split()
  cmd = ['git', 'cherry-pick', '--no-commit']
  if len(ps) > 1:
    cmd.append('--mainline=1')
  subprocess.run(cmd + [sha], check=True)


def commit_pick(sha, num, title):
  info = get_out(['git', 'log', '-1', '--format=%an <%ae>%n%ad%n%b', sha])
  auth, date, body = info.split('\n', 2)
  msg = f'Cherry pick PR #{num}: {title}\n\n'
  msg += f'Refer to original PR: #{num}\n\n{body}'
  cmd = [
      'git', 'commit', '--no-verify', f'--author={auth}', f'--date={date}',
      '-m', msg
  ]
  subprocess.run(cmd, check=True)


def main():
  p = argparse.ArgumentParser()
  p.add_argument('--target-branch', required=True)
  p.add_argument('--start-commit')
  p.add_argument('--origin-branch', default='main')
  args = p.parse_args()

  links = []
  for line in get_commits(args.origin_branch, args.target_branch,
                          args.start_commit):
    match = re.match(r'^(\w+) (.*) \(#(\d+)\)$', line)
    if match:
      sha, title, num = match.groups()
      if any(
          skip_sha.startswith(sha)
          for skip_sha in _SKIP_LIST.get(args.target_branch, [])):
        continue

      # Skip if the PR is already in the target branch.
      if get_out([
          'git', 'log', '-1', f'--grep=^Cherry pick PR #{num}:',
          args.target_branch
      ]):
        continue

      # If the PR is not on the current (autoroll) branch, cherry-pick it.
      if not get_out(
          ['git', 'log', '-1', f'--grep=^Cherry pick PR #{num}:', 'HEAD']):
        cherry_pick(sha)
        commit_pick(sha, num, title)

      links.append(f'- #{num}')

  if links:
    print('\n'.join(links))


if __name__ == '__main__':
  main()
