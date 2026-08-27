#!/usr/bin/env python3
# Copyright 2026 The Cobalt Authors. All Rights Reserved.
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
"""Script to merge autoroll PRs.

Rebases the changes and pushes directly to the target branch.
"""

import argparse
import base64
import json
import os
import shlex
import shutil
import subprocess
import sys
import tempfile
import time
import urllib.request

# Global configuration for target repository.
REPO_OWNER_PATH = os.environ.get('GITHUB_REPOSITORY', 'youtube/cobalt_sandbox')
REPO_URL = f'https://github.com/{REPO_OWNER_PATH}.git'

# GitHub App IDs for cobalt-github-releaser.
APP_ID = '3203510'
APP_INSTALLATION_ID = '119484904'


def log(msg):
  print('>> ' + msg)


def run_cmd(cmd, check=True, **kwargs):
  cmd_str = shlex.join(str(x) for x in cmd)
  print(f'+ {cmd_str}')
  return subprocess.run(cmd, check=check, **kwargs).stdout


def git(*args, authenticated=False, **kwargs):
  auth_args = []
  if authenticated:
    auth_args = [
        '-c', 'credential.helper=',
        '-c', 'credential.helper=!gh auth git-credential'
    ]
  return run_cmd(['git'] + auth_args + list(args), **kwargs)


def gh(*args, **kwargs):
  return run_cmd(['gh'] + list(args), **kwargs)


def openssl(*args, stdin=None, **kwargs):
  return run_cmd(['openssl'] + list(args), input=stdin, **kwargs)


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

  signature = openssl(
      'dgst',
      '-sha256',
      '-sign',
      private_key_path,
      stdin=signing_input,
      stdout=subprocess.PIPE)

  return f'{b64url(header)}.{b64url(payload)}.{b64url(signature)}'


def get_installation_access_token(app_id, private_key_path):
  jwt = generate_jwt(app_id, private_key_path)

  url = (f'https://api.github.com/app/installations/{APP_INSTALLATION_ID}/'
         f'access_tokens')
  req_tok = urllib.request.Request(
      url,
      headers={
          'Authorization': f'Bearer {jwt}',
          'Accept': 'application/vnd.github+json',
          'User-Agent': 'cobalt-github-releaser'
      },
      method='POST')
  with urllib.request.urlopen(req_tok) as res:
    tok_info = json.loads(res.read().decode())
    return tok_info['token']


def read_private_key():
  log('Paste the full secret from go/cram-secret >')

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
    log('Error: Private key content cannot be empty.')
    sys.exit(1)
  return key_content


def find_open_autoroll_pr(source, target, env):
  head_branch = f'autoroll-{source}-to-{target}'
  prs_json = gh(
      'pr',
      'list',
      '--repo',
      REPO_OWNER_PATH,
      '--state',
      'open',
      '--head',
      head_branch,
      '--json',
      'number,headRefName,baseRefName,title,url',
      capture_output=True,
      text=True,
      env=env)
  prs = json.loads(prs_json)

  pr = prs[0] if prs else None
  if not pr:
    log(f'No open autoroll PR found for {head_branch} -> {target}')
    sys.exit(0)

  if pr['title'].upper().startswith('CONFLICTED'):
    pr_num = pr['number']
    pr_title = pr['title']
    log(f'Error: Found matching autoroll PR #{pr_num} but it is marked '
        f'as CONFLICTED ("{pr_title}"). Please resolve manually.')
    sys.exit(1)

  return pr


def has_conflicts(filepath):
  if not os.path.exists(filepath):
    return False
  try:
    with open(filepath, 'r', encoding='utf-8') as f:
      for line in f:
        if line.startswith('CONFLICTED:') or line.startswith('<<<<<<<'):
          return True
  except Exception as e:
    log(f'Error reading {filepath}: {e}')
  return False


def rebase_and_push(target, pr, env):
  head = pr['headRefName']
  pr_number = pr['number']
  log(f'Found PR #{pr_number}: {head} -> {target}')

  tmpdir = tempfile.mkdtemp()
  worktree_added = False
  orig_cwd = os.getcwd()
  try:
    log('Fetching branches...')
    git('fetch',
        REPO_URL,
        f'+{target}:refs/remotes/origin/{target}',
        f'+{head}:refs/remotes/origin/{head}',
        authenticated=True,
        env=env)

    log('Creating temporary worktree...')
    git('worktree',
        'add',
        '--no-checkout',
        tmpdir,
        f'origin/{head}')
    worktree_added = True

    os.chdir(tmpdir)

    git('sparse-checkout', 'init', '--cone')
    git('sparse-checkout', 'set', '.github')
    git('checkout')

    autoroll_files = ['.github/AUTOROLL', '.github/AUTOROLL_CHROMIUM']
    for f in autoroll_files:
      if has_conflicts(f):
        log(f'Error: Autoroll file {f} contains conflict markers or is marked CONFLICTED.')
        sys.exit(1)

    log('Rebasing...')
    git('rebase', f'origin/{target}')

    log(f'Pushing {target}...')
    log(f"PR URL: {pr['url']}")
    response = input('Do you want to merge this PR? Type "yes" to confirm: ')
    if response.strip().lower() != 'yes':
      log('Aborting push.')
      return

    git('push',
        REPO_URL,
        f'HEAD:{target}',
        authenticated=True,
        env=env)
  finally:
    log('Cleaning up temporary worktree...')
    os.chdir(orig_cwd)
    if worktree_added:
      git('worktree', 'remove', '--force', tmpdir, check=False)
    if os.path.exists(tmpdir):
      shutil.rmtree(tmpdir, ignore_errors=True)


def main():
  parser = argparse.ArgumentParser(
      description=('Merge autoroll PRs. Target repository can be configured via '
                   'the GITHUB_REPOSITORY environment variable.'))
  parser.add_argument(
      '--source-branch', required=True, help='Source branch name')
  parser.add_argument(
      '--target-branch', required=True, help='Target branch name')
  parser.add_argument(
      '--key-file',
      help=('Path to GitHub App Private Key (.pem file).'
            'Omit to provide key from stdin.'))
  args = parser.parse_args()

  source = args.source_branch
  target = args.target_branch
  key_file = args.key_file

  if not key_file:
    private_key = read_private_key()
    temp_fd, key_file = tempfile.mkstemp(suffix='.pem')
    with os.fdopen(temp_fd, 'w') as key_file_obj:
      key_file_obj.write(private_key)

  token = get_installation_access_token(APP_ID, key_file)
  env = {'GITHUB_TOKEN': token}

  pr = find_open_autoroll_pr(source, target, env)
  rebase_and_push(target, pr, env)


if __name__ == '__main__':
  main()
