#!/usr/bin/env python3
# pylint: disable=duplicate-code
"""Base abstract class for AI-driven self-healing command resolvers.

Provides the foundational execution loop, multi-turn filesystem tools,
SEARCH/REPLACE patch application, and third-party protection guardrails
shared by all rebase phases (gclient sync, gn gen, and autoninja).
"""

import abc
import os
import re
import subprocess
import sys
from typing import Any, Callable, Dict, List, Optional, Tuple
import warnings

from reasoning_engine import CobaltReasoningEngine
from rebase_memory import load_past_experience, record_failure, record_successful_fix

# Suppress google.auth UserWarning about ADC quota project on Cloudtop
warnings.filterwarnings("ignore", category=UserWarning, module="google.auth")


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


def get_chromium_milestone(repo_path: Optional[str] = None) -> str:
  """Reads the Chromium major milestone from chrome/VERSION (e.g. 'M138')."""
  base = repo_path or os.path.expanduser("~/cobalt/src")
  version_file = os.path.join(base, "chrome", "VERSION")
  if os.path.isfile(version_file):
    try:
      with open(version_file, "r", encoding="utf-8") as f:
        for line in f:
          if line.startswith("MAJOR="):
            major_ver = line.strip().split("=")[1]
            return f"M{major_ver}"
    except OSError:
      pass
  return "M_Unknown"


def resolve_repo_file_path(raw_path: str, repo_path: str) -> str:
  """Resolves command / compiler output paths into an existing file path."""
  clean = raw_path.strip().lstrip("\"'")
  if clean.startswith("//"):
    clean = clean[2:]

  # 1. Direct absolute or relative join
  direct = os.path.join(repo_path, clean) if not os.path.isabs(clean) else clean
  if os.path.isfile(direct):
    return os.path.abspath(direct)

  # 2. Strip leading ../ and ./
  stripped = clean
  while stripped.startswith(("../", "./")):
    stripped = stripped.split("/", 1)[1] if "/" in stripped else ""

  if stripped:
    cand_direct = os.path.join(repo_path, stripped)
    if os.path.isfile(cand_direct):
      return os.path.abspath(cand_direct)

    # 3. Check cobalt/ prefix
    cand_cobalt = os.path.join(repo_path, "cobalt", stripped)
    if os.path.isfile(cand_cobalt):
      return os.path.abspath(cand_cobalt)

  # 4. Siso config fallback
  if "main.star" in clean or clean.endswith(".star"):
    siso_cand = os.path.join(repo_path, "build/config/siso",
                             os.path.basename(clean))
    if os.path.isfile(siso_cand):
      return os.path.abspath(siso_cand)

  # 5. Search by basename as fallback
  fname = os.path.basename(clean)
  if fname:
    try:
      res = subprocess.run(
          ["find", repo_path, "-name", fname, "-not", "-path", "*/.*"],
          capture_output=True,
          text=True,
          check=False,
      )
      matches = [m.strip() for m in res.stdout.splitlines() if m.strip()]
      if matches:
        return os.path.abspath(matches[0])
    except (OSError, subprocess.SubprocessError):
      pass

  return direct


def is_unmodified_third_party(file_path: str, repo_path: str) -> bool:
  """Checks if a file is pure third-party source code without Cobalt changes."""
  rel = os.path.relpath(file_path, repo_path)
  # GN build configs (*.gn, *.gni, *.star) are Chromium/Cobalt recipes
  if rel.endswith((".gn", ".gni", ".star")):
    return False
  if not rel.startswith("third_party/"):
    return False
  # Cobalt/Starboard-specific files hosted under third_party
  if "cobalt" in rel.lower() or "starboard" in rel.lower():
    return False
  try:
    with open(file_path, "r", encoding="utf-8", errors="replace") as f:
      content = f.read()
    if any(
        m in content for m in (
            "BUILDFLAG(IS_COBALT)",
            "BUILDFLAG(USE_STARBOARD_MEDIA)",
            "STARBOARD",
            "Cobalt",
            "Starboard",
            "is_starboard",
            "is_cobalt",
        )):
      return False
  except OSError:
    pass
  return True


def apply_search_replace(file_path: str, search_block: str,
                         replace_block: str) -> bool:
  """Applies a SEARCH/REPLACE block edit to a file."""
  if not os.path.isfile(file_path):
    return False

  with open(file_path, "r", encoding="utf-8", errors="replace") as f:
    content = f.read()

  # 1. Exact match
  if search_block in content:
    new_content = content.replace(search_block, replace_block, 1)
    with open(file_path, "w", encoding="utf-8") as f:
      f.write(new_content)
    return True

  # 2. Whitespace-trimmed match
  s_stripped = search_block.strip()
  if s_stripped and s_stripped in content:
    new_content = content.replace(s_stripped, replace_block.strip(), 1)
    with open(file_path, "w", encoding="utf-8") as f:
      f.write(new_content)
    return True

  # 3. Normalized line-by-line match
  s_lines = [line.strip() for line in search_block.splitlines() if line.strip()]
  if not s_lines:
    return False

  c_lines = content.splitlines()
  for i in range(len(c_lines) - len(s_lines) + 1):
    window = [c_lines[i + j].strip() for j in range(len(s_lines))]
    if window == s_lines:
      new_lines = c_lines[:i] + replace_block.splitlines(
      ) + c_lines[i + len(s_lines):]
      with open(file_path, "w", encoding="utf-8") as f:
        f.write("\n".join(new_lines) + "\n")
      return True

  return False


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
  file_path = resolve_repo_file_path(rel_file, repo_path)

  if not os.path.isfile(file_path):
    return []

  if is_unmodified_third_party(file_path, repo_path):
    print(
        f"  [GUARD] Rejecting unified diff on unmodified third-party file: "
        f"{rel_file}. Patch the referencing BUILD.gn instead.",
        file=sys.stderr,
    )
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


def apply_patch_or_replacement(patch_text: str, repo_path: str) -> List[str]:
  """Parses and dispatches AI patch responses (SEARCH/REPLACE or diffs)."""
  clean_text = patch_text.strip()
  clean_text = re.sub(r"^```[a-zA-Z0-9_-]*\n", "", clean_text)
  clean_text = re.sub(r"\n```$", "", clean_text)

  if "<<<<<<< SEARCH" in clean_text and "=======" in clean_text:
    sr_pattern = re.compile(
        r"(?:FILE|Target File):\s*([a-zA-Z0-9_/\.\-]+)\s*\n"
        r"<<<<<<<\s*SEARCH\n(.*?)\n=======\n(.*?)\n>>>>>>>\s*REPLACE",
        re.DOTALL,
    )
    matches = sr_pattern.findall(clean_text)
    if matches:
      modified_files = []
      for rel_file, search_b, replace_b in matches:
        target_file = resolve_repo_file_path(rel_file, repo_path)
        if is_unmodified_third_party(target_file, repo_path):
          print(
              f"  [GUARD] Rejecting patch on unmodified third-party file: "
              f"{rel_file}. Patch the referencing BUILD.gn instead.",
              file=sys.stderr,
          )
          return []
        applied = apply_search_replace(target_file, search_b, replace_b)
        if applied:
          modified_files.append(target_file)
        else:
          return []
      return modified_files

  return apply_unified_diff(clean_text, repo_path)


def execute_local_tool(cmd: str, repo_path: str) -> str:
  """Executes safe read-only multi-turn inspection tools for Gemini."""
  clean_cmd = cmd.strip()
  if clean_cmd.startswith("TOOL_READ_FILE:"):
    parts = clean_cmd.split(":", 1)[1].strip().split()
    if not parts:
      return "[ERROR] No file path provided."
    target_rel = parts[0]
    line_range = parts[1] if len(parts) > 1 else ""
    target_abs = resolve_repo_file_path(target_rel, repo_path)
    if not os.path.isfile(target_abs):
      return f"[ERROR] File does not exist: {target_rel}"
    try:
      with open(target_abs, "r", encoding="utf-8", errors="replace") as f:
        lines = f.readlines()
      if line_range and "-" in line_range:
        s_str, e_str = line_range.split("-", 1)
        s_line = max(1, int(s_str))
        e_line = min(len(lines), int(e_str))
        selected = lines[s_line - 1:e_line]
        numbered = [f"{s_line + i}: {l}" for i, l in enumerate(selected)]
        return "".join(numbered)
      preview = lines[:100]
      numbered = [f"{i + 1}: {l}" for i, l in enumerate(preview)]
      return "".join(numbered)
    except Exception as e:  # pylint: disable=broad-exception-caught
      return f"[ERROR] Could not read {target_rel}: {e}"

  if clean_cmd.startswith("TOOL_FIND_FILE:"):
    pattern = clean_cmd.split(":", 1)[1].strip()
    try:
      res = subprocess.run(
          ["find", ".", "-name", pattern, "-not", "-path", "*/.*"],
          cwd=repo_path,
          capture_output=True,
          text=True,
          check=False,
      )
      lines = res.stdout.splitlines()[:20]
      return "\n".join(lines) if lines else f"No matches found for: {pattern}"
    except Exception as e:  # pylint: disable=broad-exception-caught
      return f"[ERROR] Find failed: {e}"

  if clean_cmd.startswith("TOOL_GREP:"):
    query = clean_cmd.split(":", 1)[1].strip()
    try:
      res = subprocess.run(
          [
              "git",
              "grep",
              "-n",
              "-I",
              "--max-count=15",
              query,
              "--",
              "*.gn",
              "*.gni",
              "*.h",
              "*.cc",
              "*.java",
          ],
          cwd=repo_path,
          capture_output=True,
          text=True,
          errors="replace",
          check=False,
      )
      return (res.stdout.strip()
              if res.stdout.strip() else f"No matches found for: {query}")
    except Exception as e:  # pylint: disable=broad-exception-caught
      return f"[ERROR] Grep failed: {e}"

  if clean_cmd.startswith("TOOL_GIT_SHOW:"):
    ref = clean_cmd.split(":", 1)[1].strip()
    try:
      res = subprocess.run(
          ["git", "show", ref],
          cwd=repo_path,
          capture_output=True,
          text=True,
          errors="replace",
          check=False,
      )
      return (res.stdout[:3000] if res.stdout else f"Could not show ref: {ref}")
    except Exception as e:  # pylint: disable=broad-exception-caught
      return f"[ERROR] Git show failed: {e}"

  if clean_cmd.startswith("TOOL_GCLIENT_SYNC"):
    try:
      clean_env = get_clean_build_env()
      res = subprocess.run(
          ["gclient", "sync", "-D"],
          cwd=repo_path,
          capture_output=True,
          text=True,
          env=clean_env,
          check=False,
      )
      out = f"{res.stdout}\n{res.stderr}".strip()
      return (out[:4000] if out else
              f"gclient sync completed with exit code {res.returncode}")
    except Exception as e:  # pylint: disable=broad-exception-caught
      return f"[ERROR] gclient sync failed: {e}"

  return f"[ERROR] Unknown tool command: {clean_cmd}"


class BaseResolver(abc.ABC):
  """Abstract base class for all self-healing rebase command execution loops."""

  def __init__(
      self,
      repo_path: str,
      *,
      engine: Optional[CobaltReasoningEngine] = None,
      max_iterations: int = 50,
      on_patch_applied_fn: Optional[Callable[[List[str]], None]] = None,
  ):
    self.repo_path = repo_path
    self.max_iterations = max_iterations
    self.on_patch_applied_fn = on_patch_applied_fn
    self.reasoning_engine = engine or CobaltReasoningEngine()
    self.past_experience = load_past_experience()

  @property
  def model(self) -> str:
    """Active primary model name from reasoning engine."""
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
                          siso_output: str) -> List[Any]:
    """Parses output into a list of diagnostic error objects."""

  @abc.abstractmethod
  def resolve_diagnostic(
      self,
      diagnostic: Any,
      history_records: List[Dict[str, Any]],
      use_pro: bool,
  ) -> Tuple[str, str, str]:
    """Generates a patch. Returns (patch, model_used, target_file)."""

  def on_patch_applied(self, modified_files: List[str]) -> None:
    """Hook called immediately after a patch is applied."""
    if self.on_patch_applied_fn:
      self.on_patch_applied_fn(modified_files)

  def execute_investigation_tools(
      self,
      initial_patch: str,
      diagnostic: Any,
      max_rounds: int = 4,
  ) -> Tuple[str, str]:
    """Runs multi-turn tool loop if model requested a TOOL_ command."""
    current_patch = initial_patch
    model_used = self.model
    investigation_history: List[str] = []

    for round_idx in range(1, max_rounds + 1):
      tool_match = re.search(r"^(TOOL_[A-Z_]+:.*)$", current_patch.strip(),
                             re.MULTILINE)
      if not tool_match:
        break

      tool_cmd = tool_match.group(1).strip()
      print(
          f"  [{self.name}] [Investigation Round {round_idx}/{max_rounds}] "
          f"Model requested: {tool_cmd}",
          file=sys.stderr,
      )
      tool_output = execute_local_tool(tool_cmd, self.repo_path)
      investigation_history.append(
          f"### Tool Request: {tool_cmd}\n```\n{tool_output}\n```")

      patch_res, m_used, _ = self.resolve_diagnostic(
          diagnostic=diagnostic,
          history_records=[],
          use_pro=True,
      )
      current_patch = patch_res
      model_used = m_used

    return current_patch, model_used

  def run_resolution_loop(self) -> bool:
    """Executes the standard self-healing loop until clean or exhausted."""
    last_error_summary = ""
    stuck_count = 0
    history_records: List[Dict[str, Any]] = []

    for iteration in range(1, self.max_iterations + 1):
      print(
          f"\n[{self.name}] >>> Iteration {iteration}/{self.max_iterations}...",
          file=sys.stderr,
      )
      success, output, siso_out = self.run_command(iteration)
      if success:
        print(
            f"[{self.name}] [SUCCESS] Completed cleanly on iteration "
            f"{iteration}/{self.max_iterations}!",
            file=sys.stderr,
        )
        return True

      diagnostics = self.extract_diagnostics(output, siso_out)
      if not diagnostics:
        print(
            f"[{self.name}] [WARNING] Command failed but no structured "
            "diagnostics parsed. Using raw output snippet...",
            file=sys.stderr,
        )
        diagnostics = [output]

      first_diag = diagnostics[0]
      diag_msg = getattr(first_diag, "error_message", str(first_diag))
      error_summary = diag_msg.strip().splitlines()[0] if diag_msg else ""
      print(
          f"[{self.name}] Detected {len(diagnostics)} error(s):\n"
          f"  - Error: {error_summary}",
          file=sys.stderr,
      )

      # Repetition check -> escalate to Pro model
      if error_summary == last_error_summary:
        stuck_count += 1
      else:
        stuck_count = 0
      last_error_summary = error_summary

      use_pro = stuck_count >= 1
      if use_pro:
        print(
            f"  [{self.name}] [PRO_REASONING] Repeated error detected "
            f"(count: {stuck_count}). Escalating to Pro model...",
            file=sys.stderr,
        )

      patch, model_used, rel_target = self.resolve_diagnostic(
          diagnostic=first_diag,
          history_records=history_records,
          use_pro=use_pro,
      )

      # Check for multi-turn tool commands
      if patch.strip().startswith("TOOL_"):
        patch, model_used = self.execute_investigation_tools(
            initial_patch=patch,
            diagnostic=first_diag,
        )

      if not patch:
        print(
            f"[{self.name}] [FAIL] Model returned empty patch.",
            file=sys.stderr,
        )
        record_failure(
            phase=self.name,
            target=rel_target or "unknown",
            error_message=error_summary,
            attempt_num=iteration,
            details=output[:2000],
        )
        continue

      print(
          f"[{self.name}] Applying AI patch using {model_used} to "
          f"{rel_target}...",
          file=sys.stderr,
      )
      modified_files = apply_patch_or_replacement(patch, self.repo_path)
      if modified_files:
        mod_summary = ", ".join(
            os.path.relpath(f, self.repo_path) for f in modified_files)
        print(
            f"[{self.name}] [OK] Patch applied cleanly to: {mod_summary}",
            file=sys.stderr,
        )
        self.on_patch_applied(modified_files)
        record_successful_fix(
            category=self.name,
            target_file=rel_target,
            error_signature=error_summary,
            fix_summary=(
                f"Resolved on iteration {iteration}/{self.max_iterations}"),
            solution_snippet=patch,
        )
        history_records.append({
            "iteration": iteration,
            "file": rel_target,
            "error": error_summary,
            "status": "APPLIED",
        })
      else:
        print(
            f"[{self.name}] [FAIL] Could not apply patch to {rel_target}.",
            file=sys.stderr,
        )
        record_failure(
            phase=self.name,
            target=rel_target or "unknown",
            error_message=error_summary,
            attempt_num=iteration,
            details=output[:2000],
        )

    print(
        f"[{self.name}] [FAIL] Exhausted maximum iterations "
        f"({self.max_iterations}).",
        file=sys.stderr,
    )
    return False
