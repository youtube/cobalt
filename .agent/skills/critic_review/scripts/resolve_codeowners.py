#!/usr/bin/env python3
"""Deterministic GitHub CODEOWNERS resolver for critic-review skill.

Resolves modified files or pull requests against .github/CODEOWNERS according to
exact GitHub specification (sequential evaluation, last-matching-line-wins,
directory inheritance, multi-team co-ownership, and explicit unowned clearing).

Supports direct file inputs, local git diff, and GitHub CLI (gh) integration.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import subprocess
import sys
from typing import Any, Dict, List, Optional, Set, Tuple


def pattern_to_regex(pattern: str) -> re.Pattern:
  """Translate a gitignore/CODEOWNERS pattern to a compiled regex."""
  p = pattern.strip()
  anchored = p.startswith("/")
  if anchored:
    p = p[1:]
  dir_only = p.endswith("/")
  if dir_only:
    p = p[:-1]

  # Protect wildcards before escaping
  p = p.replace("**/", "__DOUBLESTAR_SLASH__")
  p = p.replace("/**", "__SLASH_DOUBLESTAR__")
  p = p.replace("**", "__DOUBLESTAR__")
  p = p.replace("*", "__STAR__")
  p = p.replace("?", "__QUESTION__")

  escaped = re.escape(p)

  escaped = escaped.replace("__DOUBLESTAR_SLASH__", r"(?:.*/)?")
  escaped = escaped.replace("__SLASH_DOUBLESTAR__", r"/.*")
  escaped = escaped.replace("__DOUBLESTAR__", r".*")
  escaped = escaped.replace("__STAR__", r"[^/]*")
  escaped = escaped.replace("__QUESTION__", r"[^/]")

  if anchored:
    regex_str = f"^{escaped}"
  else:
    regex_str = f"(?:^|.*/){escaped}"

  if dir_only:
    regex_str += r"/.*$"
  else:
    regex_str += r"(?:/.*)?$"

  return re.compile(regex_str)


def get_pattern_specificity(pattern: str) -> int:
  """Calculate specificity score based on path segments and wildcards."""
  clean = pattern.strip("/")
  segments = [s for s in clean.split("/") if s and s != "**"]
  score = len(segments) * 10
  if "*" not in pattern and "?" not in pattern:
    score += 5
  return score


def parse_codeowners(
    codeowners_path: Path) -> List[Tuple[str, re.Pattern, List[str], int]]:
  """Parse .github/CODEOWNERS sequentially into rules."""
  rules: List[Tuple[str, re.Pattern, List[str], int]] = []
  if not codeowners_path.is_file():
    return rules

  with open(codeowners_path, "r", encoding="utf-8") as f:
    for line in f:
      line_clean = line.strip()
      if not line_clean or line_clean.startswith("#"):
        continue

      line_no_comment = re.sub(r"\s+#.*$", "", line_clean).strip()
      if not line_no_comment:
        continue

      parts = line_no_comment.split()
      if not parts:
        continue

      pattern = parts[0]
      owners = parts[1:]
      regex = pattern_to_regex(pattern)
      spec = get_pattern_specificity(pattern)
      rules.append((pattern, regex, owners, spec))

  return rules


def resolve_files(
    files: List[str], rules: List[Tuple[str, re.Pattern, List[str], int]]
) -> Tuple[Dict[str, List[str]], List[str], Dict[str, str]]:
  """Resolve a list of files against CODEOWNERS rules."""
  file_to_teams: Dict[str, List[str]] = {}
  file_to_matched_pattern: Dict[str, str] = {}
  unowned_files: List[str] = []

  for f in files:
    f_clean = f.strip().lstrip("/")
    matched_owners: Optional[List[str]] = None
    matched_pat: Optional[str] = None
    matched_spec: int = -1

    for pattern, regex, owners, spec in rules:
      if regex.match(f_clean):
        if not owners:
          if matched_owners is None or spec >= matched_spec:
            matched_owners = []
            matched_pat = pattern
            matched_spec = spec
        else:
          matched_owners = owners
          matched_pat = pattern
          matched_spec = spec

    if matched_owners is not None and len(matched_owners) > 0:
      file_to_teams[f_clean] = matched_owners
      if matched_pat:
        file_to_matched_pattern[f_clean] = matched_pat
    else:
      unowned_files.append(f_clean)
      if matched_pat:
        file_to_matched_pattern[f_clean] = f"{matched_pat} (unowned)"

  return file_to_teams, unowned_files, file_to_matched_pattern


def discover_personas(repo_root: Path) -> Dict[str, Dict[str, Any]]:
  """Discover reviewer personas and map team handles to persona metadata."""
  team_to_persona: Dict[str, Dict[str, Any]] = {}
  search_dirs = [
      repo_root / ".agent" / "skills" / "critic_review" / "resources" /
      "reviewers" / "codeowners",
      repo_root / ".agent" / "skills" / "critic-review" / "resources" /
      "reviewers" / "codeowners",
      Path.home() / ".gemini" / "critics",
  ]

  seen_paths: Set[Path] = set()

  for directory in search_dirs:
    if not directory.is_dir():
      continue

    for file_path in directory.glob("*.md"):
      resolved_p = file_path.resolve()
      if resolved_p in seen_paths:
        continue
      seen_paths.add(resolved_p)

      try:
        content = file_path.read_text(encoding="utf-8")
      except OSError:
        continue

      if not content.startswith("---"):
        continue
      parts = content.split("---", 2)
      if len(parts) < 3:
        continue
      fm_text = parts[1]

      name_m = re.search(r"^name:\s*(.+)$", fm_text, re.MULTILINE)
      persona_name = (
          name_m.group(1).strip().strip("\"'") if name_m else file_path.stem)

      declared_teams: List[str] = []

      teams_m = re.search(
          r"(?:codeowner_teams|owners):\s*\n((?:\s*-\s*.*(?:\n|$))*)", fm_text)
      if teams_m:
        for line in teams_m.group(1).splitlines():
          t_val = re.sub(r"^\s*-\s*", "", line).strip().strip("\"'")
          if t_val:
            declared_teams.append(t_val)

      if not declared_teams:
        for tag_match in re.finditer(r"-\s*(@youtube/[\w-]+)", fm_text):
          declared_teams.append(tag_match.group(1).strip())

      stem = file_path.stem.replace("-reviewer", "")
      declared_teams.append(f"@youtube/{stem}")
      if stem.startswith("cobalt-") or stem.startswith("starboard-"):
        declared_teams.append(f"@youtube/{stem}-owners")

      meta = {
          "name":
              persona_name,
          "file":
              str(file_path),
          "relative_path":
              (str(file_path.relative_to(repo_root))
               if file_path.is_relative_to(repo_root) else str(file_path)),
      }

      for t in declared_teams:
        norm_t = t.lower()
        team_to_persona[norm_t] = meta

  if ("@youtube/nplb-filters" not in team_to_persona and
      "@youtube/cobalt-starboard-owners" in team_to_persona):
    team_to_persona["@youtube/nplb-filters"] = team_to_persona[
        "@youtube/cobalt-starboard-owners"]

  return team_to_persona


def get_git_diff_files(repo_root: Path,
                       base_ref: Optional[str] = None) -> List[str]:
  """Get list of changed files from local git diff."""
  cmd = ["git", "diff", "--name-only"]
  if base_ref:
    cmd.append(f"{base_ref}...HEAD")
  else:
    cmd.append("HEAD")

  try:
    res = subprocess.run(
        cmd, cwd=repo_root, capture_output=True, text=True, check=True)
    files = [f.strip() for f in res.stdout.splitlines() if f.strip()]
  except subprocess.CalledProcessError:
    files = []

  try:
    res_untracked = subprocess.run(
        ["git", "ls-files", "--others", "--exclude-standard"],
        cwd=repo_root,
        capture_output=True,
        text=True,
        check=True,
    )
    untracked = [
        f.strip() for f in res_untracked.stdout.splitlines() if f.strip()
    ]
    files.extend(untracked)
  except subprocess.CalledProcessError:
    pass

  return sorted(list(set(files)))


def get_gh_pr_files(repo_root: Path, pr_number: str) -> List[str]:
  """Get changed files for a pull request using GitHub CLI (gh)."""
  try:
    res = subprocess.run(
        ["gh", "pr", "diff", str(pr_number), "--name-only"],
        cwd=repo_root,
        capture_output=True,
        text=True,
        check=True,
    )
    files = [f.strip() for f in res.stdout.splitlines() if f.strip()]
    if files:
      return sorted(files)
  except (subprocess.CalledProcessError, FileNotFoundError):
    pass

  try:
    res = subprocess.run(
        [
            "gh", "pr", "view",
            str(pr_number), "--json", "files", "--jq", ".[].files[].path"
        ],
        cwd=repo_root,
        capture_output=True,
        text=True,
        check=True,
    )
    files = [f.strip() for f in res.stdout.splitlines() if f.strip()]
    if files:
      return sorted(files)
  except (subprocess.CalledProcessError, FileNotFoundError):
    pass

  return []


def main() -> int:
  parser = argparse.ArgumentParser(
      description="Resolve CODEOWNERS to critic-review personas.")
  parser.add_argument("files", nargs="*", help="List of file paths")
  parser.add_argument(
      "--repo-root", default=".", help="Path to repository root")
  parser.add_argument(
      "--diff",
      nargs="?",
      const="HEAD",
      default=None,
      help="Get changed files from git diff")
  parser.add_argument(
      "--pr", default=None, help="Get changed files for a PR via gh CLI")
  parser.add_argument(
      "--format",
      choices=["json", "text", "markdown"],
      default="json",
      help="Output format")

  args = parser.parse_args()
  root = Path(args.repo_root).resolve()
  codeowners_path = root / ".github" / "CODEOWNERS"

  file_list: List[str] = []
  if args.pr:
    file_list = get_gh_pr_files(root, args.pr)
  elif args.diff is not None:
    base = args.diff if args.diff != "HEAD" else None
    file_list = get_git_diff_files(root, base)
  elif args.files:
    file_list = args.files
  else:
    if not sys.stdin.isatty():
      file_list = [line.strip() for line in sys.stdin if line.strip()]
    else:
      file_list = get_git_diff_files(root)

  rules = parse_codeowners(codeowners_path)
  file_to_teams, unowned_files, pattern_matches = resolve_files(
      file_list, rules)
  team_to_persona = discover_personas(root)

  reviewers: Dict[str, Dict[str, Any]] = {}
  unmapped_teams: Set[str] = set()

  for file_path, teams in file_to_teams.items():
    for team in teams:
      team_key = team.lower()
      persona_meta = team_to_persona.get(team_key)
      if persona_meta:
        persona_name = persona_meta["name"]
        if persona_name not in reviewers:
          reviewers[persona_name] = {
              "name": persona_name,
              "team": team,
              "file": persona_meta["file"],
              "relative_path": persona_meta["relative_path"],
              "matched_files": [],
          }
        reviewers[persona_name]["matched_files"].append(file_path)
      else:
        unmapped_teams.add(team)

  result: Dict[str, Any] = {
      "files_analyzed": len(file_list),
      "reviewers": list(reviewers.values()),
      "unowned_files": unowned_files,
      "unmapped_teams": sorted(list(unmapped_teams)),
      "file_details": {
          f: {
              "teams": file_to_teams.get(f, []),
              "rule": pattern_matches.get(f, "none"),
          } for f in file_list
      },
  }

  if args.format == "json":
    print(json.dumps(result, indent=2))
  elif args.format == "markdown":
    print("### Critic Codeowner Reviewers")
    if reviewers:
      for r in reviewers.values():
        name = r["name"]
        team = r["team"]
        count = len(r["matched_files"])
        print(f"- **{name}** (`{team}`): {count} file(s)")
        for f in r["matched_files"]:
          print(f"  - `{f}`")
    else:
      print("_No mandatory codeowner reviewers required._")

    if unowned_files:
      print("\n### Unowned Files (General Critics Required)")
      for uf in unowned_files:
        rule_desc = pattern_matches.get(uf, "unmatched")
        print(f"- `{uf}` ({rule_desc})")

    if unmapped_teams:
      print("\n### Unmapped Teams")
      for ut in sorted(unmapped_teams):
        print(f"- `{ut}`")
  else:
    for r in reviewers.values():
      name = r["name"]
      files_str = ",".join(r["matched_files"])
      print(f"{name}: {files_str}")

  return 0


if __name__ == "__main__":
  sys.exit(main())
