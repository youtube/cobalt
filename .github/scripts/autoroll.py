#!/usr/bin/env python3
"""Script to automatically roll branch."""
import argparse
import sys
import autoroll_lib as lib


def cherry_pick(sha, metadata, first_commit, autoroll_metadata):
  return lib.apply_and_commit('cherry-pick', sha, metadata, first_commit,
                              autoroll_metadata)


def main():
  p = argparse.ArgumentParser()
  p.add_argument('--source-branch', required=True)
  p.add_argument('--target-branch', required=True)
  p.add_argument('--autoroll-file', required=True)
  p.add_argument('--max-commits', type=int, required=True)
  p.add_argument('--existing-pr-sha', required=True)
  p.add_argument(
      '--mode',
      choices=['full', 'label'],
      default='full',
      help='Roll mode: "full" rolls all commits, "label" rolls only PRs with '
      'the target cherry-pick label.')
  p.add_argument(
      '--prs-json',
      help='Path to JSON file containing pre-fetched PRs and labels.')
  args = p.parse_args()

  target_start = lib.get_start_sha(args.target_branch, args.autoroll_file)
  autoroll_start = lib.get_start_sha('HEAD', args.autoroll_file)

  if autoroll_start is None:
    lib.log('Autoroll branch has an unresolved CONFLICTED commit.')
    sys.exit(1)

  if args.existing_pr_sha:
    lib.run(['git', 'fetch', 'origin', args.existing_pr_sha])
    commit_title = lib.get_out(
        ['git', 'log', '-1', args.existing_pr_sha, '--format=%s']).strip()
    if commit_title.startswith('CONFLICTED'):
      lib.log('Autoroll branch has a resolved CONFLICTED commit. '
              'Squash and merge before autoroll will continue.')
      sys.exit(1)

  # Commits in source but not in target
  commits_to_target = lib.get_commits(args.source_branch, target_start)
  already_rolled_shas, already_rolled_prs = lib.get_rolled_source_items(
      f'{args.target_branch}..HEAD')
  target_shas, target_prs = lib.get_rolled_source_items(args.target_branch)

  if args.mode == 'label':
    if not args.prs_json:
      lib.log('Error: --prs-json is required in label mode.')
      sys.exit(1)
    lib.load_pr_labels_from_file(args.prs_json)

  target_label = f'cp-{args.target_branch}'
  commits_added = []
  existing_refs = set(
      lib.get_out([
          'git', 'for-each-ref', '--format=%(refname:short)', 'refs/heads/',
          'refs/remotes/origin/'
      ]).splitlines())

  for sha, title, pr_num in commits_to_target:
    identifier = f'- #{pr_num}' if pr_num else f'- {sha}'

    conflict_branch = (f'autoroll-{args.source_branch}-to-'
                       f'{args.target_branch}-conflict-{pr_num or sha[:7]}')
    had_conflict_branch = (
        conflict_branch in existing_refs or
        f'origin/{conflict_branch}' in existing_refs)

    # Skip if already merged in target branch
    pr_num_int = int(pr_num) if pr_num else None
    if sha in target_shas or (pr_num_int and pr_num_int in target_prs):
      if had_conflict_branch:
        lib.cleanup_conflict_branch(conflict_branch)
        existing_refs.discard(conflict_branch)
        existing_refs.discard(f'origin/{conflict_branch}')
      continue

    # Skip if already in autoroll (matched by SHA or original PR number)
    if sha in already_rolled_shas or (pr_num_int and
                                      pr_num_int in already_rolled_prs):
      commits_added.append(identifier)
      if had_conflict_branch:
        lib.cleanup_conflict_branch(conflict_branch)
        existing_refs.discard(conflict_branch)
        existing_refs.discard(f'origin/{conflict_branch}')
      continue

    # In label mode, only migrate PRs that have the cherry pick label applied
    if args.mode == 'label':
      if not pr_num:
        continue
      labels = lib.get_pr_labels(pr_num)
      if target_label not in labels:
        continue

    if len(commits_added) >= args.max_commits:
      lib.log(f'Reached commit limit ({args.max_commits}).')
      break

    # Commit PR
    metadata = lib.get_cherry_pick_metadata(sha, title, pr_num)
    autoroll_sha = sha if args.mode == 'full' else target_start
    autoroll_metadata = (args.autoroll_file, autoroll_sha, args.mode == 'full')

    result, _ = cherry_pick(sha, metadata, False, autoroll_metadata)

    if result == lib.CommitStatus.FAILED:
      if not had_conflict_branch:
        lib.log(f'Isolating conflicted commit ({sha}) on {conflict_branch}.')
        lib.run(['git', 'checkout', '-b', conflict_branch])
        cherry_pick(sha, metadata, True, None)
        lib.run(['git', 'checkout', '-'])
        existing_refs.add(conflict_branch)
      else:
        lib.log(f'Conflicted commit ({sha}) already isolated on '
                f'{conflict_branch}; keeping intact.')
      continue

    commits_added.append(identifier)
    if had_conflict_branch:
      lib.log(f'Conflict resolved for {sha}; cleaning up {conflict_branch}.')
      lib.cleanup_conflict_branch(conflict_branch)
      existing_refs.discard(conflict_branch)
      existing_refs.discard(f'origin/{conflict_branch}')

  if commits_added:
    print('\n'.join(commits_added))


if __name__ == '__main__':
  main()
