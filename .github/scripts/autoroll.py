#!/usr/bin/env python3
"""Script to automatically roll branch."""
import argparse
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
  args = p.parse_args()

  target_start = lib.get_start_sha(args.target_branch, args.autoroll_file)
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

  # Commits in source but not in target
  commits_to_target = lib.get_commits(args.source_branch, target_start)
  # Commits in source but not in autoroll
  commits_to_autoroll = lib.get_commits(args.source_branch, autoroll_start)
  # SHAs in source but not in autoroll
  shas_to_autoroll = {sha for sha, _, _ in commits_to_autoroll}

  commits_added = []

  for sha, title, pr_num in commits_to_target:
    if len(commits_added) >= args.max_commits:
      lib.log(f'Reached commit limit ({args.max_commits}).')
      break

    identifier = f'- #{pr_num}' if pr_num else f'- {sha}'

    # Skip if already in autoroll
    if sha not in shas_to_autoroll:
      commits_added.append(identifier)
      continue

    # Commit PR
    metadata = lib.get_cherry_pick_metadata(sha, title, pr_num)
    first_commit = not commits_added
    autoroll_metadata = (args.autoroll_file, sha)

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
