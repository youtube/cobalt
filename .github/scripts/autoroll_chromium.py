#!/usr/bin/env python3
"""Script to automatically roll Chromium branch."""
import argparse
import json
import os
import re
import ssl
import urllib.error
import urllib.request
import autoroll_lib as lib

_COBALT_SUBMODULE_DIRS = [
    'net/third_party/quiche/src',
    'third_party/angle',
    'third_party/boringssl/src',
    'third_party/cpuinfo/src',
    'third_party/googletest/src',
    'third_party/icu',
    'third_party/libc++/src',
    'third_party/perfetto',
    'third_party/skia',
    'third_party/webrtc',
    'v8',
]


def get_submodule_root_dirs():
  paths = lib.get_out(
      ['git', 'config', '--file', '.gitmodules', '--get-regexp', 'path'])

  return sorted(
      {line.split(' ', 1)[1].split('/')[0] for line in paths.splitlines()})


def remove_local_checkout():
  lib.log('Removing local checkout...')
  roots = get_submodule_root_dirs()
  if roots:
    lib.run(['rm', '-rf', '--'] + roots)
  lib.run(['git', 'rm', '-qrf', '--', '.'])
  lib.run(['git', 'clean', '-qffdx'])


def replace_submodules_with_dirs():
  lib.log('Running gclient sync...')
  repo_url = lib.get_out(['git', 'remote', 'get-url', 'origin']).strip()
  lib.run(['gclient', 'config', '--name=src', '--unmanaged', repo_url],
          cwd='..')
  lib.run(['gclient', 'sync', '--no-history'], cwd='..')
  lib.run(['rm', '-f', '--', os.path.join('..', '.gclient')])
  lib.log('Removing Chromium submodules for Cobalt directories...')
  for submodule_dir in _COBALT_SUBMODULE_DIRS:
    lib.run(['rm', '-rf', '--', os.path.join(submodule_dir, '.git')])
    lib.run([
        'git', 'rm', '-qrf', '--cached', '--ignore-unmatch', '--', submodule_dir
    ])


def fetch_chromium_tree(chromium_sha):
  """Fetches the root tree hash directly from Chromium's Gitiles API."""
  url = (f'https://chromium.googlesource.com/chromium/src/+/{chromium_sha}'
         '?format=JSON')
  req = urllib.request.Request(url, headers={'User-Agent': 'cobalt-autoroller'})

  ctx = ssl.create_default_context()
  try:
    with urllib.request.urlopen(req, context=ctx) as resp:
      text = resp.read().decode('utf-8')
  except (ssl.SSLCertVerificationError, urllib.error.URLError):
    # pylint: disable=protected-access
    unverified_ctx = ssl._create_unverified_context()
    # pylint: enable=protected-access
    with urllib.request.urlopen(req, context=unverified_ctx) as resp:
      text = resp.read().decode('utf-8')

  # Gitiles JSON API responses prepend a 4-char anti-XSSI security prefix: )]}'
  if text.startswith(")]}'"):
    text = text[4:].lstrip()

  data = json.loads(text)
  return data['tree']


def get_upstream_chromium_sha(cobalt_sha):
  """Extracts the upstream Chromium commit SHA from the commit message body."""
  body = lib.get_out(['git', 'log', '-1', '--format=%B', cobalt_sha])
  match = re.search(r'Update to commit ([0-9a-fA-F]{40})', body)
  return match.group(1) if match else None


def verify_chromium_commit(sha):
  """Verifies that current Git tree matches expected Chromium commit tree.

  Args:
    sha: The SHA of the Cobalt commit being rolled in.

  Returns:
    bool: True if the current tree matches the expected Chromium commit tree,
      False otherwise.
  """
  upstream_sha = get_upstream_chromium_sha(sha)
  if not upstream_sha:
    lib.log(
        f'ERROR: No upstream Chromium commit SHA found in message of {sha}.')
    return False

  lib.log(f'Verifying against upstream Chromium commit {upstream_sha} via '
          'Gitiles...')
  try:
    expected_tree = fetch_chromium_tree(upstream_sha)
  except Exception as e:  # pylint: disable=broad-except
    lib.log(f'ERROR: Failed to query Gitiles for Chromium commit '
            f'{upstream_sha}: {e}')
    return False

  current_tree = lib.get_out(['git', 'rev-parse', 'HEAD^{tree}']).strip()

  if current_tree == expected_tree:
    lib.log(f'Verification passed: Tree {current_tree} matches Chromium '
            f'{upstream_sha}.')
    return True

  diff_output = lib.get_out(['git', 'diff', '--name-status', sha,
                             'HEAD']).strip()
  lib.log(f'ERROR: Rolled-in tree ({current_tree}) differs from Chromium '
          f'{upstream_sha} ({expected_tree})!')
  if diff_output:
    lib.log(f'Offending files:\n{diff_output}')
  return False


def chromium_cherry_pick(previous_sha, shas, metadata, autoroll_metadata):
  """Temporarily reverts Cobalt changes to apply a Chromium cherry-pick.

  This function performs a "clean slate" cherry-pick by wiping the current
  working directory, checking out the pure Chromium state at `previous_sha`,
  and committing that as a temporary revert. It then applies the desired
  Chromium `sha` and re-applies Cobalt's modifications over the result.

  Args:
    previous_sha: The SHA of the clean Chromium base before Cobalt changes were
      applied.
    shas: A list of SHAs of the Chromium commits to be batch cherry-picked.
    metadata: Metadata associated with the cherry-pick, passed to the final
      conflicting revert call.
    autoroll_metadata: autoroll file path and sha tuple that tracks progress.

  Returns:
    CommitStatus and unmerged_files.
  """
  lib.log(f'Checking out clean Chromium state: {previous_sha}')
  remove_local_checkout()
  lib.run(['git', 'checkout', previous_sha, '--', '.'])

  replace_submodules_with_dirs()

  lib.log('Committing Cobalt revert...')
  lib.run(['git', 'add', '--', '.'])
  lib.run([
      'git', 'commit', '--no-verify', '-qm',
      'CONFLICTED Chromium Cherry pick: Revert Cobalt.'
  ])
  revert_cobalt_sha = lib.get_out(['git', 'rev-parse', 'HEAD']).strip()

  lib.log(f'Checking out clean Chromium state: {previous_sha}')
  remove_local_checkout()
  lib.run(['git', 'checkout', '-f', previous_sha, '--', '.'])

  lib.log('Committing submodules restore...')
  lib.run(['git', 'add', '--', '.'])
  lib.run(['git', 'commit', '--no-verify', '-qm', 'Restore submodules.'])

  for sha in shas:
    lib.log('Cherry picking Chromium...')
    lib.run(['git', 'cherry-pick', sha])

    if not verify_chromium_commit(sha):
      raise RuntimeError(
          f'Verification failed: Rolled-in tree for {sha} does not match '
          f'Chromium {sha}')

  replace_submodules_with_dirs()

  lib.log('Committing submodules replace...')
  lib.run(['git', 'add', '--', '.'])
  lib.run(['git', 'commit', '--no-verify', '-qm', 'Remove submodules.'])

  lib.log('Reverting Cobalt revert...')
  return lib.apply_and_commit('revert', revert_cobalt_sha, metadata, True,
                              autoroll_metadata)


def main():
  p = argparse.ArgumentParser()
  p.add_argument('--source-branch', required=True)
  p.add_argument('--autoroll-file', required=True)
  p.add_argument('--max-commits', type=int, required=True)
  p.add_argument('--existing-pr-sha', required=True)
  args = p.parse_args()

  autoroll_start = lib.get_start_sha('HEAD', args.autoroll_file)

  if autoroll_start is None:
    lib.log('Autoroll branch has an unresolved CONFLICTED commit.')
    return

  if args.existing_pr_sha:
    lib.run(['git', 'fetch', 'origin', args.existing_pr_sha])
    commit_title = lib.get_out(
        ['git', 'log', '-1', args.existing_pr_sha, '--format=%s']).strip()
    if commit_title.startswith('CONFLICTED'):
      lib.log('Autoroll branch has a resolved CONFLICTED commit. '
              'Squash and merge before autoroll will continue.')
      return

  # Commits in source but not in autoroll
  commits_to_autoroll = lib.get_commits(args.source_branch, autoroll_start)

  if not commits_to_autoroll:
    return

  shas = []
  msgs = []
  commits_added = []

  for sha, title, _ in commits_to_autoroll:
    if len(commits_added) >= args.max_commits:
      lib.log(f'Reached commit limit ({args.max_commits}).')
      break

    shas.append(sha)
    date, author, msg = lib.get_cherry_pick_metadata(sha, title, None)
    msgs.append(msg)
    commits_added.append(f'- {sha}')

  # Commits PR
  metadata = (date, author, '\n\n'.join(msgs))
  autoroll_metadata = (args.autoroll_file, shas[-1])

  result, unmerged_files = chromium_cherry_pick(autoroll_start, shas, metadata,
                                                autoroll_metadata)

  if result != lib.CommitStatus.CONFLICTED:
    raise RuntimeError('Chromium autoroll assumed to always be conflicting.')

  commits_added.append('')
  commits_added.append('CONFLICTED files:')
  commits_added.append('```')
  commits_added.extend(unmerged_files)
  commits_added.append('```')
  print('\n'.join(commits_added))


if __name__ == '__main__':
  main()
