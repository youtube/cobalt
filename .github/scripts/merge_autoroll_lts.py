#!/usr/bin/env python3
"""Script to merge LTS autoroll PRs.

Rebases the changes and pushes directly to the target branch.
"""

import base64
import json
import os
import shutil
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.request

# Global configuration for target repository
REPO_OWNER_PATH = 'youtube/cobalt_sandbox'
REPO_URL = f'https://github.com/{REPO_OWNER_PATH}.git'


def get_target_branch():
  if len(sys.argv) < 2:
    print('>> Usage: merge_autoroll_lts.py <target_branch>')
    sys.exit(1)
  return sys.argv[1]


def generate_jwt(app_id, private_key_path):

  def b64url(data):
    return base64.urlsafe_b64encode(data).rstrip(b'=').decode('utf-8')

  header = json.dumps({'alg': 'RS256', 'typ': 'JWT'}).encode('utf-8')
  now = int(time.time())
  payload = json.dumps({
      'iat': now - 60,
      'exp': now + 600,
      'iss': app_id
  }).encode('utf-8')

  signing_input = f'{b64url(header)}.{b64url(payload)}'.encode('utf-8')

  try:
    with subprocess.Popen(
        ['openssl', 'dgst', '-sha256', '-sign', private_key_path],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE) as proc:
      signature, err = proc.communicate(input=signing_input)
  except FileNotFoundError:
    print('>> Error: openssl command not found. Please install openssl.')
    sys.exit(1)

  if proc.returncode != 0:
    print(f'>> Error: Failed to sign JWT with openssl: {err.decode()}')
    sys.exit(1)

  return f'{b64url(header)}.{b64url(payload)}.{b64url(signature)}'


def get_installation_access_token(app_id, private_key_path):
  jwt = generate_jwt(app_id, private_key_path)

  # 1. Get installation ID for the repository
  req = urllib.request.Request(
      f'https://api.github.com/repos/{REPO_OWNER_PATH}/installation',
      headers={
          'Authorization': f'Bearer {jwt}',
          'Accept': 'application/vnd.github+json',
          'User-Agent': 'cobalt-gardener-script'
      })
  try:
    with urllib.request.urlopen(req) as res:
      inst_info = json.loads(res.read().decode())
      installation_id = inst_info['id']
  except urllib.error.HTTPError as e:
    print(f'>> Error retrieving app installation: {e.read().decode()}')
    sys.exit(1)

  # 2. Get installation access token
  url = (f'https://api.github.com/app/installations/{installation_id}/'
         f'access_tokens')
  req_tok = urllib.request.Request(
      url,
      headers={
          'Authorization': f'Bearer {jwt}',
          'Accept': 'application/vnd.github+json',
          'User-Agent': 'cobalt-gardener-script'
      },
      method='POST')
  try:
    with urllib.request.urlopen(req_tok) as res:
      tok_info = json.loads(res.read().decode())
      return tok_info['token']
  except urllib.error.HTTPError as e:
    print(f'>> Error generating installation access token: {e.read().decode()}')
    sys.exit(1)


def get_github_token():
  # Check if a direct token is already provided in GITHUB_TOKEN
  token = os.environ.get('GITHUB_TOKEN')
  if token:
    return token

  # Otherwise, look for GitHub App credentials
  app_id = os.environ.get('GITHUB_APP_ID')
  private_key_path = os.environ.get('GITHUB_APP_PRIVATE_KEY_PATH')

  if not app_id or not private_key_path:
    print('>> Authenticating using GitHub App (cobalt-github-releaser).')
    if not app_id:
      app_id = input('>> Enter GitHub App ID: ').strip()
    if not private_key_path:
      private_key_path = input(
          '>> Enter path to GitHub App Private Key (.pem): ').strip()

    if not app_id or not private_key_path:
      print('>> Error: Both App ID and Private Key Path are required.')
      sys.exit(1)

  if not os.path.exists(private_key_path):
    print(f'>> Error: Private key file not found at {private_key_path}')
    sys.exit(1)

  return get_installation_access_token(app_id, private_key_path)


def find_open_autoroll_pr(target, env):
  try:
    prs_json = subprocess.check_output(
        [
            'gh', 'pr', 'list', '--repo', REPO_OWNER_PATH, '--state', 'open',
            '--json', 'number,headRefName,baseRefName,title'
        ],
        env=env,
        text=True,
    )
    prs = json.loads(prs_json)
  except subprocess.CalledProcessError as e:
    print(f'>> Error listing PRs: {e}')
    sys.exit(1)

  pr = next((p for p in prs if p['baseRefName'] == target and
             p['headRefName'].startswith('autoroll-')), None)
  if not pr:
    print(f'>> No open autoroll PR found targeting {target}')
    sys.exit(1)

  if pr['title'].upper().startswith('CONFLICTED:'):
    pr_num = pr['number']
    pr_title = pr['title']
    print(f'>> Error: Found matching autoroll PR #{pr_num} but it is marked '
          f'as CONFLICTED ("{pr_title}"). Please resolve manually.')
    sys.exit(1)

  return pr


def merge_and_push(target, pr, env):
  head = pr['headRefName']
  pr_number = pr['number']
  print(f'>> Found PR #{pr_number}: {head} -> {target}')

  tmpdir = tempfile.mkdtemp()
  worktree_added = False
  try:
    print('>> Fetching branches...')
    subprocess.run([
        'git', 'fetch', REPO_URL, f'+{target}:refs/remotes/origin/{target}',
        f'+{head}:refs/remotes/origin/{head}'
    ],
                   check=True)

    print('>> Creating temporary worktree...')
    subprocess.run(
        ['git', 'worktree', 'add', '--no-checkout', tmpdir, f'origin/{head}'],
        check=True)
    worktree_added = True

    def git(*args, check=True):
      return subprocess.run(['git', '-C', tmpdir] + list(args), check=check)

    git('sparse-checkout', 'init', '--cone')
    git('sparse-checkout', 'set', '.github')
    git('checkout')

    print('>> Rebasing...')
    rebase_res = git('rebase', f'origin/{target}', check=False)
    if rebase_res.returncode != 0:
      print('>> Rebase failed due to conflicts. Please resolve manually.')
      sys.exit(1)

    print(f'>> Pushing to {target}...')
    subprocess.run([
        'git', '-C', tmpdir, '-c', 'credential.helper=', '-c',
        'credential.helper=!gh auth git-credential', 'push', REPO_URL,
        f'HEAD:{target}'
    ],
                   env=env,
                   check=True)

  finally:
    print('>> Cleaning up temporary worktree...')
    if worktree_added:
      subprocess.run(['git', 'worktree', 'remove', '--force', tmpdir],
                     check=False)
    if os.path.exists(tmpdir):
      shutil.rmtree(tmpdir, ignore_errors=True)


def close_pr(pr, target, env):
  print('>> Closing PR...')
  pr_num = pr['number']
  comment = (
      f'Closing this PR because the changes have been rebased and pushed '
      f'directly to {target}.')
  subprocess.run([
      'gh', 'pr', 'close',
      str(pr_num), '--repo', REPO_OWNER_PATH, '--comment', comment,
      '--delete-branch'
  ],
                 env=env,
                 check=True)
  print('>> Successfully merged and closed PR!')


def main():
  target = get_target_branch()
  token = get_github_token()
  env = {**os.environ, 'GITHUB_TOKEN': token}

  pr = find_open_autoroll_pr(target, env)
  merge_and_push(target, pr, env)
  close_pr(pr, target, env)


if __name__ == '__main__':
  main()
