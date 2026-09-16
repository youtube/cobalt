#!/usr/bin/env python3
"""AI-driven GN build generation and self-healing resolver library.

Provides GNGenResolver, which executes `cobalt/build/gn.py`, parses GN
tracebacks and missing dependency graphs, prompts Gemini via Vertex AI
Reasoning Engine, applies patches with third-party guards, and verifies
clean GN manifest generation with header checks.
"""

import dataclasses
import os
import re
import subprocess
import sys
from typing import Any, Callable, Dict, List, Optional, Tuple
import warnings

from base_resolver import (
    AgentChangeRecord,
    BaseResolver,
    format_history_records,
    get_clean_build_env,
)
# Suppress google.auth UserWarning about ADC quota project on Cloudtop
warnings.filterwarnings("ignore", category=UserWarning, module="google.auth")


@dataclasses.dataclass
class GNDiagnostic:
  """Represents a GN build error diagnostic parsed from gn gen output."""

  error_message: str
  raw_output: str
  target_files: Dict[str, Optional[int]]
  is_structural_break: bool
  file_path: str = ""
  line_number: int = 1


# Each row maps a regex over gn/ninja output to the files it implicates.
# Steps 1, 2 and 4 below were previously three verbatim copies of the
# same four lines differing only in the pattern, so they are data now.
# The "kind" selects how a captured group becomes a path:
#   "target_dir" -> //dir:name      => <dir>/BUILD.gn
#   "gn_file"    -> //path/file.gn  => that file, with optional :line
#   "source"     -> ERROR at //f.cc => that file AND its sibling BUILD.gn
_GN_TARGET_DIR_PATTERNS = (
    # 1. Target definitions: "The target: //dir:target"
    re.compile(r"The target:\s*(?:\n\s*)?//([a-zA-Z0-9_/\.\-]+):"),
    # 2. Caller targets missing a dependency: "dependency of //dir:target"
    re.compile(r"dependency of\s*(?:\n\s*)?//([a-zA-Z0-9_/\.\-]+):"),
    # 4. Resolve GN targets: "target(s): //dir:target" or "needs //dir:target"
    re.compile(r"(?:target(?:\(s\))?:\s+|needs\s+)//([a-zA-Z0-9_/\.\-]+):"),
)

# 3. Universal scan: any //path/to/file.gn[i] with an optional line number.
_GN_FILE_PATTERN = re.compile(r"//([a-zA-Z0-9_/\.\-]+\.gn[i]?)(?::(\d+))?")

# 5. Source files named by a GN error: "ERROR at //path/to/file.cc:line".
_GN_SOURCE_PATTERN = re.compile(
    r"ERROR at //([a-zA-Z0-9_/\.\-]+\.(?:cc|h|mm|cpp|c))(?::(\d+))?")


def _add_build_file_for_dir(
    found: Dict[str, Optional[int]],
    repo_path: str,
    target_dir: str,
) -> None:
  """Adds <target_dir>/BUILD.gn if it exists and is not already known."""
  gn_path = os.path.join(repo_path, target_dir, "BUILD.gn")
  if os.path.isfile(gn_path) and gn_path not in found:
    found[gn_path] = None


def _collect_dependency_cycle_files(
    found: Dict[str, Optional[int]],
    repo_path: str,
) -> None:
  """Prioritizes locally modified build files for a dependency cycle.

  A cycle has no single culprit line, so the files this rebase already
  touched are the best starting point.
  """
  try:
    proc = subprocess.run(
        ["git", "status", "--porcelain", "--", "*.gn", "*.gni"],
        cwd=repo_path,
        capture_output=True,
        text=True,
        check=False,
    )
    for line in proc.stdout.splitlines():
      f_rel = line.strip().split()[-1]
      if f_rel.endswith((".gn", ".gni")):
        abs_f = os.path.join(repo_path, f_rel)
        if os.path.isfile(abs_f):
          found[abs_f] = None
  except (OSError, subprocess.SubprocessError):
    pass


def extract_gn_target_files(
    output: str,
    repo_path: str,
) -> Dict[str, Optional[int]]:
  """Extracts all referenced GN build files and line numbers from output.

  This is an accumulating chain, not a fallback chain: every step runs
  and the first insertion for a path wins, so the order below is the
  precedence order for line numbers.
  """
  unique_gn_files: Dict[str, Optional[int]] = {}

  # Priority 0: Dependency cycle -> locally modified BUILD.gn files.
  if "Dependency cycle:" in output:
    _collect_dependency_cycle_files(unique_gn_files, repo_path)

  # Steps 1 and 2: //dir:target references that imply <dir>/BUILD.gn.
  for pattern in _GN_TARGET_DIR_PATTERNS[:2]:
    for target_dir in pattern.findall(output):
      _add_build_file_for_dir(unique_gn_files, repo_path, target_dir)

  # Step 3: any explicitly named .gn/.gni file.
  #
  # BUILDCONFIG.gn is deferred rather than dropped: it is global, so
  # when the output also names a specific target or missing source, the
  # narrower file is the better patch target and should be offered
  # first. BUILDCONFIG.gn is still appended at the end as a fallback.
  deferred_gn_files: Dict[str, Optional[int]] = {}
  defer_buildconfig = ("Source file not found" in output or
                       "The target:" in output)
  for f, line_str in _GN_FILE_PATTERN.findall(output):
    full_p = os.path.join(repo_path, f) if not os.path.isabs(f) else f
    if os.path.isfile(full_p) and full_p not in unique_gn_files:
      target_line = int(line_str) if line_str else None
      if f.endswith("BUILDCONFIG.gn") and defer_buildconfig:
        deferred_gn_files[full_p] = target_line
      else:
        unique_gn_files[full_p] = target_line

  # Step 4: remaining //dir:target forms.
  for target_dir in _GN_TARGET_DIR_PATTERNS[2].findall(output):
    _add_build_file_for_dir(unique_gn_files, repo_path, target_dir)

  # Step 5: source files named by a GN error, plus their sibling BUILD.gn.
  for s_rel, line_str in _GN_SOURCE_PATTERN.findall(output):
    s_abs = os.path.join(repo_path, s_rel)
    if os.path.isfile(s_abs) and s_abs not in unique_gn_files:
      unique_gn_files[s_abs] = int(line_str) if line_str else None
    _add_build_file_for_dir(unique_gn_files, repo_path, os.path.dirname(s_rel))

  for p, line_no in deferred_gn_files.items():
    if p not in unique_gn_files:
      unique_gn_files[p] = line_no

  return unique_gn_files


class GNGenResolver(BaseResolver):
  """Self-healing resolver for cobalt/build/gn.py generation failures."""

  def __init__(
      self,
      repo_path: str,
      platform: str,
      build_type: str,
      *,
      gn_check: bool = True,
      engine: Optional[Any] = None,
      max_iterations: int = 50,
      session_changes: Optional[List[AgentChangeRecord]] = None,
      on_patch_applied_fn: Optional[Callable[[List[str]], None]] = None,
      **kwargs: Any,
  ):
    super().__init__(
        repo_path=repo_path,
        engine=engine,
        max_iterations=max_iterations,
        session_changes=session_changes,
        on_patch_applied_fn=on_patch_applied_fn,
        **kwargs,
    )
    self.platform = platform
    self.build_type = build_type
    self.gn_check = gn_check

  @property
  def name(self) -> str:
    return "Phase 3 (GN Generation)"

  def run_command(self, iteration: int) -> Tuple[bool, str, str]:
    del iteration  # Unused in standard GN command execution
    gn_script = os.path.join(self.repo_path, "cobalt", "build", "gn.py")
    cmd = [
        sys.executable,
        gn_script,
        "-p",
        self.platform,
        "-C",
        self.build_type,
    ]
    if self.gn_check:
      cmd.append("--check")

    cmd_str = " ".join(cmd)
    print(
        f"\n[gn_gen] Executing: {cmd_str} in {self.repo_path}",
        file=sys.stderr,
    )
    clean_env = get_clean_build_env()
    try:
      proc = subprocess.run(
          cmd,
          cwd=self.repo_path,
          capture_output=True,
          text=True,
          env=clean_env,
          check=False,
      )
      combined_output = f"{proc.stdout}\n{proc.stderr}"
      return proc.returncode == 0, combined_output, ""
    except Exception as e:  # pylint: disable=broad-exception-caught
      return False, f"Subprocess execution failed: {e}", ""

  def extract_diagnostics(self, build_output: str,
                          siso_output: str) -> List[Any]:
    del siso_output  # Unused in GN generation
    stripped = build_output.strip()
    error_summary = stripped.splitlines()[0] if stripped else "GN Error"
    target_files = extract_gn_target_files(build_output, self.repo_path)
    is_structural = any(kw in build_output.lower() for kw in (
        "unexpected token",
        "expecting assignment",
        "syntax error",
    ))
    first_file = next(iter(target_files.keys()), "") if target_files else ""
    first_line = target_files.get(first_file) or 1 if first_file else 1
    return [
        GNDiagnostic(
            error_message=error_summary,
            raw_output=build_output,
            target_files=target_files,
            is_structural_break=is_structural,
            file_path=first_file,
            line_number=first_line,
        )
    ]

  # pylint: disable=unused-argument
  def resolve_diagnostic(
      self,
      diagnostic: Any,
      history_records: List[Dict[str, Any]],
      use_expert: bool = False,
      expert_guidance: str = "",
      **kwargs,
  ) -> Tuple[str, str, str]:
    if not isinstance(diagnostic, GNDiagnostic):
      return "", self.model, ""

    # Fast-path: Automatically remove stray conflict marker lines
    for gnf, target_line in diagnostic.target_files.items():
      stray_res = self.check_and_clean_stray_marker(gnf, target_line)
      if stray_res is not None:
        return stray_res

    send_full_file = use_expert or diagnostic.is_structural_break
    file_contexts = []
    primary_target = ""

    for gnf, target_line in list(diagnostic.target_files.items())[:3]:
      if not primary_target:
        primary_target = gnf
      try:
        with open(gnf, "r", encoding="utf-8", errors="replace") as gf:
          file_content = gf.read()
        rel_f = os.path.relpath(gnf, self.repo_path)
        lang = "gn" if gnf.endswith((".gn", ".gni")) else "cpp"
        if (send_full_file or target_line is None or
            len(file_content.splitlines()) <= 800):
          file_contexts.append(
              f"### Full File: {rel_f}\n```{lang}\n{file_content}\n```")
        else:
          lines = file_content.splitlines(keepends=True)
          start_idx = max(0, target_line - 150)
          end_idx = min(len(lines), target_line + 150)
          snippet = "".join(lines[start_idx:end_idx])
          file_contexts.append(
              f"### File: {rel_f} (Lines {start_idx + 1}-{end_idx})\n"
              f"```{lang}\n{snippet}\n```")
      except OSError:
        pass

    history_str, investigation_str = format_history_records(history_records)

    anti_oscillation_note = ""
    if "Source file not found" in diagnostic.raw_output:
      anti_oscillation_note = (
          "\n\nCRITICAL INSTRUCTION: If a target references source files "
          "that do not exist on disk at either relative path, DO NOT toggle or "
          "guess relative paths back and forth.\n"
          "If the tool/test was removed upstream and no matching "
          "source file exists, DELETE the defunct target definition from "
          "BUILD.gn using a <<<<<<< DELETE block.")

    res = self.reasoning_engine.heal_gn_error(
        error_trace=f"{diagnostic.raw_output[:32768]}{anti_oscillation_note}",
        file_context="\n\n".join(file_contexts),
        attempt_history=history_str,
        investigation_history=investigation_str,
        expert_guidance=expert_guidance,
        use_expert=use_expert,
    )
    patch = res.get("patch", "")
    model_used = res.get("model_used", self.model)
    rel_target = (
        os.path.relpath(primary_target, self.repo_path)
        if primary_target else f"{self.platform}_{self.build_type}")
    return patch, model_used, rel_target
