# Subtree Squash Commit Identification

## Task

The goal was to identify "subtree squash" commits during the spine computation and annotate them in the output JSON file. This involved not only flagging these commits but also extracting the path of the squashed subtree.

## Solution

The final implementation uses a combination of heuristics to identify subtree squash commits within the `score_commit_for_spine` function in the `main.py` script.

A commit is identified as a subtree squash if it meets the following criteria:

1.  **It is a merge commit:** The commit must have more than one parent.
2.  **It has a 1-ancestor parent:** One of the commit's parents must be a root commit (i.e., have no parents).

While many of these commits have summaries that start with "Import ", this is not used as part of the identification logic. It is, however, a good indicator for humans to eyeball.

A good example of this is the commit `3ebe510d7d862`:
```
$ git log --oneline --graph 142e601cd3512..b98699d2e866b
* 29c0519a3c3e0 Add __stack_chk_fail from musl (#6035)
*   3ebe510d7d862 Import QUICHE from commit 02c69dd2
|\  
| * 0673d35f67c08 Change SpendTokenData to use PublicMetadata instead of PublicMetadataInfo, and update its users.
* a2c10c351071e [android] Support OnFirstTunnelFrameReady callback (#5996)
```
In this example, the commit `3ebe510d7d862` is a merge commit, and one of its parents, `0673d35f67c08`, has no parents in this view, indicating it's the result of a squashed subtree.

Once a commit is identified as a subtree squash, the script attempts to determine the path of the squashed subtree. This is done by:

1.  Getting the list of all files changed in the commit.
2.  Filtering out any files that are in the root directory of the repository (e.g., `DEPS`, `.gitignore`).
3.  Finding the deepest common directory among the remaining files using the `find_deepest_common_dir` function.

The `is_subtree_squash` (boolean) and `subtree_path` (string) are then added to the `score_details` dictionary for the commit, which is then written to the final `spine.json` file.