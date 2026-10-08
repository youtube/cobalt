#!/usr/bin/env python3
"""Base abstract class for AI-driven self-healing command resolvers.

Provides the self-healing execution loop shared by all rebase phases
(gclient sync, gn gen and autoninja).
"""

import abc
import collections
import dataclasses
import logging
import os
import re
import subprocess
import time
from typing import Any, Callable, Dict, List, Optional, Tuple
import warnings

from diagnostics import Diagnostic
from patching import apply_parsed_patch, parse_patch, patch_file_changes
from tools import execute_local_tool, extract_tool_commands

# Suppress google.auth UserWarning about ADC quota project on Cloudtop
warnings.filterwarnings("ignore", category=UserWarning, module="google.auth")

log = logging.getLogger(__name__)


@dataclasses.dataclass
class AgentChangeRecord:
  """Tracks an agent modification and resulting build/command status."""

  phase: str
  iteration: int
  target_file: str
  file_changes: Dict[str, str] = dataclasses.field(default_factory=dict)
  error: Optional[str] = None
  command_output: Optional[str] = None
  applied_cleanly: bool = True
  timestamp: float = dataclasses.field(default_factory=time.time)

  @property
  def modified_files(self) -> List[str]:
    """List of files touched by this modification."""
    return list(self.file_changes.keys())

  @property
  def changes(self) -> str:
    """Formatted representation of all modifications in this record."""
    if not self.file_changes:
      return ""
    if len(self.file_changes) == 1:
      return next(iter(self.file_changes.values()))
    return "\n\n".join(f"FILE: {f}\n{c}" for f, c in self.file_changes.items())

  def to_dict(self) -> Dict[str, Any]:
    return {
        "phase": self.phase,
        "iteration": self.iteration,
        "target_file": self.target_file,
        "file_changes": self.file_changes,
        "error": self.error,
        "command_output": self.command_output,
        "modified_files": self.modified_files,
        "applied_cleanly": self.applied_cleanly,
        "timestamp": self.timestamp,
    }

  def to_prompt_str(self) -> str:
    """Formats this record for LLM trajectory consumption."""
    status_label = ("CLEAN (Build Passed)"
                    if self.error is None else f"FAILED: {self.error}")
    lines = [
        (f"#### [{self.phase}] Iteration {self.iteration} -> "
         f"Target: `{self.target_file}`"),
        f"- Patch Applied Cleanly: {self.applied_cleanly}",
        f"- Outcome Status: {status_label}",
    ]
    if self.modified_files:
      lines.append(f"- Modified Files ({len(self.modified_files)}): " +
                   ", ".join(f"`{f}`" for f in self.modified_files))
      lines.append("Patch Attempted:")
      lines.append("```diff")
      lines.append(self.changes.strip())
      lines.append("```")
    if self.error:
      lines.append(f"- Resulting Command Error: `{self.error}`")
    if self.command_output:
      lines.append("Command Output Snippet:")
      lines.append("```")
      lines.append(self.command_output[-2500:].strip())
      lines.append("```")
    return "\n".join(lines)


def extract_meaningful_error_summary(raw_msg: str) -> str:
  """Extracts the first substantive error line, ignoring generic headers."""
  if not raw_msg:
    return ""
  ignored_prefixes = (
      "Siso output:",
      "Build stdout/stderr:",
      "ninja: Entering directory",
      "ninja: build stopped",
  )
  for line in raw_msg.strip().splitlines():
    l_strip = line.strip()
    if not l_strip:
      continue
    if any(l_strip.startswith(p) for p in ignored_prefixes):
      continue
    return re.sub(r"^FAILED:\s+[0-9a-fA-F-]{36}\s+", "FAILED: ", l_strip)
  return raw_msg.strip().splitlines()[0]


def format_history_records(
    history_records: List[Dict[str, Any]],
    window: int = 6,
) -> Tuple[str, str]:
  """Splits resolver history into (patch history, investigation log).

  Records whose iteration is tagged 'Tool-' are read-only investigations
  rather than patch attempts, and the two belong in different sections
  of the model prompt: history says what was already tried, while the
  investigation log says what was already learned.

  This was previously duplicated verbatim in gn_gen, autoninja and
  gclient_sync. Three copies of a prompt-shaping rule is how the two
  diff-scoring implementations drifted apart, so it lives here now.

  Args:
    history_records: Resolver iteration records, oldest first.
    window: How many trailing records to include.

  Returns:
    A (history_str, investigation_str) pair, either of which may be "".
  """
  history_items = []
  investigation_items = []
  for record in history_records:
    iteration = str(record.get("iteration", ""))
    rec_file = record.get("file", "")
    rec_error = record.get("error", "")
    if iteration.startswith("Tool-"):
      investigation_items.append(
          f"Tool Call: `{rec_file}`\nResult:\n```\n{rec_error}\n```")
    else:
      history_items.append(
          f"- Iteration {iteration}: Modified {rec_file} to fix "
          f"\"{rec_error}\"")
  return ("\n".join(history_items[-window:]),
          "\n\n".join(investigation_items[-window:]))


@dataclasses.dataclass
class _LoopState:
  """Mutable state carried across iterations of run_resolution_loop."""

  iteration: int = 0
  history: List[Dict[str, Any]] = dataclasses.field(default_factory=list)
  last_error: str = ""  # Error summary of the most recent failure.
  stuck_count: int = 0  # Consecutive iterations with the same error.
  # Record of the last applied patch; gets the next command's outcome.
  pending_record: Optional[AgentChangeRecord] = None


class BaseResolver(abc.ABC):
  """Abstract base class for all self-healing rebase command execution loops."""

  def __init__(
      self,
      repo_path: str,
      *,
      engine: Optional[Any] = None,
      max_iterations: int = 50,
      on_patch_applied_fn: Optional[Callable[[List[str]], None]] = None,
      session_changes: Optional[List[AgentChangeRecord]] = None,
  ):
    self.repo_path = repo_path
    self.max_iterations = max_iterations
    self.on_patch_applied_fn = on_patch_applied_fn
    self.reasoning_engine = engine
    self.file_error_counts: Dict[str, int] = collections.defaultdict(int)
    self.session_changes: List[AgentChangeRecord] = (
        session_changes if session_changes is not None else [])

  @property
  def model(self) -> str:
    """Workhorse model name; "" when no reasoning engine is attached."""
    if self.reasoning_engine is None:
      return ""
    return self.reasoning_engine.flash_model

  @property
  @abc.abstractmethod
  def name(self) -> str:
    """Human-readable name of the phase/resolver."""

  @abc.abstractmethod
  def run_command(self, iteration: int) -> Tuple[bool, str, str]:
    """Runs the phase command. Returns (success, output, siso_or_stderr)."""

  @abc.abstractmethod
  def extract_diagnostics(self, build_output: str,
                          siso_output: str) -> List[Diagnostic]:
    """Parses failed command output into diagnostics (first one is fixed)."""

  @abc.abstractmethod
  # pylint: disable=too-many-positional-arguments,too-many-arguments
  def resolve_diagnostic(
      self,
      diagnostic: Diagnostic,
      history_records: List[Dict[str, Any]],
      use_expert: bool = False,
      expert_guidance: str = "",
      **kwargs,
  ) -> Tuple[str, str, str]:
    """Generates a patch. Returns (patch, model_used, target_file)."""

  def on_patch_applied(self, modified_files: List[str]) -> None:
    """Hook called immediately after a patch is applied."""
    if self.on_patch_applied_fn:
      self.on_patch_applied_fn(modified_files)

  def get_working_diff(self, max_chars: int = 200000) -> str:
    """Returns git diff of uncommitted modifications in repository."""
    try:
      proc = subprocess.run(
          ["git", "diff", "--no-color", "HEAD"],
          cwd=self.repo_path,
          stdout=subprocess.PIPE,
          stderr=subprocess.PIPE,
          text=True,
          timeout=15,
          check=False,
      )
      if proc.returncode == 0 and proc.stdout:
        return proc.stdout[:max_chars]
    except Exception:  # pylint: disable=broad-exception-caught
      pass
    return ""

  def check_and_clean_stray_marker(
      self,
      file_path: str,
      target_line: Optional[int],
  ) -> Optional[Tuple[str, str, str]]:
    """Fast-path: Automatically removes stray git conflict marker lines."""
    if not target_line or not os.path.isfile(file_path):
      return None
    try:
      with open(file_path, "r", encoding="utf-8", errors="replace") as f:
        lines = f.readlines()
      if 1 <= target_line <= len(lines):
        full_content = "".join(lines)
        if "<<<<<<<" in full_content and "=======" in full_content:
          return None
        err_line = lines[target_line - 1].strip()
        if err_line.startswith(("=======", "<<<<<<<", ">>>>>>>")):
          rel_f = os.path.relpath(file_path, self.repo_path)
          clean_patch = (f"--- a/{rel_f}\n"
                         f"+++ b/{rel_f}\n"
                         f"@@ -{target_line},1 +{target_line},0 @@\n"
                         f"-{lines[target_line - 1]}")
          return clean_patch, "auto-stray-marker-cleaner", rel_f
    except OSError:
      pass
    return None

  def execute_investigation_tools(
      self,
      initial_patch: str,
      diagnostic: Diagnostic,
      *,
      max_rounds: int = 12,
      base_history_records: Optional[List[Dict[str, Any]]] = None,
      expert_guidance: str = "",
  ) -> Tuple[str, str]:
    """Runs multi-turn tool loop supporting batch tool requests from LLM."""
    current_patch = initial_patch
    model_used = self.model
    history_records: List[Dict[str, Any]] = list(base_history_records or [])
    seen_cmds: Dict[str, int] = collections.defaultdict(int)

    for round_idx in range(1, max_rounds + 1):
      tool_cmds = extract_tool_commands(current_patch)
      if not tool_cmds:
        break

      repeated = False
      for cmd in tool_cmds:
        seen_cmds[cmd] += 1
        if seen_cmds[cmd] >= 2:
          repeated = True

      batch_outputs: List[str] = []
      for cmd_idx, tool_cmd in enumerate(tool_cmds, 1):
        prefix = (f"[{cmd_idx}/{len(tool_cmds)}] "
                  if len(tool_cmds) > 1 else "")
        log.info("  [%s] [Investigation Round %s/%s] %sModel requested: %s",
                 self.name, round_idx, max_rounds, prefix, tool_cmd)
        tool_output = execute_local_tool(
            tool_cmd, self.repo_path, session_changes=self.session_changes)
        if len(tool_cmds) > 1:
          batch_outputs.append(
              f"=== Result for {tool_cmd} ===\n{tool_output}\n")
        else:
          batch_outputs.append(tool_output)

      if repeated:
        batch_outputs.append(
            "\n=== SYSTEM NOTICE: Repeated tool request. You have already "
            "inspected these lines. Do NOT request the same file range again. "
            "Synthesize and output the final SEARCH/REPLACE patch block now "
            "(or use TOOL_UPSTREAM_DIFF / TOOL_GREP for new information). ===")

      combined_output = "\n".join(batch_outputs)
      history_records.append({
          "iteration": f"Tool-{round_idx}",
          "file": " | ".join(tool_cmds),
          "error": combined_output,
      })

      if any(seen_cmds[cmd] >= 3 for cmd in tool_cmds):
        log.info(
            "  [%s] [Anti-Loop] Breaking repeated tool loop after %s "
            "rounds.", self.name, round_idx)
        break

      patch_res, m_used, _ = self.resolve_diagnostic(
          diagnostic=diagnostic,
          history_records=history_records,
          use_expert=True,
          expert_guidance=expert_guidance,
      )
      current_patch = patch_res
      model_used = m_used

    if extract_tool_commands(current_patch):
      history_records.append({
          "iteration":
              "Tool-Final",
          "file":
              "SYSTEM_DIRECTIVE",
          "error":
              ("=== TOOL BUDGET EXHAUSTED: Do NOT output any TOOL_* commands. "
               "Synthesize the findings above and output ONLY the final FILE: "
               "and <<<<<<< SEARCH / ======= / >>>>>>> REPLACE patch block(s) "
               "now. ==="),
      })
      current_patch, model_used, _ = self.resolve_diagnostic(
          diagnostic=diagnostic,
          history_records=history_records,
          use_expert=True,
          expert_guidance=expert_guidance,
      )

    return current_patch, model_used

  def run_resolution_loop(self) -> bool:
    """Executes the standard self-healing loop until clean or exhausted."""
    state = _LoopState()
    for iteration in range(1, self.max_iterations + 1):
      state.iteration = iteration
      log.info("\n[%s] >>> Iteration %s/%s...", self.name, iteration,
               self.max_iterations)
      success, output, siso_out = self.run_command(iteration)
      if success:
        log.info("[%s] [SUCCESS] Completed cleanly on iteration %s/%s!",
                 self.name, iteration, self.max_iterations)
        if state.pending_record is not None:
          state.pending_record.error = None
        return True

      diagnostics = self._collect_diagnostics(output, siso_out)
      diag = diagnostics[0]
      rel_file = self._rel_path(diag.file_path)
      error_summary = extract_meaningful_error_summary(diag.error_message)
      self._record_build_outcome(state, error_summary,
                                 output + "\n" + (siso_out or ""))
      self._print_failure(diagnostics, error_summary, rel_file)

      if diag.file_path:
        self.file_error_counts[diag.file_path] += 1
      # Only consecutive identical errors count as being stuck.
      state.stuck_count = (
          state.stuck_count + 1 if error_summary == state.last_error else 0)
      state.last_error = error_summary

      # Escalation while the same error repeats (stuck_count N means N
      # failed fixes in a row): from 2 use the expert model; at 3 revert the
      # file to HEAD so the model can retry from scratch, and again at 5 if
      # that retry also fails; at 8 give up on this phase.
      if (state.stuck_count in (3, 5) and rel_file and
          os.path.isfile(diag.file_path)):
        self._revert_to_baseline(diag.file_path, state)
      if state.stuck_count >= 8:
        log.warning(
            "\n[%s] [CIRCUIT BREAKER] Aborting resolution loop: exceeded "
            "maximum repetition limit (%s) on %s. Halting runaway "
            "build.", self.name, state.stuck_count, rel_file or error_summary)
        return False

      use_expert = (
          state.stuck_count >= 2 or
          self.file_error_counts.get(diag.file_path, 0) >= 3)
      expert_guidance = ""
      if self.reasoning_engine is not None:
        expert_guidance = self._expert_preflight(
            diagnostics, output + "\n" + (siso_out or ""), use_expert,
            state.stuck_count)

      patch, model_used, rel_target = self.resolve_diagnostic(
          diagnostic=diag,
          history_records=state.history,
          use_expert=use_expert,
          expert_guidance=expert_guidance,
      )
      if extract_tool_commands(patch):
        patch, model_used = self.execute_investigation_tools(
            initial_patch=patch,
            diagnostic=diag,
            base_history_records=state.history,
            expert_guidance=expert_guidance,
        )
      if not patch:
        log.warning("[%s] [FAIL] Model returned empty patch.", self.name)
        continue
      self._apply_and_record(patch, model_used, rel_target, state)

    log.warning("[%s] [FAIL] Exhausted maximum iterations (%s).", self.name,
                self.max_iterations)
    return False

  def _rel_path(self, path: str) -> str:
    """Repo-relative form of a diagnostic path ("" stays "")."""
    if not path:
      return ""
    try:
      return os.path.relpath(path, self.repo_path)
    except ValueError:
      return path

  def _collect_diagnostics(self, output: str,
                           siso_out: str) -> List[Diagnostic]:
    """Parses the failure; bare strings and empty results become Diagnostic."""
    diagnostics = [
        d if isinstance(d, Diagnostic) else Diagnostic(error_message=str(d))
        for d in self.extract_diagnostics(output, siso_out)
    ]
    if not diagnostics:
      log.warning(
          "[%s] [WARNING] Command failed but no structured "
          "diagnostics parsed. Using raw output snippet...", self.name)
      diagnostics = [Diagnostic(error_message=output)]
    return diagnostics

  def _record_build_outcome(self, state: _LoopState, error_summary: str,
                            command_output: str) -> None:
    """Attaches this failure to the previous patch attempt."""
    if state.pending_record is not None:
      state.pending_record.error = error_summary
      state.pending_record.command_output = command_output[-4000:]

  def _print_failure(self, diagnostics: List[Diagnostic], error_summary: str,
                     rel_file: str) -> None:
    diag = diagnostics[0]
    loc_str = f" in {rel_file}:{diag.line_number}" if rel_file else ""
    log.info("[%s] Detected %s error(s):\n  - Error%s: %s", self.name,
             len(diagnostics), loc_str, error_summary)
    if diag.raw_snippet:
      snippet_lines = diag.raw_snippet.strip().splitlines()[:5]
      log.info("  [Compiler Snippet]:\n    %s", "\n    ".join(snippet_lines))

  def _revert_to_baseline(self, abs_path: str, state: _LoopState) -> None:
    """Anti-loop: restores a file to HEAD after repeated identical failures."""
    rel_file = self._rel_path(abs_path)
    error_summary = state.last_error
    iteration = state.iteration
    log.info(
        "  [%s] [Anti-Loop] Error '%s' repeated %s times on %s. "
        "Reverting local edits in %s to clean baseline HEAD...", self.name,
        error_summary, state.stuck_count, rel_file, rel_file)
    try:
      head_check = subprocess.run(
          ["git", "show", f"HEAD:{rel_file}"],
          cwd=self.repo_path,
          capture_output=True,
          text=True,
          check=False,
      )
      if head_check.returncode == 0 and "<<<<<<<" in head_check.stdout:
        log.info(
            "  [%s] [Anti-Loop GUARD] Cannot revert %s to HEAD: HEAD "
            "contains raw conflict markers. Keeping current working "
            "file.", self.name, rel_file)
        return
      subprocess.run(
          ["git", "checkout", "HEAD", "--", rel_file],
          cwd=self.repo_path,
          capture_output=True,
          text=True,
          check=False,
      )
      self.on_patch_applied([abs_path])
    except (OSError, subprocess.SubprocessError) as rev_err:
      log.info("  [%s] Notice: Revert failed for %s: %s", self.name, rel_file,
               rev_err)
      return
    state.history.append({
        "iteration": iteration,
        "file": rel_file,
        "error": (f"Reverted {rel_file} to clean baseline due to repeated "
                  f"failed fix attempts ({error_summary}). Please "
                  "re-investigate with an alternative approach."),
        "status": "REVERTED_TO_BASELINE",
    })
    self.session_changes.append(
        AgentChangeRecord(
            phase=self.name,
            iteration=iteration,
            target_file=rel_file,
            file_changes={
                rel_file: f"# Reverted {rel_file} to clean baseline HEAD"
            },
            error=f"Repeated failure ({error_summary}); reverted to baseline",
            applied_cleanly=True,
        ))

  def _session_trajectory(self) -> str:
    """Lists files modified so far this session, for the expert prompt."""
    entries: List[str] = []
    seen_files = set()
    for rec in sorted(
        self.session_changes,
        key=lambda r: (0 if r.phase in
                       ("resolve_conflicts", "conflicts") else 1, r.iteration),
    ):
      if not rec.file_changes:
        continue
      files = list(rec.file_changes.keys())
      seen_files.update(files)
      files_str = ", ".join(f"`{f}`" for f in files)
      other_phase = rec.phase and rec.phase != self.name
      phase_lbl = f"[{rec.phase}] " if other_phase else ""
      entries.append(f"- {phase_lbl}Iteration {rec.iteration}: {files_str}")
    if not entries:
      return ""
    log.info(
        "  [%s] [TIER-2 ARCHITECT] Injected list of %s modified "
        "session files into expert prompt.", self.name, len(seen_files))
    files_list_str = "\n".join(entries)
    return (f"=== Files Modified in Current Session "
            f"({len(seen_files)} files) ===\n"
            f"{files_list_str}\n\n"
            "FIRST-ROUND INVESTIGATION DIRECTIVE:\n"
            "Review the failure and the list of modified files above.\n"
            "Decide which file(s) you need to read and think about before "
            "determining the fix.\n"
            "Use investigation tools to inspect them on demand:\n"
            "- `TOOL_READ_FILE: <filepath> [line_range]` to read source "
            "context.\n"
            "- `TOOL_GET_HISTORY: <filepath>` to view earlier "
            "modifications/diffs for that file.\n"
            "- `TOOL_UPSTREAM_DIFF: <filepath>` to inspect upstream "
            "Chromium diff.\n"
            "- `TOOL_COBALT_DIFF: <filepath>` to inspect what Cobalt"
            "adds on top of upstream in this roll.\n")

  @staticmethod
  def _source_context(diag: Diagnostic, radius: int = 30) -> str:
    """Numbered source lines around the diagnostic location."""
    if not os.path.isfile(diag.file_path):
      return ""
    try:
      with open(diag.file_path, "r", encoding="utf-8", errors="replace") as f:
        lines = f.readlines()
    except OSError:
      return ""
    ln = diag.line_number or 1
    start = max(1, ln - radius)
    end = min(len(lines), ln + radius)
    return "".join(
        f"{start + i}: {l}" for i, l in enumerate(lines[start - 1:end]))

  def _expert_preflight(self, diagnostics: List[Diagnostic],
                        command_output: str, use_expert: bool,
                        stuck_count: int) -> str:
    """Tier-2 review: asks the expert model for a plan before patching.

    The expert may run up to two rounds of investigation tools first.
    Returns the guidance text ("" on failure).
    """
    diag = diagnostics[0]
    action_label = (f"Consulting Expert Agent (repetition: {stuck_count})"
                    if use_expert else
                    "Performing Pre-Flight architectural review")
    log.info("  [%s] [TIER-2 ARCHITECT] %s for %s...", self.name, action_label,
             diag.file_path or self.name)
    working_diff = self.get_working_diff(max_chars=200000)
    trajectory = self._session_trajectory()
    all_diags_str = "\n".join(f"- {d.file_path}:{d.line_number} "
                              f"{d.error_message}" for d in diagnostics[:20])
    source_context = self._source_context(diag)
    name = self.name.lower()
    mode = "gn" if "gn" in name else ("sync" if "sync" in name else "compiler")

    expert_guidance = ""
    investigation_history = ""
    for expert_round in range(1, 4):
      try:
        guidance_res = self.reasoning_engine.generate_expert_guidance(
            target=diag.file_path or self.name,
            diagnostics=diag.trace(),
            source_contexts=source_context,
            trajectory_history=trajectory,
            working_diff=working_diff,
            raw_log=command_output[-30000:],
            all_diagnostics=all_diags_str,
            investigation_history=investigation_history,
            mode=mode,
            expert_model=self.reasoning_engine.expert_model,
        )
        expert_guidance = guidance_res.get("guidance", "")
        tool_cmds = extract_tool_commands(expert_guidance)
        if not tool_cmds or expert_round == 3:
          break
        investigation_history += "\n\n" + self._run_expert_tools(tool_cmds)
      except Exception as e:  # pylint: disable=broad-exception-caught
        log.info("  [%s] Notice: Pre-flight expert guidance query: %s",
                 self.name, e)
        break

    if expert_guidance:
      log.info("  [%s] [TIER-2 ARCHITECT] Pre-Flight Plan:\n  >>> %s...",
               self.name,
               expert_guidance.splitlines()[0][:100])
    return expert_guidance

  def _run_expert_tools(self, tool_cmds: List[str]) -> str:
    """Runs tools requested by the expert; returns their formatted results."""
    results = []
    for t_cmd in tool_cmds:
      log.info("  [%s] [TIER-2 ARCHITECT] Tool requested: %s", self.name, t_cmd)
      t_out = execute_local_tool(
          t_cmd, self.repo_path, session_changes=self.session_changes)
      results.append(f"Tool Call: `{t_cmd}`\nResult:\n```\n{t_out}\n```")
    return "\n\n".join(results)

  def _apply_and_record(self, patch: str, model_used: str, rel_target: str,
                        state: _LoopState) -> None:
    """Applies the model's patch and records the attempt either way."""
    iteration = state.iteration
    log.info("[%s] Applying AI patch using %s to %s...", self.name, model_used,
             rel_target)
    parsed = parse_patch(patch, self.repo_path, default_file=rel_target)
    modified_files = apply_parsed_patch(parsed, self.repo_path)
    if not modified_files:
      log.warning(
          "[%s] [FAIL] Could not apply patch to %s.\n  [AI Patch "
          "Preview]:\n  %s", self.name, rel_target, patch[:300].strip())
      self.session_changes.append(
          AgentChangeRecord(
              phase=self.name,
              iteration=iteration,
              target_file=rel_target,
              file_changes={rel_target: patch},
              error=f"Patch failed to apply to {rel_target}",
              command_output=None,
              applied_cleanly=False,
          ))
      state.pending_record = None
      state.history.append({
          "iteration": iteration,
          "file": rel_target,
          "error":
              (f"Patch failed to apply to {rel_target}. Ensure <<<<<<< SEARCH "
               "matches exact file lines and >>>>>>> REPLACE contains clean "
               "code without stray conflict markers."),
          "status": "FAILED_TO_APPLY",
      })
      return

    mod_summary = ", ".join(
        os.path.relpath(f, self.repo_path) for f in modified_files)
    log.info("[%s] [OK] Patch applied cleanly to: %s", self.name, mod_summary)
    self.on_patch_applied(modified_files)
    state.pending_record = AgentChangeRecord(
        phase=self.name,
        iteration=iteration,
        target_file=rel_target,
        file_changes=patch_file_changes(
            parsed, self.repo_path, default_file=rel_target),
        error=None,
        command_output=None,
        applied_cleanly=True,
    )
    self.session_changes.append(state.pending_record)
    state.history.append({
        "iteration": iteration,
        "file": rel_target,
        "error": state.last_error,
        "status": "APPLIED",
    })
