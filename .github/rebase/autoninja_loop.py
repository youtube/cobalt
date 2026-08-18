#!/usr/bin/env python3
# pylint: disable=duplicate-code
"""AI-driven autoninja compiler self-healing feedback loop.

Iteratively invokes autoninja with a clean execution environment,
extracts compiler/linker diagnostics, prompts Gemini via Vertex AI
Reasoning Engine to fix C++, Java, and GN errors, applies patches, and
monitors compilation progress until a clean build is achieved.
"""

import argparse
import dataclasses
import os
import re
import subprocess
import sys
from typing import Dict, List, Optional, Set, Tuple

from reasoning_engine import CobaltReasoningEngine
from rebase_memory import load_past_experience, record_successful_fix


@dataclasses.dataclass
class CompilerDiagnostic:
  """Represents a compiler diagnostic error parsed from ninja build logs."""

  file_path: str
  line_number: int
  column: int
  error_message: str
  raw_snippet: str
  notes: List[str]


def get_clean_build_env(
    depot_tools_path: Optional[str] = None,) -> Dict[str, str]:
  """Prepares a clean build environment, stripping agent-specific env vars."""
  depot_tools = depot_tools_path or os.path.expanduser("~/depot_tools")
  blocked_prefixes = (
      "ANTIGRAVITY_",
      "AI_AGENT",
      "JETSKI_",
      "GEMINI_AGENT",
      "CLAUDE_",
      "CURSOR_",
  )
  clean_env = {
      k: v
      for k, v in os.environ.items()
      if not any(k.startswith(p) for p in blocked_prefixes) and k != "AI_AGENT"
  }
  if os.path.isdir(depot_tools):
    orig_path = clean_env.get("PATH", "")
    clean_env["PATH"] = f"{depot_tools}:{orig_path}"
  return clean_env


def parse_compiler_errors(build_output: str,
                          repo_path: str) -> List[CompilerDiagnostic]:
  """Parses Clang/GCC compiler diagnostics from ninja stdout/stderr."""
  diagnostics: List[CompilerDiagnostic] = []
  clang_error_pattern = re.compile(
      r"^([a-zA-Z0-9_/\.\-]+):(\d+):(?:(\d+):)?\s+(?:fatal\s+)?error:\s+(.+)$")
  note_pattern = re.compile(
      r"^([a-zA-Z0-9_/\.\-]+):(\d+):(?:(\d+):)?\s+note:\s+(.+)$")

  lines = build_output.splitlines()
  i = 0
  while i < len(lines):
    line = lines[i]
    match = clang_error_pattern.match(line)
    if match:
      raw_path, line_str, col_str, error_msg = match.groups()
      line_no = int(line_str)
      col_no = int(col_str) if col_str else 0

      abs_path = (
          os.path.join(repo_path, raw_path)
          if not os.path.isabs(raw_path) else raw_path)

      snippet_lines = [line]
      notes = []
      i += 1
      while i < len(lines):
        next_line = lines[i]
        if clang_error_pattern.match(next_line):
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

  return diagnostics


def get_source_context(file_path: str,
                       error_line: int,
                       window: int = 40) -> str:
  """Reads source file context around the compiler error line."""
  if not os.path.isfile(file_path):
    return f"[File not found on disk: {file_path}]"

  try:
    with open(file_path, "r", encoding="utf-8", errors="replace") as f:
      lines = f.read().splitlines()

    start_line = max(0, error_line - window - 1)
    end_line = min(len(lines), error_line + window)

    numbered_lines = [
        f"{i+1:4d} | {lines[i]}" for i in range(start_line, end_line)
    ]
    return "\n".join(numbered_lines)
  except OSError as e:
    return f"[Failed to read {file_path}: {e}]"


def apply_search_replace(file_path: str, search_block: str,
                         replace_block: str) -> bool:
  """Applies a single search_block replacement to a target file on disk.

    Performs surgical in-place modification on a single file:
      1. Attempts direct exact matching (`content.replace(search, replace, 1)`).
      2. If direct matching fails, falls back to whitespace-normalized
         line-by-line matching to accommodate indentation variations.

    Args:
        file_path: Absolute or relative path to the file to modify.
        search_block: Exact code chunk to locate in the file.
        replace_block: Replacement code chunk to insert.

    Returns:
        True if replaced successfully; False otherwise.
    """
  if not os.path.isfile(file_path):
    print(
        f"[autoninja_loop] [ERROR] Target file does not exist: {file_path}",
        file=sys.stderr,
    )
    return False

  with open(file_path, "r", encoding="utf-8") as f:
    content = f.read()

  search_clean = search_block.strip()
  replace_clean = replace_block.strip()

  if search_clean in content:
    new_content = content.replace(search_clean, replace_clean, 1)
    with open(file_path, "w", encoding="utf-8") as f:
      f.write(new_content)
    return True

  # Normalize whitespace fallback
  content_lines = content.splitlines()
  search_lines = [line.strip() for line in search_clean.splitlines()]

  if not any(search_lines):
    return False

  for idx, line in enumerate(content_lines):
    if line.strip() == search_lines[0]:
      match = True
      for s_idx, s_line in enumerate(search_lines):
        if (idx + s_idx >= len(content_lines) or
            content_lines[idx + s_idx].strip() != s_line):
          match = False
          break
      if match:
        matched_start = idx
        matched_end = idx + len(search_lines)
        indent = re.match(r"^(\s*)", content_lines[matched_start]).group(1)

        formatted_replacements = [
            f"{indent}{r_line}" if r_line else ""
            for r_line in replace_clean.splitlines()
        ]

        new_lines = (
            content_lines[:matched_start] + formatted_replacements +
            content_lines[matched_end:])
        with open(file_path, "w", encoding="utf-8") as f:
          f.write("\n".join(new_lines) + "\n")
        return True

  return False


def apply_patch_or_replacement(patch_text: str, repo_path: str) -> List[str]:
  """Parses and dispatches AI model output containing one or more code edits.

    Acts as the top-level parser and dispatcher for raw LLM patch responses:
      1. Multi-File SEARCH/REPLACE: Extracts every `FILE: <path>`,
         `<<<<<<< SEARCH`, `=======`, `>>>>>>> REPLACE` block from the LLM text.
         Normalizes paths (e.g. stripping GN '//' prefixes) and calls
         `apply_search_replace` for each individual file edit.
      2. Unified Diff Fallback: If the response is a standard unified diff
         (`--- a/... +++ b/... @@ ... @@`), routes to `apply_unified_diff`.

    Args:
        patch_text: Raw multi-line output returned by Gemini.
        repo_path: Root directory of repository for relative path resolution.

    Returns:
        List of modified file paths if successful, or empty list on failure.
    """
  if "<<<<<<< SEARCH" in patch_text and "=======" in patch_text:
    sr_pattern = re.compile(
        r"FILE:\s*([a-zA-Z0-9_/\.\-]+)\s*\n"
        r"<<<<<<<\s*SEARCH\n(.*?)\n=======\n(.*?)\n>>>>>>>\s*REPLACE",
        re.DOTALL,
    )
    matches = sr_pattern.findall(patch_text)
    if matches:
      modified_files = []
      for rel_file, search_b, replace_b in matches:
        clean_rel = rel_file.strip()
        if clean_rel.startswith("//"):
          clean_rel = clean_rel[2:]
        target_file = (
            os.path.join(repo_path, clean_rel)
            if not os.path.isabs(clean_rel) else clean_rel)
        applied = apply_search_replace(target_file, search_b, replace_b)
        if applied:
          modified_files.append(target_file)
        else:
          return []
      return modified_files

  return apply_unified_diff(patch_text, repo_path)


def apply_unified_diff(diff_text: str, repo_path: str) -> List[str]:
  """Applies a unified diff patch to source files, returning modified paths."""
  file_match = re.search(
      r"^(?:---|\+\+\+)\s+[ab]?/?([a-zA-Z0-9_/\.\-]+)",
      diff_text,
      re.MULTILINE,
  )
  if not file_match:
    return []

  rel_file = file_match.group(1).strip()
  if rel_file.startswith("//"):
    rel_file = rel_file[2:]
  file_path = (
      os.path.join(repo_path, rel_file)
      if not os.path.isabs(rel_file) else rel_file)

  if not os.path.isfile(file_path):
    return []

  with open(file_path, "r", encoding="utf-8") as f:
    orig_lines = f.readlines()

  hunk_pattern = re.compile(
      r"^@@\s+-(\d+)(?:,(\d+))?\s+\+(\d+)(?:,(\d+))?\s+@@")
  lines = diff_text.splitlines()
  new_lines = list(orig_lines)
  offset = 0

  try:
    i = 0
    while i < len(lines):
      line = lines[i]
      hm = hunk_pattern.match(line)
      if hm:
        orig_start = int(hm.group(1)) - 1
        i += 1
        hunk_src = []
        hunk_dst = []
        while i < len(lines) and not lines[i].startswith("@@"):
          h_line = lines[i]
          if h_line.startswith("-"):
            hunk_src.append(h_line[1:] + "\n")
          elif h_line.startswith("+"):
            hunk_dst.append(h_line[1:] + "\n")
          elif h_line.startswith(" "):
            hunk_src.append(h_line[1:] + "\n")
            hunk_dst.append(h_line[1:] + "\n")
          i += 1

        pos = orig_start + offset
        if 0 <= pos <= len(new_lines):
          current_slice = new_lines[pos:pos + len(hunk_src)]
          if current_slice != hunk_src:
            return []
          new_lines[pos:pos + len(hunk_src)] = hunk_dst
          offset += len(hunk_dst) - len(hunk_src)
        else:
          return []
      else:
        i += 1

    with open(file_path, "w", encoding="utf-8") as f:
      f.writelines(new_lines)
    return [file_path]
  except (OSError, ValueError, IndexError):
    return []


def run_autoninja_build(
    out_dir: str,
    target: str,
    cobalt_root: str,
    depot_tools_path: Optional[str] = None,
    timeout_seconds: int = 600,
) -> Tuple[bool, str, str]:
  """Runs autoninja build command with a clean environment."""
  clean_env = get_clean_build_env(depot_tools_path)
  cmd = ["autoninja", "-k", "0", "-C", f"out/{out_dir}", target]
  src_dir = (
      os.path.join(cobalt_root, "src") if os.path.isdir(
          os.path.join(cobalt_root, "src")) else cobalt_root)

  cmd_str = " ".join(cmd)
  print(
      f"\n[autoninja_loop] Executing: {cmd_str} in {src_dir}",
      file=sys.stderr,
  )
  try:
    res = subprocess.run(
        cmd,
        cwd=src_dir,
        capture_output=True,
        text=True,
        timeout=timeout_seconds,
        env=clean_env,
        check=False,
    )
    return res.returncode == 0, res.stdout, res.stderr
  except subprocess.TimeoutExpired as e:
    return False, "", f"Build timed out after {timeout_seconds}s: {e}"
  except (OSError, subprocess.SubprocessError) as e:
    return False, "", f"Build execution failed: {e}"


def run_compiler_self_healing_loop(
    out_dir: str,
    target: str,
    cobalt_root: str,
    *,
    max_iterations: int = 60,
    project_id: Optional[str] = None,
    location: str = "us-central1",
    model: str = "gemini-2.5-flash",
    skills_dir: Optional[str] = None,
) -> bool:
  """Executes the autoninja build and iterative self-healing repair loop."""
  src_dir = (
      os.path.join(cobalt_root, "src") if os.path.isdir(
          os.path.join(cobalt_root, "src")) else cobalt_root)
  reasoning_engine = CobaltReasoningEngine(
      project_id=project_id,
      location=location,
      flash_model=model,
      skills_dir=skills_dir,
  )

  print("=" * 80, file=sys.stderr)
  print(
      f"[START] AUTONINJA COMPILER FEEDBACK LOOP "
      f"(Target: {target}, Out: out/{out_dir})",
      file=sys.stderr,
  )
  print(f"  - Max Iterations: {max_iterations}", file=sys.stderr)
  print(f"  - Vertex Model:   {model}", file=sys.stderr)
  print("=" * 80, file=sys.stderr)

  seen_errors: Set[str] = set()
  history_records: List[Dict] = []

  for iteration in range(1, max_iterations + 1):
    print(
        f"\n[autoninja_loop] >>> Build Iteration "
        f"{iteration}/{max_iterations}...",
        file=sys.stderr,
    )

    success, stdout_txt, stderr_txt = run_autoninja_build(
        out_dir=out_dir,
        target=target,
        cobalt_root=cobalt_root,
    )

    if success:
      print("\n" + "=" * 80, file=sys.stderr)
      print(
          f"[SUCCESS] CLEAN BUILD ACHIEVED on Iteration {iteration}!",
          file=sys.stderr,
      )
      print("=" * 80, file=sys.stderr)
      return True

    build_log = f"{stdout_txt}\n{stderr_txt}"
    diagnostics = parse_compiler_errors(build_log, repo_path=src_dir)

    if not diagnostics:
      print(
          "[autoninja_loop] [WARNING] Build failed but no "
          "Clang/GCC errors parsed from logs.",
          file=sys.stderr,
      )
      print("--- Build Log Tail (last 30 lines) ---", file=sys.stderr)
      print("\n".join(build_log.splitlines()[-30:]), file=sys.stderr)
      return False

    print(
        f"[autoninja_loop] Detected {len(diagnostics)} compiler error(s).",
        file=sys.stderr,
    )
    first_diag = diagnostics[0]
    rel_offending = os.path.relpath(first_diag.file_path, src_dir)
    print(
        f"  - Offending File: {rel_offending}:{first_diag.line_number}",
        file=sys.stderr,
    )
    print(f"  - Error:          {first_diag.error_message}", file=sys.stderr)

    err_key = (f"{first_diag.file_path}:{first_diag.line_number}:"
               f"{first_diag.error_message}")
    escalate_pro = err_key in seen_errors
    seen_errors.add(err_key)

    if escalate_pro:
      print(
          "  - [PRO_REASONING] Repeated error detected. Escalating "
          "to Pro model...",
          file=sys.stderr,
      )

    source_excerpt = get_source_context(
        first_diag.file_path, first_diag.line_number, window=40)
    past_exp = load_past_experience()

    rel_target = os.path.relpath(first_diag.file_path, src_dir)
    source_context_payload = (
        f"### Target File: {rel_target}\n```\n{source_excerpt}\n```\n")

    resp = reasoning_engine.heal_compiler_error(
        target=target,
        diagnostics=first_diag.raw_snippet,
        source_contexts=source_context_payload,
        past_experience=past_exp,
        use_pro=escalate_pro,
    )

    patch = resp.get("patch", "")
    if not patch:
      print(
          "[autoninja_loop] [FAIL] AI returned empty patch. "
          "Aborting iteration.",
          file=sys.stderr,
      )
      continue

    model_used = resp.get("model_used", "")
    print(
        f"[autoninja_loop] Applying AI patch using {model_used}...",
        file=sys.stderr,
    )
    applied = apply_patch_or_replacement(patch, repo_path=src_dir)

    if applied:
      print(
          f"[autoninja_loop] [OK] Patch applied cleanly to {rel_target}.",
          file=sys.stderr,
      )
      record_successful_fix(
          category="Compiler",
          target_file=rel_target,
          error_signature=first_diag.error_message,
          fix_summary=f"Resolved on iteration {iteration}/{max_iterations}",
          solution_snippet=patch,
      )
      history_records.append({
          "iteration": iteration,
          "file": rel_target,
          "error": first_diag.error_message,
          "status": "APPLIED",
      })
    else:
      print(
          f"[autoninja_loop] [FAIL] Could not apply patch to "
          f"{rel_target}.",
          file=sys.stderr,
      )

  print(
      f"[FAIL] Exhausted maximum build iterations ({max_iterations}).",
      file=sys.stderr,
  )
  return False


def main():
  """Main CLI entry point for autoninja self-healing loop."""
  parser = argparse.ArgumentParser(
      description="Autoninja compiler self-healing feedback loop.")
  parser.add_argument(
      "--out-dir",
      default="android-arm_devel",
      help="Out directory name (e.g. android-arm_devel, linux-x64x11_devel)",
  )
  parser.add_argument(
      "--target",
      default="cobalt_apk",
      help="Target executable / APK (default: cobalt_apk)",
  )
  parser.add_argument(
      "--cobalt-root",
      default=os.path.expanduser("~/cobalt"),
      help="Path to Cobalt workspace root (default: ~/cobalt)",
  )
  parser.add_argument(
      "--max-iterations",
      type=int,
      default=60,
      help="Max autoninja build iterations (default: 60)",
  )
  parser.add_argument(
      "--project-id",
      default=os.environ.get("GCP_PROJECT") or
      os.environ.get("GOOGLE_CLOUD_PROJECT"),
      help="GCP Project ID for Vertex AI Reasoning Engine.",
  )
  parser.add_argument(
      "--location",
      default=os.environ.get("GCP_LOCATION", "us-central1"),
      help="Vertex AI Region (default: us-central1)",
  )
  parser.add_argument(
      "--model",
      default=os.environ.get("GEMINI_MODEL", "gemini-2.5-flash"),
      help="Gemini model name (default: gemini-2.5-flash)",
  )
  parser.add_argument(
      "--skills-dir",
      default=os.path.expanduser(
          "~/cobalt/src/.github/rebase/reasoning_engine/skills"),
      help="Path to skills directory",
  )
  args = parser.parse_args()

  success = run_compiler_self_healing_loop(
      out_dir=args.out_dir,
      target=args.target,
      cobalt_root=args.cobalt_root,
      max_iterations=args.max_iterations,
      project_id=args.project_id,
      location=args.location,
      model=args.model,
      skills_dir=args.skills_dir,
  )
  sys.exit(0 if success else 1)


if __name__ == "__main__":
  main()
