#!/usr/bin/env python3
"""Base abstract class for AI-driven self-healing command resolvers.

Provides the foundational execution loop, multi-turn filesystem tools,
SEARCH/REPLACE patch application, and third-party protection guardrails
shared by all rebase phases (gclient sync, gn gen, and autoninja).
"""

import abc
import collections
import dataclasses
import os
import re
import subprocess
import sys
import time
from typing import Any, Callable, Dict, List, Optional, Tuple
import warnings

# Suppress google.auth UserWarning about ADC quota project on Cloudtop
warnings.filterwarnings("ignore", category=UserWarning, module="google.auth")

# Precompiled pattern matching investigation tool directives (e.g.,
# TOOL_READ_FILE: ...).
#
# Models occasionally emit several directives run together on a single line
# without separators (e.g. "TOOL_GREP: fooTOOL_READ_FILE: bar 1 20"), or glue a
# trailing "FILE:" patch header onto the last directive. The negative lookahead
# terminates each match at the next directive or patch header so malformed
# batches still parse into individual commands.
_TOOL_CMD_TERMINATORS = r"TOOL_[A-Z_]+:|FILE:|TARGET FILE:|<<<<<<<|>>>>>>>"
# No leading word-boundary anchor: in run-on responses a directive begins
# immediately after an alphanumeric character (e.g. "...mojomTOOL_READ_FILE:"),
# where \b does not hold and every directive after the first would be lost.
_TOOL_CMD_PATTERN = re.compile(r"(TOOL_[A-Z_]+:\s*"
                               r"(?:(?!" + _TOOL_CMD_TERMINATORS + r")"
                               r"[^\n<`])+)")

# Upper bound on investigation directives honored from a single model response.
# Prevents a speculative dump of a dozen commands from stalling the loop.
_MAX_TOOL_CMDS_PER_TURN = 3


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


_COBALT_GIT_HISTORY_CACHE: Dict[Tuple[str, str], bool] = {}


def has_cobalt_git_history(rel_path: str, repo_path: str) -> bool:
  """Checks if git history shows Cobalt-specific commits touching the file."""
  cache_key = (os.path.abspath(repo_path), rel_path)
  if cache_key in _COBALT_GIT_HISTORY_CACHE:
    return _COBALT_GIT_HISTORY_CACHE[cache_key]

  try:
    cmd = [
        "git",
        "-C",
        repo_path,
        "log",
        "-n",
        "50",
        "--format=%ae%x09%s",
        "--",
        rel_path,
    ]
    res = subprocess.run(
        cmd, cwd=repo_path, capture_output=True, text=True, check=False)
    if res.returncode != 0:
      _COBALT_GIT_HISTORY_CACHE[cache_key] = False
      return False
    for line in res.stdout.splitlines():
      if not line.strip():
        continue
      parts = line.split("\t", 1)
      author_email = parts[0].strip().lower()
      subject = parts[1].strip() if len(parts) > 1 else ""
      s_lower = subject.lower()

      # 1. Skip automated Chromium rolling PRs
      is_roll = ("cherry pick commit" in s_lower or "update to " in s_lower or
                 "autoroll" in s_lower or "releaser-bot" in author_email)
      if is_roll:
        continue

      # 2. Skip upstream Chromium commits (@chromium.org)
      if author_email.endswith(("@chromium.org", ".chromium.org")):
        continue

      # 3. Any commit with a Cobalt PR number (#<id>) or Cobalt/Starboard
      # reference authored by developers/contractors (Google, Igalia, etc.)
      # indicates Cobalt customization.
      has_pr_number = bool(re.search(r"\(#\d+\)|cherry pick pr #", s_lower))
      has_cobalt_keyword = any(k in s_lower for k in ("cobalt", "starboard"))
      if has_pr_number or has_cobalt_keyword:
        _COBALT_GIT_HISTORY_CACHE[cache_key] = True
        return True
  except (OSError, subprocess.SubprocessError):
    _COBALT_GIT_HISTORY_CACHE[cache_key] = False
    return False

  _COBALT_GIT_HISTORY_CACHE[cache_key] = False
  return False


def is_unmodified_third_party(file_path: str, repo_path: str) -> bool:
  """Checks if a file is pure third-party source code without Cobalt changes."""
  rel = os.path.relpath(file_path, repo_path)
  if not rel.startswith("third_party/"):
    return False
  # TODO(b/547499991) This should send to the LLM with skills to decide whether
  # agent can modify.
  # In-tree generator and build tooling in third_party (e.g. jni_zero) can be
  # patched for toolchain and API compatibility.
  if rel.startswith("third_party/jni_zero/"):
    return False
  # Cobalt/Starboard-specific directories or files hosted under third_party
  rel_lower = rel.lower()
  if "cobalt" in rel_lower or "starboard" in rel_lower:
    return False
  # Check if Cobalt git history previously touched this file
  if has_cobalt_git_history(rel, repo_path):
    return False
  try:
    with open(file_path, "r", encoding="utf-8", errors="replace") as f:
      lines = f.readlines()
    # Strip git conflict marker lines so commit messages don't trigger false
    # positives
    code_lines = [
        l for l in lines
        if not (l.startswith("<<<<<<<") or l.startswith(">>>>>>>") or
                l.startswith("======="))
    ]
    content = "".join(code_lines)
    content_lower = content.lower()
    if "cobalt" in content_lower or "starboard" in content_lower:
      return False
    if any(
        m in content for m in (
            "BUILDFLAG(IS_COBALT)",
            "BUILDFLAG(USE_STARBOARD_MEDIA)",
            "defined(STARBOARD)",
            "is_starboard",
            "is_cobalt",
            "checkout_cobalt_internal",
            "checkout_copybara",
            "ENABLE_BUILDFLAG_BUILD_BASE_WITH_CPP17",
        )):
      return False
  except OSError:
    pass
  return True


def is_generated_build_artifact(file_path: str, repo_path: str) -> bool:
  """Checks if a file is an auto-generated build artifact (out/, gen/, obj/)."""
  rel = os.path.relpath(file_path, repo_path)
  return (rel.startswith("out/") or rel.startswith("gen/") or
          rel.startswith("obj/") or "/gen/" in rel)


def validate_patch_target(target_file: str,
                          rel_file: str,
                          repo_path: str,
                          operation_name: str = "patch") -> bool:
  """Validates if target_file is safe for AI patch modifications.

  Returns False (and prints guard warnings to sys.stderr) if target_file is
  a generated build artifact or an unmodified third-party source file.
  """
  if (rel_file.endswith((".apk", ".ninja", ".so", ".a", ".o")) or
      rel_file in ("cobalt_apk", "all")):
    print(
        f"  [GUARD] Rejecting {operation_name} on build target / binary: "
        f"{rel_file}. Locate and patch the referencing source (.cc/.h) or "
        "BUILD.gn file.",
        file=sys.stderr,
    )
    return False
  if is_generated_build_artifact(target_file, repo_path):
    print(
        f"  [GUARD] Rejecting {operation_name} on generated build artifact: "
        f"{rel_file}. Trace #include stack to patch referencing source.",
        file=sys.stderr,
    )
    return False
  if rel_file.startswith("cobalt/build/configs/") or rel_file.endswith(
      "args.gn"):
    print(
        f"  [GUARD] Rejecting {operation_name} on global build config file: "
        f"{rel_file}. Modify component BUILD.gn or source code instead.",
        file=sys.stderr,
    )
    return False
  if (not target_file.endswith((".gn", ".gni", ".star")) and
      is_unmodified_third_party(target_file, repo_path)):
    print(
        f"  [GUARD] Rejecting {operation_name} on unmodified third-party "
        f"source file: {rel_file}. Patch the referencing BUILD.gn instead.",
        file=sys.stderr,
    )
    return False
  return True


def apply_search_replace(file_path: str, search_block: str,
                         replace_block: str) -> bool:
  """Applies a SEARCH/REPLACE block edit to a file.

  Note on Sanitization:
    LLMs occasionally hallucinate git 3-way merge conflict syntax (`=======`,
    `<<<<<<<`, `>>>>>>>`) inside replacement blocks when operating on flat
    configuration files. We strip any accidental marker lines to prevent
    polluting the codebase with orphan markers that break GN/compiler parsing.
  """
  if not os.path.isfile(file_path):
    return False

  # Sanitize replace_block against rogue conflict markers from model output
  sanitized_lines = [
      line for line in replace_block.splitlines()
      if not line.startswith("=======") and not line.startswith("<<<<<<<") and
      not line.startswith(">>>>>>>")
  ]
  clean_replace = "\n".join(sanitized_lines)
  if replace_block.endswith("\n"):
    clean_replace += "\n"

  # Strip leading line numbers (e.g. `1060: `) if model attached them
  cleaned_search_lines = [
      re.sub(r"^\s*\d+:\s*", "", l) for l in search_block.splitlines()
  ]
  search_block = "\n".join(cleaned_search_lines)

  cleaned_replace_lines = [
      re.sub(r"^\s*\d+:\s*", "", l) for l in clean_replace.splitlines()
  ]
  clean_replace = "\n".join(cleaned_replace_lines)

  with open(file_path, "r", encoding="utf-8", errors="replace") as f:
    content = f.read()

  # Normalize CRLF to LF across search, replace, and file content
  search_block = search_block.replace("\r\n", "\n")
  clean_replace = clean_replace.replace("\r\n", "\n")
  content = content.replace("\r\n", "\n")

  # 1. Exact match
  if search_block in content:
    new_content = content.replace(search_block, clean_replace, 1)
    with open(file_path, "w", encoding="utf-8") as f:
      f.write(new_content)
    return True

  # 2. Whitespace-trimmed match
  s_stripped = search_block.strip()
  if s_stripped and s_stripped in content:
    new_content = content.replace(s_stripped, clean_replace.strip(), 1)
    with open(file_path, "w", encoding="utf-8") as f:
      f.write(new_content)
    return True

  c_lines = content.splitlines()

  # 3. Normalized line-by-line match
  s_lines = [line.strip() for line in search_block.splitlines() if line.strip()]
  if s_lines:
    for i in range(len(c_lines) - len(s_lines) + 1):
      window = [c_lines[i + j].strip() for j in range(len(s_lines))]
      if window == s_lines:
        new_lines = c_lines[:i] + clean_replace.splitlines(
        ) + c_lines[i + len(s_lines):]
        with open(file_path, "w", encoding="utf-8") as f:
          f.write("\n".join(new_lines) + "\n")
        return True

  # 4. Token-normalized & line-number stripped match (for auto-generated files)
  def norm(l: str) -> str:
    return re.sub(r"\s+", " ", re.sub(r"^\s*\d+:\s*", "", l)).strip()

  s_norm = [norm(l) for l in search_block.splitlines() if norm(l)]
  if s_norm:
    for i in range(len(c_lines) - len(s_norm) + 1):
      window = [norm(c_lines[i + j]) for j in range(len(s_norm))]
      if window == s_norm:
        new_lines = c_lines[:i] + clean_replace.splitlines(
        ) + c_lines[i + len(s_norm):]
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

  if not validate_patch_target(
      file_path, rel_file, repo_path, operation_name="unified diff"):
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


# Matches optional file directive headers produced by LLMs
# preceding patch blocks: e.g., "FILE: foo.cc", "**FILE**: 'baz.gn'"
_FILE_HEADER_PREFIX = (
    r"(?:(?:#{1,6}\s*)?"  # Optional markdown header (### )
    r"\*{0,2}(?:FILE|TARGET FILE)\*{0,2}"  # Optional bold (**)
    r":\s*"  # Colon
    r"[`'\"]*([a-zA-Z0-9_/\.\-]+)[`'\"]*"  # Captured relative path (Group 1)
    r"\s*[\r\n]+)?"  # Trailing newline (entire header is optional)
)


def apply_patch_or_replacement(
    patch_text: str,
    repo_path: str,
    default_file: Optional[str] = None,
) -> List[str]:
  """Parses and dispatches AI patch responses (SEARCH/REPLACE, DELETE, diffs).
  """
  clean_text = patch_text.strip()
  clean_text = re.sub(r"^```[a-zA-Z0-9_-]*\n", "", clean_text)
  clean_text = re.sub(r"\n```$", "", clean_text)

  # 1. Explicit DELETE block: <<<<<<< DELETE ... >>>>>>> DELETE
  if "<<<<<<< DELETE" in clean_text and ">>>>>>> DELETE" in clean_text:
    del_pattern = re.compile(
        _FILE_HEADER_PREFIX + r"(?:\s*```[a-zA-Z0-9_-]*\s*[\r\n]+)?"
        r"<<<<<<<\s*DELETE\r?\n(.*?)\r?\n>>>>>>>\s*DELETE"
        r"(?:\s*```)?",
        re.DOTALL | re.IGNORECASE,
    )
    matches = del_pattern.findall(clean_text)
    if matches:
      modified_files = []
      for rel_file, delete_b in matches:
        target_rel = rel_file.strip() if rel_file and rel_file.strip() else (
            default_file or "")
        if not target_rel:
          continue
        target_file = resolve_repo_file_path(target_rel, repo_path)
        if not validate_patch_target(
            target_file, target_rel, repo_path, operation_name="DELETE"):
          return []
        applied = apply_search_replace(target_file, delete_b, "")
        if applied:
          modified_files.append(target_file)
        else:
          return []
      if modified_files:
        return modified_files

  # 2. SEARCH / REPLACE format: <<<<<<< SEARCH ... ======= ... >>>>>>> REPLACE
  if "<<<<<<< SEARCH" in clean_text and "=======" in clean_text:
    sr_pattern = re.compile(
        _FILE_HEADER_PREFIX + r"(?:\s*```[a-zA-Z0-9_-]*\s*[\r\n]+)?"
        r"<<<<<<<\s*SEARCH\r?\n(.*?)\r?\n"
        r"=======\r?\n(.*?)\r?\n>>>>>>>\s*REPLACE"
        r"(?:\s*```)?",
        re.DOTALL | re.IGNORECASE,
    )
    matches = sr_pattern.findall(clean_text)
    if matches:
      modified_files = []
      for rel_file, search_b, replace_b in matches:
        target_rel = rel_file.strip() if rel_file and rel_file.strip() else (
            default_file or "")
        if not target_rel:
          continue
        # Strip trailing ``` from replace_b if any leaked in
        clean_replace = re.sub(r"\n```\s*$", "", replace_b)
        if not clean_replace.strip() and len(search_b.splitlines()) > 80:
          print(
              f"  [GUARD] Rejecting bulk empty REPLACE block "
              f"({len(search_b.splitlines())} lines) in {target_rel}. Use "
              "<<<<<<< DELETE ... >>>>>>> DELETE for intentional bulk "
              "removals.",
              file=sys.stderr,
          )
          return []
        if re.search(r"^(?:FILE|Target File):", clean_replace, re.MULTILINE):
          print(
              f"  [GUARD] Rejecting malformed REPLACE block in {target_rel} "
              "containing nested FILE directives.",
              file=sys.stderr,
          )
          return []
        target_file = resolve_repo_file_path(target_rel, repo_path)
        if not validate_patch_target(
            target_file, target_rel, repo_path, operation_name="patch"):
          return []
        applied = apply_search_replace(target_file, search_b, clean_replace)
        if applied:
          modified_files.append(target_file)
        else:
          return []
      if modified_files:
        return modified_files

  return apply_unified_diff(clean_text, repo_path)


def extract_file_changes_from_patch(
    patch_text: str,
    repo_path: str,
    default_file: Optional[str] = None,
) -> Dict[str, str]:
  """Extracts per-file patch/diff blocks from AI patch text."""
  clean_text = patch_text.strip()
  clean_text = re.sub(r"^```[a-zA-Z0-9_-]*\n", "", clean_text)
  clean_text = re.sub(r"\n```$", "", clean_text)
  file_changes: Dict[str, str] = {}

  # 1. Explicit DELETE block: <<<<<<< DELETE ... >>>>>>> DELETE
  if "<<<<<<< DELETE" in clean_text and ">>>>>>> DELETE" in clean_text:
    del_pattern = re.compile(
        _FILE_HEADER_PREFIX + r"(?:\s*```[a-zA-Z0-9_-]*\s*[\r\n]+)?"
        r"<<<<<<<\s*DELETE\r?\n(.*?)\r?\n>>>>>>>\s*DELETE"
        r"(?:\s*```)?",
        re.DOTALL | re.IGNORECASE,
    )
    for rel_file, delete_b in del_pattern.findall(clean_text):
      target_rel = rel_file.strip() if rel_file and rel_file.strip() else (
          default_file or "")
      if target_rel:
        try:
          rel_norm = os.path.relpath(
              resolve_repo_file_path(target_rel, repo_path), repo_path)
        except ValueError:
          rel_norm = target_rel
        block = f"<<<<<<< DELETE\n{delete_b}\n>>>>>>> DELETE"
        if rel_norm in file_changes:
          file_changes[rel_norm] += "\n\n" + block
        else:
          file_changes[rel_norm] = block
    if file_changes:
      return file_changes

  # 2. SEARCH / REPLACE format: <<<<<<< SEARCH ... ======= ... >>>>>>> REPLACE
  if "<<<<<<< SEARCH" in clean_text and "=======" in clean_text:
    sr_pattern = re.compile(
        _FILE_HEADER_PREFIX + r"(?:\s*```[a-zA-Z0-9_-]*\s*[\r\n]+)?"
        r"<<<<<<<\s*SEARCH\r?\n(.*?)\r?\n"
        r"=======\r?\n(.*?)\r?\n>>>>>>>\s*REPLACE"
        r"(?:\s*```)?",
        re.DOTALL | re.IGNORECASE,
    )
    for rel_file, search_b, replace_b in sr_pattern.findall(clean_text):
      target_rel = rel_file.strip() if rel_file and rel_file.strip() else (
          default_file or "")
      if target_rel:
        clean_replace = re.sub(r"\n```\s*$", "", replace_b)
        try:
          rel_norm = os.path.relpath(
              resolve_repo_file_path(target_rel, repo_path), repo_path)
        except ValueError:
          rel_norm = target_rel
        block = (f"<<<<<<< SEARCH\n{search_b}\n=======\n"
                 f"{clean_replace}\n>>>>>>> REPLACE")
        if rel_norm in file_changes:
          file_changes[rel_norm] += "\n\n" + block
        else:
          file_changes[rel_norm] = block
    if file_changes:
      return file_changes

  # 3. Unified diff format: detect target file from diff header
  diff_match = re.search(r"^(?:--- [ab]/(.+)|diff --git a/.* b/(.+))$",
                         clean_text, re.MULTILINE)
  if diff_match:
    target_rel = (diff_match.group(1) or diff_match.group(2) or "").strip()
    if target_rel:
      try:
        rel_norm = os.path.relpath(
            resolve_repo_file_path(target_rel, repo_path), repo_path)
      except ValueError:
        rel_norm = target_rel
      file_changes[rel_norm] = clean_text
      return file_changes

  # Fallback to default_file if available
  if default_file:
    try:
      rel_norm = os.path.relpath(
          resolve_repo_file_path(default_file, repo_path), repo_path)
    except ValueError:
      rel_norm = default_file
    file_changes[rel_norm] = patch_text

  return file_changes


def sanitize_filepath_token(raw_target: str) -> str:
  """Extracts a clean, valid repository file path token from model output.

  Strips surrounding backticks, quotes, parenthetical/inline commentary,
  and trailing punctuation.
  """
  clean = raw_target.strip().strip("`'\"[]()<>")
  tokens = re.split(r"[\s\(\[\#]+", clean)
  if tokens and tokens[0]:
    token = tokens[0].strip("`'\"[]()<>,;:")
    return token.lstrip("./")
  return clean.lstrip("./")


def extract_tool_commands(text: str,
                          max_commands: int = _MAX_TOOL_CMDS_PER_TURN
                         ) -> List[str]:
  """Extracts TOOL_ commands, stripping think tags/backticks/preambles.

  Tolerates malformed batches: directives may be wrapped in backticks, prefixed
  with markdown bullets or "Tool Call:", or run together on a single line with
  no separators. At most `max_commands` directives are honored per response so
  a speculative dump of a dozen commands cannot stall the investigation loop.
  """
  clean = re.sub(r"<think>.*?</think>", "", text, flags=re.DOTALL)
  clean = re.sub(r"</?think>.*$", "", clean, flags=re.MULTILINE)
  clean = re.sub(r"</?think>", "", clean)

  # If model has already provided a full SEARCH/REPLACE block or unified diff,
  # do not treat it as an investigation tool command.
  if (("<<<<<<< SEARCH" in clean and ">>>>>>> REPLACE" in clean) or
      ("<<<<<<< DELETE" in clean and ">>>>>>> DELETE" in clean) or
      re.search(r"^@@\s+-\d+.*?\s+\+\d+.*?@@", clean, re.MULTILINE)):
    return []

  # Drop markdown bullets / "Tool Call:" preambles so directives start cleanly.
  clean = re.sub(
      r"^(?:[-*]\s+)?(?:Tool Call:\s*|Tool:\s*)",
      "",
      clean,
      flags=re.IGNORECASE | re.MULTILINE,
  )

  commands: List[str] = []
  seen = set()
  # Scan the whole response rather than one match per line: models sometimes
  # concatenate directives without newlines.
  for match in _TOOL_CMD_PATTERN.finditer(clean):
    cmd = re.sub(r"[`'\"]+$", "", match.group(1)).strip()
    cmd = cmd.strip("`'\"").strip()
    if not cmd or cmd in seen:
      continue
    seen.add(cmd)
    commands.append(cmd)
    if len(commands) >= max_commands:
      break
  return commands


def find_roll_commit(repo_path: str, conflicted: bool) -> Optional[str]:
  """Locates a commit from the most recent Chromium autoroll.

  An autoroll lands as three commits:

    1. "Revert Cobalt."                              (upstream baseline)
    2. "Update to <milestone>."                      (pure upstream changes)
    3. "CONFLICTED Cherry pick ...: Update to <milestone>."
                                                     (Cobalt re-applied)

  Diffing #2 answers "what did upstream change?". Diffing #3 answers "what
  does Cobalt add on top of upstream, and did it still land correctly?".

  Args:
    repo_path: Repository to search.
    conflicted: When True return commit #3, otherwise return commit #2.

  Returns:
    The commit SHA, or None if no matching roll commit exists.
  """
  # Both subjects contain "Update to <milestone>", so the pure upstream
  # subject is anchored with ^ to exclude the cherry-picks.
  pattern = ("^CONFLICTED Cherry pick.*Update to [0-9]"
             if conflicted else "^Update to [0-9]")
  try:
    log_res = subprocess.run(
        ["git", "log", "-n20", f"--grep={pattern}", "--format=%H %s"],
        cwd=repo_path,
        capture_output=True,
        text=True,
        check=False,
    )
  except OSError:
    return None

  for line in log_res.stdout.splitlines():
    parts = line.split(" ", 1)
    if len(parts) != 2:
      continue
    # %H %s always begins with the SHA, so the subject must be inspected
    # explicitly rather than testing the start of the whole line.
    subject = parts[1]
    if conflicted == subject.startswith("CONFLICTED"):
      return parts[0]
  return None


def show_roll_diff(
    repo_path: str,
    sha: str,
    target_path: str,
    label: str,
) -> str:
  """Renders `git show` for a roll commit, optionally scoped to one file."""
  cmd = (["git", "show", "--stat", "-p", sha, "--", target_path]
         if target_path else ["git", "show", "--stat", sha])
  diff_res = subprocess.run(
      cmd,
      cwd=repo_path,
      capture_output=True,
      text=True,
      errors="replace",
      check=False,
  )
  if diff_res.stdout:
    return diff_res.stdout[:8000]
  return f"No {label} changes in {sha} for: {target_path}"


def execute_local_tool(
    cmd: str,
    repo_path: str,
    session_changes: Optional[List[AgentChangeRecord]] = None,
) -> str:
  """Executes safe read-only multi-turn inspection tools for LLM."""
  clean_cmd = cmd.strip()

  # 1. TOOL_READ_FILE: <path> [line_range]
  if clean_cmd.startswith("TOOL_READ_FILE:"):
    raw_args = clean_cmd.split(":", 1)[1].strip()
    tokens = [
        t.strip("`'\"<>,;:") for t in raw_args.split() if t.strip("`'\"<>,;:")
    ]
    if not tokens:
      return "[ERROR] No file path provided."
    target_rel = tokens[0]
    line_range = ""
    for t in tokens[1:]:
      if re.match(r"^\d+-\d+$", t) or re.match(r"^\d+\.\.\d+$", t):
        line_range = t.replace("..", "-")
        break
    if ":" in target_rel and not line_range:
      parts = target_rel.split(":", 1)
      target_rel = parts[0]
      if (re.match(r"^\d+-\d+$", parts[1]) or
          re.match(r"^\d+\.\.\d+$", parts[1])):
        line_range = parts[1].replace("..", "-")

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
      if len(lines) <= 250:
        numbered = [f"{i + 1}: {l}" for i, l in enumerate(lines)]
        return "".join(numbered)
      preview = lines[:150]
      numbered = [f"{i + 1}: {l}" for i, l in enumerate(preview)]
      numbered.append(
          f"\n[... Truncated {len(lines) - 150} lines. Use TOOL_READ_FILE: "
          f"{target_rel} <start>-<end> to view specific lines]\n")
      return "".join(numbered)
    except Exception as e:  # pylint: disable=broad-exception-caught
      return f"[ERROR] Could not read {target_rel}: {e}"

  # 2. TOOL_GREP: <query> [path_or_glob]
  if clean_cmd.startswith("TOOL_GREP:"):
    raw_args = clean_cmd.split(":", 1)[1].strip()
    query = ""
    path_filter = ""
    q_match = re.match(r'^([\'"])(.*?)\1(?:\s+(.*))?$', raw_args)
    if q_match:
      query = q_match.group(2).strip()
      path_filter = (q_match.group(3) or "").strip().strip("`'\"")
    else:
      parts = raw_args.split(None, 1)
      if len(parts) == 2:
        query = parts[0].strip("`'\"")
        path_filter = parts[1].strip("`'\"")
      elif len(parts) == 1:
        query = parts[0].strip("`'\"")
      else:
        return "[ERROR] No query provided to TOOL_GREP."

    query = re.sub(r"\s*\(\s*\)\s*$", "", query).strip()
    if not query:
      return "[ERROR] Empty query in TOOL_GREP."

    grep_cmd = ["git", "grep", "-n", "-I", "--max-count=15", query]
    if path_filter:
      grep_cmd.extend(["--", path_filter])
    else:
      grep_cmd.extend([
          "--",
          "*.gn",
          "*.gni",
          "*.h",
          "*.cc",
          "*.cpp",
          "*.inc",
          "*.java",
          "*.rs",
          "*.py",
      ])
    try:
      res = subprocess.run(
          grep_cmd,
          cwd=repo_path,
          capture_output=True,
          text=True,
          errors="replace",
          check=False,
      )
      lines = res.stdout.splitlines()[:30]
      text = "\n".join(lines)
      filt_desc = path_filter or "codebase"
      return (text[:6000].strip() if text else
              f"No matches found for: {query} (filter: {filt_desc})")
    except Exception as e:  # pylint: disable=broad-exception-caught
      return f"[ERROR] Grep failed: {e}"

  # 3. TOOL_FIND_FILE: <pattern>
  if clean_cmd.startswith("TOOL_FIND_FILE:"):
    raw_pat = clean_cmd.split(":", 1)[1].strip()
    tokens = [
        t.strip("`'\"<>,;:") for t in raw_pat.split() if t.strip("`'\"<>,;:")
    ]
    pattern = tokens[0] if tokens else raw_pat.strip("`'\"")
    if not pattern.startswith("*") and not pattern.endswith("*"):
      pattern = f"*{pattern}*"
    try:
      res = subprocess.run(
          ["find", ".", "-iname", pattern, "-not", "-path", "*/.*"],
          cwd=repo_path,
          capture_output=True,
          text=True,
          check=False,
      )
      lines = [l.lstrip("./") for l in res.stdout.splitlines()[:25]]
      return "\n".join(lines) if lines else f"No matches found for: {pattern}"
    except Exception as e:  # pylint: disable=broad-exception-caught
      return f"[ERROR] Find failed: {e}"

  # 4. TOOL_LIST_DIR: <dir_path>
  if clean_cmd.startswith("TOOL_LIST_DIR:"):
    raw_dir = clean_cmd.split(":", 1)[1].strip()
    tokens = [
        t.strip("`'\"<>,;:") for t in raw_dir.split() if t.strip("`'\"<>,;:")
    ]
    dir_rel = tokens[0] if tokens else "."
    dir_abs = resolve_repo_file_path(dir_rel, repo_path)
    if not os.path.isdir(dir_abs):
      return f"[ERROR] Directory does not exist: {dir_rel}"
    try:
      entries = sorted(os.listdir(dir_abs))[:50]
      formatted = []
      for e in entries:
        full_e = os.path.join(dir_abs, e)
        is_d = "/" if os.path.isdir(full_e) else ""
        formatted.append(f"{e}{is_d}")
      return "\n".join(formatted) if formatted else "(Directory is empty)"
    except Exception as e:  # pylint: disable=broad-exception-caught
      return f"[ERROR] List directory failed: {e}"

  # 5. TOOL_GIT_SHOW: <ref>
  if clean_cmd.startswith("TOOL_GIT_SHOW:"):
    raw_ref = clean_cmd.split(":", 1)[1].strip()
    tokens = [
        t.strip("`'\"<>,;:") for t in raw_ref.split() if t.strip("`'\"<>,;:")
    ]
    ref = tokens[0] if tokens else raw_ref
    try:
      res = subprocess.run(
          ["git", "show", "--no-color", ref],
          cwd=repo_path,
          capture_output=True,
          text=True,
          errors="replace",
          check=False,
      )
      return (res.stdout[:4000] if res.stdout else f"Could not show ref: {ref}")
    except Exception as e:  # pylint: disable=broad-exception-caught
      return f"[ERROR] Git show failed: {e}"

  if clean_cmd.startswith("TOOL_READ_PR:"):
    raw_pr = clean_cmd.split(":", 1)[1].strip()
    match = re.search(r"(\d+)", raw_pr)
    if not match:
      return f"[ERROR] Could not extract PR number from: {raw_pr}"
    pr_target = match.group(1)
    try:
      res = subprocess.run(
          [
              "gh", "pr", "view", pr_target, "--json",
              "number,title,body,commits"
          ],
          cwd=repo_path,
          capture_output=True,
          text=True,
          check=False,
      )
      return (res.stdout[:8000] if res.stdout else
              f"Could not view PR: {pr_target} ({res.stderr})")
    except Exception as e:  # pylint: disable=broad-exception-caught
      return f"[ERROR] gh pr view failed: {e}"

  if clean_cmd.startswith("TOOL_PR_DIFF:"):
    raw_pr = clean_cmd.split(":", 1)[1].strip()
    match = re.search(r"(\d+)", raw_pr)
    if not match:
      return f"[ERROR] Could not extract PR number from: {raw_pr}"
    pr_target = match.group(1)
    try:
      res = subprocess.run(
          ["gh", "pr", "diff", pr_target],
          cwd=repo_path,
          capture_output=True,
          text=True,
          check=False,
      )
      return (res.stdout[:16384] if res.stdout else
              f"Could not diff PR: {pr_target} ({res.stderr})")
    except Exception as e:  # pylint: disable=broad-exception-caught
      return f"[ERROR] gh pr diff failed: {e}"

  if clean_cmd.startswith("TOOL_GIT_DIFF:"):
    diff_args = clean_cmd.split(":", 1)[1].strip().split()
    try:
      res = subprocess.run(
          ["git", "diff"] + diff_args,
          cwd=repo_path,
          capture_output=True,
          text=True,
          errors="replace",
          check=False,
      )
      return (res.stdout[:16384]
              if res.stdout else f"Git diff empty or failed for: {diff_args}")
    except Exception as e:  # pylint: disable=broad-exception-caught
      return f"[ERROR] Git diff failed: {e}"

  if clean_cmd.startswith("TOOL_GIT_LOG:"):
    args = clean_cmd.split(":", 1)[1].strip().split()
    try:
      count = 5
      target_path = ""
      if args and args[0].isdigit():
        count = int(args[0])
        target_path = " ".join(args[1:])
      else:
        target_path = " ".join(args)
      cmd = ["git", "log", f"-n{count}", "--oneline"]
      if target_path:
        cmd.extend(["--", target_path])
      res = subprocess.run(
          cmd,
          cwd=repo_path,
          capture_output=True,
          text=True,
          errors="replace",
          check=False,
      )
      return (res.stdout[:4000]
              if res.stdout else f"No git log found for: {target_path}")
    except Exception as e:  # pylint: disable=broad-exception-caught
      return f"[ERROR] Git log failed: {e}"

  if clean_cmd.startswith("TOOL_UPSTREAM_DIFF:"):
    raw_path = clean_cmd.split(":", 1)[1].strip()
    target_path = sanitize_filepath_token(raw_path)
    try:
      upstream_sha = find_roll_commit(repo_path, conflicted=False)
      if not upstream_sha:
        return "Could not find upstream roll commit ('Update to <milestone>')"
      return show_roll_diff(repo_path, upstream_sha, target_path, "upstream")
    except Exception as e:  # pylint: disable=broad-exception-caught
      return f"[ERROR] Upstream diff failed: {e}"

  if clean_cmd.startswith("TOOL_COBALT_DIFF:"):
    raw_path = clean_cmd.split(":", 1)[1].strip()
    target_path = sanitize_filepath_token(raw_path)
    try:
      cobalt_sha = find_roll_commit(repo_path, conflicted=True)
      if not cobalt_sha:
        return ("Could not find Cobalt cherry-pick commit "
                "('CONFLICTED Cherry pick ...: Update to <milestone>')")
      return show_roll_diff(repo_path, cobalt_sha, target_path, "Cobalt")
    except Exception as e:  # pylint: disable=broad-exception-caught
      return f"[ERROR] Cobalt diff failed: {e}"

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

  # 10. TOOL_GET_HISTORY: <count | all | iteration_number | start-end |
  # filepath>
  if clean_cmd.startswith(
      ("TOOL_GET_HISTORY:", "TOOL_CHANGE_HISTORY:", "TOOL_HISTORY:")):
    raw_args = clean_cmd.split(":", 1)[1].strip()
    if not session_changes:
      return (
          "[NOTICE] No recorded change history available in this session yet.")

    if raw_args.lower() in ("all", "full"):
      return (
          f"=== Full Change History ({len(session_changes)} records) ===\n\n" +
          "\n\n".join(r.to_prompt_str() for r in session_changes))

    clean_target = raw_args.strip("`'\"")
    # Check if raw_args specifies a file path (or substring matching a file
    # path)
    if not re.match(
        r"^(?:iteration|iter|#)?\s*\d+(?:\s*(?:-|to|\.\.)\s*\d+)?$",
        clean_target,
        re.IGNORECASE,
    ):
      matched_files = [
          r for r in session_changes if (clean_target in r.target_file or any(
              clean_target in f for f in r.modified_files))
      ]
      if matched_files:
        return (f"=== Change Records for '{clean_target}' "
                f"({len(matched_files)} records) ===\n\n" +
                "\n\n".join(r.to_prompt_str() for r in matched_files))
      return f"[NOTICE] No change records found matching file '{clean_target}'."

    # Check for iteration range: e.g. "1-5" or "1..5"
    m_range = re.search(r"(\d+)\s*(?:-|to|\.\.)\s*(\d+)", raw_args)
    if m_range:
      s_iter = int(m_range.group(1))
      e_iter = int(m_range.group(2))
      matched = [r for r in session_changes if s_iter <= r.iteration <= e_iter]
      if not matched:
        return ("[NOTICE] No change records found in iteration range "
                f"{s_iter}-{e_iter}.")
      return (f"=== Change Records for Iterations {s_iter}-{e_iter} "
              f"({len(matched)} records) ===\n\n" +
              "\n\n".join(r.to_prompt_str() for r in matched))

    # Check for specific iteration or count
    m_iter = re.search(r"(?:iteration|iter|#)?\s*(\d+)", raw_args,
                       re.IGNORECASE)
    if m_iter:
      num = int(m_iter.group(1))
      if "iter" in raw_args.lower():
        matched = [r for r in session_changes if r.iteration == num]
        if not matched:
          return f"[NOTICE] No change record found for iteration {num}."
        return (f"=== Change Record for Iteration {num} ===\n\n" +
                "\n\n".join(r.to_prompt_str() for r in matched))
      matched = (
          session_changes[-num:]
          if num < len(session_changes) else session_changes)
      return (f"=== Last {len(matched)} Change Records (out of "
              f"{len(session_changes)}) ===\n\n" +
              "\n\n".join(r.to_prompt_str() for r in matched))

  return f"[ERROR] Unknown tool command: {clean_cmd}"


def extract_build_progress(build_output: str, siso_output: str = "") -> str:
  """Extracts step progress like [11465/39291] or [50222/50832]."""
  combined = f"{build_output}\n{siso_output}"
  matches = re.findall(r"\[\s*(\d+)\s*/\s*(\d+)\s*\]", combined)
  if matches:
    done_str, total_str = matches[-1]
    done, total = int(done_str), int(total_str)
    pct = (done / total * 100) if total > 0 else 0.0
    return f"[{done}/{total}] ({pct:.1f}%)"

  siso_matches = re.findall(r"Done:(\d+).*?Total:(\d+)", combined)
  if siso_matches:
    done_str, total_str = siso_matches[-1]
    done, total = int(done_str), int(total_str)
    pct = (done / total * 100) if total > 0 else 0.0
    return f"[{done}/{total}] ({pct:.1f}%)"
  return ""


def write_rebase_report(
    rebase_dir: str,
    platform: str,
    build_type: str,
    *,
    target: str,
    model: str,
    status: str,
    elapsed_seconds: float,
    repo_path: Optional[str] = None,
    expert_model: Optional[str] = None,
    session_changes: Optional[List[AgentChangeRecord]] = None,
) -> str:
  """Generates the final comprehensive rebase summary report."""
  milestone = get_chromium_milestone(repo_path)
  results_dir = os.path.join(rebase_dir, "results")
  os.makedirs(results_dir, exist_ok=True)
  report_filename = f"{milestone}_rebase_summary.md"
  report_path = os.path.join(results_dir, report_filename)
  comp_status = ("[OK] Clean"
                 if "SUCCESS" in status else "[WARNING] Requires Attention")
  workhorse = model or "gemini-3.7-flash"
  expert = expert_model or "gemini-3.8-flash"

  changes_section = ""
  if session_changes:
    phase_counts = collections.Counter(c.phase for c in session_changes)
    clean_counts = sum(1 for c in session_changes if c.applied_cleanly)
    error_counts = sum(1 for c in session_changes if c.error is not None)
    phase_breakdown = ", ".join(f"`{k}`: {v}" for k, v in phase_counts.items())
    changes_section = f"""
## 3. Autonomous Change Trajectory Summary
- **Total Changes Recorded**: `{len(session_changes)}`
- **Clean Patches Applied**: `{clean_counts}`
- **Subsequent Errors/Breaks**: `{error_counts}`
- **Changes by Phase**: {phase_breakdown}
"""

  content = f"""# Cobalt {milestone} Rebase Resolution & Verification Report

## 1. Executive Summary
- **Status**: **{status}**
- **Milestone**: `{milestone}`
- **Platform**: `{platform}`
- **Build Type**: `{build_type}`
- **Target**: `{target}`
- **Workhorse Model**: `{workhorse}`
- **Expert Model**: `{expert}`
- **Total Execution Time**: `{elapsed_seconds:.1f}s`

## 2. Rebase Pipeline Stages
| Phase | Stage | Description | Status |
| :--- | :--- | :--- | :--- |
| **Phase 1** | Conflict Resolution | Unified DEPS & source conflict repair | [OK] Completed |
| **Phase 2** | Toolchain Sync | `gclient sync -D` toolchain & CIPD sync | [OK] Completed |
| **Phase 3** | GN Config Check | `cobalt/build/gn.py --check` validation | [OK] Completed |
| **Phase 4** | autoninja Loop | autoninja compiler healing | {comp_status} |
{changes_section}"""
  try:
    with open(report_path, "w", encoding="utf-8") as f:
      f.write(content)
    print(
        f"[pipeline] [REPORT] Report written to: {report_path}",
        file=sys.stderr,
    )
  except OSError as e:
    print(
        f"[pipeline] [WARNING] Could not write report: {e}",
        file=sys.stderr,
    )
  return report_path


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
    return l_strip
  return raw_msg.strip().splitlines()[0]


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
      **kwargs: Any,
  ):
    del kwargs
    self.repo_path = repo_path
    self.max_iterations = max_iterations
    self.on_patch_applied_fn = on_patch_applied_fn
    self.reasoning_engine = engine
    self.file_error_counts: Dict[str, int] = collections.defaultdict(int)
    self.session_changes: List[AgentChangeRecord] = (
        session_changes if session_changes is not None else [])

  @property
  def model(self) -> str:
    """Active primary model name from reasoning engine."""
    if self.reasoning_engine is not None:
      return getattr(self.reasoning_engine, "flash_model", "gemini-3.7-flash")
    return "gemini-3.7-flash"

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
  # pylint: disable=too-many-positional-arguments,too-many-arguments
  def resolve_diagnostic(
      self,
      diagnostic: Any,
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
      diagnostic: Any,
      max_rounds: int = 12,
  ) -> Tuple[str, str]:
    """Runs multi-turn tool loop supporting batch tool requests from LLM."""
    current_patch = initial_patch
    model_used = self.model
    history_records: List[Dict[str, Any]] = []
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
        print(
            f"  [{self.name}] [Investigation Round {round_idx}/{max_rounds}] "
            f"{prefix}Model requested: {tool_cmd}",
            file=sys.stderr,
        )
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

      patch_res, m_used, _ = self.resolve_diagnostic(
          diagnostic=diagnostic,
          history_records=history_records,
          use_expert=True,
      )
      current_patch = patch_res
      model_used = m_used

      if any(seen_cmds[cmd] >= 3 for cmd in tool_cmds):
        print(
            f"  [{self.name}] [Anti-Loop] Breaking repeated tool loop after "
            f"{round_idx} rounds.",
            file=sys.stderr,
        )
        break

    return current_patch, model_used

  def run_resolution_loop(self) -> bool:
    """Executes the standard self-healing loop until clean or exhausted."""
    last_error_summary = ""
    stuck_count = 0
    history_records: List[Dict[str, Any]] = []
    pending_fix: Optional[Dict[str, Any]] = None
    pending_record: Optional[AgentChangeRecord] = None

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
        if pending_record is not None:
          pending_record.error = None
        if pending_fix and self.reasoning_engine is not None:
          self.reasoning_engine.record_successful_fix(
              issue_description=pending_fix["error"],
              solution_diff=pending_fix["patch"],
              target_file=pending_fix["file"],
          )
        return True

      diagnostics = self.extract_diagnostics(output, siso_out)
      progress_str = extract_build_progress(output, siso_out)
      if progress_str:
        print(f"[{self.name}] Build Progress: {progress_str}", file=sys.stderr)

      if not diagnostics:
        print(
            f"[{self.name}] [WARNING] Command failed but no structured "
            "diagnostics parsed. Using raw output snippet...",
            file=sys.stderr,
        )
        diagnostics = [output]

      first_diag = diagnostics[0]
      diag_msg = getattr(first_diag, "error_message", str(first_diag))
      diag_file = getattr(first_diag, "file_path", "")
      diag_line = getattr(first_diag, "line_number", 0)
      loc_str = ""
      rel_f = ""
      if diag_file:
        try:
          rel_f = os.path.relpath(diag_file, self.repo_path)
          loc_str = f" in {rel_f}:{diag_line}"
        except ValueError:
          rel_f = diag_file
          loc_str = f" in {diag_file}:{diag_line}"

      error_summary = extract_meaningful_error_summary(diag_msg)

      # Update the outcome of the previous patch attempt
      if pending_record is not None:
        pending_record.error = error_summary
        pending_record.command_output = (output + "\n" +
                                         (siso_out or ""))[-4000:]

      # If previous fix succeeded in eliminating that error, record it
      if pending_fix and self.reasoning_engine is not None:
        if (pending_fix["error"] != error_summary or
            pending_fix["file"] != rel_f):
          self.reasoning_engine.record_successful_fix(
              issue_description=pending_fix["error"],
              solution_diff=pending_fix["patch"],
              target_file=pending_fix["file"],
          )
      pending_fix = None

      print(
          f"[{self.name}] Detected {len(diagnostics)} error(s):\n"
          f"  - Error{loc_str}: {error_summary}",
          file=sys.stderr,
      )
      if raw_snip := getattr(first_diag, "raw_snippet", ""):
        snippet_lines = raw_snip.strip().splitlines()[:5]
        print(
            "  [Compiler Snippet]:\n    " + "\n    ".join(snippet_lines),
            file=sys.stderr,
        )

      # Track per-file error counts across build run and across iterations
      file_diag_counts: Dict[str, int] = collections.defaultdict(int)
      for d in diagnostics:
        f_path = getattr(d, "file_path", "")
        if f_path:
          file_diag_counts[f_path] += 1

      target_f = getattr(first_diag, "file_path", "")
      rel_target_file = (
          os.path.relpath(target_f, self.repo_path) if target_f else "")
      if target_f:
        self.file_error_counts[target_f] += 1

      # Repetition check -> escalate to Expert Agent
      if (error_summary == last_error_summary or
          self.file_error_counts.get(target_f, 0) >= 2):
        stuck_count += 1
      else:
        stuck_count = 0
      last_error_summary = error_summary

      # --- Anti-Loop: Revert bad edits after repeated failures ---
      if stuck_count in (3, 5) and rel_target_file and os.path.isfile(target_f):
        print(
            f"  [{self.name}] [Anti-Loop] Error '{error_summary}' repeated "
            f"{stuck_count} times on {rel_target_file}. Reverting local edits "
            f"in {rel_target_file} to clean baseline HEAD...",
            file=sys.stderr,
        )
        try:
          head_check = subprocess.run(
              ["git", "show", f"HEAD:{rel_target_file}"],
              cwd=self.repo_path,
              capture_output=True,
              text=True,
              check=False,
          )
          if head_check.returncode == 0 and "<<<<<<<" in head_check.stdout:
            print(
                f"  [{self.name}] [Anti-Loop GUARD] Cannot revert "
                f"{rel_target_file} to HEAD: HEAD contains raw conflict "
                "markers. Keeping current working file.",
                file=sys.stderr,
            )
          else:
            subprocess.run(
                ["git", "checkout", "HEAD", "--", rel_target_file],
                cwd=self.repo_path,
                capture_output=True,
                text=True,
                check=False,
            )
            self.on_patch_applied([target_f])
            revert_msg = (
                f"Reverted {rel_target_file} to clean baseline due to "
                f"repeated failed fix attempts ({error_summary}). Please "
                "re-investigate with an alternative approach.")
            history_records.append({
                "iteration": iteration,
                "file": rel_target_file,
                "error": revert_msg,
                "status": "REVERTED_TO_BASELINE",
            })
            self.session_changes.append(
                AgentChangeRecord(
                    phase=self.name,
                    iteration=iteration,
                    target_file=rel_target_file,
                    file_changes={
                        rel_target_file:
                            (f"# Reverted {rel_target_file} to clean "
                             "baseline HEAD")
                    },
                    error=(f"Repeated failure ({error_summary}); reverted to "
                           "baseline"),
                    applied_cleanly=True,
                ))
        except (OSError, subprocess.SubprocessError) as rev_err:
          print(
              f"  [{self.name}] Notice: Revert failed for "
              f"{rel_target_file}: {rev_err}",
              file=sys.stderr,
          )

      # --- Anti-Loop Circuit Breaker: Abort runaway build loops ---
      if stuck_count >= 8:
        print(
            f"\n[{self.name}] [CIRCUIT BREAKER] Aborting resolution loop: "
            f"exceeded maximum repetition limit ({stuck_count}) on "
            f"{rel_target_file or error_summary}. Halting runaway build.",
            file=sys.stderr,
        )
        break

      use_expert = stuck_count >= 2 or self.file_error_counts.get(target_f,
                                                                  0) >= 3

      # --- Step 1: Pre-Flight Strategic Review by Expert Agent ---
      expert_guidance = ""
      if self.reasoning_engine is not None:
        action_label = (f"Consulting Expert Agent (repetition: {stuck_count})"
                        if use_expert else
                        "Performing Pre-Flight architectural review")
        print(
            f"  [{self.name}] [TIER-2 ARCHITECT] {action_label} for "
            f"{target_f or self.name}...",
            file=sys.stderr,
        )
        working_diff = self.get_working_diff(max_chars=200000)

        # Collect modified files sorted by iteration number
        entries_by_iter: List[str] = []
        seen_files = set()
        for rec in sorted(
            self.session_changes,
            key=lambda r: (
                0 if getattr(r, "phase", "") in
                ("resolve_conflicts", "conflicts") else 1,
                r.iteration,
            ),
        ):
          if not rec.file_changes:
            continue
          files = list(rec.file_changes.keys())
          seen_files.update(files)
          files_str = ", ".join(f"`{f}`" for f in files)
          phase_lbl = (f"[{rec.phase}] " if getattr(rec, "phase", "") and
                       rec.phase != self.name else "")
          entries_by_iter.append(
              f"- {phase_lbl}Iteration {rec.iteration}: {files_str}")

        if entries_by_iter:
          files_list_str = "\n".join(entries_by_iter)
          full_trajectory = (
              f"=== Files Modified in Current Session "
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
          print(
              f"  [{self.name}] [TIER-2 ARCHITECT] Injected list of "
              f"{len(seen_files)} modified session files into expert prompt.",
              file=sys.stderr,
          )
        else:
          full_trajectory = ""
        raw_cmd_tail = (output + "\n" + (siso_out or ""))[-30000:]

        def _format_diag(d: Any) -> str:
          fp = getattr(d, "file_path", "")
          ln = getattr(d, "line_number", 1)
          msg = getattr(d, "error_message", str(d))
          return f"- {fp}:{ln} {msg}"

        all_diags_str = "\n".join(_format_diag(d) for d in diagnostics[:20])

        if hasattr(first_diag, "error_message"):
          notes_part = ("\n" + "\n".join(first_diag.notes)) if getattr(
              first_diag, "notes", None) else ""
          snippet_part = (f"\nSnippet:\n{first_diag.raw_snippet}") if getattr(
              first_diag, "raw_snippet", None) else ""
          diag_line = getattr(first_diag, "line_number", 1)
          diag_file = getattr(first_diag, "file_path", "")
          prefix = f"{diag_file}:{diag_line}: " if diag_file else ""
          diag_trace = getattr(first_diag, "diagnostic_trace", None) or (
              f"{prefix}{first_diag.error_message}{snippet_part}{notes_part}")
        else:
          diag_trace = str(first_diag)

        file_ctx = ""
        if hasattr(first_diag, "file_path") and os.path.isfile(
            first_diag.file_path):
          try:
            with open(
                first_diag.file_path, "r", encoding="utf-8",
                errors="replace") as f:
              lines = f.readlines()
            ln = getattr(first_diag, "line_number", 1) or 1
            s_l = max(1, ln - 30)
            e_l = min(len(lines), ln + 30)
            file_ctx = "".join(
                f"{s_l + i}: {l}" for i, l in enumerate(lines[s_l - 1:e_l]))
          except OSError:
            pass

        expert_investigation_history = ""
        for expert_round in range(1, 4):
          try:
            guidance_res = self.reasoning_engine.generate_expert_guidance(
                target=getattr(first_diag, "file_path", self.name),
                diagnostics=diag_trace,
                source_contexts=file_ctx,
                trajectory_history=full_trajectory,
                working_diff=working_diff,
                raw_log=raw_cmd_tail,
                all_diagnostics=all_diags_str,
                investigation_history=expert_investigation_history,
                mode="gn" if "gn" in self.name.lower() else
                ("sync" if "sync" in self.name.lower() else "compiler"),
                expert_model=getattr(self.reasoning_engine, "expert_model",
                                     "gemini-3.8-flash"),
            )
            expert_guidance = guidance_res.get("guidance", "")
            tool_cmds = extract_tool_commands(expert_guidance)
            if tool_cmds and expert_round < 3:
              batch_tool_res = []
              for t_cmd in tool_cmds:
                print(
                    f"  [{self.name}] [TIER-2 ARCHITECT] Tool requested: "
                    f"{t_cmd}",
                    file=sys.stderr,
                )
                t_out = execute_local_tool(
                    t_cmd,
                    self.repo_path,
                    session_changes=self.session_changes,
                )
                batch_tool_res.append(
                    f"Tool Call: `{t_cmd}`\nResult:\n```\n{t_out}\n```")
              expert_investigation_history += ("\n\n" +
                                               "\n\n".join(batch_tool_res))
              continue
            break
          except Exception as e:  # pylint: disable=broad-exception-caught
            print(
                f"  [{self.name}] Notice: Pre-flight expert guidance query: "
                f"{e}",
                file=sys.stderr,
            )
            break

        if expert_guidance:
          first_g_line = expert_guidance.splitlines()[0][:100]
          print(
              f"  [{self.name}] [TIER-2 ARCHITECT] Pre-Flight Plan:\n"
              f"  >>> {first_g_line}...",
              file=sys.stderr,
          )

      # --- Step 2: Workhorse Coding Agent Patch Generation (Gemini 3.7) ---
      patch, model_used, rel_target = self.resolve_diagnostic(
          diagnostic=first_diag,
          history_records=history_records,
          use_expert=use_expert,
          expert_guidance=expert_guidance,
      )

      # Check for multi-turn tool commands
      if extract_tool_commands(patch):
        patch, model_used = self.execute_investigation_tools(
            initial_patch=patch,
            diagnostic=first_diag,
        )

      if not patch:
        print(
            f"[{self.name}] [FAIL] Model returned empty patch.",
            file=sys.stderr,
        )
        continue

      print(
          f"[{self.name}] Applying AI patch using {model_used} to "
          f"{rel_target}...",
          file=sys.stderr,
      )
      modified_files = apply_patch_or_replacement(
          patch, self.repo_path, default_file=rel_target)
      if modified_files:
        mod_summary = ", ".join(
            os.path.relpath(f, self.repo_path) for f in modified_files)
        print(
            f"[{self.name}] [OK] Patch applied cleanly to: {mod_summary}",
            file=sys.stderr,
        )
        self.on_patch_applied(modified_files)
        if self.reasoning_engine is not None:
          pending_fix = {
              "error": error_summary,
              "patch": patch,
              "file": rel_target,
          }
        file_changes = extract_file_changes_from_patch(
            patch, self.repo_path, default_file=rel_target)
        new_record = AgentChangeRecord(
            phase=self.name,
            iteration=iteration,
            target_file=rel_target,
            file_changes=file_changes,
            error=None,
            command_output=None,
            applied_cleanly=True,
        )
        self.session_changes.append(new_record)
        pending_record = new_record
        history_records.append({
            "iteration": iteration,
            "file": rel_target,
            "error": error_summary,
            "status": "APPLIED",
        })
      else:
        print(
            f"[{self.name}] [FAIL] Could not apply patch to {rel_target}.\n"
            f"  [AI Patch Preview]:\n"
            f"  {patch[:300].strip()}",
            file=sys.stderr,
        )
        fail_record = AgentChangeRecord(
            phase=self.name,
            iteration=iteration,
            target_file=rel_target,
            file_changes={rel_target: patch},
            error=f"Patch failed to apply to {rel_target}",
            command_output=None,
            applied_cleanly=False,
        )
        self.session_changes.append(fail_record)
        pending_record = None
        history_records.append({
            "iteration": iteration,
            "file": rel_target,
            "error": (
                f"Patch failed to apply to {rel_target}. Ensure <<<<<<< SEARCH "
                "matches exact file lines and >>>>>>> REPLACE contains clean "
                "code without stray conflict markers."),
            "status": "FAILED_TO_APPLY",
        })

    print(
        f"[{self.name}] [FAIL] Exhausted maximum iterations "
        f"({self.max_iterations}).",
        file=sys.stderr,
    )
    return False
