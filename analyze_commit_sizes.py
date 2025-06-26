#!/usr/bin/env python3
"""
A standalone Python script to analyze the size of commits from a JSON file,
generate a JSON file with the sizes, and plot a histogram of the sizes.

This script requires the GitPython and matplotlib libraries.
You can install them with:
    pip install GitPython matplotlib
"""

import os
import sys
import json
import argparse
from git import Repo, InvalidGitRepositoryError
import matplotlib.pyplot as plt

def get_commit_sizes(repo, commit_shas):
    """
    Calculates the size (total lines changed) for a list of commits.

    Args:
        repo (git.Repo): The repository object to inspect.
        commit_shas (list): A list of commit SHAs to analyze.

    Returns:
        dict: A dictionary mapping each commit SHA to its size.
    """
    commit_sizes = {}
    for i, sha in enumerate(commit_shas):
        try:
            commit = repo.commit(sha)
            stats = commit.stats.total
            size = stats['insertions'] + stats['deletions']
            commit_sizes[sha] = size
            if (i + 1) % 50 == 0:
                print(f"Processed {i + 1}/{len(commit_shas)} commits...")
        except Exception as e:
            print(f"Could not process commit {sha}: {e}", file=sys.stderr)
    return commit_sizes

def plot_histogram(sizes, output_image_path):
    """
    Generates and saves a histogram of the commit sizes.

    Args:
        sizes (list): A list of commit sizes.
        output_image_path (str): The path to save the histogram image.
    """
    plt.figure(figsize=(12, 6))
    plt.hist(sizes, bins=50, color='skyblue', edgecolor='black')
    plt.title('Distribution of Commit Sizes')
    plt.xlabel('Total Lines Changed (Insertions + Deletions)')
    plt.ylabel('Number of Commits')
    plt.grid(axis='y', alpha=0.75)
    plt.savefig(output_image_path)
    print(f"Histogram saved to {output_image_path}")

def main():
    """
    Main function to parse arguments and run the analysis.
    """
    parser = argparse.ArgumentParser(
        description="Analyze commit sizes from a JSON file and plot a histogram."
    )
    parser.add_argument(
        "spine_file",
        help="Path to the JSON file containing the spine commits (e.g., full_spine_final.json)."
    )
    parser.add_argument(
        "--repo-path",
        default=".",
        help="Path to the Git repository (default: current directory)."
    )
    parser.add_argument(
        "--output-json",
        default="commit_sizes.json",
        help="Path to save the output JSON file with commit sizes."
    )
    parser.add_argument(
        "--output-image",
        default="commit_sizes_histogram.png",
        help="Path to save the output histogram image."
    )
    args = parser.parse_args()

    # Validate spine file path
    if not os.path.exists(args.spine_file):
        print(f"Error: Spine file not found at '{args.spine_file}'", file=sys.stderr)
        sys.exit(1)

    # Load commit SHAs from the spine file
    try:
        with open(args.spine_file, 'r') as f:
            spine_data = json.load(f)
        commit_shas = [commit['hexsha'] for commit in spine_data.get('all_commits', [])]
        if not commit_shas:
            print("Error: No commits found in the spine file.", file=sys.stderr)
            sys.exit(1)
    except (json.JSONDecodeError, KeyError) as e:
        print(f"Error reading or parsing spine file: {e}", file=sys.stderr)
        sys.exit(1)

    # Initialize the repository
    try:
        repo = Repo(args.repo_path, search_parent_directories=True)
    except InvalidGitRepositoryError:
        print(f"Error: '{args.repo_path}' is not a valid Git repository.", file=sys.stderr)
        sys.exit(1)

    # Get commit sizes
    print(f"Analyzing {len(commit_shas)} commits from '{args.spine_file}'...")
    commit_sizes = get_commit_sizes(repo, commit_shas)

    # Save commit sizes to JSON
    with open(args.output_json, 'w') as f:
        json.dump(commit_sizes, f, indent=2)
    print(f"Commit sizes saved to '{args.output_json}'")

    # Plot histogram
    sizes = list(commit_sizes.values())
    if sizes:
        plot_histogram(sizes, args.output_image)
    else:
        print("No commit sizes to plot.")

if __name__ == "__main__":
    main()
