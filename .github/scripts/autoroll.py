#!/usr/bin/env python3
"""Script to automatically roll branch.

Two roll modes are supported:

- full: every commit in source..target is rolled in order onto the autoroll
  branch and the AUTOROLL cursor file tracks the last rolled commit. If a commit
  conflicts, rolling stops to preserve linear history and the conflicting commit
  is committed on its own `autoroll-conflict-<source>-to-<target>-<id>` branch.
- label: only merged PRs carrying the `cp-<target>` label are rolled.
  Conflicting PRs are committed on their own
  `autoroll-conflict-<source>-to-<target>-<pr>` branches, while clean labeled
  PRs continue rolling.

In both modes, the clean autoroll branch (`autoroll-<source>-to-<target>`) only
ever holds clean cherry-picks. Each conflicting commit is committed on its own
`autoroll-conflict-<source>-to-<target>-<id>` branch, built from the target
branch, so a conflict never blocks clean cherry-picks and never gets mixed
with clean ones.

When --manifest is given, a JSON description of the resulting branches is
written there for the workflow to push and open/update/close PRs.
"""
import argparse
import json
import sys
import autoroll_lib as lib


def cherry_pick(sha, metadata, first_commit, autoroll_metadata=None):
  return lib.apply_and_commit('cherry-pick', sha, metadata, first_commit,
                              autoroll_metadata)


def conflict_branch_prefix(source, target):
  return f'autoroll-conflict-{source}-to-{target}-'


def conflict_files_block(files):
  return '\n'.join(['', 'CONFLICTED files:', '```'] + list(files) + ['```'])


def parse_args():
  p = argparse.ArgumentParser()
  p.add_argument('--source-branch', required=True)
  p.add_argument('--target-branch', required=True)
  p.add_argument('--autoroll-file', required=True)
  p.add_argument('--max-commits', type=int, required=True)
  p.add_argument(
      '--mode',
      choices=['full', 'label'],
      default='full',
      help='Roll mode: "full" rolls all commits, "label" rolls only PRs with '
      'the target cherry-pick label.')
  p.add_argument(
      '--prs-json',
      help='Path to JSON file containing pre-fetched PRs and labels.')
  p.add_argument(
      '--max-conflict-prs',
      type=int,
      default=3,
      help='Label mode: maximum number of concurrent conflict branches.')
  p.add_argument(
      '--manifest',
      help='Path to write a JSON manifest describing the resulting branches.')
  return p.parse_args()


def get_candidates(args, target_start):
  """Returns (sha, title, pr_num) commits eligible for rolling, in order."""
  commits_to_target = lib.get_commits(args.source_branch, target_start)
  target_shas, target_prs = lib.get_rolled_source_items(args.target_branch)
  target_label = f'cp-{args.target_branch}'

  candidates = []
  for sha, title, pr_num in commits_to_target:
    # Skip if already merged in target branch
    pr_num_int = int(pr_num) if pr_num else None
    if sha in target_shas or (pr_num_int and pr_num_int in target_prs):
      continue

    # In label mode, only migrate PRs that have the cherry pick label applied
    if args.mode == 'label':
      if not pr_num:
        continue
      if target_label not in lib.get_pr_labels(pr_num):
        continue

    candidates.append((sha, title, pr_num))
  return candidates


def roll_clean(args, candidates, manifest):
  """Builds the clean autoroll branch on HEAD.

  Returns:
    (commits_added, conflicted): commit identifiers on the branch and a list of
    dicts describing candidates that could not be applied cleanly.
  """
  target = args.target_branch
  clean_branch = lib.current_branch()
  clean_old_sha = lib.rev_parse('HEAD')
  manifest['clean'] = {
      'branch': clean_branch,
      'old_sha': clean_old_sha,
      'changed': False,
      'rebuilt': False,
      'items': [],
  }
  manifest['delete'] = []

  subjects = lib.get_subjects(f'{target}..HEAD')
  if not lib.is_bot_branch(target, 'HEAD'):
    lib.log(f'Leaving {clean_branch} alone: it has commits not made by the '
            'autoroller. Merge or close its PR before autoroll will continue.')
    manifest['clean']['hands_off'] = True
    return [], None

  already_rolled_shas, already_rolled_prs = lib.get_rolled_source_items(
      f'{target}..HEAD')

  # Rebuild from target when a rolled commit is no longer a candidate (label
  # removed, or landed in target by other means) or when the branch holds a
  # conflict from before conflicts moved to their own branches. Otherwise
  # stack on the existing branch to keep commit SHAs and review context
  # stable. Only the exact "(cherry picked from commit ...)" SHAs are used for
  # this decision; PR numbers parsed from commit bodies are too loose a match.
  candidate_shas = {sha for sha, _, _ in candidates}
  stale = already_rolled_shas - candidate_shas
  conflicted_branch = any(s.startswith('CONFLICTED ') for s in subjects)
  rebuilt = False
  if stale or conflicted_branch:
    lib.log(f'Rebuilding {clean_branch} from {target} '
            f'(stale={sorted(stale)}, conflicted={conflicted_branch}).')
    lib.run(['git', 'reset', '--hard', target])
    already_rolled_shas, already_rolled_prs = set(), set()
    rebuilt = True

  commits_added = []
  conflicted = []

  for index, (sha, title, pr_num) in enumerate(candidates):
    identifier = f'- #{pr_num}' if pr_num else f'- {sha}'
    pr_num_int = int(pr_num) if pr_num else None
    if sha in already_rolled_shas or (pr_num_int and
                                      pr_num_int in already_rolled_prs):
      commits_added.append(identifier)
      continue

    if len(commits_added) >= args.max_commits:
      lib.log(f'Reached commit limit ({args.max_commits}).')
      break

    metadata = lib.get_cherry_pick_metadata(sha, title, pr_num)
    autoroll_metadata = (args.autoroll_file, sha,
                         True) if (args.mode == 'full' and
                                   args.autoroll_file) else None
    # Never commit conflicts on the clean branch.
    result, unmerged_files = cherry_pick(
        sha, metadata, first_commit=False, autoroll_metadata=autoroll_metadata)

    if result == lib.CommitStatus.FAILED:
      lib.log(f'Skipping conflicting commit ({sha}).')
      conflict_id = str(pr_num) if pr_num else sha[:10]
      conflicted.append({
          'id': conflict_id,
          'pr': int(pr_num) if pr_num else None,
          'sha': sha,
          'title': title,
          'files': unmerged_files,
      })
      if args.mode == 'full':
        # Full mode preserves linear history: halt further candidate rolling.
        # Keep reporting any already-rolled commits that came after this
        # candidate on the branch.
        commits_added.extend(f'- #{p}' if p else f'- {s}'
                             for s, _, p in candidates[index + 1:]
                             if s in already_rolled_shas or
                             (int(p) in already_rolled_prs if p else False))
        break
      continue

    commits_added.append(identifier)

  manifest['clean'].update({
      'changed': lib.rev_parse('HEAD') != clean_old_sha,
      'rebuilt': rebuilt,
      'items': commits_added,
  })
  if rebuilt and not commits_added:
    # Every rolled commit lost its candidate status (label removed or landed).
    # Deleting the branch closes its PR.
    manifest['delete'].append({
        'branch': clean_branch,
        'old_sha': clean_old_sha
    })
  return commits_added, conflicted


def build_conflict_branch(args, conflict, old_sha):
  """Commits a single CONFLICTED cherry-pick of conflict on top of target.

  Returns:
    (status, sha): status is CONFLICTED with the branch tip SHA, or SUCCESS
    if the commit unexpectedly applies cleanly to the bare target branch.
    HEAD is left detached.
  """
  lib.run(['git', 'checkout', '--quiet', '--detach', args.target_branch])
  metadata = lib.get_cherry_pick_metadata(conflict['sha'], conflict['title'],
                                          conflict['pr'])
  autoroll_metadata = (args.autoroll_file, conflict['sha'],
                       True) if (args.mode == 'full' and
                                 args.autoroll_file) else None
  result, unmerged_files = cherry_pick(
      conflict['sha'],
      metadata,
      first_commit=True,
      autoroll_metadata=autoroll_metadata)
  if result != lib.CommitStatus.CONFLICTED:
    return result, None

  conflict['files'] = unmerged_files
  new_sha = lib.rev_parse('HEAD')
  if old_sha and lib.patch_id(old_sha) == lib.patch_id(new_sha):
    # The conflict itself is unchanged (the target may have moved elsewhere);
    # keep the existing branch to avoid churn.
    return result, old_sha
  return result, new_sha


def roll_conflicts(args, conflicted, manifest):
  """Reconciles autoroll-conflict-* branches with the conflicting candidates."""
  target = args.target_branch
  clean_branch = lib.current_branch()
  prefix = conflict_branch_prefix(args.source_branch, target)
  existing = lib.list_branches(prefix)

  def branch_for(conflict_id):
    return f'{prefix}{conflict_id}'

  # Existing conflict branches keep their slot; new ones fill the remainder.
  ordered = ([c for c in conflicted if branch_for(c['id']) in existing] +
             [c for c in conflicted if branch_for(c['id']) not in existing])

  conflicts = []
  pending = []
  delete = manifest['delete']
  hands_off = []
  handled = set()

  try:
    for conflict in ordered:
      branch = branch_for(conflict['id'])
      # Only branches that actually hold a conflict occupy a slot.
      if len(conflicts) + len(hands_off) >= args.max_conflict_prs:
        if branch not in existing:
          pending.append({
              'id': conflict['id'],
              'pr': conflict['pr'],
              'reason': 'waiting for a free conflict PR slot '
                        f'(max {args.max_conflict_prs})',
          })
        continue
      handled.add(branch)

      old_sha = existing.get(branch)
      if old_sha and not lib.is_bot_conflict_branch(target, old_sha):
        lib.log(f'Leaving {branch} alone: it has resolution commits.')
        hands_off.append({'branch': branch})
        continue

      conflict_sha = conflict['sha']
      status, sha = build_conflict_branch(args, conflict, old_sha)
      if status != lib.CommitStatus.CONFLICTED:
        lib.log(f'Commit ({conflict_sha}) applies cleanly to {target}; '
                'it only conflicts with the pending autoroll branch.')
        pending.append({
            'id': conflict['id'],
            'pr': conflict['pr'],
            'reason': f'applies cleanly to {target} but conflicts with the '
                      f'pending `{clean_branch}` roll; retried after it merges',
        })
        if old_sha:
          delete.append({'branch': branch, 'old_sha': old_sha})
        continue

      changed = sha != old_sha
      if changed:
        lib.run(['git', 'branch', '--force', branch, sha])
      conflicts.append({
          'id':
              conflict['id'],
          'pr':
              conflict['pr'],
          'branch':
              branch,
          'old_sha':
              old_sha or '',
          'changed':
              changed,
          'title':
              lib.show_commit(sha, '%s'),
          'body':
              lib.show_commit(sha, '%b') + '\n' +
              conflict_files_block(conflict['files']),
      })
      state = 'updated' if changed else 'unchanged'
      lib.log(f'Conflict branch {branch}: {state}.')
  finally:
    lib.run(['git', 'checkout', '--quiet', clean_branch])

  wanted_branches = {branch_for(c['id']) for c in conflicted}
  for branch, old_sha in sorted(existing.items()):
    if branch in handled:
      continue
    suffix = branch[len(prefix):]
    if not suffix.isalnum():
      lib.log(f'Ignoring {branch}: not an autoroll conflict branch.')
      continue
    if branch in wanted_branches:
      # Still conflicting but over the cap; leave untouched.
      continue
    if lib.is_bot_conflict_branch(target, old_sha):
      lib.log(f'Deleting {branch}: no longer a conflicting candidate.')
      delete.append({'branch': branch, 'old_sha': old_sha})
    else:
      lib.log(f'Leaving {branch} alone: no longer a conflicting candidate '
              'but it has resolution commits.')
      hands_off.append({'branch': branch})

  manifest['conflicts'] = conflicts
  manifest['pending'] = pending
  manifest['hands_off'] = hands_off


def main():
  args = parse_args()

  if args.mode == 'label':
    if not args.prs_json:
      lib.log('Error: --prs-json is required in label mode.')
      sys.exit(1)
    lib.load_pr_labels_from_file(args.prs_json)

  target_start = lib.get_start_sha(args.target_branch, args.autoroll_file)
  candidates = get_candidates(args, target_start)
  manifest = {}

  commits_added, conflicted = roll_clean(args, candidates, manifest)
  # Without a clean roll the conflicting set is unknown; leave the conflict
  # branches as they are.
  if conflicted is not None:
    roll_conflicts(args, conflicted, manifest)

  if args.manifest:
    with open(args.manifest, 'w', encoding='utf-8') as f:
      json.dump(manifest, f, indent=2)

  if commits_added:
    print('\n'.join(commits_added))


if __name__ == '__main__':
  main()
