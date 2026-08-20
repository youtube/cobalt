#!/usr/bin/env python3
# pylint: disable=duplicate-code
"""AI-driven autoninja compiler self-healing feedback loop library.

Provides AutoninjaResolver, which iteratively builds with autoninja,
parses Clang/GCC/TypeScript/Siso diagnostics, prompts Gemini via Vertex AI
Reasoning Engine, applies patches with third-party guards, and monitors
compilation progress until a clean build is achieved.
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
from reasoning_engine import CobaltReasoningEngine

# Suppress google.auth UserWarning about ADC quota project on Cloudtop
warnings.filterwarnings("ignore", category=UserWarning, module="google.auth")


@dataclasses.dataclass
class CompilerDiagnostic:
  """Represents a compiler diagnostic error parsed from ninja build logs."""

  file_path: str
  line_number: int
  column: int
  error_message: str
  raw_snippet: str
  notes: List[str]


def find_referencing_build_file(
    missing_file: str,
    repo_path: str,
) -> Tuple[Optional[str], int]:
  """Searches repository BUILD.gn files for references to a missing file."""
  clean_name = os.path.basename(missing_file.strip("\"'"))
  try:
    res = subprocess.run(
        ["git", "grep", "-n", "-I", clean_name, "--", "*.gn", "*.gni"],
        cwd=repo_path,
        capture_output=True,
        text=True,
        errors="replace",
        check=False,
    )
    lines = [
        l.strip()
        for l in res.stdout.splitlines()
        if l.strip() and not l.startswith("out/")
    ]
    if lines:
      first_match = lines[0]
      parts = first_match.split(":", 2)
      if len(parts) >= 2:
        rel_path = parts[0]
        line_num = int(parts[1]) if parts[1].isdigit() else 1
        return os.path.join(repo_path, rel_path), line_num
  except (OSError, subprocess.SubprocessError):
    pass
  return None, 1


def parse_compiler_errors(build_output: str,
                          repo_path: str) -> List[CompilerDiagnostic]:
  """Parses Clang/GCC/TypeScript compiler and action diagnostics."""
  diagnostics: List[CompilerDiagnostic] = []
  clang_error_pattern = re.compile(
      r"^\s*(?:\d+\.\d+s\s+)?(?:fatal\s+)?"
      r"(?:error|Error|ERROR):\s*(?:@config//|//)?"
      r"([a-zA-Z0-9_/\.\-]+):(\d+):(?:(\d+):)?\s+(.+)$")
  standard_error_pattern = re.compile(
      r"^([a-zA-Z0-9_/\.\-]+):(\d+):(?:(\d+):)?\s+(?:fatal\s+)?error:\s+(.+)$")
  ts_error_pattern = re.compile(
      r"^([a-zA-Z0-9_/\.\-]+):(\d+):(?:(\d+):)?\s+-\s+error\s+(TS\d+:\s+.+)$")
  note_pattern = re.compile(
      r"^([a-zA-Z0-9_/\.\-]+):(\d+):(?:(\d+):)?\s+note:\s+(.+)$")

  lines = build_output.splitlines()
  i = 0
  while i < len(lines):
    line = re.sub(r"\x1b\[[0-9;]*m", "", lines[i])
    match = (
        standard_error_pattern.match(line) or clang_error_pattern.match(line) or
        ts_error_pattern.match(line))
    if match:
      raw_path, line_str, col_str, error_msg = match.groups()
      line_no = int(line_str)
      col_no = int(col_str) if col_str else 0

      abs_path = resolve_repo_file_path(raw_path, repo_path)

      snippet_lines = [line]
      notes = []
      i += 1
      while i < len(lines):
        next_line = re.sub(r"\x1b\[[0-9;]*m", "", lines[i])
        if (standard_error_pattern.match(next_line) or
            clang_error_pattern.match(next_line) or
            ts_error_pattern.match(next_line)):
          break
        if next_line.startswith("[") and "]" in next_line:
          break
        note_match = note_pattern.match(next_line)
        if note_match:
          notes.append(next_line)
        snippet_lines.append(next_line)
        i += 1

      diagnostics.append(
          CompilerDiagnostic(
              file_path=abs_path,
              line_number=line_no,
              column=col_no,
              error_message=error_msg.strip(),
              raw_snippet="\n".join(snippet_lines[:15]),
              notes=notes,
          ))
    else:
      i += 1

  # Fallback: Parse Ninja missing dependency / missing rule graph errors
  if not diagnostics:
    ninja_missing_pattern = re.compile(
        r"[\"']([^\"']+)[\"'],\s+needed by\s+[\"']([^\"']+)[\"'],"
        r"\s+missing and no known rule to make it")
    for line in lines:
      if m := ninja_missing_pattern.search(line):
        missing_file, _ = m.groups()
        ref_file, ref_line = find_referencing_build_file(
            missing_file, repo_path)
        target_file = ref_file or os.path.join(repo_path, "BUILD.gn")

        clean_rel = missing_file.strip("\"'").lstrip("./")
        abs_missing = (
            os.path.join(repo_path, clean_rel)
            if not os.path.isabs(clean_rel) else clean_rel)
        missing_dir = os.path.dirname(abs_missing)
        diag_notes = [f"Referenced at: {target_file}:{ref_line}"]
        if os.path.isdir(missing_dir):
          existing_files = os.listdir(missing_dir)
          rel_dir = os.path.relpath(missing_dir, repo_path)
          diag_notes.append(
              f"Actual files on disk in '{rel_dir}': {existing_files}")

        diagnostics.append(
            CompilerDiagnostic(
                file_path=target_file,
                line_number=ref_line,
                column=1,
                error_message=(
                    f"Missing build dependency or rule for: {missing_file}"),
                raw_snippet=line.strip(),
                notes=diag_notes,
            ))
        break

  return diagnostics


def read_siso_output_snippet(siso_out_path: str, max_bytes: int = 65536) -> str:
  """Safely reads the top failed action traces from siso_output."""
  if not os.path.isfile(siso_out_path):
    return ""
  try:
    with open(siso_out_path, "r", encoding="utf-8", errors="replace") as sf:
      return sf.read(max_bytes)
  except OSError:
    return ""


class AutoninjaResolver(BaseResolver):
  """Self-healing resolver for autoninja compilation failures."""

  def __init__(
      self,
      repo_path: str,
      out_dir: str,
      target: str,
      *,
      keep_going: int = 1,
      engine: Optional[CobaltReasoningEngine] = None,
      max_iterations: int = 60,
      on_patch_applied_fn: Optional[Callable[[List[str]], None]] = None,
  ):
    super().__init__(
        repo_path=repo_path,
        engine=engine,
        max_iterations=max_iterations,
        on_patch_applied_fn=on_patch_applied_fn,
    )
    self.out_dir = out_dir
    self.target = target
    self.keep_going = keep_going

  @property
  def name(self) -> str:
    return "Phase 4 (autoninja compiler loop)"

  def run_command(self, iteration: int) -> Tuple[bool, str, str]:
    del iteration  # Unused in standard autoninja command execution
    clean_env = get_clean_build_env()
    cmd = [
        "autoninja",
        "-k",
        str(self.keep_going),
        "-C",
        f"out/{self.out_dir}",
        self.target,
    ]
    cmd_str = " ".join(cmd)
    print(
        f"\n[autoninja] Executing: {cmd_str} in {self.repo_path}",
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
      siso_path = os.path.join(self.repo_path, "out", self.out_dir,
                               "siso_output")
      siso_snippet = read_siso_output_snippet(siso_path)
      return proc.returncode == 0, combined_output, siso_snippet
    except Exception as e:  # pylint: disable=broad-exception-caught
      return False, f"Subprocess execution failed: {e}", ""

  def extract_diagnostics(self, build_output: str,
                          siso_output: str) -> List[Any]:
    diags = parse_compiler_errors(build_output, self.repo_path)
    if not diags and siso_output:
      diags = parse_compiler_errors(siso_output, self.repo_path)
    return diags

  def resolve_diagnostic(
      self,
      diagnostic: Any,
      history_records: List[Dict[str, Any]],
      use_pro: bool,
  ) -> Tuple[str, str, str]:
    if not isinstance(diagnostic, CompilerDiagnostic):
      return "", self.model, ""

    file_context = ""
    if os.path.isfile(diagnostic.file_path):
      try:
        with open(
            diagnostic.file_path, "r", encoding="utf-8", errors="replace") as f:
          lines = f.readlines()
        s_line = max(1, diagnostic.line_number - 30)
        e_line = min(len(lines), diagnostic.line_number + 30)
        file_context = "".join(f"{s_line + i}: {l}"
                               for i, l in enumerate(lines[s_line - 1:e_line]))
      except OSError:
        pass

    notes_str = "\n".join(diagnostic.notes) if diagnostic.notes else ""
    error_trace = (f"File: {diagnostic.file_path}:{diagnostic.line_number}:"
                   f"{diagnostic.column}\n"
                   f"Error: {diagnostic.error_message}\n"
                   f"Snippet:\n{diagnostic.raw_snippet}\n"
                   f"{notes_str}")

    history_items = []
    for h in history_records[-5:]:
      it = h.get("iteration", "")
      hf = h.get("file", "")
      he = h.get("error", "")
      history_items.append(f"- Iteration {it}: Modified {hf} to fix \"{he}\"")
    history_str = "\n".join(history_items)

    res = self.reasoning_engine.heal_compiler_error(
        error_trace=error_trace,
        file_context=file_context,
        target_file=diagnostic.file_path,
        history=history_str,
        past_experience=self.past_experience,
        use_pro=use_pro,
    )
    patch = res.get("patch", "")
    model_used = res.get("model_used", self.model)
    rel_target = os.path.relpath(diagnostic.file_path, self.repo_path)
    return patch, model_used, rel_target
