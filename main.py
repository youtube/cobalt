import argparse
import git
import os
import shutil
import json


def find_deepest_common_dir(paths):
    if not paths:
        return '/'
    
    # Obtain directory paths for each file path
    dir_paths = [os.path.dirname(p) for p in paths]
    # Split directory paths into components
    split_paths = [p.split('/') for p in dir_paths]
    
    # Handle absolute paths correctly
    if split_paths and split_paths[0] and split_paths[0][0] == '':
        # For absolute paths, the first element is an empty string.
        # We need to preserve this to correctly join the path later.
        for p in split_paths:
            if not p or p[0] != '':
                # Mixed absolute and relative paths, no common dir
                return ''
        
        min_len = min(len(p) for p in split_paths)
        common_path = [''] # Start with root
        for i in range(1, min_len):
            if all(split_paths[j][i] == split_paths[0][i] for j in range(len(split_paths))):
                common_path.append(split_paths[0][i])
            else:
                break
        
        return os.path.join(*common_path) if len(common_path) > 1 else '/'

    else: # Handle relative paths
        min_len = min(len(p) for p in split_paths)
        
        common_path = []
        for i in range(min_len):
            if all(split_paths[j][i] == split_paths[0][i] for j in range(len(split_paths))):
                common_path.append(split_paths[0][i])
            else:
                break
                
        return '/'.join(common_path) if common_path else ''


def linearize_history(repo, args):
    """
    Creates a new branch with a linearized history.
    """
    start_commit = repo.commit(args.start_commit_ref)
    end_commit = repo.commit(args.end_commit_ref)

    # Initial Branch Setup
    if args.new_branch_name in repo.heads:
        repo.git.checkout(args.source_branch)
        repo.delete_head(args.new_branch_name, force=True)
    new_branch = repo.create_head(args.new_branch_name, start_commit)
    repo.head.reference = new_branch
    repo.head.reset(index=True, working_tree=True)
    print(f'Created new branch "{args.new_branch_name}" at {args.start_commit_ref}')

    # Identify Commits to Replay    
    print(f'Getting ancestry path from {start_commit.hexsha[:8]} to {end_commit.hexsha[:8]}...')
    ancestry_path_output = repo.git.log(
        '--first-parent',
        '--reverse',
        '--pretty=format:%H',
        f'{start_commit.hexsha}..{end_commit.hexsha}'
    )
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

        print(f'Applying commit {i}/{len(commits_to_replay)}: {commit.hexsha[:8]} [{commit_type}] {size_info}')
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
                print(f'    ✅ Tree object verified identical: {new_tree_hexsha}')
            else:
                print(f'    ⚠️ Tree differs: expected {tree.hexsha}, got {new_tree_hexsha}')
                return

            print(f'  ✍️  Committing...')
            git.objects.commit.Commit.create_from_tree(
                repo=repo,
                tree=new_tree_hexsha,
                message=commit.message + f'\noriginal-hexsha: {commit.hexsha}',
                parent_commits=[repo.head.commit],
                head=True,
                author=commit.author,
                author_date=commit.authored_datetime,
                committer=commit.committer,
                commit_date=commit.committed_datetime,
            )

            print(f'  ✅ Merge commit {i}/{len(commits_to_replay)} completed successfully')

            print(f'  🧹 Cleaning working tree...')
            repo.git.reset('--hard')
        else:
            # Regular commits: Use cherry-pick for speed
            print(f'  🍒 Regular commit - cherry-picking...')
            repo.git.cherry_pick('--no-commit', commit.hexsha)

            print(f'  ✍️  Committing...')
            repo.git.commit(
                '--allow-empty',
                '--no-verify',
                '--author', f'{commit.author.name} <{commit.author.email}>',
                '--date', commit.authored_datetime.isoformat(),
                '--message', commit.message + f'\noriginal-hexsha: {commit.hexsha}',
                env={
                    'GIT_COMMITTER_NAME': commit.committer.name,
                    'GIT_COMMITTER_EMAIL': commit.committer.email,
                    'GIT_COMMITTER_DATE': commit.committed_datetime.isoformat(),
                }
            )

            print(f'  ✅ Regular commit {i}/{len(commits_to_replay)} completed successfully')

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
    linear_commits = list(repo.iter_commits(args.new_branch_name, max_count=len(commits_to_replay)))
    linear_commits.reverse()
    verification_passed = True

    print(f'\n🔍 Verifying {len(commits_to_replay)} commits...')
    for i, (original_commit, linear_commit) in enumerate(zip(commits_to_replay, linear_commits), 1):
        print(f'  {i:2d}. Checking {original_commit.hexsha[:8]} → {linear_commit.hexsha[:8]}')

        # Compare tree objects (content equivalence)
        if original_commit.tree == linear_commit.tree:
            print(f'      ✅ Tree equivalence verified')
        else:
            print(f'      ❌ Tree mismatch!')
            verification_passed = False
            tree_diff = repo.git.diff_tree(original_commit.tree, linear_commit.tree, '--name-status')
            if tree_diff:
                diff_lines = tree_diff.splitlines()
                print(f'      {len(diff_lines)} tree differences:')
                for line in diff_lines[:10]:  # Show first 10 differences
                    print(f'        {line}')
                if len(diff_lines) > 10:
                    print(f'        ... and {len(diff_lines) - 10} more differences')

    print('\n📊 Final content verification...')
    diff = repo.git.diff(args.new_branch_name, args.end_commit_ref)
    if diff:
        print('⚠️  Working tree differs from original end commit:')
        verification_passed = False
        diff_lines = diff.splitlines()
        file_changes = [line for line in diff_lines if line.startswith('diff --git')]
        print(f'   {len(file_changes)} files differ')
        for change in file_changes[:10]:  # Show first 10 files
            print(f'   {change}')
        if len(file_changes) > 10:
            print(f'   ... and {len(file_changes) - 10} more files')
    else:
        print('✅ Working tree matches original end commit')

    # Write commit mapping to JSON file
    mapping_filename = f'{args.new_branch_name}_commit_mapping.json'
    print(f'\n📄 Writing commit mapping to {mapping_filename}...')

    with open(mapping_filename, 'w', encoding='utf-8') as f:
        import json
        output_data = {
            'linearization_info': {
                'source_branch': args.source_branch,
                'start_commit': args.start_commit_ref,
                'end_commit': args.end_commit_ref,
                'new_branch': args.new_branch_name,
                'total_commits_to_replay': len(commits_to_replay),
                'total_linearized_commits': len(commit_mapping)
            },
            'commit_mapping': commit_mapping
        }
        json.dump(output_data, f, indent=2)

    if verification_passed:
        print('\n🎉 SUCCESS: Linearized history created successfully!')
    else:
        print('\n⚠️ COMPLETED WITH ISSUES: Linearized history created but verification found differences')


def main():
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest='command')

    # Linearize command
    linearize_parser = subparsers.add_parser('linearize', help='Create a new branch with a linearized history.')
    linearize_parser.add_argument('--repo-path', type=str, required=True, help='The path to the local repository.')
    linearize_parser.add_argument('--source-branch', type=str, required=True, help='The branch to linearize.')
    linearize_parser.add_argument('--start-commit-ref', type=str, required=True, help='The starting commit SHA or ref.')
    linearize_parser.add_argument('--end-commit-ref', type=str, required=True, help='The ending commit SHA or ref.')
    linearize_parser.add_argument('--new-branch-name', type=str, required=True, help='The name of the new linear branch.')

    # Commits command
    commits_parser = subparsers.add_parser('commits', help='Extract commits to a JSON file.')
    commits_parser.add_argument('--repo-path', type=str, required=True, help='The path to the local repository.')
    commits_parser.add_argument('--start-commit-ref', type=str, required=True, help='The starting commit SHA or ref.')
    commits_parser.add_argument('--end-commit-ref', type=str, required=True, help='The ending commit SHA or ref.')
    commits_parser.add_argument('--output-file', type=str, default='commits.json', help='The output JSON file for the commits.')

    # Rebase command
    rebase_parser = subparsers.add_parser('rebase', help='Rebase commits onto a new branch.')
    rebase_parser.add_argument('--repo-path', type=str, required=True, help='The path to the local repository.')
    rebase_parser.add_argument('--base-branch', type=str, required=True, help='The base branch to rebase onto.')
    rebase_parser.add_argument('--new-branch', type=str, required=True, help='The name of the new rebased branch.')
    rebase_parser.add_argument('--commits-file', type=str, default='commits.json', help='The JSON file with the commits to rebase.')

    args = parser.parse_args()

    repo = git.Repo(args.repo_path)

    if args.command == 'linearize':
        linearize_history(repo, args)
    elif args.command == 'commits':
        commits = list(repo.iter_commits(f'{args.start_commit_ref}..{args.end_commit_ref}'))
        commit_data = []
        for commit in commits:
            stats = commit.stats
            files = list(stats.files.keys())
            common_dir = find_deepest_common_dir(files)
            is_merge = len(commit.parents) > 1
            commit_data.append({
                'hexsha': commit.hexsha,
                'author': commit.author.name,
                'datetime': commit.authored_dfatetime.isoformat(),
                'summary': commit.summary,
                'stats': {
                    'files': stats.total['files'],
                    'insertions': stats.total['insertions'],
                    'deletions': stats.total['deletions'],
                },
                'common_dir': common_dir,
                'is_merge': is_merge
            })
        with open(args.output_file, 'w', encoding='utf-8') as f:
            json.dump(commit_data, f, indent=2)
        print(f'Extracted {len(commit_data)} commits to {args.output_file}')
    elif args.command == 'rebase':
        repo.git.checkout(args.base_branch, b=args.new_branch)
        with open(args.commits_file, 'r') as f:
            commits = json.load(f)
        last_successful_commit = None
        for commit in reversed(commits):
            if commit['is_merge']:
                print(f'❌ Merge commits are not allowed: {commit['hexsha']}')
                return
            try:
                repo.git.cherry_pick(commit['hexsha'])
                last_successful_commit = commit['hexsha']
                print(f'✅ Successfully cherry-picked: {commit['hexsha']}')
            except git.exc.GitCommandError as e:
                print(f'❌ Failed to cherry-pick: {commit['hexsha']}')
                print(f'  Last successful commit: {last_successful_commit}')
                print(f'  Error: {e}')
                repo.git.cherry_pick('--abort')
                return


if __name__ == '__main__':
    main()
