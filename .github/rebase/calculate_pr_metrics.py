#!/usr/bin/env python3
"""Batch Metrics Calculator for Human vs AI Rebase PR Benchmarks.

Calculates Jaccard similarity and divergence metrics across historical
Cobalt roll rebase PRs, evaluating only fix commits from commit 6 onwards.

Default PR Benchmark Suite:
- M141.7351: Human #12228 vs AI #12259
- M140.7339: Human #12161 vs AI #12176
- M140.7318: Human #12155 vs AI #12262
- M140.7298: Human #12086 vs AI #12261
- M140.7278: Human #12051 vs AI #12136
- M139.7244: Human #11890 vs AI #12038
- M139.7217: Human #11722 vs AI #12258

Usage:
  python3 .github/rebase/calculate_pr_metrics.py
  python3 .github/rebase/calculate_pr_metrics.py --out out/benchmark_metrics.md
"""

import argparse
import os
import sys
from typing import Any, Dict, List, Tuple

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PARENT_DIR = os.path.dirname(SCRIPT_DIR)
if SCRIPT_DIR not in sys.path:
  sys.path.insert(0, SCRIPT_DIR)
if PARENT_DIR not in sys.path:
  sys.path.insert(0, PARENT_DIR)

# pylint: disable=wrong-import-position
from review_pipeline import (
    calculate_ai_pr_metrics,
    extract_modified_files_from_diff,
    extract_pr_number,
    fetch_pr_info,
    get_diff_for_commits,
    partition_roll_commits,
    run_cmd,
)

DEFAULT_BENCHMARK_PAIRS: List[Tuple[str, str, str]] = [
    ("M141.7351", "12228", "12259"),
    ("M140.7339", "12161", "12176"),
    ("M140.7318", "12155", "12262"),
    ("M140.7298", "12086", "12261"),
    ("M140.7278", "12051", "12136"),
    ("M139.7244", "11890", "12038"),
    ("M139.7217", "11722", "12258"),
]


def evaluate_pr_pair(
    milestone: str,
    human_pr: str,
    ai_pr: str,
    repo_root: str,
) -> Dict[str, Any]:
  """Evaluates fix commit diffs and calculates Jaccard metrics for a PR pair."""
  h_num = extract_pr_number(human_pr)
  a_num = extract_pr_number(ai_pr)

  print(
      f"  [METRICS] Processing {milestone}: Human #{h_num} vs AI #{a_num}...",
      file=sys.stderr,
  )

  h_data = fetch_pr_info(h_num)
  a_data = fetch_pr_info(a_num)

  if not h_data or not a_data:
    print(
        f"  [ERROR] Could not fetch PR data for #{h_num} or #{a_num}",
        file=sys.stderr,
    )
    return {
        "milestone": milestone,
        "human_pr": h_num,
        "ai_pr": a_num,
        "status": "FETCH_ERROR",
    }

  # Partition commits: first 5 commits = roll baseline, remaining = fixes
  h_commits = h_data.get("commits", [])
  h_infra, h_fixes = partition_roll_commits(h_commits)
  h_base = h_infra[-1].get("oid", "") if h_infra else None
  h_diff = get_diff_for_commits(
      h_fixes, repo_root, base_sha=h_base, pr_num=h_num)
  if not h_diff:
    _, h_diff, _ = run_cmd(["gh", "pr", "diff", h_num])

  a_commits = a_data.get("commits", [])
  a_infra, a_fixes = partition_roll_commits(a_commits)
  if a_infra and a_fixes:
    a_base = a_infra[-1].get("oid", "")
    a_diff = get_diff_for_commits(
        a_fixes, repo_root, base_sha=a_base, pr_num=a_num)
  else:
    a_diff = get_diff_for_commits(a_commits, repo_root, pr_num=a_num)
  if not a_diff:
    _, a_diff, _ = run_cmd(["gh", "pr", "diff", a_num])

  h_files = extract_modified_files_from_diff(h_diff)
  a_files = extract_modified_files_from_diff(a_diff)

  pair_metrics = calculate_ai_pr_metrics(h_files, a_files, h_diff, a_diff)
  pair_metrics.update({
      "milestone": milestone,
      "human_pr": h_num,
      "ai_pr": a_num,
      "human_title": h_data.get("title", ""),
      "ai_title": a_data.get("title", ""),
      "human_fix_commits_count": len(h_fixes) if h_fixes else len(h_commits),
      "ai_fix_commits_count": len(a_fixes) if a_fixes else len(a_commits),
      "status": "SUCCESS",
  })
  return pair_metrics


def format_benchmark_table(results: List[Dict[str, Any]]) -> str:
  """Renders an aggregated markdown table of all benchmark results."""
  valid = [r for r in results if r.get("status") == "SUCCESS"]
  if not valid:
    return "No benchmark results calculated."

  lines = [
      "# Autonomous Rebase Benchmark Scorecard\n",
      "Quantitative comparison of Human Ground-Truth PR fixes vs. ",
      "AI Rebase attempts across Chromium milestone rolls "
      "(fix commits only).\n",
      "| Milestone | Human PR | AI PR | Modified Files (Jaccard) | "
      "Shared Files | Missed / Extra | Functional Code (Jaccard) | "
      "Matching Lines | Overall Alignment |",
      "| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: | "
      ":---: |",
  ]

  tot_ovr = 0.0
  tot_fj = 0.0
  tot_cj = 0.0
  tot_shared_files = 0
  tot_human_files = 0
  tot_ai_files = 0
  tot_shared_code = 0
  tot_human_code = 0
  tot_ai_code = 0

  for r in valid:
    m = r["milestone"]
    h_pr = r["human_pr"]
    a_pr = r["ai_pr"]
    f_jaccard = r["file_jaccard"]
    s_files_cnt = r["shared_files_count"]
    h_only_fcnt = r["human_only_files_count"]
    a_only_fcnt = r["ai_only_files_count"]
    c_jaccard = r["code_jaccard"]
    s_lines_cnt = r["shared_code_lines_count"]
    ovr_sim = r["overall_similarity"]

    hp = f"#{h_pr}"
    ap = f"#{a_pr}"
    fj = f"{f_jaccard}%"
    s_files = f"{s_files_cnt}"
    diff_files = f"{h_only_fcnt}m / {a_only_fcnt}e"
    cj = f"{c_jaccard}%"
    s_lines = f"{s_lines_cnt}"
    ovr = f"**{ovr_sim}%**"

    lines.append(f"| **{m}** | {hp} | {ap} | {fj} | {s_files} | "
                 f"{diff_files} | {cj} | {s_lines} | {ovr} |")

    tot_ovr += r["overall_similarity"]
    tot_fj += r["file_jaccard"]
    tot_cj += r["code_jaccard"]
    tot_shared_files += r["shared_files_count"]
    tot_human_files += r["total_human_files"]
    tot_ai_files += r["total_ai_files"]
    tot_shared_code += r["shared_code_lines_count"]
    tot_human_code += r["total_human_code_lines"]
    tot_ai_code += r["total_ai_code_lines"]

  n = len(valid)
  avg_ovr = round(tot_ovr / n, 1)
  avg_fj = round(tot_fj / n, 1)
  avg_cj = round(tot_cj / n, 1)

  summary_files = (f"**{tot_shared_files}** (of {tot_human_files} human / "
                   f"{tot_ai_files} AI)")
  lines.append(
      f"| **AVERAGE / TOTAL** | — | — | **{avg_fj}%** | "
      f"{summary_files} | — | **{avg_cj}%** | **{tot_shared_code}** lines | "
      f"**{avg_ovr}%** |")

  lines.append("\n### Detailed Breakdown by Milestone\n")
  for r in valid:
    ms = r["milestone"]
    h_num = r["human_pr"]
    a_num = r["ai_pr"]
    ovr_sim = r["overall_similarity"]
    f_jac = r["file_jaccard"]
    s_cnt = r["shared_files_count"]
    h_miss = r["human_only_files_count"]
    a_extra = r["ai_only_files_count"]
    c_jac = r["code_jaccard"]
    s_lines_cnt = r["shared_code_lines_count"]
    h_miss_lines = r["human_only_code_lines_count"]
    a_div_lines = r["ai_only_code_lines_count"]
    h_fix_cnt = r["human_fix_commits_count"]
    a_fix_cnt = r["ai_fix_commits_count"]

    ms_title = f"#### {ms}: Human PR #{h_num} vs AI PR #{a_num}\n"
    shared_files_str = f"({s_cnt} shared, {h_miss} missed, {a_extra} extra)"
    shared_lines_str = (f"({s_lines_cnt} exact lines matched, "
                        f"{h_miss_lines} missed, {a_div_lines} divergent)")
    commits_str = (
        f"- **Fix Commits Analyzed**: {h_fix_cnt} human fix commits vs. "
        f"{a_fix_cnt} AI fix commits\n")

    lines.append(f"{ms_title}"
                 f"- **Overall Alignment**: {ovr_sim}%\n"
                 f"- **Modified Files**: {f_jac}% Jaccard "
                 f"{shared_files_str}\n"
                 f"- **Functional Code Lines**: {c_jac}% Jaccard "
                 f"{shared_lines_str}\n"
                 f"{commits_str}")

  return "\n".join(lines)


def main():
  """Main entry point for batch metrics calculation."""
  parser = argparse.ArgumentParser(
      description="Batch Metrics Calculator for Human vs AI Rebase PRs")
  parser.add_argument(
      "--pairs",
      nargs="*",
      default=[],
      help="Optional list of Milestone:HumanPR:AIPR triplets",
  )
  parser.add_argument(
      "--out",
      default="",
      help="Optional file path to write Markdown scorecard table",
  )
  args = parser.parse_args()

  repo_root = os.path.abspath(os.path.join(SCRIPT_DIR, "..", ".."))

  pairs_to_run: List[Tuple[str, str, str]] = []
  if args.pairs:
    for item in args.pairs:
      parts = item.split(":")
      if len(parts) == 3:
        pairs_to_run.append((parts[0], parts[1], parts[2]))
      elif len(parts) == 2:
        pairs_to_run.append((f"PR-{parts[0]}", parts[0], parts[1]))
  else:
    pairs_to_run = DEFAULT_BENCHMARK_PAIRS

  print(
      f"[BENCHMARK] Calculating metrics across {len(pairs_to_run)} pairs...\n",
      file=sys.stderr,
  )

  results = []
  for milestone, human_pr, ai_pr in pairs_to_run:
    r = evaluate_pr_pair(milestone, human_pr, ai_pr, repo_root)
    results.append(r)

  report = format_benchmark_table(results)

  print("\n" + "=" * 80)
  print(report)
  print("=" * 80 + "\n")

  if args.out:
    try:
      os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
      with open(args.out, "w", encoding="utf-8") as f:
        f.write(report)
      print(f"[BENCHMARK] Report saved to: {args.out}", file=sys.stderr)
    except OSError as e:
      print(
          f"[ERROR] Could not write report to {args.out}: {e}", file=sys.stderr)


if __name__ == "__main__":
  main()
