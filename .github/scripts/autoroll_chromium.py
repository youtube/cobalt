#!/usr/bin/env python3
"""Script to automatically roll Chromium branch using worktree-free git plumbing."""
import argparse
import contextlib
import os
import re
import subprocess
import sys
import tempfile
import autoroll_lib as lib
import gerrit_util

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

# These hashes correspond to 145.7595 and 145.7613 in the chromium/main branch.
# See b/565697787 for more context.
_REVISIONS_WITH_BROKEN_ANGLE_SUBDEP = (
    '38f4bd69219cb5db170704cdb0221dc3ea6eb039',
    'a829ba1ad70c664608ac2dd4005e0dee339edbe4',
)


def ancestor_meta_files(dirs):
  """Collects ancestor .gitignore and .gitattributes for target directories."""
  files = {'.gitignore', '.gitattributes'}
  for d in dirs:
    parts = d.split('/')
    for i in range(1, len(parts)):
      for n in ('.gitignore', '.gitattributes'):
        files.add('/'.join(parts[:i] + [n]))
  return sorted(files)


def fetch_submodule_pins(commits, dirs):
  """Fetches submodule pins in parallel into isolated shallow stores.

  Attaches the stores to the repo via objects/info/alternates to avoid
  shallow.lock collisions and redownloads.
  """
  git_dir = lib.get_out(['git', 'rev-parse', '--git-dir']).strip()
  alternates_file = os.path.join(git_dir, 'objects', 'info', 'alternates')
  os.makedirs(os.path.dirname(alternates_file), exist_ok=True)

  existing_alternates = set()
  if os.path.exists(alternates_file):
    with open(alternates_file, 'r', encoding='utf-8') as f:
      existing_alternates = {line.strip() for line in f if line.strip()}

  subs_base = os.path.join(tempfile.gettempdir(), 'cobalt_submodule_pins')
  os.makedirs(subs_base, exist_ok=True)

  pin_url_pairs = set()
  for c in commits:
    for p in dirs:
      try:
        pin = lib.get_out(['git', 'rev-parse', f'{c}:{p}']).strip()
      except subprocess.CalledProcessError:
        continue
      try:
        name_output = lib.get_out([
            'git', 'config', '--blob', f'{c}:.gitmodules', '--get-regexp',
            r'^submodule\..*\.path$'
        ])
      except subprocess.CalledProcessError:
        continue
      url = None
      for line in name_output.splitlines():
        parts = line.split()
        if len(parts) >= 2 and parts[1] == p:
          url_key = parts[0].replace('.path', '.url')
          url = lib.get_out(
              ['git', 'config', '--blob', f'{c}:.gitmodules', '--get',
               url_key]).strip()
          break
      if pin and url:
        pin_url_pairs.add((pin, url))

  procs = []
  new_alternates = []
  for pin, url in pin_url_pairs:
    pin_dir = os.path.join(subs_base, pin)
    pin_obj_dir = os.path.join(pin_dir, 'objects')
    new_alternates.append(pin_obj_dir)
    if not os.path.exists(pin_obj_dir):
      os.makedirs(pin_dir, exist_ok=True)
      subprocess.run(['git', 'init', '-q', '--bare', pin_dir], check=True)
      p = subprocess.Popen([
          'git', '-C', pin_dir, 'fetch', '-q', '--depth=1', '--no-tags', url,
          pin
      ])
      procs.append(p)

  for p in procs:
    p.wait()

  with open(alternates_file, 'a', encoding='utf-8') as f:
    for alt in new_alternates:
      if alt not in existing_alternates:
        f.write(f'{alt}\n')
        existing_alternates.add(alt)


def vendor_dirs(commit, dirs):
  """Returns tree of `commit` with gitlinks at `dirs` replaced by contents."""
  with tempfile.TemporaryDirectory(prefix='vendor_wt_') as tmp:
    wt = os.path.join(tmp, 'wt')
    os.makedirs(wt, exist_ok=True)
    env = {'GIT_INDEX_FILE': os.path.join(tmp, 'index'), 'GIT_WORK_TREE': wt}

    lib.run(['git', 'read-tree', commit], env=env)
    meta = lib.get_out(
        ['git', 'ls-files', '--', *ancestor_meta_files(dirs)],
        env=env, cwd=wt).splitlines()
    if meta:
      lib.run(['git', 'checkout-index', '-f', '--', *meta], env=env, cwd=wt)

    for d in dirs:
      pin = lib.get_out(['git', 'rev-parse', f'{commit}:{d}']).strip()
      sub_env = {
          'GIT_INDEX_FILE': os.path.join(tmp, f'sub-index-{os.path.basename(d)}'),
          'GIT_WORK_TREE': os.path.join(wt, d)
      }
      os.makedirs(sub_env['GIT_WORK_TREE'], exist_ok=True)
      lib.run(['git', 'read-tree', pin], env=sub_env)
      lib.run(['git', 'checkout-index', '-a', '-f'], env=sub_env,
              cwd=sub_env['GIT_WORK_TREE'])

    lib.run(['git', 'update-index', '--force-remove', '--', *dirs], env=env,
            cwd=wt)
    lib.run(['git', 'add', '--', *dirs], env=env, cwd=wt)
    return lib.get_out(['git', 'write-tree', '--missing-ok'], env=env,
                       cwd=wt).strip()


def fetch_chromium_tree(chromium_sha):
  """Fetches the root tree hash directly from Chromium's Gitiles API."""
  with contextlib.redirect_stdout(sys.stderr):
    conn = gerrit_util.CreateHttpConn(
        'chromium.googlesource.com',
        f'chromium/src/+/{chromium_sha}?format=JSON')
    data = gerrit_util.ReadHttpJsonResponse(conn)
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
    bool: True if current tree matches expected Chromium commit tree.
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

  if sha in _REVISIONS_WITH_BROKEN_ANGLE_SUBDEP:
    modified_files = lib.get_out(['git', 'diff', '--name-only', sha,
                                  'HEAD']).strip()
    if modified_files == 'DEPS':
      lib.log(f'Verification passed: Tree {current_tree} matches Chromium '
              f'{upstream_sha} with one change to DEPS to remove ANGLE from '
              f'recursedeps')
      return True

  diff_output = lib.get_out(['git', 'diff', '--name-status', sha,
                             'HEAD']).strip()
  lib.log(f'ERROR: Rolled-in tree ({current_tree}) differs from Chromium '
          f'{upstream_sha} ({expected_tree})!')
  if diff_output:
    lib.log(f'Offending files:\n{diff_output}')
  return False


def chromium_cherry_pick(previous_sha, shas, metadata, autoroll_metadata):
  """Applies a Chromium cherry-pick sequence using pure git plumbing.

  Args:
    previous_sha: Clean Chromium base before Cobalt modifications.
    shas: List of SHAs of Chromium commits to roll.
    metadata: Metadata tuple (date, author, msg) for final conflicting commit.
    autoroll_metadata: (autoroll_file, sha) tuple tracking progress.

  Returns:
    CommitStatus and unmerged_files.
  """
  head = lib.get_out(['git', 'rev-parse', 'HEAD']).strip()
  date, author, _ = metadata

  lib.log('Fetching submodule pins in parallel...')
  fetch_submodule_pins([previous_sha, *shas], _COBALT_SUBMODULE_DIRS)

  lib.log(f'Vendoring clean Chromium base: {previous_sha}')
  revert_cobalt_tree = vendor_dirs(previous_sha, _COBALT_SUBMODULE_DIRS)
  revert_cobalt_sha = lib.commit_tree(
      revert_cobalt_tree, head, date, author,
      'CONFLICTED Chromium Cherry pick: Revert Cobalt.')

  lib.log('Committing submodules restore...')
  restore_tree = lib.get_out(['git', 'rev-parse',
                              f'{previous_sha}^{{tree}}']).strip()
  restore_sha = lib.commit_tree(restore_tree, revert_cobalt_sha, date, author,
                                'Restore submodules.')

  current_restore_sha = restore_sha
  for sha in shas:
    lib.log(f'Cherry picking Chromium commit {sha}...')
    cp_tree, cp_conflicts = lib.merge_trees('cherry-pick', sha,
                                            current_restore_sha)
    if cp_conflicts:
      lib.log(f'Warning: Upstream Chromium cherry-pick conflict: {cp_conflicts}')
    current_restore_sha = lib.commit_tree(
        cp_tree, current_restore_sha, date, author, f'Update to {sha}.')
    lib.run(['git', 'update-ref', 'HEAD', current_restore_sha])
    if not verify_chromium_commit(sha):
      raise RuntimeError(
          f'Verification failed: Rolled-in tree for {sha} does not match '
          f'Chromium {sha}')

  lib.log('Vendoring new Chromium state...')
  remove_submodules_tree = vendor_dirs(current_restore_sha,
                                       _COBALT_SUBMODULE_DIRS)
  remove_submodules_sha = lib.commit_tree(
      remove_submodules_tree, current_restore_sha, date, author,
      'Remove submodules.')
  lib.run(['git', 'update-ref', 'HEAD', remove_submodules_sha])

  lib.log('Reverting Cobalt revert...')
  return lib.apply_and_commit('revert', revert_cobalt_sha, metadata, True,
                              autoroll_metadata, head=remove_submodules_sha)


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
