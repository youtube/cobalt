#!/usr/bin/env python3
"""Separates autoroll baseline commits from human/AI fix commits.

A Cobalt autoroll PR opens with five roller-generated commits before any
fix work begins. For the M143 roll these were, oldest first:

  CONFLICTED Chromium Cherry pick: Revert Cobalt.
  Restore submodules.
  Update to 143.7471.
  Remove submodules.
  CONFLICTED Cherry pick commit <sha>: Update to 143.7471.

Comparing two rebase attempts is only meaningful across the *fix*
commits. If the baseline leaks into either diff, both sides share the
entire Chromium roll and similarity scores approach 100% no matter how
the fixes differ.

Partitioning is therefore anchored on the commit subject rather than on
a hardcoded count: the baseline always ends with the "CONFLICTED Cherry
pick ...: Update to <version>" commit. Counting to five breaks silently
the moment the roller changes shape, and the failure inflates the score.
"""

import re
import subprocess
from typing import Any, Dict, List, Optional, Tuple

# The final baseline commit. Everything after it is fix work.
ROLL_ANCHOR_PATTERN = re.compile(r"^CONFLICTED Cherry pick.*Update to [0-9]")

# The remaining baseline subjects, kept for validation and logging.
ROLL_BASELINE_PATTERNS = (
    re.compile(r"^CONFLICTED Chromium Cherry pick:\s*Revert Cobalt"),
    re.compile(r"^Restore submodules"),
    re.compile(r"^Update to [0-9]"),
    re.compile(r"^Remove submodules"),
    ROLL_ANCHOR_PATTERN,
)


class RollPartitionError(Exception):
  """Raised when the roll baseline cannot be identified with confidence."""


def _subject(commit: Dict[str, Any]) -> str:
  """Returns a commit's subject line from gh JSON."""
  headline = commit.get("messageHeadline")
  if headline:
    return headline
  message = commit.get("message") or ""
  lines = message.splitlines()
  return lines[0] if lines else ""


def is_baseline_subject(subject: str) -> bool:
  """Reports whether a commit subject looks roller-generated."""
  return any(p.match(subject) for p in ROLL_BASELINE_PATTERNS)


def partition_roll_commits(
    commits: List[Dict[str, Any]]
) -> Tuple[List[Dict[str, Any]], List[Dict[str, Any]]]:
  """Splits commits into (roller baseline, fix commits).

  Commits must be in chronological order, oldest first, as returned by
  `gh pr view --json commits`.

  Raises:
    RollPartitionError: if no roll anchor commit is present. Callers must
      not fall back to the full PR diff, which would silently reintroduce
      the entire Chromium roll into the comparison.
  """
  if not commits:
    raise RollPartitionError("PR contains no commits.")

  anchor_idx = -1
  for idx, commit in enumerate(commits):
    if ROLL_ANCHOR_PATTERN.match(_subject(commit)):
      anchor_idx = idx

  if anchor_idx < 0:
    subjects = "\n".join(f"  - {_subject(c)}" for c in commits[:10])
    raise RollPartitionError(
        "Could not locate the roll anchor commit matching "
        f"'{ROLL_ANCHOR_PATTERN.pattern}'.\nFirst commits were:\n{subjects}\n"
        "Refusing to guess: including roller commits in the comparison "
        "inflates similarity toward 100%.")

  return commits[:anchor_idx + 1], commits[anchor_idx + 1:]


def describe_partition(label: str, baseline: List[Dict[str, Any]],
                       fixes: List[Dict[str, Any]]) -> str:
  """Renders an auditable description of how a PR was partitioned."""
  lines = [f"{label}: {len(baseline)} baseline + {len(fixes)} fix commit(s)"]
  for commit in baseline:
    lines.append(f"    [baseline] {_subject(commit)}")
  for commit in fixes:
    lines.append(f"    [fix]      {_subject(commit)}")
  return "\n".join(lines)


def _run(cmd: List[str], cwd: Optional[str]) -> Tuple[bool, str]:
  """Runs a command, returning (succeeded, stdout)."""
  try:
    proc = subprocess.run(
        cmd,
        cwd=cwd,
        capture_output=True,
        text=True,
        errors="replace",
        timeout=120,
        check=False,
    )
    return proc.returncode == 0, proc.stdout
  except (OSError, subprocess.SubprocessError):
    return False, ""


def resolve_fix_diff(
    baseline: List[Dict[str, Any]],
    fixes: List[Dict[str, Any]],
    repo_root: str,
    github_repo: str = "youtube/cobalt",
) -> str:
  """Returns the combined diff of the fix commits only.

  Tries a local range diff first, then per-commit diffs fetched from the
  GitHub API. Both paths are restricted to fix commits.

  Notably absent is a `gh pr diff` fallback. That returns the whole PR
  including the roller baseline, which is precisely the contamination
  this module exists to prevent, and it fails invisibly.

  Raises:
    RollPartitionError: if no fix diff can be produced.
  """
  if not fixes:
    return ""

  base_sha = baseline[-1].get("oid", "") if baseline else ""
  last_sha = fixes[-1].get("oid", "")

  if base_sha and last_sha:
    have_base, _ = _run(["git", "cat-file", "-e", f"{base_sha}^{{commit}}"],
                        repo_root)
    have_last, _ = _run(["git", "cat-file", "-e", f"{last_sha}^{{commit}}"],
                        repo_root)
    if have_base and have_last:
      ok, out = _run(["git", "diff", f"{base_sha}..{last_sha}"], repo_root)
      if ok and out.strip():
        return out

  # Per-commit fallback. Safe: only fix commits are fetched.
  chunks = []
  for commit in fixes:
    sha = commit.get("oid", "")
    if not sha:
      continue
    ok, out = _run(["git", "show", "--stat", "-p", sha], repo_root)
    if not (ok and out.strip()):
      ok, out = _run([
          "gh", "api", f"repos/{github_repo}/commits/{sha}", "-H",
          "Accept: application/vnd.github.v3.diff"
      ], repo_root)
    if ok and out.strip():
      chunks.append(out)

  if not chunks:
    raise RollPartitionError(
        f"Could not resolve a diff for {len(fixes)} fix commit(s). "
        "The local checkout may be missing these objects and the GitHub "
        "API fetch also failed. Refusing to fall back to the full PR diff.")

  return "\n\n".join(chunks)
