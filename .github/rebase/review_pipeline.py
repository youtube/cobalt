#!/usr/bin/env python3
"""Automated Comparative Review & Skill Learning Pipeline.

Compares a Human Ground-Truth PR fix against an AI rebase attempt for
the same Chromium roll, and reports how far apart they are as a *count
of functional differences* rather than a text-similarity score.

Pipeline:
1. Partition both PRs into roller baseline vs fix commits.
2. Diff only the fix commits, and pre-filter to the files that actually
   differ functionally.
3. Ask the Tier-2 Expert Model to enumerate each difference as a
   discrete block, with its concrete impact and a question for the
   human reviewer.
4. Count the parsed blocks. The count is derived from the enumerated
   items, never asserted by the model, so every counted item is visible
   and individually reviewable.
5. Optionally persist lessons to the GCS Knowledge Bank and update the
   skill documentation.

The reviewer is assumed to be time-constrained and unwilling to read
the whole AI PR, so the output leads with the differences and the
questions, ranked by severity.

Convention:
  In Cobalt autoroll PRs the leading commits are the roller baseline,
  ending with the 'CONFLICTED Cherry pick ...: Update to <ver>' commit.
  Everything after that anchor is a fix commit. See roll_partition.py.

Usage:
  python3 .github/rebase/review_pipeline.py \
    --human-pr 12161 \
    --ai-pr 12176 \
    --update-skills
"""

import argparse
import json
import os
import re
import subprocess
import sys
from typing import Any, Dict, List, Optional, Tuple

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PARENT_DIR = os.path.dirname(SCRIPT_DIR)
if SCRIPT_DIR not in sys.path:
  sys.path.insert(0, SCRIPT_DIR)
if PARENT_DIR not in sys.path:
  sys.path.insert(0, PARENT_DIR)

# pylint: disable=wrong-import-position
from base_resolver import execute_local_tool
from base_resolver import extract_tool_commands
from base_resolver import sanitize_filepath_token
import diff_metrics
import differences
from engine_client import ReasoningEngineClient
from reasoning_engine.engine import CobaltReasoningEngine
import roll_partition

SKILLS_DIR = os.path.join(SCRIPT_DIR, "reasoning_engine", "skills")


def run_cmd(cmd: List[str], cwd: Optional[str] = None) -> Tuple[bool, str, str]:
  """Runs a shell command and returns (success, stdout, stderr)."""
  try:
    proc = subprocess.run(
        cmd,
        cwd=cwd,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        timeout=90,
        check=False,
    )
    return proc.returncode == 0, proc.stdout.strip(), proc.stderr.strip()
  except Exception as e:  # pylint: disable=broad-exception-caught
    return False, "", str(e)


def extract_pr_number(pr_input: str) -> str:
  """Extracts numeric PR ID from number or GitHub PR URL."""
  m = re.search(r"/pull/(\d+)", pr_input)
  if m:
    return m.group(1)
  clean = pr_input.strip().lstrip("#")
  if clean.isdigit():
    return clean
  return pr_input


def fetch_pr_info(pr_num: str) -> Optional[Dict[str, Any]]:
  """Fetches PR metadata, commits, and files via gh CLI."""
  ok, out, err = run_cmd([
      "gh", "pr", "view", pr_num, "--json",
      "number,title,body,commits,files,baseRefName,headRefName"
  ])
  if not ok:
    print(f"[ERROR] Failed to fetch PR #{pr_num}: {err}", file=sys.stderr)
    return None
  try:
    return json.loads(out)
  except json.JSONDecodeError:
    return None


# Roll partitioning lives in roll_partition.py, diff parsing in
# diff_metrics.py, and the difference model in differences.py. All were
# previously reimplemented here and in the deleted calculate_jaccard.py,
# and the copies had drifted: they classified docstrings and
# #error/#warning differently, and this module's file extraction read
# only '+++ b/' lines, which silently dropped deleted and renamed files.


def extract_single_file_diff(diff_text: str, target_file: str) -> str:
  """Extracts the diff section for one file from a multi-file diff.

  Matches on parsed 'diff --git' paths. The previous substring test let
  a request for 'foo.h' match 'ui/foo.html' and concatenate unrelated
  sections. An ambiguous bare filename now returns an error naming the
  candidates so the model can retry with a full path.
  """
  clean_target = sanitize_filepath_token(target_file)
  if not clean_target:
    return ""

  sections = diff_metrics.split_diff_by_file(diff_text)
  matches = diff_metrics.resolve_diff_path(sections, clean_target)

  if not matches:
    return ""
  if len(matches) > 1:
    listed = "\n".join(f"  - {p}" for p in matches)
    return (f"[AMBIGUOUS] '{clean_target}' matches {len(matches)} files. "
            f"Re-request using a full path:\n{listed}")
  return sections[matches[0]]


def execute_review_tool(
    cmd: str,
    repo_root: str,
    human_diff: str,
    ai_diff: str,
) -> str:
  """Executes safe read-only inspection tools for review pipeline."""
  clean_cmd = cmd.strip()
  if clean_cmd.startswith("TOOL_DIFF_FILE:"):
    raw_path = clean_cmd.split(":", 1)[1].strip()
    file_path = sanitize_filepath_token(raw_path)
    if not file_path:
      return "[ERROR] No file path provided to TOOL_DIFF_FILE."
    h_file_diff = extract_single_file_diff(human_diff, file_path)
    a_file_diff = extract_single_file_diff(ai_diff, file_path)
    h_msg = h_file_diff or "(No changes in Human PR)"
    a_msg = a_file_diff or "(No changes in AI PR)"
    return (f"=== Target File: {file_path} ===\n\n"
            f"--- Human Ground-Truth Fix Diff ---\n"
            f"{h_msg}\n\n"
            f"--- AI Rebase Attempt Diff ---\n"
            f"{a_msg}\n")

  return execute_local_tool(clean_cmd, repo_root)


def load_cobalt_skills_context(skills_dir: str = SKILLS_DIR) -> str:
  """Loads Cobalt architectural rules and rebase principles from skills."""
  if not os.path.isdir(skills_dir):
    return ""

  # Load every skill the resolver itself is given. Previously three were
  # omitted, including cobalt_rebase_patterns.md, which holds the JNI
  # Zero and mojom [EnableIf] case studies. The reviewer was therefore
  # judging divergences without the precedents that explain them.
  docs = []
  skill_files = [
      "cobalt_rebase.md",
      "cobalt_rebase_patterns.md",
      "compiler_healing.md",
      "conflict_resolution.md",
      "cpp_api_migrations.md",
      "gn_healing.md",
      "roll_history.md",
  ]
  for filename in skill_files:
    fpath = os.path.join(skills_dir, filename)
    if os.path.isfile(fpath):
      try:
        with open(fpath, "r", encoding="utf-8", errors="replace") as f:
          content = f.read().strip()
          docs.append(f"### Domain Skill Guide: {filename}\n{content}")
      except Exception:  # pylint: disable=broad-exception-caught
        pass

  return "\n\n".join(docs)


# pylint: disable=too-many-positional-arguments,too-many-arguments,protected-access
def generate_comparative_review(
    engine_or_client: Any,
    human_pr_data: Dict[str, Any],
    ai_pr_data: Dict[str, Any],
    human_diff: str,
    ai_diff: str,
    both_files: List[str],
    human_only_files: List[str],
    ai_only_files: List[str],
    differing_files: Optional[List[str]] = None,
    repo_root: str = "",
    expert_model: Optional[str] = None,
) -> str:
  """Uses Tier-2 Expert Agent to interactively inspect file diffs."""
  differing_files = differing_files or []
  h_num = human_pr_data.get("number", "")
  h_title = human_pr_data.get("title", "")
  a_num = ai_pr_data.get("number", "")
  a_title = ai_pr_data.get("title", "")

  both_summary = "\n".join(f"- {f}" for f in both_files) or "None"
  human_only_summary = "\n".join(f"- {f}" for f in human_only_files) or "None"
  ai_only_summary = "\n".join(f"- {f}" for f in ai_only_files) or "None"
  differing_summary = "\n".join(f"- {f}" for f in differing_files) or "None"

  skills_context = load_cobalt_skills_context()

  sys_inst = (
      "You are the Senior Principal Chromium and Cobalt Systems Architect.\n"
      "You are comparing a Human Expert (Igalia/Google) rebase fix against "
      "an AI rebase attempt for the same Chromium roll.\n\n"
      f"=== Cobalt Architectural & Rebase Knowledge Base ===\n"
      f"{skills_context}\n\n"
      "Context:\n"
      "- Both PRs start from the same roller baseline commits, which have "
      "already been excluded. You are seeing fix commits only.\n"
      "- Your reader is a busy Igalia reviewer who will NOT read the full "
      "AI PR. They need to know what differs and why it might matter.\n\n"
      "Investigation tools (one per line, nothing else on that line):\n"
      "    TOOL_DIFF_FILE: <full/path/to/file>\n"
      "    TOOL_UPSTREAM_DIFF: <full/path/to/file>\n"
      "    TOOL_COBALT_DIFF: <full/path/to/file>\n"
      "    TOOL_READ_FILE: <path> <start>-<end>\n"
      "    TOOL_GIT_LOG: <count> <filepath>\n"
      "    TOOL_GREP: <symbol>\n\n"
      "Always pass full repository paths. A bare filename such as "
      "'BUILD.gn' is ambiguous and will be rejected.\n\n"
      "Protocol:\n"
      "1. While investigating, emit ONLY tool directives and nothing else.\n"
      "2. When you have enough evidence, emit ONLY the final report, with "
      "no tool directives anywhere in it.\n\n"
      "=== REQUIRED FINAL OUTPUT ===\n"
      "Enumerate every FUNCTIONAL difference as its own fenced block. A "
      "functional difference changes behavior, build configuration, or "
      "generated output. Do NOT emit blocks for comment wording, "
      "formatting, or naming choices that cannot change behavior.\n\n"
      "```difference\n"
      "FILE: <full repository path>\n"
      "CATEGORY: MISSED | EXTRA | DIVERGENT\n"
      "SEVERITY: HIGH | MEDIUM | LOW\n"
      "HUMAN: <what the human did, or 'no change'>\n"
      "AI: <what the AI did, or 'no change'>\n"
      "IMPACT: <the concrete functional consequence>\n"
      "QUESTION: <one direct question for the human reviewer>\n"
      "```\n\n"
      "CATEGORY meanings:\n"
      "  MISSED    - human changed it, AI did not\n"
      "  EXTRA     - AI changed it, human did not\n"
      "  DIVERGENT - both changed it, in incompatible ways\n\n"
      "SEVERITY meanings:\n"
      "  HIGH   - likely breaks the build or changes runtime behavior\n"
      "  MEDIUM - works, but diverges from Cobalt conventions\n"
      "  LOW    - cosmetic or defensive difference\n\n"
      "QUESTION guidance: the reviewer knows the Cobalt intent and the AI "
      "does not. Ask what you could not determine from the diff alone, for "
      "example whether a change was required by an upstream restructure or "
      "merely one valid option. Do not ask questions the diff answers.\n\n"
      "After the blocks, add a short section:\n"
      "## Summary\n"
      "<3-6 sentences on the overall pattern of divergence>\n\n"
      "Then any lessons for the AI agent:\n"
      "```lesson\n"
      "TARGET: <filepath>\n"
      "ISSUE: <brief description of what went wrong>\n"
      "RULE: <actionable rule for future rebases>\n"
      "```\n"
      "Mark a lesson only where you are confident. If confirmation from "
      "the reviewer is needed first, say so in the RULE text.")

  prompt = (
      f"Human Ground-Truth PR: #{h_num} - {h_title}\n"
      f"AI Rebase Attempt PR: #{a_num} - {a_title}\n\n"
      f"=== Modified Files Inventory (fix commits only) ===\n"
      f"Touched by both ({len(both_files)}):\n{both_summary}\n\n"
      f"Of those, functionally different ({len(differing_files)}) - these "
      f"are the highest-value files to inspect:\n{differing_summary}\n\n"
      f"Human only, i.e. candidate MISSED ({len(human_only_files)}):\n"
      f"{human_only_summary}\n\n"
      f"AI only, i.e. candidate EXTRA ({len(ai_only_files)}):\n"
      f"{ai_only_summary}\n\n"
      "Request the file diffs you need with `TOOL_DIFF_FILE: <full/path>`, "
      "or emit your final report if you already have enough evidence.")

  exp_label = expert_model or "Default"
  print(
      f"  [REVIEW] Starting Interactive Review Session with ({exp_label})...",
      file=sys.stderr)
  current_prompt = prompt
  accumulated_tool_context = []
  # Bound before the loop: if the engine exposes neither expert entry
  # point the loop breaks on round 1, and the trailing return would
  # otherwise raise UnboundLocalError instead of returning empty.
  response = ""

  for round_idx in range(1, 8):
    if hasattr(engine_or_client, "_generate_expert_content"):
      response = engine_or_client._generate_expert_content(
          current_prompt, sys_inst, expert_model=expert_model)
    elif hasattr(engine_or_client, "query"):
      res = engine_or_client.query(
          action="chat",
          message=f"{sys_inst}\n\n{current_prompt}",
          expert_model=expert_model,
          use_pro=True,
      )
      response = res.get("response", "") if isinstance(res, dict) else str(res)
    else:
      break

    if not response:
      break

    # A response carrying difference blocks is the final report, even if
    # it mentions a tool name in prose. The old check treated any
    # "TOOL_" substring as a tool request, so a report documenting its
    # own investigation was discarded and the loop wasted a round.
    if differences.parse_differences(response):
      return response

    tool_matches = extract_tool_commands(response.strip())
    if not tool_matches:
      return response

    print(
        f"  [REVIEW] [Turn {round_idx}] Expert requested "
        f"{len(tool_matches)} tool(s):",
        file=sys.stderr)
    round_tool_results = []
    for tool_cmd in tool_matches:
      clean_tool = tool_cmd.strip()
      print(f"    -> {clean_tool}", file=sys.stderr)
      tool_output = execute_review_tool(clean_tool, repo_root, human_diff,
                                        ai_diff)
      round_tool_results.append(
          f"Tool `{clean_tool}` Output:\n```\n{tool_output[:5000]}\n```")

    accumulated_tool_context.extend(round_tool_results)
    if round_idx >= 3 or len(accumulated_tool_context) >= 15:
      next_instruction = (
          "You have now inspected the key diffs. Emit your final report "
          "only: one ```difference block per functional difference, then "
          "## Summary, then any ```lesson blocks. Do not emit any tool "
          "directives.")
    else:
      next_instruction = (
          "If you need further file diffs, output `TOOL_DIFF_FILE: "
          "<full/path>` and nothing else. Otherwise emit your final "
          "report of ```difference blocks.")

    accum_len = len(accumulated_tool_context)
    current_prompt = (f"{prompt}\n\n"
                      f"--- Investigation Tool Results ({accum_len}) ---\n"
                      f"{chr(10).join(accumulated_tool_context[-20:])}\n\n"
                      f"{next_instruction}")

  return response or ""


def apply_skill_updates(review_text: str) -> List[str]:
  """Appends skill update blocks to files in reasoning_engine/skills/."""
  updates = re.findall(r"```skill_update:\s*([a-zA-Z0-9_\-\.]+)\n(.*?)```",
                       review_text, re.DOTALL)
  updated_files = []

  for filename, content in updates:
    target_path = os.path.join(SKILLS_DIR, filename.strip())
    clean_content = content.strip()
    if not clean_content:
      continue

    # Create or append to skill file
    if os.path.isfile(target_path):
      try:
        with open(target_path, "r", encoding="utf-8") as f:
          existing = f.read()
        # Avoid duplicate appending
        if clean_content[:60] in existing:
          continue
        with open(target_path, "a", encoding="utf-8") as f:
          f.write(f"\n\n---\n\n## Expert Review Insights\n\n{clean_content}\n")
        updated_files.append(filename)
        print(f"  [SKILL_DOC] Updated skill file: {filename}", file=sys.stderr)
      except OSError as e:
        print(
            f"  [SKILL_DOC] Warning: Could not update {filename}: {e}",
            file=sys.stderr)
    else:
      try:
        title_str = (filename.replace(".md", "").replace("_", " ").title())
        with open(target_path, "w", encoding="utf-8") as f:
          f.write(f"# {title_str}\n\n{clean_content}\n")
        updated_files.append(filename)
        print(
            f"  [SKILL_DOC] Created new skill file: {filename}",
            file=sys.stderr)
      except OSError as e:
        print(
            f"  [SKILL_DOC] Warning: Could not create {filename}: {e}",
            file=sys.stderr)

  return updated_files


# pylint: disable=protected-access
def generate_skill_updates_from_review(
    engine_or_client: Any,
    review_text: str,
    expert_model: Optional[str] = None,
) -> str:
  """Prompts the expert model to synthesize explicit skill updates."""
  sys_inst = (
      "You are the Senior Principal Cobalt Systems Architect.\n"
      "Your task is to take the post-mortem comparative review and produce "
      "actionable atomic knowledge items and skill documentation updates.")
  prompt = (
      f"Here is the post-mortem review findings:\n\n{review_text[:15000]}\n\n"
      "Based on these findings, please produce:\n"
      "1. Extracted atomic lessons for our memory bank:\n"
      "```lesson\n"
      "TARGET: <filepath>\n"
      "ISSUE: <brief description of break>\n"
      "RULE: <actionable rule/technique for AI>\n"
      "```\n\n"
      "2. Explicit skill documentation additions to append to our skills "
      "(`cobalt_rebase.md`, `compiler_healing.md`, `gn_healing.md`):\n"
      "```skill_update: <skill_file.md>\n"
      "### <Section Title>\n"
      "<detailed markdown guidance to append to the skill file>\n"
      "```\n")
  if hasattr(engine_or_client, "_generate_expert_content"):
    return engine_or_client._generate_expert_content(
        prompt, sys_inst, expert_model=expert_model) or ""
  if hasattr(engine_or_client, "query"):
    res = engine_or_client.query(
        action="chat",
        message=f"{sys_inst}\n\n{prompt}",
        expert_model=expert_model,
        use_pro=True,
    )
    return res.get("response", "") if isinstance(res, dict) else str(res)
  return ""


def persist_lessons_to_memory(review_text: str, engine_or_client: Any) -> int:
  """Extracts ```lesson blocks and records them to GCS Knowledge Bank."""
  lesson_blocks = re.findall(r"```lesson\s*(.*?)\s*```", review_text, re.DOTALL)
  saved = 0

  for block in lesson_blocks:
    lines = block.strip().splitlines()
    target = ""
    issue = ""
    rule = ""
    for l in lines:
      if l.startswith("TARGET:"):
        target = l.replace("TARGET:", "").strip()
      elif l.startswith("ISSUE:"):
        issue = l.replace("ISSUE:", "").strip()
      elif l.startswith("RULE:"):
        rule = l.replace("RULE:", "").strip()

    if target and (issue or rule):
      desc = f"[{target}] {issue} -> {rule}"
      print(
          f"  [KNOWLEDGE_BANK] Persisting lesson for: {target}...",
          file=sys.stderr)
      if hasattr(engine_or_client, "record_successful_fix"):
        engine_or_client.record_successful_fix(
            issue_description=desc,
            solution_diff=rule,
            target_file=target,
        )
      else:
        engine_or_client.query(
            action="record_successful_fix",
            issue_description=desc,
            solution_diff=rule,
            target_file=target,
        )
      saved += 1

  return saved


def main():
  """Main entry point for review pipeline."""
  parser = argparse.ArgumentParser(
      description="Compare Human vs AI Rebase PR and Update Skills")
  parser.add_argument(
      "--human-pr",
      required=True,
      help="Human/Bot Roll PR (e.g. 12161 or PR URL)",
  )
  parser.add_argument(
      "--ai-pr",
      required=True,
      help="AI Rebase PR (e.g. 12176 or PR URL)",
  )
  parser.add_argument(
      "--local",
      action="store_true",
      help="Run Reasoning Engine locally in-process",
  )
  parser.add_argument(
      "--remote",
      "--resource-id",
      dest="resource_id",
      default=os.environ.get("REASONING_ENGINE_ID", ""),
      help="Hosted Vertex AI Reasoning Engine Resource ID",
  )
  parser.add_argument(
      "--project-id",
      default=os.environ.get("GCP_PROJECT") or "lxn-test",
      help="GCP Project ID",
  )
  parser.add_argument(
      "--location",
      default=os.environ.get("GCP_LOCATION", "us-central1"),
      help="Vertex AI Location",
  )
  parser.add_argument(
      "--expert-model",
      default=os.environ.get("EXPERT_MODEL", "gemini-3.8-flash"),
      help=("Expert LLM model (default: gemini-3.8-flash, matching the "
            "deploy.py default; also supports gemini-3.7-flash, "
            "gemini-2.5-pro, glm-5.2, claude-sonnet-5, claude-opus-5)"),
  )
  parser.add_argument(
      "--expert-location",
      default=os.environ.get("EXPERT_LOCATION", "global"),
      help="Vertex AI region for Expert model (default: global)",
  )
  parser.add_argument(
      "--update-skills",
      action="store_true",
      help="Automatically update skill docs with extracted lessons",
  )
  parser.add_argument(
      "--gcs-memory-uri",
      default=os.environ.get("GCS_MEMORY_URI",
                             "gs://lxn-test/rebase_memory/knowledge_bank.json"),
      help="GCS URI for knowledge memory bank",
  )
  parser.add_argument(
      "--out",
      default="",
      help="Optional path to save markdown review report",
  )
  args = parser.parse_args()

  repo_root = os.path.abspath(os.path.join(SCRIPT_DIR, "..", ".."))

  h_num = extract_pr_number(args.human_pr)
  a_num = extract_pr_number(args.ai_pr)

  print(f"[PIPELINE] Fetching Human PR #{h_num} data...", file=sys.stderr)
  h_data = fetch_pr_info(h_num)
  if not h_data:
    sys.exit(1)

  print(f"[PIPELINE] Fetching AI PR #{a_num} data...", file=sys.stderr)
  a_data = fetch_pr_info(a_num)
  if not a_data:
    sys.exit(1)

  # Partition both PRs into roller baseline vs fix commits. The baseline
  # is identified by commit subject, not by position: a miscount silently
  # folds the entire Chromium roll into both diffs and drives similarity
  # toward 100%.
  try:
    h_infra, h_fixes = roll_partition.partition_roll_commits(
        h_data.get("commits", []))
    print(
        roll_partition.describe_partition(f"Human PR #{h_num}", h_infra,
                                          h_fixes),
        file=sys.stderr)
    human_diff = roll_partition.resolve_fix_diff(h_infra, h_fixes, repo_root)

    a_infra, a_fixes = roll_partition.partition_roll_commits(
        a_data.get("commits", []))
    print(
        roll_partition.describe_partition(f"AI PR #{a_num}", a_infra, a_fixes),
        file=sys.stderr)
    ai_diff = roll_partition.resolve_fix_diff(a_infra, a_fixes, repo_root)
  except roll_partition.RollPartitionError as err:
    print(f"[ERROR] {err}", file=sys.stderr)
    sys.exit(1)

  if not human_diff or not ai_diff:
    print(
        "[ERROR] One or both PRs produced an empty fix diff. Refusing to "
        "score an empty comparison.",
        file=sys.stderr)
    sys.exit(1)

  # Factual file inventory. The only deterministic measurement here; the
  # difference count itself comes from the expert model.
  inventory = diff_metrics.build_file_inventory(human_diff, ai_diff)
  both_files = inventory["shared"]
  human_only_files = inventory["reference_only"]
  ai_only_files = inventory["candidate_only"]
  differing_files = inventory["shared_differing"]

  print(
      f"[PIPELINE] File inventory (fix commits only):\n"
      f"  - Touched by both : {len(both_files)} "
      f"({len(differing_files)} differ functionally)\n"
      f"  - Human only      : {len(human_only_files)}\n"
      f"  - AI only         : {len(ai_only_files)}",
      file=sys.stderr,
  )

  if args.local or not args.resource_id:
    print(
        "[PIPELINE] Running CobaltReasoningEngine in Local Mode...",
        file=sys.stderr,
    )
    engine = CobaltReasoningEngine(
        project_id=args.project_id,
        location=args.location,
        expert_model=args.expert_model,
        expert_location=args.expert_location,
        gcs_memory_uri=args.gcs_memory_uri,
    )
  else:
    print(
        f"[PIPELINE] Connecting to Remote Reasoning Engine: "
        f"{args.resource_id}...",
        file=sys.stderr,
    )
    engine = ReasoningEngineClient(
        resource_id=args.resource_id,
        project_id=args.project_id,
        location=args.location,
    )

  review_report = generate_comparative_review(
      engine_or_client=engine,
      human_pr_data=h_data,
      ai_pr_data=a_data,
      human_diff=human_diff,
      ai_diff=ai_diff,
      both_files=both_files,
      human_only_files=human_only_files,
      ai_only_files=ai_only_files,
      differing_files=differing_files,
      repo_root=repo_root,
      expert_model=args.expert_model,
  )

  # The headline number is derived by counting parsed blocks, not by
  # asking the model for a total. That keeps it auditable: every counted
  # item is visible in the report below it.
  found = differences.parse_differences(review_report)
  summary = differences.format_summary(found, f"#{h_num}", f"#{a_num}",
                                       inventory)
  full_report = f"{summary}\n\n---\n\n## Expert Notes\n\n{review_report}"

  print(
      f"[PIPELINE] Functional differences found: {len(found)}", file=sys.stderr)
  by_sev = differences.count_by(found, "severity")
  for sev in differences.VALID_SEVERITIES:
    print(f"  - {sev}: {by_sev.get(sev, 0)}", file=sys.stderr)

  print("\n" + "=" * 80)
  print(full_report)
  print("=" * 80 + "\n")

  # Persist lessons to memory
  saved = persist_lessons_to_memory(review_report, engine)

  # Apply skill updates if requested
  if args.update_skills:
    updated_docs = apply_skill_updates(review_report)
    if not updated_docs:
      print(
          "[PIPELINE] Synthesizing explicit skill updates and lessons...",
          file=sys.stderr,
      )
      skill_content = generate_skill_updates_from_review(
          engine, review_report, expert_model=args.expert_model)
      if skill_content:
        saved += persist_lessons_to_memory(skill_content, engine)
        updated_docs = apply_skill_updates(skill_content)
        review_report += (
            f"\n\n---\n\n## 4. Synthesized Skill Updates & Knowledge Items\n\n"
            f"{skill_content}")
        full_report = (
            f"{summary}\n\n---\n\n## Expert Notes\n\n{review_report}")

    doc_str = ", ".join(updated_docs) if updated_docs else "None"
    print(
        f"[PIPELINE] Saved {saved} new knowledge items to GCS Knowledge Bank.",
        file=sys.stderr,
    )
    print(
        f"[PIPELINE] Updated {len(updated_docs)} skill documents: {doc_str}",
        file=sys.stderr,
    )

  if args.out:
    try:
      with open(args.out, "w", encoding="utf-8") as f:
        f.write(full_report)
      print(f"[PIPELINE] Saved review report to: {args.out}", file=sys.stderr)
    except OSError as e:
      print(
          f"[ERROR] Could not write report to {args.out}: {e}", file=sys.stderr)


if __name__ == "__main__":
  main()
