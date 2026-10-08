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

  for sha, title, pr_num in commits_to_target:
    identifier = f'- #{pr_num}' if pr_num else f'- {sha}'

    # Skip if already merged in target branch
    pr_num_int = int(pr_num) if pr_num else None
    if sha in target_shas or (pr_num_int and pr_num_int in target_prs):
      continue

    # Skip if already in autoroll (matched by SHA or original PR number)
    if sha in already_rolled_shas or (pr_num_int and
                                      pr_num_int in already_rolled_prs):
      commits_added.append(identifier)
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
    first_commit = not commits_added
    autoroll_metadata = ((args.autoroll_file,
                          sha) if args.mode == 'full' else None)

    result, unmerged_files = cherry_pick(sha, metadata, first_commit,
                                         autoroll_metadata)

    if result == lib.CommitStatus.FAILED:
      lib.log(f'Reached FAILED commit ({sha}).')
      break

    commits_added.append(identifier)

    if result == lib.CommitStatus.CONFLICTED:
      commits_added.append('')
      commits_added.append('CONFLICTED files:')
      commits_added.append('```')
      commits_added.extend(unmerged_files)
      commits_added.append('```')
      lib.log(f'Reached CONFLICTED commit ({sha}).')
      break

  if commits_added:
    print('\n'.join(commits_added))


if __name__ == '__main__':
  main()
