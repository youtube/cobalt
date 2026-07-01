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
REPO_OWNER_PATH = os.environ.get('GITHUB_REPOSITORY', 'youtube/cobalt')
REPO_URL = f'https://github.com/{REPO_OWNER_PATH}.git'

# Hardcoded GitHub App ID for cobalt-github-releaser
APP_ID = '3203510'


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

  print('>> Authenticating using GitHub App (cobalt-github-releaser).')
  print('>> Please paste the content of the App Private Key (.pem file):')
  print('>> (Paste the full block including BEGIN/END headers)')

  key_lines = []
  while True:
    try:
      line = input()
    except EOFError:
      break
    key_lines.append(line)
    if '-----END' in line:
      break

  key_content = '\n'.join(key_lines).strip()
  if not key_content:
    print('>> Error: Private key content cannot be empty.')
    sys.exit(1)

  # Create a secure temporary file to write the key content to,
  # so openssl can read it.
  temp_fd, temp_path = tempfile.mkstemp(suffix='.pem')
  try:
    with os.fdopen(temp_fd, 'w') as f:
      f.write(key_content)

    return get_installation_access_token(APP_ID, temp_path)
  finally:
    # Overwrite the temp file with zeros (shred it) to protect the
    # key content before deleting.
    if os.path.exists(temp_path):
      try:
        with open(temp_path, 'wb') as f:
          f.write(b'\0' * len(key_content))
      except IOError:
        pass
      os.unlink(temp_path)


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
        'git', '-c', 'credential.helper=', '-c',
        'credential.helper=!gh auth git-credential', 'fetch', REPO_URL,
        f'+{target}:refs/remotes/origin/{target}',
        f'+{head}:refs/remotes/origin/{head}'
    ],
                   env=env,
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
  head = pr['headRefName']

  try:
    subprocess.run([
        'gh', 'pr', 'close',
        str(pr_num), '--repo', REPO_OWNER_PATH, '--comment', comment,
        '--delete-branch'
    ],
                   env=env,
                   capture_output=True,
                   text=True,
                   check=True)
    print('>> PR closed and branch deleted successfully.')
  except subprocess.CalledProcessError as e:
    # If the PR was successfully closed but deleting the branch failed,
    # gh prints the PR closure confirmation to stdout, and the branch deletion
    # failure to stderr.
    if ('Closed pull request' in e.stdout or
        'failed to delete remote branch' in e.stderr):
      print('>> PR closed successfully.')
      print(f'>> Warning: Failed to delete remote branch {head} (it may have '
            f'already been deleted or you may lack permissions).')
    else:
      print(f'>> Error closing PR: {e.stderr}')
      sys.exit(1)


def main():
  target = get_target_branch()
  token = get_github_token()
  env = {**os.environ, 'GITHUB_TOKEN': token}

  pr = find_open_autoroll_pr(target, env)
  merge_and_push(target, pr, env)
  close_pr(pr, target, env)


if __name__ == '__main__':
  main()
