#!/usr/bin/env python3
"""AI-driven gclient sync and toolchain self-healing resolver library.

Provides GClientSyncResolver, which executes `gclient sync -D`, automatically
recovers with `--force --reset` on dirty submodule state, extracts diagnostic
traces, prompts Gemini via Vertex AI Reasoning Engine to heal DEPS syntax
and revision errors, and validates Python AST before resuming sync.
"""

import dataclasses
import os
import re
import subprocess
import sys
from typing import Any, Callable, Dict, List, Optional, Tuple
import warnings

from base_resolver import (
    BaseResolver,
    get_clean_build_env,
    resolve_repo_file_path,
)
# Suppress google.auth UserWarning about ADC quota project on Cloudtop
warnings.filterwarnings("ignore", category=UserWarning, module="google.auth")

# Keywords that mark the beginning of an error or traceback in gclient sync
# output. Covers standard Python exception names (since gclient and DEPS are
# evaluated in Python) as well as tool-level error phrases.
SYNC_ERROR_KEYWORDS = (
    "error:",
    "traceback",
    "failed to",
    "syntaxerror",
    "syntax error",
    "keyerror",
    "key error",
    "attributeerror",
    "attribute error",
    "exception",
    "cannot find",
    "conflict",
    "fatal:",
)

# Default robust flags for CI and local automated sync
DEFAULT_SYNC_FLAGS = [
    "-D",
    "--no-history",
    "--shallow",
    "--delete_unversioned_trees",
]


@dataclasses.dataclass
class GClientSyncDiagnostic:
  """Represents a gclient sync error diagnostic."""

  error_message: str
  raw_output: str
  diagnostic_trace: str


def extract_sync_diagnostic_trace(output: str) -> str:
  """Extracts relevant error and traceback lines from gclient sync output."""
  lines = output.splitlines()
  error_lines = []
  capture = False
  for line in lines:
    if any(kw in line.lower() for kw in SYNC_ERROR_KEYWORDS):
      capture = True
    if capture:
      error_lines.append(line)
  if error_lines:
    if len(error_lines) > 40:
      return "\n".join(error_lines[:15] +
                       ["... [truncated intermediate stack frames] ..."] +
                       error_lines[-25:])
    return "\n".join(error_lines)
  return "\n".join(lines[-25:] if len(lines) > 25 else lines)


class GClientSyncResolver(BaseResolver):
  """Self-healing resolver for gclient sync and DEPS dependency failures."""

  def __init__(
      self,
      repo_path: str,
      *,
      engine: Optional[Any] = None,
      flags: Optional[List[str]] = None,
      max_iterations: int = 10,
      on_patch_applied_fn: Optional[Callable[[List[str]], None]] = None,
  ):
    super().__init__(
        repo_path=repo_path,
        engine=engine,
        max_iterations=max_iterations,
        on_patch_applied_fn=on_patch_applied_fn,
    )
    self.flags = flags if flags is not None else list(DEFAULT_SYNC_FLAGS)

  @property
  def name(self) -> str:
    return "Phase 2 (Toolchain & Dependency Sync)"

  def _ensure_siso_configured(self, env: Dict[str, str]) -> None:
    """Ensures build/config/siso/.sisoenv exists and is configured for Siso."""
    sisoenv_path = os.path.join(self.repo_path, "build", "config", "siso",
                                ".sisoenv")
    if os.path.exists(sisoenv_path):
      return
    rbe_inst = os.environ.get(
        "RBE_instance",
        "projects/cobalt-actions-prod/instances/default_instance")
    cfg_script = os.path.join(self.repo_path, "build", "config", "siso",
                              "configure_siso.py")
    if os.path.exists(cfg_script):
      print(
          f"[{self.name}] Configuring Siso environment via "
          "configure_siso.py...",
          file=sys.stderr,
      )
      subprocess.run(
          [sys.executable, cfg_script, f"--rbe_instance={rbe_inst}"],
          cwd=self.repo_path,
          env=env,
          capture_output=True,
          check=False,
      )

  def run_command(self, iteration: int) -> Tuple[bool, str, str]:
    del iteration  # Unused in standard sync command execution
    clean_env = get_clean_build_env()
    cmd = ["gclient", "sync"] + self.flags
    cmd_str = " ".join(cmd)
    print(
        f"\n[gclient_sync] Executing: {cmd_str} in {self.repo_path}",
        file=sys.stderr,
    )
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
      if proc.returncode == 0:
        self._ensure_siso_configured(clean_env)
        return True, combined_output, ""

      # Auto-recover with --force --reset
      print(
          f"[WARNING] gclient sync returned {proc.returncode}. "
          "Retrying with --force --reset...",
          file=sys.stderr,
      )
      retry_cmd = ["gclient", "sync"] + self.flags + ["--force", "--reset"]
      proc_retry = subprocess.run(
          retry_cmd,
          cwd=self.repo_path,
          capture_output=True,
          text=True,
          env=clean_env,
          check=False,
      )
      retry_output = f"{proc_retry.stdout}\n{proc_retry.stderr}"
      if proc_retry.returncode != 0:
        print(
            f"[ERROR] gclient sync failed with exit code "
            f"{proc_retry.returncode}:\n{retry_output.strip()}",
            file=sys.stderr,
        )
      else:
        self._ensure_siso_configured(clean_env)
      return proc_retry.returncode == 0, retry_output, ""
    except Exception as e:  # pylint: disable=broad-exception-caught
      return False, f"Subprocess execution failed: {e}", ""

  def extract_diagnostics(self, build_output: str,
                          siso_output: str) -> List[Any]:
    del siso_output  # Unused in gclient sync
    diag_trace = extract_sync_diagnostic_trace(build_output)
    non_empty = [l.strip() for l in diag_trace.splitlines() if l.strip()]
    if non_empty:
      if "traceback" in non_empty[0].lower() and len(non_empty) > 1:
        first_line = non_empty[-1]
      else:
        first_line = non_empty[0]
    else:
      first_line = "Sync Error"
    return [
        GClientSyncDiagnostic(
            error_message=first_line,
            raw_output=build_output,
            diagnostic_trace=diag_trace,
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
    del history_records  # Unused for single DEPS resolution
    if not isinstance(diagnostic, GClientSyncDiagnostic):
      return "", self.model, "DEPS"

    # Dynamically extract target DEPS file path from diagnostic trace
    deps_path = os.path.join(self.repo_path, "DEPS")
    rel_deps = "DEPS"
    file_match = re.search(
        r"(?:file\s+['\"]?|in\s+['\"]?)([^'\"\n\r]+DEPS[^'\"\n\r]*)",
        diagnostic.diagnostic_trace,
        re.IGNORECASE,
    )
    if file_match:
      cand_raw = file_match.group(1).strip().rstrip(":,")
      cand_abs = resolve_repo_file_path(cand_raw, self.repo_path)
      if os.path.isfile(cand_abs):
        deps_path = cand_abs
        rel_deps = os.path.relpath(cand_abs, self.repo_path)

    if not os.path.isfile(deps_path):
      return "", self.model, rel_deps

    try:
      with open(deps_path, "r", encoding="utf-8", errors="replace") as f:
        lines = f.readlines()
        current_deps = "".join(lines)
    except OSError:
      return "", self.model, rel_deps

    line_match = re.search(r"(?:line\s+|:)(\d+)", diagnostic.diagnostic_trace)
    if line_match and len(lines) > 250:
      error_line = int(line_match.group(1))
      s_line = max(1, error_line - 50)
      e_line = min(len(lines), error_line + 50)
      context_snippet = "".join(lines[s_line - 1:e_line])
    else:
      context_snippet = current_deps if len(lines) <= 250 else "".join(
          lines[:250])

    instruction = (
        f"gclient sync failed on {rel_deps} with the following diagnostic:\n"
        f"--------------------------------------------------\n"
        f"{diagnostic.diagnostic_trace}\n"
        f"--------------------------------------------------\n\n"
        f"Resolve the syntax error or duplicate key in {rel_deps}.\n"
        f"Output the standard SEARCH / REPLACE block:\n"
        f"FILE: {rel_deps}\n"
        f"<<<<<<< SEARCH\n"
        f"<exact lines from {rel_deps} to replace>\n"
        f"=======\n"
        f"<fixed replacement lines>\n"
        f">>>>>>> REPLACE\n")

    res = self.reasoning_engine.resolve_conflict(
        file_path=rel_deps,
        language="Python",
        raw_conflict=context_snippet,
        instruction=instruction,
        expert_guidance=expert_guidance,
        use_expert=use_expert,
    )
    if isinstance(res, dict):
      raw_patch = res.get("patch", "") or res.get("replacement", "")
      model_used = res.get("model_used", self.model)
    else:
      raw_patch = str(res)
      model_used = self.model

    return raw_patch, model_used, rel_deps
