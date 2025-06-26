#!/usr/bin/env python3
"""
A standalone Python script to reliably list the names of conflicted files
in the current Git repository tree.

This script uses the GitPython library to inspect the repository's index
for unmerged entries, which is the most reliable way to identify files
that are in a conflicted state (e.g., during a rebase or merge).
"""

import os
import sys
from git import Repo, InvalidGitRepositoryError

def get_conflicted_files(repo):
    """
    Identifies files that are currently in a conflicted (unmerged) state
    by checking the repository's index.

    Args:
        repo (git.Repo): The repository object to inspect.

    Returns:
        list: A list of file paths (relative to the repo root) that are
              currently in a conflicted state. The list is sorted for
              consistent output.
    """
    conflicted_files = set()
    
    # The unmerged_blobs() method returns a dictionary where keys are the
    # paths of files with unmerged changes (i.e., conflicts).
    # Each value is a dict of stage -> blob pairs.
    # Stage 1: Common ancestor (the 'base')
    # Stage 2: Our version (the 'HEAD' or 'target' branch in a rebase)
    # Stage 3: Their version (the commit being rebased)
    unmerged = repo.index.unmerged_blobs()
    
    for path in unmerged:
        conflicted_files.add(path)
        
    return sorted(list(conflicted_files))

def main():
    """
    Main function to find and print conflicted files.
    """
    try:
        # Instantiate the repository object from the current working directory.
        # search_parent_directories=True allows this to work even if run in a subdirectory.
        repo = Repo(os.getcwd(), search_parent_directories=True)
    except InvalidGitRepositoryError:
        print("Error: This script is not running within a Git repository.", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"An unexpected error occurred: {e}", file=sys.stderr)
        sys.exit(1)

    conflicted_files = get_conflicted_files(repo)

    if not conflicted_files:
        print("No conflicted files found in the current Git tree.")
    else:
        print("Conflicted files:")
        for file_path in conflicted_files:
            print(file_path)

if __name__ == "__main__":
    main()
