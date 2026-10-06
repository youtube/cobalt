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


def fetch_submodule_pins(commits, dirs):
  """Fetches submodule pins in parallel into isolated shallow stores attached via alternates."""
  git_dir = lib.get_out(['git', 'rev-parse', '--git-dir']).strip()
  alt_file = os.path.join(git_dir, 'objects', 'info', 'alternates')
  os.makedirs(os.path.dirname(alt_file), exist_ok=True)
  existing = set(open(alt_file).read().splitlines()) if os.path.exists(alt_file) else set()
  subs_base = os.path.join(tempfile.gettempdir(), 'cobalt_submodule_pins')

  procs, new_alts = [], []
  for c in commits:
    for p in dirs:
      try:
        pin = lib.get_out(['git', 'rev-parse', f'{c}:{p}']).strip()
        url = lib.get_out(['git', 'config', '--blob', f'{c}:.gitmodules', f'submodule.{p}.url']).strip()
      except subprocess.CalledProcessError:
        continue
      pin_dir = os.path.join(subs_base, pin)
      obj_dir = os.path.join(pin_dir, 'objects')
      if obj_dir not in existing and not os.path.exists(obj_dir):
        subprocess.run(['git', 'init', '-q', '--bare', pin_dir], check=True)
        procs.append(subprocess.Popen(['git', '-C', pin_dir, 'fetch', '-q', '--depth=1', '--no-tags', url, pin]))
        new_alts.append(obj_dir)
  for p in procs:
    p.wait()
  if new_alts:
    with open(alt_file, 'a', encoding='utf-8') as f:
      f.write('\n'.join(new_alts) + '\n')


def vendor_dirs(commit, dirs):
  """Returns tree of `commit` with gitlinks at `dirs` replaced by contents."""
  with tempfile.TemporaryDirectory(prefix='vendor_') as tmp:
    wt = os.path.join(tmp, 'wt')
    os.makedirs(wt, exist_ok=True)
    env = {'GIT_INDEX_FILE': os.path.join(tmp, 'index'), 'GIT_WORK_TREE': wt}
    lib.run(['git', 'read-tree', commit], env=env)
    for d in dirs:
      pin = lib.get_out(['git', 'rev-parse', f'{commit}:{d}']).strip()
      sub_wt = os.path.join(wt, d)
      os.makedirs(sub_wt, exist_ok=True)
      sub_env = {'GIT_INDEX_FILE': os.path.join(tmp, 'sub_index'), 'GIT_WORK_TREE': sub_wt}
      lib.run(['git', 'read-tree', pin], env=sub_env)
      lib.run(['git', 'checkout-index', '-a', '-f'], env=sub_env, cwd=sub_wt)
    lib.run(['git', 'update-index', '--force-remove', '--', *dirs], env=env, cwd=wt)
    lib.run(['git', 'add', '--', *dirs], env=env, cwd=wt)
    return lib.get_out(['git', 'write-tree', '--missing-ok'], env=env, cwd=wt).strip()


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
  """Verifies that current Git tree matches expected Chromium commit tree."""
  upstream_sha = get_upstream_chromium_sha(sha)
  if not upstream_sha:
    lib.log(f'ERROR: No upstream Chromium commit SHA found in message of {sha}.')
    return False

  lib.log(f'Verifying against upstream Chromium commit {upstream_sha} via Gitiles...')
  try:
    expected_tree = fetch_chromium_tree(upstream_sha)
  except Exception as e:  # pylint: disable=broad-except
    lib.log(f'ERROR: Failed to query Gitiles for Chromium commit {upstream_sha}: {e}')
    return False

  current_tree = lib.get_out(['git', 'rev-parse', 'HEAD^{tree}']).strip()
  if current_tree == expected_tree:
    lib.log(f'Verification passed: Tree {current_tree} matches Chromium {upstream_sha}.')
    return True

  if sha in _REVISIONS_WITH_BROKEN_ANGLE_SUBDEP:
    modified_files = lib.get_out(['git', 'diff', '--name-only', sha, 'HEAD']).strip()
    if modified_files == 'DEPS':
      lib.log(f'Verification passed: Tree {current_tree} matches Chromium {upstream_sha} with DEPS fix')
      return True

  diff_output = lib.get_out(['git', 'diff', '--name-status', sha, 'HEAD']).strip()
  lib.log(f'ERROR: Rolled-in tree ({current_tree}) differs from Chromium {upstream_sha} ({expected_tree})!')
  if diff_output:
    lib.log(f'Offending files:\n{diff_output}')
  return False


def chromium_cherry_pick(previous_sha, shas, metadata, autoroll_metadata):
  """Applies a Chromium cherry-pick sequence using pure git plumbing."""
  head = lib.get_out(['git', 'rev-parse', 'HEAD']).strip()
  date, author, _ = metadata
  fetch_submodule_pins([previous_sha, *shas], _COBALT_SUBMODULE_DIRS)

  lib.log(f'Vendoring clean Chromium base: {previous_sha}')
  revert_cobalt_sha = lib.commit_tree(
      vendor_dirs(previous_sha, _COBALT_SUBMODULE_DIRS), head, date, author,
      'CONFLICTED Chromium Cherry pick: Revert Cobalt.')

  lib.log('Committing submodules restore...')
  restore_sha = lib.commit_tree(
      lib.get_out(['git', 'rev-parse', f'{previous_sha}^{{tree}}']).strip(),
      revert_cobalt_sha, date, author, 'Restore submodules.')

  curr = restore_sha
  for sha in shas:
    lib.log(f'Cherry picking Chromium commit {sha}...')
    cp_tree, cp_conflicts = lib.merge_trees('cherry-pick', sha, curr)
    if cp_conflicts:
      lib.log(f'Warning: Upstream Chromium cherry-pick conflict: {cp_conflicts}')
    curr = lib.commit_tree(cp_tree, curr, date, author, f'Update to {sha}.')
    lib.run(['git', 'update-ref', 'HEAD', curr])
    if not verify_chromium_commit(sha):
      raise RuntimeError(f'Verification failed: Rolled-in tree for {sha} does not match Chromium {sha}')

  lib.log('Vendoring new Chromium state...')
  remove_submodules_sha = lib.commit_tree(
      vendor_dirs(curr, _COBALT_SUBMODULE_DIRS), curr, date, author,
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

  commits_to_autoroll = lib.get_commits(args.source_branch, autoroll_start)
  if not commits_to_autoroll:
    return

  shas, msgs, commits_added = [], [], []
  for sha, title, _ in commits_to_autoroll:
    if len(commits_added) >= args.max_commits:
      lib.log(f'Reached commit limit ({args.max_commits}).')
      break
    shas.append(sha)
    date, author, msg = lib.get_cherry_pick_metadata(sha, title, None)
    msgs.append(msg)
    commits_added.append(f'- {sha}')

  metadata = (date, author, '\n\n'.join(msgs))
  autoroll_metadata = (args.autoroll_file, shas[-1])
  result, unmerged_files = chromium_cherry_pick(autoroll_start, shas, metadata,
                                                autoroll_metadata)

  if result != lib.CommitStatus.CONFLICTED:
    raise RuntimeError('Chromium autoroll assumed to always be conflicting.')

  commits_added.extend(['', 'CONFLICTED files:', '```', *unmerged_files, '```'])
  print('\n'.join(commits_added))


if __name__ == '__main__':
  main()
