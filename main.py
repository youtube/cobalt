import argparse
import git
import os
import shutil
import json


def find_deepest_common_dir(paths):
    if not paths:
        return "/"
    
    # Otain directory paths for each file path
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
                return ""
        
        min_len = min(len(p) for p in split_paths)
        common_path = [''] # Start with root
        for i in range(1, min_len):
            if all(split_paths[j][i] == split_paths[0][i] for j in range(len(split_paths))):
                common_path.append(split_paths[0][i])
            else:
                break
        
        return os.path.join(*common_path) if len(common_path) > 1 else "/"

    else: # Handle relative paths
        min_len = min(len(p) for p in split_paths)
        
        common_path = []
        for i in range(min_len):
            if all(split_paths[j][i] == split_paths[0][i] for j in range(len(split_paths))):
                common_path.append(split_paths[0][i])
            else:
                break
                
        return '/'.join(common_path) if common_path else ""


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
        '--ancestry-path',
        '--first-parent',
        '--topo-order',
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

        # Record commit mapping
        commit_mapping.append({
            'original_commit': commit.hexsha,
            'new_commit': repo.head.commit.hexsha,
            'commit_type': commit_type,
            'author': commit.author.name,
            'author_email': commit.author.email,
            'date': commit.authored_datetime.isoformat(),
            'iso': commit.authored_datetime.isoformat(),
            'subject': commit.summary
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


def reconstruct_merge_commit(repo, args):
    """
    Reconstructs a squashed merge commit as a proper merge commit with two parents.
    """
    print(f"🔀 Reconstructing merge commit...")
    
    # 1. Validate inputs
    try:
        original_commit = repo.commit(args.original_commit)
        base_commit = repo.commit(args.base_commit)
        merged_commit = repo.commit(args.merged_commit)
    except (git.exc.BadName, IndexError, git.exc.GitCommandError) as e:
        print(f"❌ Error: Invalid commit reference provided. {e}")
        return
    
    print(f"📋 Original commit: {original_commit.hexsha[:8]} - {original_commit.summary}")
    print(f"📋 Base commit (parent 1): {base_commit.hexsha[:8]} - {base_commit.summary}")  
    print(f"📋 Merged commit (parent 2): {merged_commit.hexsha[:8]} - {merged_commit.summary}")
    
    # 2. Create new branch starting from base commit
    if args.target_branch in repo.heads:
        print(f"⚠️  Branch '{args.target_branch}' already exists, deleting...")
        repo.git.checkout('main')  # Switch away from target branch
        repo.delete_head(args.target_branch, force=True)
    
    new_branch = repo.create_head(args.target_branch, base_commit)
    repo.head.reference = new_branch
    repo.head.reset(index=True, working_tree=True)
    print(f"✅ Created branch '{args.target_branch}' starting at {base_commit.hexsha[:8]}")
    
    # 3. Get the tree from the original commit
    target_tree = original_commit.tree
    print(f"🌳 Using tree from original commit: {target_tree.hexsha}")
    
    # 4. Create the merge commit with both parents
    try:
        # Get the commit message and metadata from original commit
        commit_message = original_commit.message
        author = original_commit.author
        committer = original_commit.committer
        author_date = int(original_commit.authored_datetime.timestamp())
        commit_date = int(original_commit.committed_datetime.timestamp())
        
        print(f"📝 Creating merge commit with message: {original_commit.summary}")
        
        # Use git commit-tree to create merge commit with two parents
        import subprocess
        cmd = [
            'git', 'commit-tree', target_tree.hexsha, 
            '-p', base_commit.hexsha,
            '-p', merged_commit.hexsha
        ]
        env = {
            **os.environ,
            'GIT_AUTHOR_NAME': author.name,
            'GIT_AUTHOR_EMAIL': author.email,
            'GIT_AUTHOR_DATE': f'{author_date} +0000',
            'GIT_COMMITTER_NAME': committer.name,
            'GIT_COMMITTER_EMAIL': committer.email,
            'GIT_COMMITTER_DATE': f'{commit_date} +0000',
        }
        
        result = subprocess.run(cmd, input=commit_message.encode('utf-8'), 
                              capture_output=True, env=env, cwd=repo.working_dir)
        if result.returncode != 0:
            raise Exception(f"commit-tree failed: {result.stderr.decode()}")
        
        new_commit_sha = result.stdout.decode().strip()
        print(f"🔧 Created merge commit: {new_commit_sha}")
        
        # 5. Update the branch to point to the new merge commit
        repo.git.reset('--hard', new_commit_sha)
        print(f"✅ Updated branch '{args.target_branch}' to point to new merge commit")
        
        # 6. Verify the result
        new_commit = repo.commit(new_commit_sha)
        print(f"\n🔍 Verification:")
        print(f"  New commit: {new_commit.hexsha[:8]} - {new_commit.summary}")
        print(f"  Parents: {len(new_commit.parents)} ({', '.join(p.hexsha[:8] for p in new_commit.parents)})")
        print(f"  Tree match: {'✅' if new_commit.tree == original_commit.tree else '❌'}")
        
        if new_commit.tree == original_commit.tree:
            print(f"🎉 SUCCESS: Merge commit reconstructed successfully!")
            print(f"  Original: {original_commit.hexsha[:8]} (single commit)")
            print(f"  New:      {new_commit.hexsha[:8]} (proper merge)")
            print(f"  Branch:   {args.target_branch}")
        else:
            print(f"❌ ERROR: Tree mismatch - reconstruction failed")
            
    except Exception as e:
        print(f"❌ Error creating merge commit: {e}")
        return

def compute_main_spine(repo, args):
    """
    Compute the main development spine through complex Git history using heuristics.
    """
    print(f"🧠 Computing main spine from {args.start_commit_ref} to {args.end_commit_ref}...")
    
    try:
        start_commit = repo.commit(args.start_commit_ref)
        end_commit = repo.commit(args.end_commit_ref)
    except (git.exc.BadName, IndexError, git.exc.GitCommandError) as e:
        print(f"❌ Error: Invalid commit reference provided. {e}")
        return
    
    print(f"📋 Start: {start_commit.hexsha[:8]} - {start_commit.summary}")
    print(f"📋 End: {end_commit.hexsha[:8]} - {end_commit.summary}")
    
    # 1. Get all commits in the range using ancestry-path
    print(f"\n🔍 Analyzing commit graph...")
    try:
        ancestry_path_output = repo.git.log(
            '--ancestry-path',
            '--topo-order',
            '--reverse',
            '--pretty=format:%H',
            f'{start_commit.hexsha}..{end_commit.hexsha}'
        )
        
        if ancestry_path_output.strip():
            commit_shas = ancestry_path_output.strip().split('\n')
            all_commits = [repo.commit(sha) for sha in commit_shas]
        else:
            all_commits = []
            
        print(f"📊 Found {len(all_commits)} commits in ancestry path")
    except Exception as e:
        print(f"❌ Error getting ancestry path: {e}")
        return
    
    # 2. Score each commit based on heuristics
    print(f"\n🎯 Scoring commits using heuristics...")
    scored_commits = []
    
    for commit in all_commits:
        score = score_commit_for_spine(commit)
        scored_commits.append({
            "hexsha": commit.hexsha,
            "summary": commit.summary,
            "author": commit.author.name,
            "date": commit.authored_datetime.isoformat(),
            "parents": [p.hexsha for p in commit.parents],
            "is_merge": len(commit.parents) > 1,
            "score": score,
            "score_details": score  # Will contain breakdown
        })
    
    # 3. Build the main spine path
    print(f"\n🏗️  Building main spine path...")
    spine_path = build_spine_path(scored_commits, start_commit.hexsha, end_commit.hexsha)
    
    # 4. Generate output
    spine_data = {
        "spine_info": {
            "start_commit": args.start_commit_ref,
            "end_commit": args.end_commit_ref,
            "source_branch": args.source_branch,
            "total_commits_analyzed": len(all_commits),
            "spine_commits": len(spine_path),
            "timestamp": end_commit.committed_datetime.isoformat()
        },
        "spine_path": spine_path,
        "all_commits": scored_commits  # For debugging
    }
    
    # 5. Write to file
    print(f"\n💾 Writing spine to {args.output_file}...")
    with open(args.output_file, 'w') as f:
        import json
        json.dump(spine_data, f, indent=2)
    
    print(f"✅ Main spine computed successfully!")
    print(f"📊 Spine contains {len(spine_path)} commits out of {len(all_commits)} total")
    print(f"📄 Spine saved to {args.output_file}")
    
    # Show summary
    print(f"\n📋 Spine Summary:")
    for i, commit_info in enumerate(spine_path[:10]):  # Show first 10
        print(f"   {i+1:3d}. {commit_info['hexsha'][:8]} - {commit_info['summary'][:80]}")
    if len(spine_path) > 10:
        print(f"   ... and {len(spine_path) - 10} more commits")

def score_commit_for_spine(commit):
    """
    Score a commit for inclusion in the main spine using heuristics.
    Higher score = more likely to be on main development path.
    """
    score_details = {}
    total_score = 0
    
    # 1. PR number heuristic (highest weight - machine signal)
    import re
    pr_match = re.search(r'\(#(\d+)\)', commit.summary)
    if pr_match:
        score_details["has_pr_number"] = 20  # Increased weight for PR commits
        total_score += 20
    else:
        score_details["has_pr_number"] = 0  # Neutral for no PR (not penalty)
        total_score += 0
    
    # 2. Detect workflow artifacts that should be avoided
    summary_lower = commit.summary.lower()
    
    # Heavy penalty for intermediate merge commits
    if "merge branch" in summary_lower and not pr_match:
        score_details["workflow_artifact_penalty"] = -15
        total_score -= 15
    else:
        score_details["workflow_artifact_penalty"] = 0
    
    # Penalty for "merge commit" pattern without PR
    if summary_lower.startswith("merge commit") and not pr_match:
        score_details["merge_commit_penalty"] = -10
        total_score -= 10
    else:
        score_details["merge_commit_penalty"] = 0
    
    # Penalty for revert commits without PR (intermediate reverts)
    if summary_lower.startswith("revert commit") and not pr_match:
        score_details["revert_penalty"] = -8
        total_score -= 8
    else:
        score_details["revert_penalty"] = 0
    
    # 3. Real merge commits (with 2+ parents) get moderate bonus only if they have PR
    if len(commit.parents) > 1:
        if pr_match:
            score_details["is_merge_with_pr"] = 5  # Good merge
            total_score += 5
        else:
            score_details["is_merge_with_pr"] = -3  # Intermediate merge
            total_score -= 3
    else:
        score_details["is_merge_with_pr"] = 0
    
    # 4. File count (substantial changes are more likely to be main milestones)
    try:
        file_count = len(commit.stats.files)
        if file_count > 50:
            file_score = 3  # Major changes
        elif file_count > 10:
            file_score = 1  # Medium changes
        else:
            file_score = 0  # Small changes
        score_details["file_count_score"] = file_score
        total_score += file_score
    except:
        score_details["file_count_score"] = 0
    
    # 5. Prefer substantial line changes (indicates real work vs administrative commits)
    try:
        total_changes = commit.stats.total['insertions'] + commit.stats.total['deletions']
        if total_changes > 1000:
            change_score = 2
        elif total_changes > 100:
            change_score = 1
        else:
            change_score = 0
        score_details["change_magnitude_score"] = change_score
        total_score += change_score
    except:
        score_details["change_magnitude_score"] = 0

    # 6. Identify subtree squash commits
    is_subtree_squash = False
    subtree_path = None
    if len(commit.parents) > 1:
        for p in commit.parents:
            if len(p.parents) == 0:
                is_subtree_squash = True
                break

    if is_subtree_squash:
        total_score += 50
        files = list(commit.stats.files.keys())
        # Filter out root-level files like DEPS, .pre-commit-yaml, etc.
        filtered_files = [f for f in files if '/' in f]
        subtree_path = find_deepest_common_dir(filtered_files)

    score_details["is_subtree_squash"] = is_subtree_squash
    score_details["subtree_path"] = subtree_path

    score_details["total"] = total_score
    return score_details

def are_summaries_similar(summary1, summary2):
    """
    Check if two commit summaries represent similar/duplicate work.
    Remove PR numbers and common variations before comparing.
    """
    import re
    
    # Remove PR numbers for comparison
    clean1 = re.sub(r'\s*\(#\d+\)\s*$', '', summary1).strip()
    clean2 = re.sub(r'\s*\(#\d+\)\s*$', '', summary2).strip()
    
    # Direct match after cleaning
    if clean1.lower() == clean2.lower():
        return True
    
    # Check if one is a substring of the other (with some tolerance)
    if len(clean1) > 10 and len(clean2) > 10:
        longer = clean1 if len(clean1) > len(clean2) else clean2
        shorter = clean2 if len(clean1) > len(clean2) else clean1
        
        # If shorter is 80%+ of longer and is contained, consider similar
        if len(shorter) / len(longer) > 0.8 and shorter.lower() in longer.lower():
            return True
    
    return False

def build_spine_path(scored_commits, start_sha, end_sha):
    """
    Build the main spine path by selecting highest-scoring commits and resolving author workflows.
    """
    import re
    pr_pattern = r'\(#(\d+)\)'
    
    # Phase 1: Apply author-based workflow resolution
    print(f"   🔍 Resolving author workflows...")
    
    # Group commits by author and proximity
    author_groups = {}
    for i, commit_info in enumerate(scored_commits):
        author = commit_info["author"]
        if author not in author_groups:
            author_groups[author] = []
        author_groups[author].append((i, commit_info))
    
    # Find and resolve author workflow conflicts
    commits_to_exclude = set()
    
    for author, commits in author_groups.items():
        if len(commits) < 2:
            continue  # Single commit, no conflicts
        
        # Look for workflow patterns: intermediate commits vs PR commits
        for i, (idx1, commit1) in enumerate(commits):
            for j, (idx2, commit2) in enumerate(commits):
                if i >= j or abs(idx1 - idx2) > 10:  # Only check nearby commits
                    continue
                
                commit1_has_pr = bool(re.search(pr_pattern, commit1["summary"]))
                commit2_has_pr = bool(re.search(pr_pattern, commit2["summary"]))
                
                # If one has PR and other doesn't, prefer the PR one
                if commit1_has_pr and not commit2_has_pr:
                    # Check if commit2 is workflow artifact
                    if ("merge branch" in commit2["summary"].lower() or 
                        "merge commit" in commit2["summary"].lower() or
                        commit2["score"]["total"] < 0):
                        commits_to_exclude.add(commit2["hexsha"])
                        print(f"      Excluding {commit2['hexsha'][:8]} (workflow artifact) in favor of PR {commit1['hexsha'][:8]}")
                
                elif commit2_has_pr and not commit1_has_pr:
                    # Check if commit1 is workflow artifact
                    if ("merge branch" in commit1["summary"].lower() or 
                        "merge commit" in commit1["summary"].lower() or
                        commit1["score"]["total"] < 0):
                        commits_to_exclude.add(commit1["hexsha"])
                        print(f"      Excluding {commit1['hexsha'][:8]} (workflow artifact) in favor of PR {commit2['hexsha'][:8]}")
                
                # Check for duplicate work (same author, similar summary, one with PR one without)
                if commit1_has_pr and not commit2_has_pr:
                    # Check if summaries are very similar (potential duplicate work)
                    if are_summaries_similar(commit1["summary"], commit2["summary"]):
                        commits_to_exclude.add(commit2["hexsha"])
                        print(f"      Excluding {commit2['hexsha'][:8]} (duplicate work) in favor of PR {commit1['hexsha'][:8]}")
                
                elif commit2_has_pr and not commit1_has_pr:
                    # Check if summaries are very similar (potential duplicate work)
                    if are_summaries_similar(commit1["summary"], commit2["summary"]):
                        commits_to_exclude.add(commit1["hexsha"])
                        print(f"      Excluding {commit1['hexsha'][:8]} (duplicate work) in favor of PR {commit2['hexsha'][:8]}")
    
    # Phase 2: Select commits based on improved scoring
    spine_commits = []
    excluded_count = 0
    
    for commit_info in scored_commits:
        # Skip if marked for exclusion by workflow resolution
        if commit_info["hexsha"] in commits_to_exclude:
            excluded_count += 1
            continue
            
        # Include commits with positive scores or important merges with PRs
        score = commit_info["score"]["total"]
        has_pr = bool(re.search(pr_pattern, commit_info["summary"]))
        is_merge = commit_info["is_merge"]
        
        # More selective criteria
        if (score > 0 or                           # Positive score
            (is_merge and has_pr) or               # Merge with PR
            (has_pr and score >= -5)):             # PR commits even with slightly negative score
            spine_commits.append(commit_info)
    
    print(f"   ✅ Selected {len(spine_commits)} commits for spine")
    print(f"   🚫 Excluded {excluded_count} workflow artifacts")
    print(f"   📊 Reduction: {len(scored_commits)} → {len(spine_commits)} commits")
    
    return spine_commits

def linearize_from_spine(repo, args):
    """
    Linearize using a pre-computed spine file.
    """
    print(f"🏗️  Linearizing from spine file: {args.spine_file}")
    
    # Load spine data
    try:
        with open(args.spine_file, 'r') as f:
            import json
            spine_data = json.load(f)
    except Exception as e:
        print(f"❌ Error reading spine file: {e}")
        return
    
    spine_path = spine_data["spine_path"]
    spine_info = spine_data["spine_info"]
    print(f"📊 Loaded spine with {len(spine_path)} commits")
    print(f"📋 Range: {spine_info['start_commit']} → {spine_info['end_commit']}")
    
    # Get start and end commits from spine info
    try:
        start_commit = repo.commit(spine_info["start_commit"])
        end_commit = repo.commit(spine_info["end_commit"])
    except Exception as e:
        print(f"❌ Error resolving spine commits: {e}")
        return
    
    # Create new branch starting from start_commit
    if args.new_branch_name in repo.heads:
        print(f"⚠️  Branch '{args.new_branch_name}' already exists, deleting...")
        repo.git.checkout('main')  # Switch away from target branch
        repo.delete_head(args.new_branch_name, force=True)
    
    new_branch = repo.create_head(args.new_branch_name, start_commit)
    repo.head.reference = new_branch
    repo.head.reset(index=True, working_tree=True)
    print(f"✅ Created new branch '{args.new_branch_name}' starting at {start_commit.hexsha[:8]}")
    
    # Convert spine path to commit objects
    commits_to_replay = []
    for commit_info in spine_path:
        try:
            commit = repo.commit(commit_info["hexsha"])
            commits_to_replay.append(commit)
        except Exception as e:
            print(f"⚠️  Could not resolve commit {commit_info['hexsha'][:8]}: {e}")
    
    print(f"📊 Processing {len(commits_to_replay)} commits from spine...")
    
    # Use the same linearization logic as linearize_history
    commit_mapping = []
    
    for i, commit in enumerate(commits_to_replay):
        print(f"\nApplying commit {i+1}/{len(commits_to_replay)}: {commit.hexsha[:8]} [{('MERGE' if len(commit.parents) > 1 else 'REGULAR')}] ({len(commit.stats.files)} files, +{commit.stats.total['insertions']}/-{commit.stats.total['deletions']})")
        print(f"  {commit.summary}")
        
        try:
            if len(commit.parents) > 1:
                # Handle merge commits using tree reconstruction
                print(f"  🔀 Merge commit - using tree reconstruction...")
                
                # Get the final tree from the merge commit
                target_tree = commit.tree
                print(f"    🌳 Target tree: {target_tree.hexsha}")
                
                # Reconstruct the tree in working directory
                print(f"    🔧 Reconstructing tree in working directory...")
                repo.git.read_tree('--empty')
                repo.git.read_tree(target_tree.hexsha)
                repo.git.checkout_index('--all', '--force')
                
                # Create commit with preserved metadata
                commit_message = commit.message
                author = commit.author
                committer = commit.committer
                author_date = int(commit.authored_datetime.timestamp())
                commit_date = int(commit.committed_datetime.timestamp())
                
                parent_sha = repo.head.commit.hexsha
                
                print(f"    🏗️  Creating commit object...")
                # Use git commit-tree to create the commit
                import subprocess
                cmd = [
                    'git', 'commit-tree', target_tree.hexsha, '-p', parent_sha
                ]
                env = {
                    **os.environ,
                    'GIT_AUTHOR_NAME': author.name,
                    'GIT_AUTHOR_EMAIL': author.email,
                    'GIT_AUTHOR_DATE': f'{author_date} +0000',
                    'GIT_COMMITTER_NAME': committer.name,
                    'GIT_COMMITTER_EMAIL': committer.email,
                    'GIT_COMMITTER_DATE': f'{commit_date} +0000',
                }
                result = subprocess.run(cmd, input=commit_message.encode('utf-8'), 
                                      capture_output=True, env=env, cwd=repo.working_dir)
                if result.returncode != 0:
                    raise Exception(f"commit-tree failed: {result.stderr.decode()}")
                new_commit_sha = result.stdout.decode().strip()
                
                # Update HEAD to point to new commit
                print(f"    🔄 Updating HEAD to new commit...")
                repo.git.reset('--hard', new_commit_sha)
                
                print(f"  ✅ Merge commit {i+1}/{len(commits_to_replay)} completed successfully (tree-object)")
                
            else:
                # Handle regular commits using cherry-pick
                print(f"  🍒 Regular commit - using cherry-pick...")
                print(f"    🔧 Cherry-picking {commit.hexsha[:8]}...")
                repo.git.cherry_pick('--no-commit', commit.hexsha)
                
                # Commit with preserved metadata
                commit_message = commit.message
                author = commit.author
                committer = commit.committer
                author_date = commit.authored_datetime.isoformat()
                commit_date = commit.committed_datetime.isoformat()
                
                print(f"    ✍️  Creating commit with preserved metadata...")
                repo.git.commit(
                    '--no-verify',
                    '--message', commit_message,
                    '--author', f'{author.name} <{author.email}>',
                    '--date', author_date,
                    env={
                        'GIT_COMMITTER_NAME': committer.name,
                        'GIT_COMMITTER_EMAIL': committer.email,
                        'GIT_COMMITTER_DATE': commit_date,
                    }
                )
                print(f"  ✅ Regular commit {i+1}/{len(commits_to_replay)} completed successfully (cherry-pick)")
            
            # Clean up working tree after commit
            repo.git.clean('-fdx')
            
            # Record commit mapping
            commit_mapping.append({
                "original_commit": commit.hexsha,
                "new_commit": repo.head.commit.hexsha,
                "author": commit.author.name,
                "author_email": commit.author.email,
                "date": commit.authored_datetime.isoformat(),
                "subject": commit.summary
            })
            
        except Exception as e:
            print(f"❌ ERROR processing commit {commit.hexsha[:8]}: {e}")
            print(f"   This should not happen with spine-based linearization!")
            print(f"   The spine may be incorrect or the commit is corrupted.")
            return
    
    # Write results
    mapping_filename = f"{args.new_branch_name}_spine_mapping.json"
    print(f"\n💾 Writing commit mapping to {mapping_filename}...")
    
    with open(mapping_filename, 'w') as f:
        output_data = {
            "linearization_info": {
                "method": "spine_based",
                "spine_file": args.spine_file,
                "new_branch": args.new_branch_name,
                "processed_commits": len(commit_mapping),
                "timestamp": repo.head.commit.committed_datetime.isoformat()
            },
            "spine_info": spine_info,
            "commit_mappings": commit_mapping
        }
        json.dump(output_data, f, indent=2)
    
    print(f"✅ Spine-based linearization completed successfully!")
    print(f"📊 Processed {len(commit_mapping)} commits")
    print(f"🔧 New branch '{args.new_branch_name}' created")
    print(f"📄 Mapping saved to {mapping_filename}")

def validate_spine(repo, args):
    """
    Validate a computed spine for suboptimal path selection based on PR workflow patterns.
    """
    print(f"🔍 Validating spine file: {args.spine_file}")
    
    # Load spine data
    try:
        with open(args.spine_file, 'r') as f:
            import json
            spine_data = json.load(f)
    except Exception as e:
        print(f"❌ Error reading spine file: {e}")
        return
    
    spine_path = spine_data["spine_path"]
    all_commits = spine_data["all_commits"]
    
    print(f"📊 Analyzing spine with {len(spine_path)} selected commits out of {len(all_commits)} total")
    
    # Build a map of all commits for easy lookup
    commit_map = {c["hexsha"]: c for c in all_commits}
    
    # Extract PR numbers for analysis
    import re
    pr_pattern = r'\(#(\d+)\)'
    
    # Find potential issues
    issues = []
    
    print(f"\n🎯 Checking for suboptimal path selections...")
    
    # Check each selected commit
    for i, selected_commit in enumerate(spine_path):
        selected_sha = selected_commit["hexsha"]
        selected_summary = selected_commit["summary"]
        selected_author = selected_commit["author"]
        
        # Check if this commit has a PR number
        selected_pr = re.search(pr_pattern, selected_summary)
        
        # If no PR number, look for better alternatives by same author
        if not selected_pr:
            # Find other commits by same author that DO have PR numbers
            same_author_with_pr = []
            
            for commit in all_commits:
                if (commit["author"] == selected_author and 
                    commit["hexsha"] != selected_sha):
                    
                    commit_pr = re.search(pr_pattern, commit["summary"])
                    if commit_pr:
                        same_author_with_pr.append({
                            "commit": commit,
                            "pr_number": commit_pr.group(1)
                        })
            
            if same_author_with_pr:
                # Check if any of these PR commits are close in the timeline
                selected_idx = None
                for idx, all_commit in enumerate(all_commits):
                    if all_commit["hexsha"] == selected_sha:
                        selected_idx = idx
                        break
                
                if selected_idx is not None:
                    # Look for PR commits within a reasonable range
                    window = 10  # commits before/after
                    nearby_pr_commits = []
                    
                    for pr_commit_info in same_author_with_pr:
                        pr_commit = pr_commit_info["commit"]
                        for idx, all_commit in enumerate(all_commits):
                            if (all_commit["hexsha"] == pr_commit["hexsha"] and
                                abs(idx - selected_idx) <= window):
                                nearby_pr_commits.append({
                                    **pr_commit_info,
                                    "distance": abs(idx - selected_idx),
                                    "index": idx
                                })
                    
                    if nearby_pr_commits:
                        # Find the closest PR commit
                        closest_pr = min(nearby_pr_commits, key=lambda x: x["distance"])
                        
                        issues.append({
                            "type": "missing_pr_preference",
                            "selected_commit": {
                                "sha": selected_sha[:8],
                                "summary": selected_summary,
                                "author": selected_author,
                                "spine_position": i
                            },
                            "better_alternative": {
                                "sha": closest_pr["commit"]["hexsha"][:8],
                                "summary": closest_pr["commit"]["summary"],
                                "pr_number": closest_pr["pr_number"],
                                "distance": closest_pr["distance"]
                            },
                            "recommendation": f"Prefer commit with PR #{closest_pr['pr_number']} over non-PR commit"
                        })
    
    # Report findings
    print(f"\n📋 Validation Results:")
    
    if not issues:
        print(f"✅ No obvious suboptimal selections found")
        print(f"   Spine appears to follow PR milestone pattern correctly")
    else:
        print(f"⚠️  Found {len(issues)} potential improvements:")
        
        for i, issue in enumerate(issues, 1):
            print(f"\n   Issue #{i}: {issue['type']}")
            print(f"   Current selection: {issue['selected_commit']['sha']} - {issue['selected_commit']['summary'][:60]}")
            print(f"   Better alternative: {issue['better_alternative']['sha']} - {issue['better_alternative']['summary'][:60]}")
            print(f"   Reason: {issue['recommendation']}")
            print(f"   Distance: {issue['better_alternative']['distance']} commits apart")
    
    # Show PR coverage statistics
    print(f"\n📊 PR Coverage Analysis:")
    spine_with_pr = sum(1 for c in spine_path if re.search(pr_pattern, c["summary"]))
    spine_without_pr = len(spine_path) - spine_with_pr
    
    all_with_pr = sum(1 for c in all_commits if re.search(pr_pattern, c["summary"]))
    all_without_pr = len(all_commits) - all_with_pr
    
    print(f"   Selected commits with PR: {spine_with_pr}/{len(spine_path)} ({spine_with_pr/len(spine_path)*100:.1f}%)")
    print(f"   Total commits with PR: {all_with_pr}/{len(all_commits)} ({all_with_pr/len(all_commits)*100:.1f}%)")
    print(f"   Selected commits without PR: {spine_without_pr}")
    
    if spine_without_pr > 0:
        print(f"   💡 Consider reviewing commits without PR numbers for potential alternatives")
    
    # Show author diversity
    spine_authors = set(c["author"] for c in spine_path)
    all_authors = set(c["author"] for c in all_commits)
    print(f"   Author diversity: {len(spine_authors)} authors in spine, {len(all_authors)} total")
    
    if issues:
        print(f"\n💡 Recommendation: Review the flagged commits and consider updating spine computation heuristics")
    else:
        print(f"\n✅ Spine validation passed - no obvious improvements needed")

def extract_commit_graph(repo, args):
    """
    Extract the full commit graph structure with metadata for path-finding algorithms.
    Phase 1: Get complete graph structure, not just ancestry-path.
    """
    print(f"📊 Extracting commit graph from {args.start_commit_ref} to {args.end_commit_ref}...")
    
    try:
        start_commit = repo.commit(args.start_commit_ref)
        end_commit = repo.commit(args.end_commit_ref)
    except (git.exc.BadName, IndexError, git.exc.GitCommandError) as e:
        print(f"❌ Error: Invalid commit reference provided. {e}")
        return
    
    print(f"📋 Start: {start_commit.hexsha[:8]} - {start_commit.summary}")
    print(f"📋 End: {end_commit.hexsha[:8]} - {end_commit.summary}")
    
    # Get ALL commits reachable from end_commit but not from start_commit's parents
    print(f"\n🔍 Extracting full commit graph (not just ancestry-path)...")
    
    try:
        # Use git rev-list to get all commits in the range
        # This gives us the FULL set of commits, including all branches
        all_commits_output = repo.git.rev_list(
            '--topo-order',  # Topological order
            f'{start_commit.hexsha}..{end_commit.hexsha}'
        )
        
        if all_commits_output.strip():
            commit_shas = all_commits_output.strip().split('\n')
        else:
            commit_shas = []
            
        print(f"📊 Found {len(commit_shas)} total commits in range")
        
        # Extract detailed metadata for each commit
        print(f"🔍 Extracting metadata for each commit...")
        graph_nodes = []
        
        for i, sha in enumerate(commit_shas):
            if i % 100 == 0:
                print(f"   Processing commit {i+1}/{len(commit_shas)}...")
            
            try:
                commit = repo.commit(sha)
                
                # Extract PR number if present
                import re
                pr_match = re.search(r'\(#(\d+)\)', commit.summary)
                pr_number = pr_match.group(1) if pr_match else None
                
                # Build node data
                node = {
                    "hexsha": commit.hexsha,
                    "short_sha": commit.hexsha[:8],
                    "summary": commit.summary,
                    "message": commit.message,
                    "author": {
                        "name": commit.author.name,
                        "email": commit.author.email
                    },
                    "committer": {
                        "name": commit.committer.name,
                        "email": commit.committer.email
                    },
                    "authored_date": commit.authored_datetime.isoformat(),
                    "committed_date": commit.committed_datetime.isoformat(),
                    "parents": [p.hexsha for p in commit.parents],
                    "parent_count": len(commit.parents),
                    "is_merge": len(commit.parents) > 1,
                    "pr_number": pr_number,
                    "has_pr": pr_number is not None,
                    "stats": {
                        "files_changed": len(commit.stats.files),
                        "insertions": commit.stats.total['insertions'],
                        "deletions": commit.stats.total['deletions'],
                        "total_changes": commit.stats.total['insertions'] + commit.stats.total['deletions']
                    }
                }
                
                graph_nodes.append(node)
                
            except Exception as e:
                print(f"⚠️  Error processing commit {sha[:8]}: {e}")
                continue
        
        print(f"✅ Extracted metadata for {len(graph_nodes)} commits")
        
        # Build graph structure data
        graph_data = {
            "extraction_info": {
                "start_commit": args.start_commit_ref,
                "end_commit": args.end_commit_ref,
                "source_branch": args.source_branch,
                "total_commits": len(graph_nodes),
                "extraction_method": "rev-list_full_range",
                "timestamp": end_commit.committed_datetime.isoformat()
            },
            "nodes": graph_nodes,
            "test_cases": [
                {
                    "name": "duplicate_work_detection",
                    "description": "Andrew Savage's duplicate work: PR vs non-PR version",
                    "commits": ["3918e8816a2a0982fee8bca2fa0e41adc82ff08c", "da7b2b40cdc3fd83d761dc0cb9928c20fcd61fab"],
                    "expected_choice": "3918e8816a2a0982fee8bca2fa0e41adc82ff08c",
                    "reason": "Choose PR commit over duplicate non-PR commit"
                },
                {
                    "name": "merge_branch_vs_pr",
                    "description": "Charley's workflow: merge branch vs final PR",
                    "commits": ["1008d9cd4df1d53990695c9d225b322d758e9ea3", "6e406d3baf63e9bd05d7180b7c22cd25ee52631d"],
                    "expected_choice": "6e406d3baf63e9bd05d7180b7c22cd25ee52631d",
                    "reason": "Choose final PR over intermediate merge branch"
                },
                {
                    "name": "main_vs_derp_branch",
                    "description": "Avoid obviously named side branches",
                    "commits": ["23a99b1ce869ad70b0a93a1bcc0ad8f863c14b48"],
                    "expected_choice": None,
                    "reason": "Merge branch 'main' into derp should be excluded"
                }
            ]
        }
        
        # Write to file
        print(f"\n💾 Writing graph data to {args.output_file}...")
        with open(args.output_file, 'w') as f:
            import json
            json.dump(graph_data, f, indent=2)
        
        # Show statistics
        pr_commits = sum(1 for node in graph_nodes if node["has_pr"])
        merge_commits = sum(1 for node in graph_nodes if node["is_merge"])
        authors = len(set(node["author"]["name"] for node in graph_nodes))
        
        print(f"✅ Graph extraction completed!")
        print(f"📊 Statistics:")
        print(f"   Total commits: {len(graph_nodes)}")
        print(f"   Commits with PR: {pr_commits} ({pr_commits/len(graph_nodes)*100:.1f}%)")
        print(f"   Merge commits: {merge_commits}")
        print(f"   Unique authors: {authors}")
        print(f"📄 Graph saved to {args.output_file}")
        print(f"🧪 Test cases included for unit testing")
        
    except Exception as e:
        print(f"❌ Error extracting graph: {e}")
        return

def find_optimal_path_through_dag(graph_data):
    """
    Find the optimal single path through the commit DAG using PR-prioritization.
    This is the core Phase 2 algorithm.
    """
    nodes = graph_data["nodes"]
    
    # Build lookup maps
    node_map = {node["hexsha"]: node for node in nodes}
    children_map = {}  # parent_sha -> [child_shas]
    
    # Build parent->children mapping
    for node in nodes:
        for parent_sha in node["parents"]:
            if parent_sha not in children_map:
                children_map[parent_sha] = []
            children_map[parent_sha].append(node["hexsha"])
    
    # Find start and end nodes
    start_ref = graph_data["extraction_info"]["start_commit"] 
    end_ref = graph_data["extraction_info"]["end_commit"]
    
    # Find end commit in graph
    end_sha = None
    for node in nodes:
        if node["hexsha"].startswith(end_ref) or node["short_sha"] == end_ref:
            end_sha = node["hexsha"]
            break
    
    if not end_sha:
        print(f"❌ Could not find end commit ({end_ref}) in graph")
        return None
    
    # For start, find the commit with no parents within our graph
    # (since git rev-list start..end excludes the start commit)
    potential_starts = []
    for node in nodes:
        # Check if any of this node's parents are in our graph
        parents_in_graph = [p for p in node["parents"] if p in node_map]
        if len(parents_in_graph) == 0:
            potential_starts.append(node["hexsha"])
    
    if len(potential_starts) == 0:
        print(f"❌ Could not find any starting point in graph")
        return None
    
    # Use the first potential start (could be improved with better heuristics)
    start_sha = potential_starts[0]
    
    print(f"🧭 Finding optimal path from {start_sha[:8]} to {end_sha[:8]}...")
    print(f"📊 Graph has {len(nodes)} nodes to navigate")
    
    # Work backwards from tip to origin using greedy selection with strong PR bias
    def find_backwards_path():
        path = [end_sha]  # Start from the tip
        current_sha = end_sha
        
        print(f"   Starting backwards traversal from tip: {current_sha[:8]}")
        
        while True:
            current_node = node_map[current_sha]
            parents = current_node["parents"]
            
            # Filter parents to only those in our graph
            valid_parents = [p for p in parents if p in node_map]
            
            if not valid_parents:
                # Reached a root node
                print(f"   Reached root node: {current_sha[:8]}")
                break
            
            if len(valid_parents) == 1:
                # No choice, follow the single parent
                current_sha = valid_parents[0]
                path.append(current_sha)
                continue
            
            # FORK DECISION: Multiple parents available
            print(f"   Fork at {current_sha[:8]} - choosing from {len(valid_parents)} parents")
            
            # Score each parent path based on PR presence and other factors
            parent_scores = []
            for parent_sha in valid_parents:
                parent_node = node_map[parent_sha]
                
                # Strong bias for PR commits
                pr_score = 100 if parent_node["has_pr"] else 0
                
                # Penalty for obvious workflow artifacts
                artifact_penalty = 0
                summary_lower = parent_node["summary"].lower()
                if "merge branch" in summary_lower and not parent_node["has_pr"]:
                    artifact_penalty = -50
                elif summary_lower.startswith("fix ") and not parent_node["has_pr"]:
                    artifact_penalty = -20
                elif summary_lower.startswith("revert ") and not parent_node["has_pr"]:
                    artifact_penalty = -30
                
                # Bonus for substantial changes (real work vs minor fixes)
                change_bonus = min(parent_node["stats"]["total_changes"] / 100, 10)
                
                total_score = pr_score + artifact_penalty + change_bonus
                
                parent_scores.append({
                    "sha": parent_sha,
                    "score": total_score,
                    "pr_score": pr_score,
                    "artifact_penalty": artifact_penalty,
                    "change_bonus": change_bonus,
                    "summary": parent_node["summary"][:50]
                })
            
            # Choose the highest scoring parent
            best_parent = max(parent_scores, key=lambda p: p["score"])
            
            print(f"      Choices at fork:")
            for ps in sorted(parent_scores, key=lambda p: p["score"], reverse=True):
                marker = "✅" if ps["sha"] == best_parent["sha"] else "  "
                print(f"      {marker} {ps['sha'][:8]} (score: {ps['score']:3.0f}) - {ps['summary']}")
            
            # Move to the chosen parent
            current_sha = best_parent["sha"]
            path.append(current_sha)
            
            # Safety check to prevent infinite loops
            if len(path) > len(nodes):
                print(f"   ⚠️  Path length exceeded node count, stopping")
                break
        
        # Reverse to get chronological order (origin to tip)
        path.reverse()
        return path
    
    # Find the optimal path using backwards greedy traversal
    optimal_path = find_backwards_path()
    
    if optimal_path:
        print(f"✅ Found optimal path with {len(optimal_path)} commits")
        
        # Build path details
        path_details = []
        for sha in optimal_path:
            node = node_map[sha]
            path_details.append({
                "hexsha": sha,
                "short_sha": sha[:8],
                "summary": node["summary"],
                "has_pr": node["has_pr"],
                "pr_number": node["pr_number"],
                "is_merge": node["is_merge"],
                "author": node["author"]["name"]
            })
        
        # Show path statistics
        pr_commits = sum(1 for node in path_details if node["has_pr"])
        print(f"📊 Path statistics:")
        print(f"   Total commits in path: {len(path_details)}")
        print(f"   Commits with PR: {pr_commits} ({pr_commits/len(path_details)*100:.1f}%)")
        
        return {
            "path": path_details,
            "statistics": {
                "total_commits": len(path_details),
                "pr_commits": pr_commits,
                "pr_percentage": pr_commits/len(path_details)*100 if path_details else 0
            }
        }
    else:
        print(f"❌ Could not find path from start to end")
        return None

def compute_spine_from_graph(repo, args):
    """
    Compute spine using pre-extracted graph data and DAG path-finding.
    """
    print(f"🧠 Computing spine from graph file: {args.extracted_file}")
    
    # Load graph data
    try:
        with open(args.extracted_file, 'r') as f:
            import json
            graph_data = json.load(f)
    except Exception as e:
        print(f"❌ Error reading graph file: {e}")
        return
    
    print(f"📊 Loaded graph with {len(graph_data['nodes'])} commits")
    
    # Find optimal path using DAG algorithm
    result = find_optimal_path_through_dag(graph_data)
    
    if result:
        # Create spine output
        spine_data = {
            "spine_info": {
                **graph_data["extraction_info"],
                "method": "dag_pathfinding",
                "spine_commits": result["statistics"]["total_commits"]
            },
            "spine_path": result["path"],
            "statistics": result["statistics"],
            "test_validation": []
        }
        
        # Validate against test cases
        print(f"\n🧪 Validating against test cases...")
        for test_case in graph_data.get("test_cases", []):
            validation_result = validate_path_against_test_case(result["path"], test_case)
            spine_data["test_validation"].append(validation_result)
            
            status = "✅ PASS" if validation_result["passed"] else "❌ FAIL"
            print(f"   {status} {test_case['name']}: {validation_result['message']}")
        
        # Write result
        with open(args.output_file, 'w') as f:
            json.dump(spine_data, f, indent=2)
        
        print(f"\n✅ Spine computed successfully!")
        print(f"📄 Saved to {args.output_file}")
        
        # Show first few commits in path
        print(f"\n📋 Spine path preview:")
        for i, commit in enumerate(result["path"][:10]):
            pr_indicator = f"#{commit['pr_number']}" if commit['has_pr'] else "no-PR"
            print(f"   {i+1:3d}. {commit['short_sha']} - {commit['summary'][:60]} ({pr_indicator})")
        
        if len(result["path"]) > 10:
            print(f"   ... and {len(result['path']) - 10} more commits")
    
    else:
        print(f"❌ Failed to compute spine - no valid path found")

def validate_path_against_test_case(spine_path, test_case):
    """
    Validate that the computed path makes the right choices for a test case.
    """
    path_shas = set(commit["hexsha"] for commit in spine_path)
    test_commit_shas = test_case["commits"]
    expected_choice = test_case["expected_choice"]
    
    # Check what commits from the test case are in the path
    included_commits = [sha for sha in test_commit_shas if sha in path_shas]
    
    if expected_choice is None:
        # Test case expects NO commits to be included
        passed = len(included_commits) == 0
        message = f"Expected no commits, found {len(included_commits)}" if not passed else "Correctly excluded all commits"
    else:
        # Test case expects specific commit to be chosen
        passed = expected_choice in included_commits and len(included_commits) == 1
        if passed:
            message = f"Correctly chose {expected_choice[:8]}"
        else:
            message = f"Expected {expected_choice[:8]}, but path includes: {[sha[:8] for sha in included_commits]}"
    
    return {
        "test_name": test_case["name"],
        "passed": passed,
        "message": message,
        "included_commits": [sha[:8] for sha in included_commits],
        "expected": expected_choice[:8] if expected_choice else "none"
    }

def main():
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest="command")

    # Linearize command
    linearize_parser = subparsers.add_parser("linearize", help="Create a new branch with a linearized history.")
    linearize_parser.add_argument("--repo-path", type=str, required=True, help="The path to the local repository.")
    linearize_parser.add_argument("--source-branch", type=str, required=True, help="The branch to linearize.")
    linearize_parser.add_argument("--start-commit-ref", type=str, required=True, help="The starting commit SHA or ref.")
    linearize_parser.add_argument("--end-commit-ref", type=str, required=True, help="The ending commit SHA or ref.")
    linearize_parser.add_argument("--new-branch-name", type=str, required=True, help="The name of the new linear branch.")

    # Commits command
    commits_parser = subparsers.add_parser("commits", help="Extract commits to a JSON file.")
    commits_parser.add_argument("--repo-path", type=str, required=True, help="The path to the local repository.")
    commits_parser.add_argument("--start-commit-ref", type=str, required=True, help="The starting commit SHA or ref.")
    commits_parser.add_argument("--end-commit-ref", type=str, required=True, help="The ending commit SHA or ref.")
    commits_parser.add_argument("--output-file", type=str, default="commits.json", help="The output JSON file for the commits.")

    # Rebase command
    rebase_parser = subparsers.add_parser("rebase", help="Rebase commits onto a new branch.")
    rebase_parser.add_argument("--repo-path", type=str, required=True, help="The path to the local repository.")
    rebase_parser.add_argument("--base-branch", type=str, required=True, help="The base branch to rebase onto.")
    rebase_parser.add_argument("--new-branch", type=str, required=True, help="The name of the new rebased branch.")
    rebase_parser.add_argument("--commits-file", type=str, default="commits.json", help="The JSON file with the commits to rebase.")

    
    
    # Reconstruct merge command
    reconstruct_parser = subparsers.add_parser("reconstruct-merge", help="Reconstruct a squashed merge as a proper merge commit.")
    reconstruct_parser.add_argument("--repo-path", type=str, default=".", help="Local path to repository (default: current directory).")
    reconstruct_parser.add_argument("--target-branch", type=str, required=True, help="Name of the new branch to create with the reconstructed merge.")
    reconstruct_parser.add_argument("--original-commit", type=str, required=True, help="The original squashed commit to reconstruct.")
    reconstruct_parser.add_argument("--base-commit", type=str, required=True, help="The base commit (first parent of the merge).")
    reconstruct_parser.add_argument("--merged-commit", type=str, required=True, help="The commit that was merged (second parent of the merge).")
    
    # Extract commit data command (Phase 1)
    extract_parser = subparsers.add_parser("extract-commits", help="Extract commit data for spine computation (Phase 1).")
    extract_parser.add_argument("--repo-path", type=str, default=".", help="Local path to repository (default: current directory).")
    extract_parser.add_argument("--start-commit-ref", type=str, required=True, help="The starting commit SHA or ref.")
    extract_parser.add_argument("--end-commit-ref", type=str, required=True, help="The ending commit SHA or ref.")
    extract_parser.add_argument("--output-file", type=str, required=True, help="Output JSON file for extracted commit data.")
    extract_parser.add_argument("--source-branch", type=str, default="main", help="The source branch to analyze (default: main).")
    
    # Compute spine command (now Phase 2)
    spine_parser = subparsers.add_parser("compute-spine", help="Compute the main spine path for linearization.")
    spine_parser.add_argument("--repo-path", type=str, default=".", help="Local path to repository (default: current directory).")
    spine_parser.add_argument("--start-commit-ref", type=str, required=True, help="The starting commit SHA or ref.")
    spine_parser.add_argument("--end-commit-ref", type=str, required=True, help="The ending commit SHA or ref.")
    spine_parser.add_argument("--output-file", type=str, required=True, help="Output JSON file for the computed spine.")
    spine_parser.add_argument("--source-branch", type=str, default="main", help="The source branch to analyze (default: main).")
    spine_parser.add_argument("--from-extracted", type=str, help="Use pre-extracted commit data from this JSON file.")
    
    # Compute spine from extracted data only
    spine_only_parser = subparsers.add_parser("compute-spine-only", help="Compute spine from pre-extracted commit data (fast iteration).")
    spine_only_parser.add_argument("--extracted-file", type=str, required=True, help="JSON file with extracted commit data.")
    spine_only_parser.add_argument("--output-file", type=str, required=True, help="Output JSON file for the computed spine.")
    
    # Linearize from spine command  
    spine_linearize_parser = subparsers.add_parser("linearize-from-spine", help="Linearize using a pre-computed spine.")
    spine_linearize_parser.add_argument("--repo-path", type=str, default=".", help="Local path to repository (default: current directory).")
    spine_linearize_parser.add_argument("--spine-file", type=str, required=True, help="JSON file containing the pre-computed spine.")
    spine_linearize_parser.add_argument("--new-branch-name", type=str, required=True, help="The name of the new linear branch.")
    
    # Validate spine command
    validate_spine_parser = subparsers.add_parser("validate-spine", help="Validate and analyze a computed spine for suboptimal paths.")
    validate_spine_parser.add_argument("--repo-path", type=str, default=".", help="Local path to repository (default: current directory).")
    validate_spine_parser.add_argument("--spine-file", type=str, required=True, help="JSON file containing the spine to validate.")
    
    args = parser.parse_args()

    repo = git.Repo(args.repo_path)

    if args.command == "linearize":
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
                'date': commit.authored_datetime.isoformat(),
                'summary': commit.summary,
                'stats': {
                    'insertions': stats.total['insertions'],
                    'deletions': stats.total['deletions'],
                    'lines': stats.total['lines'],
                    'files': stats.total['files'],
                },
                'common_dir': common_dir,
                'is_merge': is_merge
            })
        with open(args.output_file, 'w', encoding='utf-8') as f:
            json.dump(commit_data, f, indent=2)
        print(f'Extracted {len(commit_data)} commits to {args.output_file}')
    elif args.command == "rebase":
        # Create and checkout the new branch
        repo.git.checkout(args.base_branch, b=args.new_branch)

        with open(args.commits_file, "r") as f:
            commits = json.load(f)

        last_successful_commit = None
        for commit in reversed(commits):
            if commit["is_merge"]:
                print(f"Skipping merge commit {commit['hexsha']}")
                continue

            try:
                repo.git.cherry_pick(commit["hexsha"])
                last_successful_commit = commit["hexsha"]
                print(f"Successfully cherry-picked {commit['hexsha']}")
            except git.exc.GitCommandError as e:
                print(f"Failed to cherry-pick {commit['hexsha']}")
                print(f"Last successful commit: {last_successful_commit}")
                print(f"Error: {e}")
                repo.git.cherry_pick("--abort")
                break
    elif args.command == "reconstruct-merge":
        reconstruct_merge_commit(repo, args)
    elif args.command == "compute-spine":
        compute_main_spine(repo, args)
    elif args.command == "linearize-from-spine":
        linearize_from_spine(repo, args)
    elif args.command == "validate-spine":
        validate_spine(repo, args)
    elif args.command == "extract-commits":
        extract_commit_graph(repo, args)
    elif args.command == "compute-spine-only":
        compute_spine_from_graph(None, args)  # No repo needed, works from graph file


if __name__ == "__main__":
    main()