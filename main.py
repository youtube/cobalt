import argparse
import git
import json
import os
import shutil
import subprocess


def linearize_history(repo, args):
    """
    Creates a new branch with a linearized history.
    """
    start_commit = repo.commit(args.start_commit_ref)
    end_commit = repo.commit(args.end_commit_ref)

    # Initial Branch Setup
    repo.git.checkout(args.source_branch)
    if args.new_branch_name in repo.heads:
        repo.delete_head(args.new_branch_name, force=True)
    new_branch = repo.create_head(args.new_branch_name, start_commit)
    repo.head.reference = new_branch
    repo.head.reset(index=True, working_tree=True)
    print(
        f'Created new branch "{args.new_branch_name}" at {args.start_commit_ref}'
    )

    # Identify Commits to Replay
    print(
        f'Getting ancestry path from {start_commit.hexsha[:8]} to {end_commit.hexsha[:8]}...'
    )
    ancestry_path_output = repo.git.log(
        '--first-parent', '--reverse', '--pretty=format:%H',
        f'{start_commit.hexsha}..{end_commit.hexsha}')
    if ancestry_path_output.strip():
        commit_shas = ancestry_path_output.strip().split('\n')
        commits_to_replay = [repo.commit(sha) for sha in commit_shas]
    else:
        commits_to_replay = []
    print(f'Found {len(commits_to_replay)} commits in ancestry path')
    print('Commits to linearize:')
    for i, commit in enumerate(commits_to_replay, 1):
        print(f'  {i}. {commit.hexsha[:8]} {commit.summary}')

    # Replay Commits
    commit_mapping = []  # Track original -> new commit mappings

    for i, commit in enumerate(commits_to_replay, 1):
        # Get commit size info for progress indicator
        files = commit.stats.total['files']
        insertions = commit.stats.total['insertions']
        deletions = commit.stats.total['deletions']
        size_info = f'({files} files, +{insertions}/-{deletions})'

        is_merge = len(commit.parents) > 1
        commit_type = 'MERGE' if is_merge else 'REGULAR'

        print(
            f'Applying commit {i}/{len(commits_to_replay)}: {commit.hexsha[:8]} [{commit_type}] {size_info}'
        )
        print(f'  {commit.summary}')

        if is_merge:
            # Merge commits: Use tree object reconstruction
            print(f'  🔀 Merge commit - using tree object reconstruction...')
            print(f'  🌳 Reading tree object...')
            tree = commit.tree
            repo.git.read_tree(tree.hexsha)

            print(f'  🌱 Writing tree object...')
            new_tree_hexsha = repo.git.write_tree()

            # Verify tree equivalence
            if new_tree_hexsha == tree.hexsha:
                print(
                    f'    ✅ Tree object verified identical: {new_tree_hexsha}')
            else:
                print(
                    f'    ⚠️ Tree differs: expected {tree.hexsha}, got {new_tree_hexsha}'
                )
                return

            print(f'  ✍️  Committing...')
            git.objects.commit.Commit.create_from_tree(
                repo=repo,
                tree=new_tree_hexsha,
                message=commit.message + f'\n\noriginal-hexsha: {commit.hexsha}',
                parent_commits=[repo.head.commit],
                head=True,
                author=commit.author,
                author_date=commit.authored_datetime,
                committer=commit.committer,
                commit_date=commit.committed_datetime,
            )

            print(
                f'  ✅ Merge commit {i}/{len(commits_to_replay)} completed successfully'
            )

            print(f'  🧹 Cleaning working tree...')
            repo.git.reset('--hard')
        else:
            # Regular commits: Use cherry-pick for speed
            print(f'  🍒 Regular commit - cherry-picking...')
            repo.git.cherry_pick('--no-commit', commit.hexsha)

            print(f'  ✍️  Committing...')
            repo.git.commit('--allow-empty',
                            '--no-verify',
                            '--author',
                            f'{commit.author.name} <{commit.author.email}>',
                            '--date',
                            commit.authored_datetime.isoformat(),
                            '--message',
                            commit.message +
                            f'\n\noriginal-hexsha: {commit.hexsha}',
                            env={
                                'GIT_COMMITTER_NAME':
                                commit.committer.name,
                                'GIT_COMMITTER_EMAIL':
                                commit.committer.email,
                                'GIT_COMMITTER_DATE':
                                commit.committed_datetime.isoformat(),
                            })

            print(
                f'  ✅ Regular commit {i}/{len(commits_to_replay)} completed successfully'
            )

        repo.git.clean('-fdx')

        # Record commit mapping
        commit_mapping.append({
            'original_hexsha': commit.hexsha,
            'new_hexsha': repo.head.commit.hexsha,
            'commit_type': commit_type,
            'author': commit.author.name,
            'author_email': commit.author.email,
            'datetime': commit.authored_datetime.isoformat(),
            'summary': commit.summary
        })

    # Verify each commit's tree equivalence
    linear_commits = list(
        repo.iter_commits(args.new_branch_name,
                          max_count=len(commits_to_replay)))
    linear_commits.reverse()
    verification_passed = True

    print(f'\n🔍 Verifying {len(commits_to_replay)} commits...')
    for i, (original_commit,
            linear_commit) in enumerate(zip(commits_to_replay, linear_commits),
                                        1):
        print(
            f'  {i:2d}. Checking {original_commit.hexsha[:8]} → {linear_commit.hexsha[:8]}'
        )

        # Compare tree objects (content equivalence)
        if original_commit.tree == linear_commit.tree:
            print(f'      ✅ Tree equivalence verified')
        else:
            print(f'      ❌ Tree mismatch!')
            verification_passed = False
            tree_diff = repo.git.diff_tree(original_commit.tree,
                                           linear_commit.tree, '--name-status')
            if tree_diff:
                diff_lines = tree_diff.splitlines()
                print(f'      {len(diff_lines)} tree differences:')
                for line in diff_lines[:10]:  # Show first 10 differences
                    print(f'        {line}')
                if len(diff_lines) > 10:
                    print(
                        f'        ... and {len(diff_lines) - 10} more differences'
                    )

    print('\n📊 Final content verification...')
    diff = repo.git.diff(args.new_branch_name, args.end_commit_ref)
    if diff:
        print('⚠️  Working tree differs from original end commit:')
        verification_passed = False
        diff_lines = diff.splitlines()
        file_changes = [
            line for line in diff_lines if line.startswith('diff --git')
        ]
        print(f'   {len(file_changes)} files differ')
        for change in file_changes[:10]:  # Show first 10 files
            print(f'   {change}')
        if len(file_changes) > 10:
            print(f'   ... and {len(file_changes) - 10} more files')
    else:
        print('✅ Working tree matches original end commit')

    if verification_passed:
        print('\n🎉 SUCCESS: Linearized history created successfully!')
    else:
        print(
            '\n⚠️ COMPLETED WITH ISSUES: Linearized history created but verification found differences'
        )


def get_conflicted_files(repo):
    """
    Identifies files that are currently in a conflicted (unmerged) state.
    Returns a list of file paths relative to the repository root.
    """
    conflicted_files = []
    status_output = repo.git.status('--porcelain').splitlines()
    for line in status_output:
        if line.startswith(('UU ', 'AA ', 'DD ', 'UD ', 'DU ')):
            file_path = line[3:].strip()
            conflicted_files.append(file_path)
    return conflicted_files


def record_conflict(repo, commit_record_dir):
    """
    Records the conflicted files, prompts the user to resolve, then records
    the resolved files along with a corresponding patch.
    """
    conflict_dir = os.path.join(commit_record_dir, 'conflict')
    resolved_dir = os.path.join(commit_record_dir, 'resolved')
    patch_dir = os.path.join(commit_record_dir, 'patch')
    os.makedirs(conflict_dir, exist_ok=True)
    os.makedirs(resolved_dir, exist_ok=True)
    os.makedirs(patch_dir, exist_ok=True)

    print(f'\n💾 Recording conflict to: {commit_record_dir}')
    conflicted_files = get_conflicted_files(repo)
    attempt_patching = True
    patches_to_apply = {}

    for file_path in conflicted_files:
        dst_path = os.path.join(conflict_dir, file_path)
        os.makedirs(os.path.dirname(dst_path), exist_ok=True)
        source_path = os.path.join(repo.working_dir, file_path)
        if os.path.exists(source_path):
            shutil.copy2(source_path, dst_path)
        else:
            print(
                f'   ℹ️ {file_path} - Not found in working tree (delete conflict), recording empty file.'
            )
            open(dst_path, 'a').close()

        if '.' in file_path:
            file_name, file_ext = file_path.rsplit('.', 1)
        else:
            file_name = file_path
            file_ext = None
        same_names = [x for x in conflicted_files if x.startswith(file_name)]
        if len(same_names) > 1:
            patch_path = f'{file_name}_{file_ext}.patch'
        else:
            patch_path = f'{file_name}.patch'
        full_patch_path = os.path.join(os.getcwd(), patch_dir, patch_path)
        if os.path.exists(full_patch_path):
            print(f'   ✅ {file_path} - Patch found')
            patches_to_apply[file_path] = full_patch_path
        else:
            print(f'   ❌ {file_path} - No patch found')
            attempt_patching = False

    resolved_conflict = False
    if attempt_patching:
        for file_path, patch_path in patches_to_apply.items():
            print(f'   🔨 {file_path} - Patching')
            result = subprocess.run(
                ['patch', file_path, '-i', patch_path, '-s'],
                cwd=repo.working_dir)
            if result.returncode == 0:
                resolved_conflict = True
            else:
                print(f'   ❌ Patch failed.')
                resolved_conflict = False
                print('   🪄 Restoring original conflicted state.')
                for file_path in conflicted_files:
                    shutil.copy2(os.path.join(conflict_dir, file_path),
                                 os.path.join(repo.working_dir, file_path))
                break

    if not resolved_conflict:
        print('   ⚠️  Manual conflict resolution required')
        input('      Press Enter after resolving conflicts to create patch...')

    for file_path in conflicted_files:
        dst_path = os.path.join(resolved_dir, file_path)
        os.makedirs(os.path.dirname(dst_path), exist_ok=True)
        source_path = os.path.join(repo.working_dir, file_path)
        if os.path.exists(source_path):
            shutil.copy2(source_path, dst_path)
        else:
            # If the file doesn't exist after resolution, it means the user
            # resolved the conflict by deleting the file. We record this
            # by creating an empty file in the resolved directory.
            open(dst_path, 'a').close()

    if not resolved_conflict:
        print('   📄 Creating conflict resolution patches...')
        for file_path in conflicted_files:
            conflict_path = os.path.join(conflict_dir, file_path)
            resolved_path = os.path.join(resolved_dir, file_path)

            if '.' in file_path:
                file_name, file_ext = file_path.rsplit('.', 1)
            else:
                file_name = file_path
                file_ext = None
            same_names = [x for x in conflicted_files if x.startswith(file_name)]
            if len(same_names) > 1:
                patch_path = os.path.join(patch_dir, f'{file_name}_{file_ext}.patch')
            else:
                patch_path = os.path.join(patch_dir, f'{file_name}.patch')

            os.makedirs(os.path.dirname(patch_path), exist_ok=True)
            with open(patch_path, 'w', encoding='utf-8') as f:
                subprocess.run([
                    'diff', '-u', '--label', file_path, '--label', file_path,
                    conflict_path, resolved_path
                ],
                               stdout=f)
    repo.git.add('.')
    try:
        repo.git.cherry_pick('--continue')
    except git.exc.GitCommandError as e:
        if 'The previous cherry-pick is now empty' in e.stderr:
            print('⏩ Cherry-pick is empty after conflict resolution, skipping.')
            repo.git.cherry_pick('--skip')
        else:
            raise


def main():
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest='command')

    # Linearize command
    linearize_parser = subparsers.add_parser(
        'linearize', help='Create a new branch with a linearized history.')
    linearize_parser.add_argument('--repo-path',
                                  type=str,
                                  required=True,
                                  help='The path to the local repository.')
    linearize_parser.add_argument('--source-branch',
                                  type=str,
                                  required=True,
                                  help='The branch to linearize.')
    linearize_parser.add_argument('--start-commit-ref',
                                  type=str,
                                  required=True,
                                  help='The starting commit SHA or ref.')
    linearize_parser.add_argument('--end-commit-ref',
                                  type=str,
                                  required=True,
                                  help='The ending commit SHA or ref.')
    linearize_parser.add_argument('--new-branch-name',
                                  type=str,
                                  required=True,
                                  help='The name of the new linear branch.')

    # Commits command
    commits_parser = subparsers.add_parser(
        'commits', help='Extract commits to a JSON file.')
    commits_parser.add_argument('--repo-path',
                                type=str,
                                required=True,
                                help='The path to the local repository.')
    commits_parser.add_argument('--source-branch',
                                type=str,
                                required=True,
                                help='The linear branch.')
    commits_parser.add_argument('--start-commit-ref',
                                type=str,
                                required=True,
                                help='The starting commit SHA or ref.')
    commits_parser.add_argument('--end-commit-ref',
                                type=str,
                                required=True,
                                help='The ending commit SHA or ref.')
    commits_parser.add_argument('--output-file',
                                type=str,
                                default='commits.json',
                                help='The output JSON file for the commits.')

    # Rebase command
    rebase_parser = subparsers.add_parser(
        'rebase', help='Rebase commits onto a new branch.')
    rebase_parser.add_argument('--repo-path',
                               type=str,
                               required=True,
                               help='The path to the local repository.')
    rebase_parser.add_argument('--rebase-branch',
                               type=str,
                               required=True,
                               help='The branch to rebase onto.')
    rebase_parser.add_argument(
        '--commits-file',
        type=str,
        default='commits.json',
        help='The JSON file with the commits to rebase.')
    rebase_parser.add_argument(
        '--conflicts-dir',
        type=str,
        default='conflicts',
        help='The path to the conflicts resolution directory.')
    rebase_parser.add_argument('--last-successful-commit-ref',
                               type=str,
                               default=None,
                               help='The commit SHA or ref to resume from.')
    rebase_parser.add_argument('--resume-commit-index',
                               type=int,
                               default='1',
                               help='The commit index to resume from.')
    args = parser.parse_args()

    repo = git.Repo(args.repo_path)

    if args.command == 'linearize':
        linearize_history(repo, args)
    elif args.command == 'commits':
        repo.git.checkout(args.source_branch)
        commits = list(
            repo.iter_commits(
                f'{args.start_commit_ref}..{args.end_commit_ref}'))
        commit_data = []
        for commit in commits:
            commit_trailers = commit.trailers_dict
            if 'original-hexsha' in commit_trailers:
                original_hexsha = commit.trailers_dict['original-hexsha'][0]
            else:
                original_hexsha = commit.hexsha
            stats = commit.stats
            is_linear = len(commit.parents) == 1
            commit_data.append({
                'hexsha': commit.hexsha,
                'original_hexsha':original_hexsha,
                'author': commit.author.name,
                'datetime': commit.authored_datetime.isoformat(),
                'summary': commit.summary,
                'stats': {
                    'files': stats.total['files'],
                    'insertions': stats.total['insertions'],
                    'deletions': stats.total['deletions'],
                },
                'is_linear': is_linear
            })
        with open(args.output_file, 'w', encoding='utf-8') as f:
            json.dump(commit_data, f, indent=2)
        print(f'Extracted {len(commit_data)} commits to {args.output_file}')
    elif args.command == 'rebase':
        print('Checking out rebase_branch: %s...' % args.rebase_branch)
        repo.git.checkout(args.rebase_branch)
        with open(args.commits_file, 'r', encoding='utf-8') as f:
            commits = json.load(f)
        last_successful_commit = None
        resume = args.last_successful_commit_ref is not None
        for i, commit in enumerate(reversed(commits), 1):
            if resume:
                if commit['hexsha'] == args.last_successful_commit_ref:
                    resume = False
                print(f'💤 {i}/{len(commits)} Skipped: {commit["hexsha"]}')
                continue
            if i < args.resume_commit_index:
                print(f'💤 {i}/{len(commits)} Skipped: {commit["hexsha"]}')
                continue

            if not commit['is_linear']:
                print(f'❌ Non-linear commits are not allowed: {commit["hexsha"]}')
                return
            try:
                repo.git.cherry_pick(commit['hexsha'])
                last_successful_commit = commit['hexsha']
                commit_id = repo.git.rev_parse('HEAD')
                print(
                    f'✅ {i}/{len(commits)} cherry-picked successfully: {commit["hexsha"]} as {repo.commit(commit_id).hexsha}'
                )
            except git.exc.GitCommandError as e:
                if 'The previous cherry-pick is now empty' in e.stderr:
                    print(f'⏩ {i}/{len(commits)} cherry-pick is empty, skipping: {commit["hexsha"]}')
                    repo.git.cherry_pick('--skip')
                    continue

                # If the cherry-pick fails, but the working tree is clean, it means
                # the commit was empty or already applied.
                if not repo.git.status('--porcelain'):
                    print(f'⏩ {i}/{len(commits)} cherry-pick resulted in a clean working tree, skipping: {commit["hexsha"]}')
                    # If a cherry-pick was in progress, abort it.
                    if os.path.exists(os.path.join(repo.git_dir, 'CHERRY_PICK_HEAD')):
                        repo.git.cherry_pick('--abort')
                    continue

                print(f'❌ Failed to cherry-pick: {commit["hexsha"]}')
                record_conflict(repo, os.path.join(args.conflicts_dir, commit['original_hexsha'][:7]))
                commit_id = repo.git.rev_parse('HEAD')
                print(
                    f'✅ {i}/{len(commits)} cherry-picked successfully: {commit["hexsha"]} as {repo.commit(commit_id).hexsha}\n'
                )
        print('\n🎉 SUCCESS: rebase created successfully!')


if __name__ == '__main__':
    main()
