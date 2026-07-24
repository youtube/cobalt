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
"""Script to merge LTS autoroll PRs.

Rebases the changes and pushes directly to the target branch.
"""

import argparse
import base64
import json
import os
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


def git(*args, check=True, **kwargs):
  return subprocess.run(['git'] + list(args), check=check, **kwargs).stdout


def gh(*args, check=True, **kwargs):
  return subprocess.run(['gh'] + list(args), check=check, **kwargs).stdout


def openssl(*args, stdin=None, check=True, **kwargs):
  return subprocess.run(
      ['openssl'] + list(args), input=stdin, check=check, **kwargs).stdout


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
      'number,headRefName,baseRefName,title',
      capture_output=True,
      text=True,
      env=env)
  prs = json.loads(prs_json)

  pr = prs[0] if prs else None
  if not pr:
    log(f'No open autoroll PR found for {head_branch} -> {target}')
    sys.exit(0)

  if pr['title'].upper().startswith('CONFLICTED:'):
    pr_num = pr['number']
    pr_title = pr['title']
    log(f'Error: Found matching autoroll PR #{pr_num} but it is marked '
        f'as CONFLICTED ("{pr_title}"). Please resolve manually.')
    sys.exit(1)

  return pr


def rebase_and_push(target, pr, env):
  head = pr['headRefName']
  pr_number = pr['number']
  log(f'Found PR #{pr_number}: {head} -> {target}')

  tmpdir = tempfile.mkdtemp()
  worktree_added = False
  orig_cwd = os.getcwd()
  try:
    log('Fetching branches...')
    git('-c',
        'credential.helper=',
        '-c',
        'credential.helper=!gh auth git-credential',
        'fetch',
        REPO_URL,
        f'+{target}:refs/remotes/origin/{target}',
        f'+{head}:refs/remotes/origin/{head}',
        check=True,
        env=env)

    log('Creating temporary worktree...')
    git('worktree',
        'add',
        '--no-checkout',
        tmpdir,
        f'origin/{head}',
        check=True)
    worktree_added = True

    os.chdir(tmpdir)

    git('sparse-checkout', 'init', '--cone')
    git('sparse-checkout', 'set', '.github')
    git('checkout')

    log('Rebasing...')
    git('rebase', f'origin/{target}', check=False)

    log(f'Pushing {target}...')
    git('-c',
        'credential.helper=',
        '-c',
        'credential.helper=!gh auth git-credential',
        'push',
        REPO_URL,
        f'HEAD:{target}',
        check=True,
        env=env)
  finally:
    log('Cleaning up temporary worktree...')
    os.chdir(orig_cwd)
    if worktree_added:
      git('worktree', 'remove', '--force', tmpdir, check=False)
    if os.path.exists(tmpdir):
      shutil.rmtree(tmpdir, ignore_errors=True)


def main():
  parser = argparse.ArgumentParser(description='Merge LTS autoroll PRs.')
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
