#!/usr/bin/env python3
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
    AgentChangeRecord,
    BaseResolver,
    get_clean_build_env,
    is_unmodified_third_party,
    resolve_repo_file_path,
)
# Suppress google.auth UserWarning about ADC quota project on Cloudtop
warnings.filterwarnings("ignore", category=UserWarning, module="google.auth")

SOURCE_CODE_EXTENSIONS = (
    ".cc",
    ".cpp",
    ".c",
    ".h",
    ".hpp",
    ".java",
    ".kt",
    ".mm",
    ".m",
)

BUILD_FILE_EXTENSIONS = (
    ".gn",
    ".gni",
    ".star",
    ".starlark",
)

TEXT_FILE_EXTENSIONS = SOURCE_CODE_EXTENSIONS + BUILD_FILE_EXTENSIONS + (
    ".py",
    ".js",
    ".ts",
    ".xml",
    ".json",
    ".rst",
    ".md",
    ".txt",
    ".sh",
)

MAX_CONTEXT_FILE_SIZE_BYTES = 5 * 1024 * 1024  # 5 MB


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


def find_build_file_for_object(
    obj_str: str,
    repo_path: str,
) -> Optional[str]:
  """Finds the BUILD.gn responsible for an object path like obj/foo/baz.o."""
  clean = obj_str.strip().lstrip("./")
  if clean.startswith("obj/"):
    clean = clean[4:]
  # Strip archive member syntax: libfreetype.a(autofit.o) -> libfreetype.a
  clean = re.sub(r"\(.*?\)", "", clean)
  dir_cand = os.path.dirname(clean)
  while dir_cand and dir_cand != ".":
    gn_cand = os.path.join(repo_path, dir_cand, "BUILD.gn")
    if os.path.isfile(gn_cand):
      return gn_cand
    dir_cand = os.path.dirname(dir_cand)
  return None


def find_action_target_in_gn(
    gn_file: str,
    action_name: str,
) -> int:
  """Returns 1-based line number of action target in BUILD.gn."""
  if not os.path.isfile(gn_file):
    return 1
  try:
    with open(gn_file, "r", encoding="utf-8", errors="replace") as f:
      pattern = re.compile(r"(?:action|compiled_action|action_foreach|"
                           r"compiled_action_foreach)\s*\(\s*[\"']" +
                           re.escape(action_name) + r"[\"']")
      for idx, line in enumerate(f):
        if pattern.search(line):
          return idx + 1
  except OSError:
    pass
  return 1


def parse_compiler_errors(build_output: str,
                          repo_path: str) -> List[CompilerDiagnostic]:
  """Parses compiler/linker/action diagnostics with universal catch-all."""
  diagnostics: List[CompilerDiagnostic] = []
  clean_output = re.sub(r"\x1b\[[0-9;]*m", "", build_output)
  lines = clean_output.splitlines()

  # Pre-scan for Siso/Ninja action failures, which is ahead to the
  # compile errors.
  #e.g.: FAILED: ... ACTION //third_party/blink/...:character_data(...)
  action_fail_pattern = re.compile(
      r"FAILED:.*?\s+ACTION\s+//([a-zA-Z0-9_/\.\-]+):([a-zA-Z0-9_]+)")
  failing_action_note = None
  failing_action_gn = None
  failing_action_line = 1
  for line in lines:
    m_act = action_fail_pattern.search(line.strip())
    if m_act:
      target_dir, target_name = m_act.group(1), m_act.group(2)
      gn_cand = os.path.join(repo_path, target_dir, "BUILD.gn")
      failing_action_line = find_action_target_in_gn(gn_cand, target_name)
      failing_action_gn = gn_cand
      failing_action_note = (f"Failing Action: //{target_dir}:{target_name} "
                             f"({target_dir}/BUILD.gn:{failing_action_line})")
      break

  # 1. Fast Path: Standard Clang / GCC error format
  standard_error_pattern = re.compile(
      r"^([a-zA-Z0-9_/\.\-]+\.[a-zA-Z0-9_]+):(\d+):(?:(\d+):)?\s*"
      r"(?:fatal\s+)?error:\s*(.+)$")
  clang_error_pattern = re.compile(
      r"^\s*(?:\d+\.\d+s\s+)?(?:fatal\s+)?"
      r"(?:error|Error|ERROR):\s*(?:@config//|//)?"
      r"([a-zA-Z0-9_/\.\-]+):(\d+):(?:(\d+):)?\s+(.+)$")

  for idx, line in enumerate(lines):
    l_strip = line.strip()
    match = standard_error_pattern.match(l_strip) or clang_error_pattern.match(
        l_strip)
    if match:
      raw_path, line_str, col_str, error_msg = match.groups()
      line_no = int(line_str)
      col_no = int(col_str) if col_str else 0
      abs_path = resolve_repo_file_path(raw_path, repo_path)

      # Grab surrounding compiler lines (up to 30 lines before to capture
      # include stacks, 20 lines after)
      snippet_lines = lines[max(0, idx - 30):min(len(lines), idx + 20)]
      notes = [failing_action_note] if failing_action_note else []
      diagnostics.append(
          CompilerDiagnostic(
              file_path=abs_path,
              line_number=line_no,
              column=col_no,
              error_message=error_msg.strip(),
              raw_snippet="\n".join(snippet_lines),
              notes=notes,
          ))
      break

  # 2. Linker errors (ld.lld / lld-link / gold / ld)
  if not diagnostics:
    lld_pattern = re.compile(
        r"(?:ld\.lld|lld-link|lld|gold|ld):\s+error:\s+(.+)$")
    obj_pattern = re.compile(
        r"obj/([a-zA-Z0-9_/\.\-]+(?:\([a-zA-Z0-9_/\.\-]+\))?)")
    ref_file_pattern = re.compile(
        r"referenced by\s+([a-zA-Z0-9_/\.\-]+\.[a-zA-Z0-9_]+):(\d+)")
    linker_lines = []
    primary_err = ""
    target_build_file = None
    ref_file = None
    ref_line = 1

    for line in lines:
      l_strip = line.strip()
      if m := lld_pattern.search(l_strip):
        if not primary_err:
          primary_err = m.group(1).strip()
        linker_lines.append(l_strip)
        if not target_build_file:
          if obj_m := obj_pattern.search(l_strip):
            target_build_file = find_build_file_for_object(
                obj_m.group(0), repo_path)
      elif primary_err and (l_strip.startswith(">>>") or
                            "referenced by" in l_strip):
        linker_lines.append(l_strip)
        if not target_build_file:
          if obj_m := obj_pattern.search(l_strip):
            target_build_file = find_build_file_for_object(
                obj_m.group(0), repo_path)
        if not ref_file:
          if ref_m := ref_file_pattern.search(l_strip):
            cand_ref = resolve_repo_file_path(ref_m.group(1), repo_path)
            if os.path.isfile(cand_ref):
              ref_file = cand_ref
              ref_line = int(ref_m.group(2))

    if primary_err:
      target_f = target_build_file or ref_file or os.path.join(
          repo_path, "BUILD.gn")
      diagnostics.append(
          CompilerDiagnostic(
              file_path=target_f,
              line_number=ref_line if target_f == ref_file else 1,
              column=1,
              error_message=f"Linker error: {primary_err}",
              raw_snippet="\n".join(linker_lines[:30]),
              notes=[failing_action_note] if failing_action_note else [],
          ))

  # 3. Chromium base logging FATAL / CHECK / ERROR assertions
  # e.g.: [0910/211026.345187:FATAL:path/to/file.cc:48] Check failed: ...
  if not diagnostics:
    fatal_log_pattern = re.compile(
        r"^\s*(?:\[\d+/\d+\.\d+:FATAL:([a-zA-Z0-9_/\.\-]+):(\d+)\]|"
        r"\[FATAL:([a-zA-Z0-9_/\.\-]+):(\d+)\])\s*(.+)$")
    for idx, line in enumerate(lines):
      l_strip = line.strip()
      m_fatal = fatal_log_pattern.match(l_strip)
      if m_fatal:
        f1, l1, f2, l2, err_msg = m_fatal.groups()
        raw_path = f1 or f2
        line_no = int(l1 or l2)
        abs_path = resolve_repo_file_path(raw_path, repo_path)
        snippet_lines = lines[max(0, idx - 20):min(len(lines), idx + 20)]
        notes = [failing_action_note] if failing_action_note else []
        diagnostics.append(
            CompilerDiagnostic(
                file_path=abs_path,
                line_number=line_no,
                column=1,
                error_message=err_msg.strip(),
                raw_snippet="\n".join(snippet_lines),
                notes=notes,
            ))
        break

  # 4. Python traceback in action scripts
  if not diagnostics:
    py_tb_pattern = re.compile(
        r'^\s*File "([a-zA-Z0-9_/\.\-]+\.py)", line (\d+)(?:, in (.+))?')
    py_err_pattern = re.compile(r"^([a-zA-Z0-9_.]+(?:Error|Exception):\s*.+)$")
    matched_py_file = None
    matched_py_line = 1
    matched_py_err = ""
    py_idx = -1
    for idx, line in enumerate(lines):
      l_strip = line.strip()
      if m_tb := py_tb_pattern.match(l_strip):
        cand_f = resolve_repo_file_path(m_tb.group(1), repo_path)
        if os.path.isfile(cand_f):
          matched_py_file = cand_f
          matched_py_line = int(m_tb.group(2))
          py_idx = idx
      elif m_err := py_err_pattern.match(l_strip):
        if not matched_py_err:
          matched_py_err = m_err.group(1).strip()
    if matched_py_file:
      snippet_lines = lines[max(0, py_idx - 10):min(len(lines), py_idx + 25)]
      notes = [failing_action_note] if failing_action_note else []
      diagnostics.append(
          CompilerDiagnostic(
              file_path=matched_py_file,
              line_number=matched_py_line,
              column=1,
              error_message=matched_py_err or "Python script execution error",
              raw_snippet="\n".join(snippet_lines),
              notes=notes,
          ))

  # 5. GN Action failure without direct source crash -> target BUILD.gn
  if not diagnostics and failing_action_gn:
    cand_snippet = []
    for idx, line in enumerate(lines):
      if action_fail_pattern.search(line.strip()):
        cand_snippet = lines[max(0, idx - 5):min(len(lines), idx + 35)]
        break
    diagnostics.append(
        CompilerDiagnostic(
            file_path=failing_action_gn,
            line_number=failing_action_line,
            column=1,
            error_message=failing_action_note or "Action execution failed",
            raw_snippet=("\n".join(cand_snippet)
                         if cand_snippet else "\n".join(lines[:30])),
            notes=[],
        ))

  return diagnostics


def read_siso_output_snippet(siso_out_path: str, max_bytes: int = 65536) -> str:
  """Safely reads the top failed action traces from siso_output or siso.log."""
  candidate_paths = [
      siso_out_path,
      siso_out_path.replace("siso_output", ".siso_output"),
      siso_out_path.replace("siso_output", "siso.log"),
      siso_out_path.replace("siso_output", ".siso.log"),
  ]
  for p in candidate_paths:
    if os.path.isfile(p):
      try:
        with open(p, "r", encoding="utf-8", errors="replace") as sf:
          content = sf.read(max_bytes)
          if content.strip():
            return content
      except OSError:
        pass
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
      engine: Optional[Any] = None,
      max_iterations: int = 60,
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
      combined_output = f"{proc.stdout}\n{proc.stderr}".strip()
      siso_path = os.path.join(self.repo_path, "out", self.out_dir,
                               "siso_output")
      siso_snippet = read_siso_output_snippet(siso_path)

      if proc.returncode != 0:
        print(
            f"\n[autoninja FAIL (exit code {proc.returncode})]",
            file=sys.stderr,
        )
        if proc.stdout and proc.stdout.strip():
          print(
              f"--- stdout ---\n{proc.stdout.strip()[:10000]}", file=sys.stderr)
        if proc.stderr and proc.stderr.strip():
          print(
              f"--- stderr ---\n{proc.stderr.strip()[:10000]}", file=sys.stderr)
        if siso_snippet and siso_snippet.strip():
          print(
              f"--- siso_output ---\n{siso_snippet.strip()[:10000]}",
              file=sys.stderr)

      return proc.returncode == 0, combined_output, siso_snippet
    except Exception as e:  # pylint: disable=broad-exception-caught
      err_msg = f"Subprocess execution failed: {e}"
      print(f"[autoninja ERROR] {err_msg}", file=sys.stderr)
      return False, err_msg, ""

  def extract_diagnostics(self, build_output: str,
                          siso_output: str) -> List[Any]:
    diags: List[Any] = []
    if siso_output and siso_output.strip():
      diags = parse_compiler_errors(siso_output, self.repo_path)
    if not diags and build_output and build_output.strip():
      diags = parse_compiler_errors(build_output, self.repo_path)
    if not diags:
      raw_chunks = []
      if siso_output and siso_output.strip():
        raw_chunks.append(f"Siso output:\n{siso_output.strip()[:8192]}")
      if build_output and build_output.strip():
        meaningful_lines = [
            l for l in build_output.splitlines()
            if not l.startswith("ninja: Entering directory")
        ]
        if meaningful_lines:
          raw_chunks.append("Build stdout/stderr:\n" +
                            "\n".join(meaningful_lines[:100]))
      if raw_chunks:
        diags = ["\n\n".join(raw_chunks)]
    return diags

  # pylint: disable=unused-argument
  def resolve_diagnostic(
      self,
      diagnostic: Any,
      history_records: List[Dict[str, Any]],
      use_expert: bool = False,
      expert_guidance: str = "",
      **kwargs,
  ) -> Tuple[str, str, str]:
    if isinstance(diagnostic, str):
      error_trace = diagnostic[:32768]
      history_items = []
      for h in history_records[-5:]:
        it = h.get("iteration", "")
        hf = h.get("file", "")
        he = h.get("error", "")
        history_items.append(f"- Iteration {it}: Modified {hf} to fix \"{he}\"")
      history_str = "\n".join(history_items)

      # Attempt to deduce a candidate build file from action references
      target_cand = ""
      m_act = re.search(r"ACTION\s+//([a-zA-Z0-9_/\.\-]+):([a-zA-Z0-9_]+)",
                        error_trace)
      if m_act:
        cand_gn = os.path.join(m_act.group(1), "BUILD.gn")
        if os.path.isfile(os.path.join(self.repo_path, cand_gn)):
          target_cand = cand_gn

      res = self.reasoning_engine.heal_compiler_error(
          error_trace=error_trace,
          file_context="",
          target_file=target_cand,
          history=history_str,
          expert_guidance=expert_guidance,
          use_expert=use_expert,
      )
      patch = res.get("patch", "")
      model_used = res.get("model_used", self.model)
      return patch, model_used, target_cand or self.target

    if not isinstance(diagnostic, CompilerDiagnostic):
      return "", self.model, ""

    # Fast-path: Automatically remove stray conflict marker lines
    stray_res = self.check_and_clean_stray_marker(diagnostic.file_path,
                                                  diagnostic.line_number)
    if stray_res is not None:
      return stray_res

    file_context = ""
    is_text_file = diagnostic.file_path.endswith(TEXT_FILE_EXTENSIONS)

    if is_text_file and os.path.isfile(diagnostic.file_path):
      try:
        # Protect against opening abnormally huge files into memory
        if os.path.getsize(diagnostic.file_path) <= MAX_CONTEXT_FILE_SIZE_BYTES:
          with open(
              diagnostic.file_path,
              "r",
              encoding="utf-8",
              errors="replace",
          ) as f:
            lines = f.readlines()
          is_build_file = diagnostic.file_path.endswith(BUILD_FILE_EXTENSIONS)
          # Send full context for small build files or repeated errors (>= 3)
          send_full_file = (len(lines) <= 300 and is_build_file) or (
              len(lines) <= 300 and
              self.file_error_counts.get(diagnostic.file_path, 0) >= 3)
          if send_full_file:
            rel_path = os.path.relpath(diagnostic.file_path, self.repo_path)
            label = "build" if is_build_file else "source"
            print(
                f"  [{self.name}] Sending full {label} file context "
                f"({len(lines)} lines) for {rel_path}...",
                file=sys.stderr,
            )
            file_context = "".join(f"{i + 1}: {l}" for i, l in enumerate(lines))
          else:
            s_line = max(1, diagnostic.line_number - 35)
            e_line = min(len(lines), diagnostic.line_number + 35)
            file_context = "".join(
                f"{s_line + i}: {l}" for i, l in enumerate(lines[s_line -
                                                                 1:e_line]))
      except OSError:
        pass

    rel_target = os.path.relpath(diagnostic.file_path, self.repo_path)
    is_third_party = is_unmodified_third_party(diagnostic.file_path,
                                               self.repo_path)
    guard_msg = ""
    if is_third_party:
      guard_msg = (
          f"\n\nCRITICAL GUARD: '{rel_target}' is an UNMODIFIED THIRD-PARTY "
          "FILE and is strictly READ-ONLY. Do NOT output a patch for this "
          "file.\n"
          "You MUST investigate referencing first-party files or BUILD.gn:\n"
          "- Use `TOOL_READ_FILE: <path> <start>-<end>` to inspect caller "
          "files in the #include stack trace or the target's BUILD.gn.\n"
          "- Output a patch for the referencing first-party file or "
          "BUILD.gn instead.")

    notes_str = "\n".join(diagnostic.notes) if diagnostic.notes else ""
    error_trace = (f"File: {diagnostic.file_path}:{diagnostic.line_number}:"
                   f"{diagnostic.column}\n"
                   f"Error: {diagnostic.error_message}\n"
                   f"Snippet:\n{diagnostic.raw_snippet}\n"
                   f"{notes_str}"
                   f"{guard_msg}")

    history_items = []
    investigation_items = []
    for h in history_records[-6:]:
      it = str(h.get("iteration", ""))
      hf = h.get("file", "")
      he = h.get("error", "")
      if it.startswith("Tool-"):
        investigation_items.append(
            f"Tool Call: `{hf}`\nResult:\n```\n{he}\n```")
      else:
        history_items.append(f"- Iteration {it}: Modified {hf} to fix \"{he}\"")
    history_str = "\n".join(history_items)
    investigation_str = "\n\n".join(investigation_items)

    res = self.reasoning_engine.heal_compiler_error(
        error_trace=error_trace,
        file_context=file_context,
        target_file=diagnostic.file_path,
        history=history_str,
        investigation_history=investigation_str,
        expert_guidance=expert_guidance,
        use_expert=use_expert,
    )
    patch = res.get("patch", "")
    model_used = res.get("model_used", self.model)
    return patch, model_used, rel_target
