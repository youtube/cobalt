#!/usr/bin/env python3
"""Script to automatically roll LTS branch."""
import argparse
import re
import subprocess


def get_out(cmd):
  res = subprocess.run(cmd, capture_output=True, text=True, check=True)
  return res.stdout.strip()


def get_commits(start, target):
  cmd = [
      'git', 'rev-list', '--oneline', '--reverse', 'main', f'^{target}',
      f'{start}^..main'
  ]
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
  p.add_argument('--start-commit', required=True)
  args = p.parse_args()

  links = []
  for line in get_commits(args.start_commit, args.target_branch):
    match = re.match(r'^(\w+) (.*) \(#(\d+)\)$', line)
    if match:
      sha, title, num = match.groups()
      if not get_out([
          'git', 'log', '-1', f'--grep=^Cherry pick PR #{num}:',
          args.target_branch
      ]):
        cherry_pick(sha)
        commit_pick(sha, num, title)
        links.append(f'- #{num}')

  if links:
    print('\n'.join(links))


if __name__ == '__main__':
  main()
