#!/usr/bin/env python3
"""Standalone Jaccard Similarity Calculator for PRs, Commits, and Diffs.

Computes:
1. File-level Jaccard Similarity (J_files)
2. Functional Code Line Jaccard Similarity (J_code) [no comments/whitespace]
3. Overall Alignment Score: S_overall = (J_files + J_code) / 2

Usage Examples:
  # 1. Compare two GitHub Pull Requests:
  python3 .github/rebase/calculate_jaccard.py --human 12086 --ai 12261

  # 2. Compare two Git branches / commits:
  python3 .github/rebase/calculate_jaccard.py --git1 main --git2 exp_branch

  # 3. Compare two patch / diff files:
  python3 .github/rebase/calculate_jaccard.py --diff1 h.patch --diff2 a.patch
"""

import argparse
import json
import subprocess
import sys
from typing import Any, Dict, List, Optional, Set, Tuple


def run_command(cmd: List[str],
                cwd: Optional[str] = None) -> Tuple[int, str, str]:
  """Runs a subprocess command and returns (returncode, stdout, stderr)."""
  try:
    proc = subprocess.run(
        cmd,
        cwd=cwd,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        check=False,
    )
    return proc.returncode, proc.stdout, proc.stderr
  except OSError as err:
    return 1, "", str(err)


def is_comment_or_noise(line: str) -> bool:
  """Checks if a diff line is a comment, blank, or diff header metadata."""
  stripped = line.strip()
  if not stripped or stripped in ("+", "-"):
    return True
  if stripped.startswith(("+++", "---", "@@", "diff --git", "index ")):
    return True
  content = stripped[1:].strip() if stripped[0] in ("+", "-") else stripped
  if content.startswith(("//", "/*", "*", "*/", "#")):
    # Preserve preprocessor directives
    cpp_directives = (
        "#include",
        "#if",
        "#ifdef",
        "#ifndef",
        "#else",
        "#elif",
        "#endif",
        "#define",
        "#undef",
        "#pragma",
    )
    return not any(content.startswith(d) for d in cpp_directives)
  return False


def extract_functional_lines_from_diff(diff_text: str) -> Set[str]:
  """Extracts normalized functional added/deleted code lines from a diff."""
  functional_lines = set()
  for line in diff_text.splitlines():
    if line.startswith(("+", "-")) and not line.startswith(("+++", "---")):
      if is_comment_or_noise(line):
        continue
      prefix = line[0]
      content = line[1:].strip()
      normalized_content = " ".join(content.split())
      functional_lines.add(f"{prefix} {normalized_content}")
  return functional_lines


def extract_modified_files_from_diff(diff_text: str) -> Set[str]:
  """Extracts the set of touched repository file paths from a diff."""
  files = set()
  for line in diff_text.splitlines():
    if line.startswith("diff --git "):
      parts = line.split()
      if len(parts) >= 4:
        b_path = parts[3]
        if b_path.startswith("b/"):
          files.add(b_path[2:])
        else:
          files.add(b_path)
    elif line.startswith("+++ b/"):
      files.add(line[6:].strip())
  return files


def compute_jaccard_similarity(
    files1: Set[str],
    files2: Set[str],
    diff1: str,
    diff2: str,
) -> Dict[str, Any]:
  """Computes file-level, code-level, and composite Jaccard similarities."""
  # 1. File Jaccard
  shared_files = files1 & files2
  only1_files = files1 - files2
  only2_files = files2 - files1
  union_files = files1 | files2

  j_files = ((len(shared_files) / len(union_files)) *
             100.0 if union_files else 100.0)

  # 2. Code Jaccard
  code1 = extract_functional_lines_from_diff(diff1)
  code2 = extract_functional_lines_from_diff(diff2)
  shared_code = code1 & code2
  only1_code = code1 - code2
  only2_code = code2 - code1
  union_code = code1 | code2

  j_code = ((len(shared_code) / len(union_code)) *
            100.0 if union_code else 100.0)

  # 3. Overall Alignment: (J_files + J_code) / 2
  overall_alignment = round((j_files + j_code) / 2.0, 2)

  return {
      "overall_alignment_percent": overall_alignment,
      "file_jaccard_percent": round(j_files, 2),
      "code_jaccard_percent": round(j_code, 2),
      "files_reference_count": len(files1),
      "files_candidate_count": len(files2),
      "files_shared_count": len(shared_files),
      "files_only_reference_count": len(only1_files),
      "files_only_candidate_count": len(only2_files),
      "shared_files_list": sorted(list(shared_files)),
      "only_reference_files_list": sorted(list(only1_files)),
      "only_candidate_files_list": sorted(list(only2_files)),
      "code_lines_reference_count": len(code1),
      "code_lines_candidate_count": len(code2),
      "code_lines_shared_count": len(shared_code),
      "code_lines_only_reference_count": len(only1_code),
      "code_lines_only_candidate_count": len(only2_code),
  }


def get_pr_diff(pr_number: str) -> str:
  """Fetches diff for a GitHub pull request using gh CLI."""
  clean_num = pr_number.lstrip("#")
  code, out, err = run_command(["gh", "pr", "diff", clean_num])
  if code != 0:
    print(
        f"Error fetching PR #{clean_num} diff: {err}",
        file=sys.stderr,
    )
    return ""
  return out


def get_git_diff(ref: str, repo_dir: str = ".") -> str:
  """Fetches git diff against main or parent commit."""
  code, out, _ = run_command(["git", "diff", f"{ref}^...{ref}"], cwd=repo_dir)
  if code != 0 or not out:
    code, out, _ = run_command(["git", "diff", f"origin/main...{ref}"],
                               cwd=repo_dir)
  return out


def format_report_markdown(metrics: Dict[str, Any], label1: str,
                           label2: str) -> str:
  """Renders Markdown summary report of Jaccard Similarity metrics."""
  ovr = metrics["overall_alignment_percent"]
  jf = metrics["file_jaccard_percent"]
  jc = metrics["code_jaccard_percent"]

  f_ref = metrics["files_reference_count"]
  f_cand = metrics["files_candidate_count"]
  f_shared = metrics["files_shared_count"]
  f_miss = metrics["files_only_reference_count"]
  f_extra = metrics["files_only_candidate_count"]

  c_ref = metrics["code_lines_reference_count"]
  c_cand = metrics["code_lines_candidate_count"]
  c_shared = metrics["code_lines_shared_count"]
  c_miss = metrics["code_lines_only_reference_count"]
  c_extra = metrics["code_lines_only_candidate_count"]

  diff_files_str = f"{f_miss} missed / {f_extra} extra"
  diff_code_str = f"{c_miss} missed / {c_extra} divergent"

  lines = [
      "# Jaccard Similarity & Alignment Scorecard\n",
      f"- **Reference Target**: `{label1}`",
      f"- **Evaluated Candidate**: `{label2}`\n",
      "| Metric | Score | Reference | Candidate | Shared | Divergence |",
      "| :--- | :---: | :---: | :---: | :---: | :--- |",
      (f"| **Overall Alignment** | **{ovr}%** | — | — | — | "
       "(50% Files + 50% Code) |"),
      (f"| **File Jaccard ($J_{{files}}$)** | **{jf}%** | {f_ref} files | "
       f"{f_cand} files | {f_shared} files | {diff_files_str} |"),
      (f"| **Code Jaccard ($J_{{code}}$)** | **{jc}%** | {c_ref} lines | "
       f"{c_cand} lines | {c_shared} lines | {diff_code_str} |"),
  ]

  shared_len = len(metrics["shared_files_list"])
  if metrics["shared_files_list"]:
    lines.append(f"\n### Shared Files ({shared_len}):")
    for f in metrics["shared_files_list"][:20]:
      lines.append(f"- `{f}`")
    if shared_len > 20:
      lines.append(f"- *... and {shared_len - 20} more files*")

  ref_only_len = len(metrics["only_reference_files_list"])
  if metrics["only_reference_files_list"]:
    lines.append("\n### Missed Files (in Reference only):")
    for f in metrics["only_reference_files_list"][:15]:
      lines.append(f"- `{f}`")
    if ref_only_len > 15:
      lines.append(f"- *... and {ref_only_len - 15} more*")

  cand_only_len = len(metrics["only_candidate_files_list"])
  if metrics["only_candidate_files_list"]:
    lines.append("\n### Divergent / Extra Files (in Candidate only):")
    for f in metrics["only_candidate_files_list"][:15]:
      lines.append(f"- `{f}`")
    if cand_only_len > 15:
      lines.append(f"- *... and {cand_only_len - 15} more*")

  return "\n".join(lines)


def main():
  """CLI entry point for calculating Jaccard Similarity."""
  parser = argparse.ArgumentParser(
      description="Calculate Jaccard Similarity for PRs, commits, and diffs.")
  parser.add_argument("--human", "--pr1", help="Reference/Human PR Number")
  parser.add_argument("--ai", "--pr2", help="Candidate/AI PR Number")
  parser.add_argument("--git1", help="Reference Git Commit/Branch")
  parser.add_argument("--git2", help="Candidate Git Commit/Branch")
  parser.add_argument("--diff1", help="Path to reference diff file")
  parser.add_argument("--diff2", help="Path to candidate diff file")
  parser.add_argument("--file1", help="Path to reference source file")
  parser.add_argument("--file2", help="Path to candidate source file")
  parser.add_argument(
      "--json", action="store_true", help="Output raw JSON metrics")
  parser.add_argument(
      "--out", default="", help="Output filepath for Markdown report")
  args = parser.parse_args()

  diff1 = ""
  diff2 = ""
  label1 = "Reference"
  label2 = "Candidate"

  if args.human and args.ai:
    clean_h = args.human.lstrip("#")
    clean_a = args.ai.lstrip("#")
    label1 = f"Human PR #{clean_h}"
    label2 = f"AI PR #{clean_a}"
    diff1 = get_pr_diff(args.human)
    diff2 = get_pr_diff(args.ai)
  elif args.diff1 and args.diff2:
    label1 = args.diff1
    label2 = args.diff2
    with open(args.diff1, "r", encoding="utf-8", errors="replace") as f:
      diff1 = f.read()
    with open(args.diff2, "r", encoding="utf-8", errors="replace") as f:
      diff2 = f.read()
  elif args.git1 and args.git2:
    label1 = args.git1
    label2 = args.git2
    diff1 = get_git_diff(args.git1)
    diff2 = get_git_diff(args.git2)
  elif args.file1 and args.file2:
    label1 = args.file1
    label2 = args.file2
    with open(args.file1, "r", encoding="utf-8", errors="replace") as f1:
      c1 = [f"+ {l.strip()}" for l in f1 if l.strip()]
    with open(args.file2, "r", encoding="utf-8", errors="replace") as f2:
      c2 = [f"+ {l.strip()}" for l in f2 if l.strip()]
    diff1 = "\n".join(c1)
    diff2 = "\n".join(c2)
  else:
    parser.print_help()
    sys.exit(1)

  if not diff1 or not diff2:
    print("Error: Could not retrieve diff content.", file=sys.stderr)
    sys.exit(1)

  files1 = extract_modified_files_from_diff(diff1) if not args.file1 else {
      args.file1
  }
  files2 = extract_modified_files_from_diff(diff2) if not args.file2 else {
      args.file2
  }

  metrics = compute_jaccard_similarity(files1, files2, diff1, diff2)

  if args.json:
    print(json.dumps(metrics, indent=2))
  else:
    report = format_report_markdown(metrics, label1, label2)
    print(report)
    if args.out:
      with open(args.out, "w", encoding="utf-8") as of:
        of.write(report)
      print(f"\nReport written to: {args.out}")


if __name__ == "__main__":
  main()
