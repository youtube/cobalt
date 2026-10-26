#!/usr/bin/env python3
"""Unified AI-driven merge conflict resolver library for Cobalt Chromium rebase.

Provides ConflictResolver, which resolves merge conflicts across all files in
the repository (DEPS, C++, Java, GN, etc.) using Vertex AI Reasoning Engine,
validates Python AST on DEPS, and generates structured rebase summaries.
"""

import ast
import dataclasses
import logging
import os
import re
import subprocess
from typing import Any, List, Optional, Tuple
import warnings

from base_resolver import consult_expert
from change_record import NO_RATIONALE, AgentChangeRecord, guidance_summary
from fix_commits import (commit_files, conflict_commit_message,
                         conflict_rationales, diff_for, request_message)
from patching import (NEEDS_HUMAN_PREFIX, exec_script_calls, exec_script_spans)
from repo_guards import is_unmodified_third_party
from tools import (
    execute_local_tool,
    extract_line_anchored_tool_commands,
)

log = logging.getLogger(__name__)

# Suppress google.auth UserWarning about ADC quota project on Cloudtop
warnings.filterwarnings("ignore", category=UserWarning, module="google.auth")


@dataclasses.dataclass
class ConflictBlock:
  """Represents an extracted merge conflict chunk with surrounding context."""

  index: int
  start_line: int
  end_line: int
  raw_block: str
  ours_content: str
  base_content: Optional[str]
  theirs_content: str
  context_before: str
  context_after: str

  @property
  def upstream_is_ours(self) -> bool:
    """Whether the '<<<<<<<' (ours) side holds the new upstream Chromium code.

    Polarity depends on how the conflict was produced:

    * Normal merge / cherry-pick of an upstream change onto Cobalt:
      ours = Cobalt, theirs = upstream. Upstream is on the 'theirs' side.

    * autoroll_chromium.py, which reverts the Cobalt commits off a new
      Chromium snapshot: the conflict is labelled
      '>>>>>>> parent of <sha> (CONFLICTED Chromium Cherry pick: Revert
      Cobalt.)'. Here 'theirs' is the OLD pre-roll Cobalt content and
      'ours' (HEAD) is the NEW Chromium snapshot, so upstream is on the
      'ours' side.

    Getting this backwards silently rewinds untouched third-party files to
    the previous milestone, so the polarity is derived from the marker
    label rather than assumed.
    """
    lines = self.raw_block.splitlines()
    if not lines:
      return False
    closing = lines[-1]
    if not closing.startswith(">>>>>>>"):
      return False
    label = closing[len(">>>>>>>"):].strip().lower()
    return label.startswith("parent of") or "revert cobalt" in label

  @property
  def upstream_content(self) -> str:
    """The side of the conflict holding the new upstream Chromium code."""
    return self.ours_content if self.upstream_is_ours else self.theirs_content

  @property
  def cobalt_content(self) -> str:
    """The side of the conflict holding Cobalt's pre-roll code."""
    return self.theirs_content if self.upstream_is_ours else self.ours_content


@dataclasses.dataclass
class EscalationItem:
  """Records complex conflict blocks that require human review."""

  file_path: str
  block_index: int
  reason: str


def detect_language(file_path: str) -> str:
  """Detects programming language from file path/extension."""
  basename = os.path.basename(file_path)
  if basename == "DEPS":
    return "Python (Chromium DEPS)"

  ext = os.path.splitext(file_path)[1].lower()
  mapping = {
      ".cc": "C++",
      ".cpp": "C++",
      ".cxx": "C++",
      ".c": "C",
      ".h": "C/C++ Header",
      ".hh": "C/C++ Header",
      ".hpp": "C/C++ Header",
      ".inc": "C/C++ Header",
      ".mm": "Objective-C++",
      ".m": "Objective-C",
      ".java": "Java",
      ".tmpl": "Java / Build Template",
      ".gn": "GN Build File",
      ".gni": "GN Build File",
      ".py": "Python",
      ".spec": "Grit / Spec Config",
      ".json": "JSON",
      ".xml": "XML",
      ".grd": "Grit Resource",
      ".grdp": "Grit Resource",
  }
  return mapping.get(ext, "Source Code")


def sort_conflict_priority(file_path: str) -> Tuple[int, str]:
  """Prioritizes toolchain sync scripts and build configs before sources."""
  base = os.path.basename(file_path)
  norm = file_path.replace("\\", "/")
  if base == "DEPS":
    return 0, norm
  if "tools/clang/scripts" in norm or "tools/rust" in norm:
    return 1, norm
  if norm.endswith(".star"):
    return 2, norm
  if norm.endswith((".gn", ".gni")):
    return 3, norm
  if norm.endswith((".java", ".tmpl")):
    return 4, norm
  if norm.endswith((".h", ".hh", ".hpp", ".inc")):
    return 5, norm
  if norm.endswith((".cc", ".cpp", ".cxx", ".mm", ".c")):
    return 6, norm
  return 7, norm


def extract_git_context(repo_path: str) -> str:
  """Extracts branch, rebase commits, and merge base information."""
  ctx_lines = []

  def _run_git(args: List[str]) -> str:
    try:
      res = subprocess.run(
          ["git"] + args,
          cwd=repo_path,
          capture_output=True,
          text=True,
          errors="replace",
          check=False,
      )
      return res.stdout.strip()
    except (OSError, subprocess.SubprocessError):
      return ""

  branch = _run_git(["branch", "--show-current"])
  if branch:
    ctx_lines.append(f"Current Branch: {branch}")

  head_sha = _run_git(["rev-parse", "--short", "HEAD"])
  if head_sha:
    ctx_lines.append(f"HEAD Commit: {head_sha}")

  git_dir = _run_git(["rev-parse", "--git-dir"])
  if git_dir:
    onto_file = os.path.join(repo_path, git_dir, "rebase-merge", "onto")
    if os.path.isfile(onto_file):
      with open(onto_file, "r", encoding="utf-8") as f:
        ctx_lines.append(f"Rebase Onto Commit: {f.read().strip()[:10]}")

  merge_base = _run_git(["merge-base", "HEAD", "HEAD@{u}"])
  if merge_base:
    ctx_lines.append(f"Merge Base: {merge_base[:10]}")

  return "\n".join(ctx_lines)


def is_ignored_conflict_path(rel_path: str) -> bool:
  """Excludes test suites, rebase tooling, and build output dirs."""
  norm = rel_path.replace("\\", "/")
  return (norm.startswith("out/") or
          norm.startswith("cobalt/devinfra/rebase/") or
          norm.startswith("results/"))


def find_all_conflicted_files(repo_path: str) -> List[str]:
  """Finds all files in git working tree that have unmerged conflict markers."""
  conflicted = []
  try:
    res = subprocess.run(
        ["git", "diff", "--name-only", "--diff-filter=U"],
        cwd=repo_path,
        capture_output=True,
        text=True,
        errors="replace",
        check=False,
    )
    if res.returncode == 0 and res.stdout.strip():
      conflicted = [
          l.strip()
          for l in res.stdout.splitlines()
          if l.strip() and not is_ignored_conflict_path(l.strip())
      ]
  except (OSError, subprocess.SubprocessError):
    pass

  # Fallback: scan for <<<<<<< HEAD markers
  if not conflicted:
    try:
      res2 = subprocess.run(
          ["git", "grep", "-l", "-I", "^<<<<<<<", "--", "."],
          cwd=repo_path,
          capture_output=True,
          text=True,
          errors="replace",
          check=False,
      )
      if res2.returncode == 0 and res2.stdout.strip():
        conflicted = [
            l.strip()
            for l in res2.stdout.splitlines()
            if l.strip() and not is_ignored_conflict_path(l.strip())
        ]
    except (OSError, subprocess.SubprocessError):
      pass

  conflicted.sort(key=sort_conflict_priority)
  return conflicted


def extract_conflict_blocks(
    content: str,
    context_lines: int = 15,
) -> List[ConflictBlock]:
  """Extracts all diff3 or normal merge conflict blocks from file text."""
  lines = content.splitlines(keepends=True)
  blocks = []
  i = 0
  idx = 1

  while i < len(lines):
    if lines[i].startswith("<<<<<<<"):
      start = i
      ours = []
      base = []
      theirs = []
      has_base = False
      in_base = False
      in_theirs = False
      i += 1
      while i < len(lines):
        if lines[i].startswith("|||||||"):
          has_base = True
          in_base = True
          in_theirs = False
          i += 1
          continue
        if lines[i].startswith("======="):
          in_base = False
          in_theirs = True
          i += 1
          continue
        if lines[i].startswith(">>>>>>>"):
          end = i
          raw_blk = "".join(lines[start:end + 1])
          ctx_before = "".join(lines[max(0, start - context_lines):start])
          ctx_after = "".join(lines[end +
                                    1:min(len(lines), end + 1 + context_lines)])
          blocks.append(
              ConflictBlock(
                  index=idx,
                  start_line=start + 1,
                  end_line=end + 1,
                  raw_block=raw_blk,
                  ours_content="".join(ours),
                  base_content="".join(base) if has_base else None,
                  theirs_content="".join(theirs),
                  context_before=ctx_before,
                  context_after=ctx_after,
              ))
          idx += 1
          i += 1
          break

        if in_theirs:
          theirs.append(lines[i])
        elif in_base:
          base.append(lines[i])
        else:
          ours.append(lines[i])
        i += 1
    else:
      i += 1
  return blocks


def clean_output(text: str) -> str:
  """Strips markdown code fences if model wrapped response in ```."""
  stripped = text.strip()
  match = re.search(r"^```(?:[a-zA-Z0-9_\+\-]+)?\s*\n(.*?)\n```$", stripped,
                    re.DOTALL)
  if match:
    return match.group(1)
  if stripped.startswith("```") and stripped.endswith("```"):
    lines = stripped.splitlines()
    if len(lines) >= 2:
      return "\n".join(lines[1:-1])
  return text.rstrip()


def _exec_script_resolution_problem(content: str, block: ConflictBlock,
                                    resolved_code: str, rel_path: str) -> str:
  """Why a resolution of a GN conflict needs a human ("" if it is fine).

  gn gen runs exec_script() commands, so a conflict that overlaps an
  exec_script() call may only be resolved with code Chromium or Cobalt
  already reviewed: either side alone, or both sides (in either order) with
  the markers removed. Any other exec_script() result was written by the
  model.
  """
  if not rel_path.endswith((".gn", ".gni")):
    return ""
  start = content.index(block.raw_block)
  end = start + len(block.raw_block)
  if not any(a < end and start < b for a, b in exec_script_spans(content)):
    return ""

  def calls(code: str) -> List[str]:
    return exec_script_calls(content.replace(block.raw_block, code, 1))

  ours, theirs = block.ours_content, block.theirs_content
  reviewed = (ours, theirs, ours + theirs, theirs + ours)
  if calls(resolved_code) in [calls(code) for code in reviewed]:
    return ""
  return (f"{NEEDS_HUMAN_PREFIX}conflict block #{block.index} in {rel_path} "
          "overlaps an exec_script() call and the resolution is not one side "
          "or both sides of the conflict; gn gen would run it, so resolve it "
          "by hand")


def _block_review(rel_path: str, block: ConflictBlock) -> dict:
  """Review fields naming a conflict block and keeping both of its sides.

  Everything here comes from the local repository: the file, the block's
  line numbers, and the upstream and Cobalt sides as they were before the
  resolution, so a reviewer can judge the result without replaying the merge.
  """
  return {
      "trigger": (f"Conflict block #{block.index} in {rel_path} "
                  f"(lines {block.start_line}-{block.end_line})"),
      "upstream_side": block.upstream_content,
      "cobalt_side": block.cobalt_content,
  }


def resolve_file_conflicts(
    file_path: str,
    repo_path: str,
    git_context: str,
    *,
    engine: Optional[Any] = None,
    escalations: Optional[List[EscalationItem]] = None,
    session_changes: Optional[List[AgentChangeRecord]] = None,
    max_tool_rounds: int = 5,
    mock_mode: bool = False,
) -> bool:
  """Resolves all conflict markers in a specific file."""
  if not os.path.isfile(file_path):
    return False

  with open(file_path, "r", encoding="utf-8", errors="replace") as f:
    content = f.read()

  blocks = extract_conflict_blocks(content)
  if not blocks:
    return True

  lang = detect_language(file_path)
  rel_path = os.path.relpath(file_path, repo_path)

  # 1. Fast-path: Check if this is an unmodified third_party file
  if is_unmodified_third_party(file_path, repo_path):
    side = "ours/HEAD" if blocks[0].upstream_is_ours else "theirs"
    log.info(
        "\n[resolve_conflicts] Fast-path: %s is unmodified "
        "third_party. Resolving %s conflict(s) with upstream "
        "(%s)...", rel_path, len(blocks), side)
    for block in blocks:
      upstream = block.upstream_content
      content = content.replace(block.raw_block, upstream, 1)
      if session_changes is not None:
        session_changes.append(
            AgentChangeRecord(
                phase="resolve_conflicts",
                iteration=block.index,
                target_file=rel_path,
                file_changes={
                    rel_path: ("Unmodified third_party resolved with upstream "
                               f"({side}):\n{upstream}")
                },
                error=None,
                applied_cleanly=True,
                rationale=("Cobalt never modified this third_party file, so "
                           "the upstream side of the conflict was taken "
                           "without consulting the model."),
                model="fast-path",
                **_block_review(rel_path, block),
            ))
    with open(file_path, "w", encoding="utf-8") as f:
      f.write(content)
    log.info("  [OK] Resolved all %s block(s) in %s using upstream.",
             len(blocks), rel_path)
    return True

  log.info("\n[resolve_conflicts] Resolving %s conflict(s) in %s "
           "(%s)...", len(blocks), rel_path, lang)

  # Records of resolved blocks are kept until the file is written, so a
  # later block or the DEPS check failing leaves no applied_cleanly records
  # for changes that never reached the file.
  resolved_records: List[AgentChangeRecord] = []
  for block in blocks:
    log.info("  - Block #%s (lines %s-%s, %s lines)...",
             block.index, block.start_line, block.end_line,
             len(block.raw_block.splitlines()))

    # Step 1: Pre-Flight Strategic Review by Expert Agent. The expert may
    # ask for tools; consult_expert feeds the results back until it answers
    # with a plan, so the worker (and the change record) get its conclusion
    # rather than the sentence it wrote before its first tool request.
    trajectory_str = git_context

    expert_plan = ""
    expert_guidance = ""
    if engine is not None and not mock_mode:
      expert_plan, expert_tools = consult_expert(
          engine,
          repo_path,
          session_changes,
          log_label="resolve_conflicts",
          target=rel_path,
          diagnostics=(f"Merge conflict in {rel_path} ({lang}):\n"
                       f"{block.raw_block}"),
          source_contexts=(
              f"Context before conflict:\n{block.context_before}\n\n"
              f"Context after conflict:\n{block.context_after}"),
          trajectory_history=trajectory_str,
          mode="conflict",
      )
      # The worker sees the plan and what the expert looked at.
      expert_guidance = expert_plan + expert_tools
      if expert_plan:
        first_g_line = expert_plan.splitlines()[0][:100]
        log.info("    [TIER-2 ARCHITECT] Pre-Flight Plan:\n    >>> %s...",
                 first_g_line)

    resolved_code: Optional[str] = None
    investigation_history = ""
    model_used = ""
    for attempt in range(2):
      use_expert = attempt > 0
      if use_expert:
        log.info("    [EXPERT_RETRY] Retrying Block #%s with Expert Agent...",
                 block.index)
      for _ in range(max_tool_rounds):
        if mock_mode:
          resolved_code = block.theirs_content
          model_used = "mock"
          break

        if engine is None:
          raise ValueError(
              "ReasoningEngine client required for conflict resolution.")

        try:
          res = engine.resolve_conflict(
              file_path=rel_path,
              language=lang,
              raw_conflict=block.raw_block,
              context_before=block.context_before,
              context_after=block.context_after,
              git_context=git_context,
              investigation_history=investigation_history,
              expert_guidance=expert_guidance,
              use_expert=use_expert,
          )
        except Exception as e:  # pylint: disable=broad-exception-caught
          log.warning("    [FAIL] Reasoning Engine Error: %s", e)
          break

        raw_replacement = ""
        if isinstance(res, dict):
          raw_replacement = res.get("replacement", "")
          model_used = res.get("model_used") or model_used
        elif isinstance(res, str):
          raw_replacement = res

        # Line-anchored, not the permissive scanner: this payload is
        # source code, where an indented 'case TOOL_TIP:' would
        # otherwise parse as a tool request and burn the round.
        tool_cmds = extract_line_anchored_tool_commands(raw_replacement)[:1]
        if tool_cmds:
          tool_cmd = tool_cmds[0]
          log.info("    [TOOL_USE] Model requested: %s", tool_cmd)
          tool_output = execute_local_tool(
              tool_cmd, repo_path, session_changes=session_changes)
          investigation_history += (
              f"\n\nTool Call: `{tool_cmd}`\nResult:\n```\n{tool_output}\n```")
          continue

        resolved_code = clean_output(raw_replacement)
        break

      if resolved_code is not None and "<<<<<<<" not in resolved_code:
        break

    # Review context for this block: which block and its two sides, why it
    # was resolved this way (the expert's plan; the resolver itself returns
    # only code), and by which model.
    review = dict(
        _block_review(rel_path, block),
        rationale=guidance_summary(expert_plan) or NO_RATIONALE,
        model=model_used)

    if resolved_code is None or "<<<<<<<" in resolved_code:
      log.info("    [ESCALATE] Block #%s could not be cleanly resolved.",
               block.index)
      if session_changes is not None:
        session_changes.append(
            AgentChangeRecord(
                phase="resolve_conflicts",
                iteration=block.index,
                target_file=rel_path,
                file_changes={rel_path: block.raw_block},
                error=f"Unresolved conflict markers in block #{block.index}",
                applied_cleanly=False,
                **review,
            ))
      if escalations is not None:
        escalations.append(
            EscalationItem(rel_path, block.index,
                           "Unresolved conflict markers"))
      return False

    if (block.raw_block.endswith("\n") and resolved_code and
        not resolved_code.endswith("\n")):
      resolved_code += "\n"
    reason = _exec_script_resolution_problem(content, block, resolved_code,
                                             rel_path)
    if reason:
      log.warning("  [STOP] %s", reason)
      if session_changes is not None:
        session_changes.append(
            AgentChangeRecord(
                phase="resolve_conflicts",
                iteration=block.index,
                target_file=rel_path,
                file_changes={
                    rel_path: (f"Conflict Block #{block.index}:\n"
                               f"{block.raw_block}\n\nProposed Resolution:\n"
                               f"{resolved_code}")
                },
                error=reason,
                applied_cleanly=False,
                **review,
            ))
      if escalations is not None:
        escalations.append(EscalationItem(rel_path, block.index, reason))
      return False
    content = content.replace(block.raw_block, resolved_code, 1)
    resolved_records.append(
        AgentChangeRecord(
            phase="resolve_conflicts",
            iteration=block.index,
            target_file=rel_path,
            file_changes={
                rel_path: (f"Conflict Block #{block.index}:\n{block.raw_block}"
                           f"\n\nResolved Code:\n{resolved_code}")
            },
            error=None,
            applied_cleanly=True,
            **review,
        ))
    log.info("    [OK] Resolved Block #%s", block.index)

  if os.path.basename(file_path) == "DEPS":
    try:
      ast.parse(content)
      log.info("  [OK] DEPS Python AST syntax validated.")
    except SyntaxError as e:
      log.warning("  [FAIL] DEPS AST Syntax Error: %s", e)
      if session_changes is not None:
        session_changes.append(
            AgentChangeRecord(
                phase="resolve_conflicts",
                iteration=0,
                target_file=rel_path,
                file_changes={rel_path: "DEPS AST Validation"},
                error=f"DEPS AST Syntax Error: {e}",
                applied_cleanly=False,
            ))
      if escalations is not None:
        escalations.append(
            EscalationItem(rel_path, 0, f"DEPS AST Syntax Error: {e}"))
      return False

  with open(file_path, "w", encoding="utf-8") as f:
    f.write(content)
  if session_changes is not None:
    session_changes.extend(resolved_records)
  log.info("[resolve_conflicts] Successfully processed %s.", rel_path)
  return True


class ConflictResolver:
  """Phase 1 resolver for git merge conflicts (DEPS & source files)."""

  def __init__(
      self,
      repo_path: str,
      *,
      engine: Optional[Any] = None,
      session_changes: Optional[List[AgentChangeRecord]] = None,
      commit_per_fix: bool = False,
  ):
    self.repo_path = repo_path
    self.reasoning_engine = engine
    self.session_changes = session_changes
    # Commit each resolved file on its own, with the reason for every
    # block in the message (see fix_commits.conflict_commit_message).
    self.commit_per_fix = commit_per_fix
    self.git_context = extract_git_context(repo_path)
    self.escalations: List[EscalationItem] = []

  def _commit_resolved_file(self, rel_path: str, first_record: int) -> None:
    """Commits one resolved file; its records are session_changes[first:]."""
    records = (
        self.session_changes[first_record:]
        if self.session_changes is not None else [])
    blocks = [r for r in records if r.applied_cleanly]
    message = request_message(
        self.reasoning_engine,
        phase="Phase 1 (conflict resolution)",
        trigger=(f"Merge conflict in {rel_path}: {len(blocks)} block(s) "
                 "between the new Chromium code and Cobalt's changes"),
        outcome="all conflict markers in the file resolved",
        diff=diff_for(self.repo_path, [rel_path]),
        rationales=conflict_rationales(records))
    subject, body = conflict_commit_message(rel_path, records,
                                            message.get("subject", ""),
                                            message.get("body", ""))
    sha = commit_files(self.repo_path, [rel_path], subject, body)
    if sha:
      for rec in records:
        rec.commit = sha

  def run_resolution_loop(self) -> bool:
    """Resolves all conflicted files in the repository."""
    target_files = [
        os.path.join(self.repo_path, f)
        for f in find_all_conflicted_files(self.repo_path)
    ]
    if not target_files:
      log.info("[resolve_conflicts] No conflicted files found in "
               "repository.")
      return True

    log.info("=" * 70)
    log.info("[resolve_conflicts] FOUND %s CONFLICTED FILE(S):",
             len(target_files))
    for idx, tf in enumerate(target_files, 1):
      rel = os.path.relpath(tf, self.repo_path)
      lang = detect_language(tf)
      log.info("  %2d. %s [%s]", idx, rel, lang)
    log.info("=" * 70)

    resolved = 0
    for tf in target_files:
      if not os.path.isfile(tf):
        continue
      first_record = (
          len(self.session_changes) if self.session_changes is not None else 0)
      if resolve_file_conflicts(
          file_path=tf,
          repo_path=self.repo_path,
          git_context=self.git_context,
          escalations=self.escalations,
          engine=self.reasoning_engine,
          session_changes=self.session_changes,
      ):
        resolved += 1
        if self.commit_per_fix:
          self._commit_resolved_file(
              os.path.relpath(tf, self.repo_path), first_record)
      else:
        log.warning("[WARNING] Issues detected in: %s", tf)

    log.info("\n%s", "=" * 70)
    log.info("[resolve_conflicts] ALL %s CONFLICTED FILES PROCESSED!", resolved)
    if self.escalations:
      log.warning(
          "  - [WARNING] Escalations Flagged: %s block(s) require "
          "human review", len(self.escalations))
    log.info("=" * 70)

    return resolved == len(target_files)
