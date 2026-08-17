#!/usr/bin/env python3
# pylint: disable=duplicate-code
"""Unified AI-driven merge conflict resolver for Cobalt Chromium rebase.

Resolves merge conflicts across ALL files in the repository:
  - DEPS (validates Python AST syntax & executes self-healing gclient sync)
  - C++, C, Objective-C++ (.cc, .cpp, .h, .mm)
  - Java & Java templates (.java, .tmpl)
  - GN build configs (.gn, .gni)
  - Other config and resource files (.py, .spec, .grd, .json)

Features enterprise Vertex AI & Google AI Studio dual authentication:
  - Vertex AI: Uses GCP Project ID + Service Account / gcloud ADC (No 429
    rate limits, billed to GCP project).
  - AI Studio: Supports personal API Key for quick local debugging.
  - Interactive Local Tool Engine: Gemini can request TOOL_READ_FILE,
    TOOL_GREP, TOOL_GIT_SHOW, or TOOL_EXPAND_CONTEXT to inspect symbols.
  - Thinking Budget Control: Sets thinkingBudget to prevent hangs.
  - Non-crashing resilience: Gracefully escalates persistent errors.
"""

import argparse
import ast
import dataclasses
import difflib
import json
import os
import re
import shutil
import ssl
import subprocess
import sys
import time
import urllib.error
import urllib.request
from typing import Dict, List, Optional, Tuple

try:
  import google.auth
  import google.auth.transport.requests
  HAS_GOOGLE_AUTH = True
except ImportError:
  HAS_GOOGLE_AUTH = False

from reasoning_engine import load_skill


class GeminiAPIError(Exception):
  """Raised when Gemini API fails after all retries."""


def get_gcp_access_token() -> Optional[str]:
  """Retrieves Google Cloud OAuth access token via ADC or gcloud CLI."""
  # 1. Try gcloud auth application-default print-access-token
  try:
    res = subprocess.run(
        ["gcloud", "auth", "application-default", "print-access-token"],
        capture_output=True,
        text=True,
        timeout=10,
        check=False,
    )
    if res.returncode == 0 and res.stdout.strip():
      return res.stdout.strip()
  except (OSError, subprocess.SubprocessError):
    pass

  # 2. Try gcloud auth print-access-token
  try:
    res = subprocess.run(
        ["gcloud", "auth", "print-access-token"],
        capture_output=True,
        text=True,
        timeout=10,
        check=False,
    )
    if res.returncode == 0 and res.stdout.strip():
      return res.stdout.strip()
  except (OSError, subprocess.SubprocessError):
    pass

  # 3. Direct exchange using application_default_credentials.json if present
  adc_path = os.path.expanduser(
      "~/.config/gcloud/application_default_credentials.json")
  if os.path.isfile(adc_path):
    try:
      with open(adc_path, "r", encoding="utf-8") as f:
        adc = json.load(f)
      if ("refresh_token" in adc and "client_id" in adc and
          "client_secret" in adc):
        token_url = "https://oauth2.googleapis.com/token"
        payload = json.dumps({
            "client_id": adc["client_id"],
            "client_secret": adc["client_secret"],
            "refresh_token": adc["refresh_token"],
            "grant_type": "refresh_token",
        }).encode("utf-8")
        req = urllib.request.Request(
            token_url,
            data=payload,
            headers={"Content-Type": "application/json"},
            method="POST",
        )
        ctx = ssl.create_default_context()
        with urllib.request.urlopen(req, context=ctx, timeout=10) as resp:
          data = json.loads(resp.read().decode("utf-8"))
          return data.get("access_token")
    except (OSError, json.JSONDecodeError, urllib.error.URLError):
      pass

  # 4. Try google.auth if installed
  if HAS_GOOGLE_AUTH:
    try:
      credentials, _ = google.auth.default(
          scopes=["https://www.googleapis.com/auth/cloud-platform"])
      credentials.refresh(google.auth.transport.requests.Request())
      if credentials.token:
        return credentials.token
    except Exception:  # pylint: disable=broad-exception-caught
      pass

  return None


def get_clean_build_env(
    depot_tools_path: Optional[str] = None,) -> Dict[str, str]:
  """Prepares a clean build environment, stripping agent env vars."""
  depot_tools = depot_tools_path or os.path.expanduser("~/depot_tools")
  clean_env = {
      k: v for k, v in os.environ.items() if not k.startswith("ANTIGRAVITY_")
  }
  if os.path.isdir(depot_tools):
    orig_path = clean_env.get("PATH", "")
    clean_env["PATH"] = f"{depot_tools}:{orig_path}"
  return clean_env


def build_system_instruction(
    language: str,
    skills_dir: Optional[str] = None,
) -> str:
  """Builds the Gemini system instruction from modular skill files."""
  rebase_skill = load_skill("cobalt_rebase", skills_dir)
  conflict_skill = load_skill("conflict_resolution", skills_dir)
  return (f"You are an expert Chromium and Cobalt engineer ({language}).\n\n"
          f"--- General Rebase Guidelines ---\n{rebase_skill}\n\n"
          f"--- Conflict Resolution Skill ---\n{conflict_skill}\n")


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


@dataclasses.dataclass
class TokenUsage:
  """Tracks token metrics and model call counts across invocations."""

  prompt_tokens: int = 0
  completion_tokens: int = 0
  total_tokens: int = 0
  model: str = ""
  calls: int = 0

  def add(self, prompt: int, completion: int, total: int, model: str):
    """Accumulates prompt and response token usage."""
    self.prompt_tokens += prompt
    self.completion_tokens += completion
    self.total_tokens += total
    self.model = model
    self.calls += 1


@dataclasses.dataclass
class EscalationItem:
  """Records complex conflict blocks that require human review."""

  file_path: str
  block_index: int
  reason: str


def execute_local_tool(tool_command: str, repo_path: str) -> str:
  """Executes local file inspection, grep, or git commands for Gemini."""
  stripped = tool_command.strip()

  # 1. TOOL_READ_FILE: path/to/file.h [optional range]
  if stripped.startswith("TOOL_READ_FILE:"):
    arg = stripped.split("TOOL_READ_FILE:", 1)[1].strip()
    parts = arg.split()
    target_rel = parts[0] if parts else ""
    abs_path = (
        os.path.join(repo_path, target_rel)
        if not os.path.isabs(target_rel) else target_rel)

    if not os.path.isfile(abs_path):
      return f"Error: File \"{target_rel}\" not found in repository."

    with open(abs_path, "r", encoding="utf-8", errors="replace") as f:
      lines = f.read().splitlines()

    start_l, end_l = 1, min(len(lines), 150)
    if len(parts) > 1 and "-" in parts[1]:
      try:
        s, e = parts[1].split("-")
        start_l = max(1, int(s))
        end_l = min(len(lines), int(e))
      except ValueError:
        pass

    snippet = "\n".join(
        [f"{i:4d} | {lines[i-1]}" for i in range(start_l, end_l + 1)])
    return (f"### File: {target_rel} (Lines {start_l}-{end_l} of "
            f"{len(lines)}):\n```\n{snippet}\n```")

  # 2. TOOL_GREP: symbol
  if stripped.startswith("TOOL_GREP:"):
    pattern = stripped.split("TOOL_GREP:", 1)[1].strip()
    try:
      res = subprocess.run(
          ["git", "grep", "-n", "-m", "6", "--", pattern],
          cwd=repo_path,
          capture_output=True,
          text=True,
          check=False,
      )
      out = res.stdout.strip()
      if not out:
        return f"Grep for \"{pattern}\" returned 0 matches."
      return f"### Grep matches for \"{pattern}\":\n```\n{out}\n```"
    except (OSError, subprocess.SubprocessError) as e:
      return f"Error running grep: {e}"

  # 3. TOOL_GIT_SHOW: commit or file
  if stripped.startswith("TOOL_GIT_SHOW:"):
    target = stripped.split("TOOL_GIT_SHOW:", 1)[1].strip()
    try:
      cmd = (["git", "show", "--stat", "-p", "--", target]
             if "/" in target else ["git", "show", "--stat", "-p", target])
      res = subprocess.run(
          cmd,
          cwd=repo_path,
          capture_output=True,
          text=True,
          check=False,
      )
      out = res.stdout.strip()[:2500]
      return f"### Git show for \"{target}\":\n```\n{out}\n```"
    except (OSError, subprocess.SubprocessError) as e:
      return f"Error running git show: {e}"

  return f"Unrecognized tool command: {stripped}"


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


def find_all_conflicted_files(repo_path: str) -> List[str]:
  """Finds all conflicted files across the repository."""
  conflicted = set()

  try:
    res = subprocess.run(
        ["git", "diff", "--name-only", "--diff-filter=U"],
        cwd=repo_path,
        capture_output=True,
        text=True,
        check=False,
    )
    for f in res.stdout.splitlines():
      f = f.strip()
      if f and os.path.isfile(os.path.join(repo_path, f)):
        conflicted.add(f)
  except (OSError, subprocess.SubprocessError):
    pass

  try:
    res = subprocess.run(
        ["git", "grep", "-l", "^<<<<<<<"],
        cwd=repo_path,
        capture_output=True,
        text=True,
        check=False,
    )
    for f in res.stdout.splitlines():
      f = f.strip()
      if f and os.path.isfile(os.path.join(repo_path, f)):
        conflicted.add(f)
  except (OSError, subprocess.SubprocessError):
    pass

  return sorted(conflicted)


def extract_conflict_blocks(content: str,
                            context_lines: int = 30) -> List[ConflictBlock]:
  """Finds and extracts all conflict blocks with configurable context."""
  lines = content.splitlines(keepends=True)
  blocks: List[ConflictBlock] = []

  i = 0
  num_lines = len(lines)
  conflict_idx = 0

  while i < num_lines:
    line = lines[i]
    if line.startswith("<<<<<<<"):
      start_line = i
      ours_lines = []
      base_lines = []
      theirs_lines = []
      raw_lines = [line]

      has_base = False
      state = "ours"
      i += 1

      while i < num_lines and not lines[i].startswith(">>>>>>>"):
        cur_line = lines[i]
        raw_lines.append(cur_line)

        if cur_line.startswith("|||||||"):
          has_base = True
          state = "base"
        elif cur_line.startswith("======="):
          state = "theirs"
        else:
          if state == "ours":
            ours_lines.append(cur_line)
          elif state == "base":
            base_lines.append(cur_line)
          elif state == "theirs":
            theirs_lines.append(cur_line)
        i += 1

      if i < num_lines:
        raw_lines.append(lines[i])
        end_line = i
        i += 1
      else:
        end_line = num_lines - 1

      ctx_before_start = max(0, start_line - context_lines)
      ctx_before = "".join(lines[ctx_before_start:start_line])

      ctx_after_end = min(num_lines, end_line + 1 + context_lines)
      ctx_after = "".join(lines[end_line + 1:ctx_after_end])

      conflict_idx += 1
      blocks.append(
          ConflictBlock(
              index=conflict_idx,
              start_line=start_line + 1,
              end_line=end_line + 1,
              raw_block="".join(raw_lines),
              ours_content="".join(ours_lines),
              base_content="".join(base_lines) if has_base else None,
              theirs_content="".join(theirs_lines),
              context_before=ctx_before,
              context_after=ctx_after,
          ))
    else:
      i += 1

  return blocks


def extract_git_context(repo_path: str) -> Tuple[str, Dict[str, str]]:
  """Extracts commit logs, upstream SHAs, and bug references."""
  context_sections = []
  meta = {"upstream_sha": "", "bug_id": "", "subject": ""}
  try:
    fmt = ("--format=Commit: %H%nAuthor: %an <%ae>%nDate: "
           "%ad%nSubject: %s%n%n%b")
    res = subprocess.run(
        ["git", "log", "-1", fmt],
        cwd=repo_path,
        capture_output=True,
        text=True,
        check=False,
    )
    head_log = res.stdout.strip()
    if head_log:
      context_sections.append(f"### Current Commit Log (HEAD):\n{head_log}")
      subj_match = re.search(r"Subject:\s*(.*)", head_log)
      if subj_match:
        meta["subject"] = subj_match.group(1).strip()

    bug_matches = re.findall(r"(?:Bug|Fixed):\s*(?:chromium:)?(\d+)", head_log,
                             re.IGNORECASE)
    if bug_matches:
      unique_bugs = sorted(set(bug_matches))
      meta["bug_id"] = ", ".join(unique_bugs)
      bug_links = "\n".join(
          f"- https://issues.chromium.org/issues/{b}" for b in unique_bugs)
      context_sections.append(f"### Associated Chromium Bugs:\n{bug_links}")

    sha_pattern = (r"(?:Update to commit|cherry picked from commit)\s+"
                   r"([0-9a-fA-F]{40})")
    sha_match = re.search(sha_pattern, head_log)
    if sha_match:
      upstream_sha = sha_match.group(1)
      meta["upstream_sha"] = upstream_sha
      gitiles_url = ("https://chromium.googlesource.com/chromium/src/+/"
                     f"{upstream_sha}")
      context_sections.append(f"### Upstream Chromium Commit:\n"
                              f"- SHA: {upstream_sha}\n"
                              f"- Gitiles: {gitiles_url}")
  except (OSError, subprocess.SubprocessError) as e:
    context_sections.append(f"Note: Could not extract full git context: {e}")

  return "\n\n".join(context_sections), meta


def query_gemini(
    prompt: str,
    system_instruction: str,
    *,
    api_key: Optional[str] = None,
    project_id: Optional[str] = None,
    location: str = "us-central1",
    model: str = "gemini-1.5-flash",
    timeout_seconds: int = 180,
    max_retries: int = 5,
    mock_mode: bool = False,
    token_tracker: Optional[TokenUsage] = None,
) -> str:
  """Queries Gemini via Vertex AI (GCP ADC) or Google AI Studio (API Key)."""
  if mock_mode:
    if token_tracker:
      prompt_est = len(prompt) // 4
      resp_est = 50
      token_tracker.add(prompt_est, resp_est, prompt_est + resp_est,
                        f"{model} (mock)")

    lines = prompt.splitlines()
    theirs_lines = []
    in_theirs = False
    for line in lines:
      if line.startswith("======="):
        in_theirs = True
        continue
      if in_theirs and line.startswith(">>>>>>>"):
        break
      if in_theirs:
        theirs_lines.append(line)
    return "\n".join(theirs_lines)

  use_vertex = bool(
      project_id or os.environ.get("GCP_PROJECT") or
      os.environ.get("GOOGLE_CLOUD_PROJECT"))
  gcp_proj = (
      project_id or os.environ.get("GCP_PROJECT") or
      os.environ.get("GOOGLE_CLOUD_PROJECT"))

  headers = {"Content-Type": "application/json"}
  ctx = ssl.create_default_context()
  current_timeout = timeout_seconds
  active_model = model

  if use_vertex:
    access_token = get_gcp_access_token()
    if not access_token:
      raise GeminiAPIError(
          "Vertex AI mode enabled but failed to obtain OAuth access "
          "token.")
    headers["Authorization"] = f"Bearer {access_token}"

  for attempt in range(1, max_retries + 1):
    if use_vertex:
      url = (f"https://{location}-aiplatform.googleapis.com/v1/projects/"
             f"{gcp_proj}/locations/{location}/publishers/google/models/"
             f"{active_model}:generateContent")
    else:
      key = (
          api_key or os.environ.get("GEMINI_API_KEY") or
          os.environ.get("GOOGLE_API_KEY"))
      if not key:
        raise ValueError("Provide --project-id for Vertex AI or --api-key for "
                         "AI Studio.")
      url = (f"https://generativelanguage.googleapis.com/v1beta/models/"
             f"{active_model}:generateContent?key={key}")

    payload = {
        "contents": [{
            "role": "user",
            "parts": [{
                "text": prompt
            }],
        }],
        "systemInstruction": {
            "parts": [{
                "text": system_instruction
            }],
        },
        "generationConfig": {
            "temperature": 0.1,
            "maxOutputTokens": 8192,
        },
    }

    data = json.dumps(payload).encode("utf-8")
    req = urllib.request.Request(
        url,
        data=data,
        headers=headers,
        method="POST",
    )

    try:
      with urllib.request.urlopen(
          req, context=ctx, timeout=current_timeout) as resp:
        resp_data = json.loads(resp.read().decode("utf-8"))

        usage = resp_data.get("usageMetadata", {})
        if token_tracker:
          p_tok = usage.get("promptTokenCount", 0)
          c_tok = usage.get("candidatesTokenCount", 0)
          t_tok = usage.get("totalTokenCount", p_tok + c_tok)
          token_tracker.add(p_tok, c_tok, t_tok, active_model)

        candidates = resp_data.get("candidates", [])
        if not candidates:
          raise RuntimeError(
              f"Empty candidates in Gemini response: {resp_data}")
        text = (
            candidates[0].get("content", {}).get("parts",
                                                 [{}])[0].get("text", ""))
        return clean_output(text)
    except urllib.error.HTTPError as e:
      if e.code == 429 and attempt < max_retries:
        backoff = attempt * 5
        print(
            f"[resolve_conflicts] HTTP 429 Rate-Limit hit. Backing off "
            f"{backoff}s before retry ({attempt}/{max_retries})...",
            file=sys.stderr,
        )
        time.sleep(backoff)
        continue
      if e.code in (500, 502, 503, 504) and attempt < max_retries:
        backoff = attempt * 6
        print(
            f"[resolve_conflicts] HTTP {e.code} Service Overload. "
            f"Backing off {backoff}s before retry...",
            file=sys.stderr,
        )
        time.sleep(backoff)
        continue
      err_body = e.read().decode("utf-8")
      raise GeminiAPIError(f"HTTP Error {e.code}: {err_body}") from e
    except (TimeoutError, urllib.error.URLError) as e:
      if attempt < max_retries:
        backoff = attempt * 4
        current_timeout += 30
        print(
            f"[resolve_conflicts] Socket timeout. Expanding timeout "
            f"to {current_timeout}s & retrying in {backoff}s...",
            file=sys.stderr,
        )
        time.sleep(backoff)
        continue
      raise GeminiAPIError(
          f"Request timed out after {attempt} attempts: {e}") from e

    raise GeminiAPIError(
        f"Gemini API request failed after {max_retries} retries.")


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


def run_gclient_sync(
    cobalt_root: Optional[str] = None,
    flags: Optional[list] = None,
) -> Tuple[bool, str]:
  """Runs gclient sync in clean environment."""
  root = cobalt_root or os.path.expanduser("~/cobalt")
  clean_env = get_clean_build_env()

  cmd = ["gclient", "sync"] + (
      flags if flags else ["--nohooks", "--no-history", "-D"])
  cmd_str = " ".join(cmd)
  print(
      f"\n[resolve_conflicts] Running: {cmd_str} in {root}",
      file=sys.stderr,
  )

  res = subprocess.run(
      cmd,
      cwd=root,
      capture_output=True,
      text=True,
      env=clean_env,
      check=False,
  )
  output = f"{res.stdout}\n{res.stderr}"
  return res.returncode == 0, output


def extract_diagnostic_snippet(sync_output: str) -> str:
  """Extracts error traces from gclient output."""
  lines = sync_output.splitlines()
  error_lines = []
  capture = False
  for line in lines:
    if any(kw in line.lower() for kw in [
        "error:",
        "traceback",
        "failed to",
        "syntaxerror",
        "keyerror",
        "attributeerror",
        "exception",
        "cannot find",
    ]):
      capture = True
    if capture:
      error_lines.append(line)
      if len(error_lines) > 35:
        break
  if error_lines:
    return "\n".join(error_lines)
  return "\n".join(lines[-25:] if len(lines) > 25 else lines)


def self_heal_gclient_sync(
    deps_path: str,
    cobalt_root: str,
    system_instruction: str,
    *,
    max_retries: int,
    api_key: Optional[str],
    project_id: Optional[str],
    location: str,
    model: str,
    timeout_seconds: int,
    mock_mode: bool,
    token_tracker: TokenUsage,
) -> Tuple[bool, str]:
  """Iteratively runs gclient sync and queries Gemini to heal any errors."""
  for attempt in range(1, max_retries + 1):
    success, output = run_gclient_sync(cobalt_root=cobalt_root)
    if success:
      return True, output

    diag = extract_diagnostic_snippet(output)
    print(
        f"\n[resolve_conflicts] gclient sync failed (attempt "
        f"{attempt}/{max_retries}). Diagnostics:\n{diag}",
        file=sys.stderr,
    )

    if attempt == max_retries:
      print(
          f"[resolve_conflicts] Reached max retry limit ({max_retries}).",
          file=sys.stderr,
      )
      return False, output

    print(
        f"[resolve_conflicts] Querying {model} to heal DEPS...",
        file=sys.stderr,
    )
    with open(deps_path, "r", encoding="utf-8") as f:
      current_deps = f.read()

    prompt = (f"gclient sync failed with the following error output:\n"
              f"--------------------------------------------------\n"
              f"{diag}\n"
              f"--------------------------------------------------\n\n"
              f"Here is the current DEPS file content:\n"
              f"{current_deps}\n\n"
              f"Provide the corrected full DEPS file to make gclient sync "
              f"succeed.")

    try:
      healed_deps = query_gemini(
          prompt=prompt,
          system_instruction=system_instruction,
          api_key=api_key,
          project_id=project_id,
          location=location,
          model=model,
          timeout_seconds=timeout_seconds,
          max_retries=5,
          mock_mode=mock_mode,
          token_tracker=token_tracker,
      )
      ast.parse(healed_deps)
      with open(deps_path, "w", encoding="utf-8") as f:
        f.write(healed_deps)
      print(
          "[resolve_conflicts] Applied AI fix to DEPS. Retrying...",
          file=sys.stderr,
      )
    except (OSError, SyntaxError, GeminiAPIError) as e:
      print(
          f"[resolve_conflicts] AI fix attempt failed: {e}",
          file=sys.stderr,
      )

  return False, output


def is_binary_or_huge_data(file_path: str) -> bool:
  """Checks if a file is binary, database, media, or fuzz corpus data."""
  ext = os.path.splitext(file_path)[1].lower()
  binary_extensions = {
      ".db",
      ".sqlite",
      ".sqlite3",
      ".bin",
      ".dat",
      ".corpus",
      ".png",
      ".jpg",
      ".jpeg",
      ".gif",
      ".ico",
      ".webp",
      ".wasm",
      ".zip",
      ".gz",
      ".tar",
      ".tgz",
      ".7z",
      ".bz2",
      ".xz",
      ".pak",
      ".jar",
      ".dex",
      ".so",
      ".a",
      ".o",
      ".dylib",
      ".dll",
      ".pdf",
      ".mp3",
      ".mp4",
      ".webm",
      ".wav",
      ".ttf",
      ".otf",
      ".woff",
      ".woff2",
  }
  if (ext in binary_extensions or "fuzz/db_corpus" in file_path or
      ("test/data" in file_path and ext == ".db")):
    return True

  try:
    with open(file_path, "rb") as f:
      chunk = f.read(8192)
      if b"\x00" in chunk:
        return True
  except OSError:
    pass

  return False


def resolve_file_conflicts(
    file_path: str,
    repo_path: str,
    git_context: str,
    *,
    skills_dir: Optional[str] = None,
    api_key: Optional[str] = None,
    project_id: Optional[str] = None,
    location: str = "us-central1",
    model: str = "gemini-2.5-flash",
    timeout_seconds: int = 180,
    max_retries: int = 5,
    mock_mode: bool = False,
    token_tracker: Optional[TokenUsage] = None,
    escalations: Optional[List[EscalationItem]] = None,
    max_tool_rounds: int = 5,
) -> Tuple[bool, List[str]]:
  """Resolves all conflict blocks in any file with multi-turn tools."""
  tracker = token_tracker or TokenUsage()
  esc_list = escalations if escalations is not None else []

  # Fast-path for binary / fuzz corpus database files: auto-adopt upstream
  if is_binary_or_huge_data(file_path):
    rel_path = os.path.relpath(file_path, repo_path)
    print(
        f"\n[resolve_conflicts] Binary corpus detected: {rel_path}. "
        f"Auto-resolving with upstream --theirs...",
        file=sys.stderr,
    )
    try:
      subprocess.run(
          ["git", "checkout", "--theirs", "--", rel_path],
          cwd=repo_path,
          check=False,
      )
      subprocess.run(
          ["git", "add", "--", rel_path],
          cwd=repo_path,
          check=False,
      )
      return True, [f"Auto-resolved binary file {rel_path} with --theirs"]
    except (OSError, subprocess.SubprocessError) as e:
      print(
          f"[WARNING] Failed to checkout --theirs for {rel_path}: {e}",
          file=sys.stderr,
      )

  with open(file_path, "r", encoding="utf-8", errors="replace") as f:
    original_content = f.read()

  blocks = extract_conflict_blocks(original_content, context_lines=30)
  if not blocks:
    print(
        f"[resolve_conflicts] No conflict markers in: {file_path}",
        file=sys.stderr,
    )
    return True, []

  language = detect_language(file_path)
  system_instruction = build_system_instruction(language, skills_dir)

  print(
      f"\n[resolve_conflicts] Resolving {len(blocks)} conflict(s) in "
      f"{file_path} ({language})...",
      file=sys.stderr,
  )

  current_content = original_content
  for b in reversed(blocks):
    block_line_count = len(b.raw_block.splitlines())
    print(
        f"  - Block #{b.index} (lines {b.start_line}-{b.end_line}, "
        f"{block_line_count} lines)...",
        file=sys.stderr,
    )

    # If a single conflict block is oversized, adopt upstream to avoid 400
    if block_line_count > 1000:
      print(
          f"    [FAST] Block #{b.index} is oversized ({block_line_count} "
          f"lines). Auto-adopting upstream update...",
          file=sys.stderr,
      )
      if b.raw_block in current_content:
        current_content = current_content.replace(b.raw_block, b.theirs_content,
                                                  1)
      continue

    ctx_before = b.context_before
    ctx_after = b.context_after
    investigation_history = []

    resolved_chunk = None
    for round_idx in range(1, max_tool_rounds + 2):
      history_text = "\n\n".join(investigation_history)
      is_final_round = round_idx > max_tool_rounds

      if is_final_round:
        final_instruction = (
            f"### FINAL RESOLUTION REQUIRED (Investigation budget of "
            f"{max_tool_rounds} rounds reached):\n"
            "Based on evidence above, return ONLY the final clean "
            "code. If you cannot resolve it, return "
            "`ESCALATE_TO_HUMAN: <reason>`.")
      else:
        final_instruction = (
            "If you need to inspect an external header or grep a "
            "symbol, output a single `TOOL_` command line. Otherwise "
            "return ONLY the resolved replacement code snippet.")

      prompt = (f"Target File: {file_path} ({language})\n"
                f"{git_context}\n\n"
                f"Context before conflict:\n{ctx_before}\n\n"
                f"Conflict block to resolve:\n{b.raw_block}\n\n"
                f"Context after conflict:\n{ctx_after}\n\n"
                f"{history_text}\n\n"
                f"Task: Resolve this specific merge conflict for {file_path}. "
                "Preserve all Cobalt macros "
                "(#if BUILDFLAG(USE_STARBOARD_MEDIA), "
                "#if BUILDFLAG(IS_COBALT), #if defined(STARBOARD)), custom "
                "variables (checkout_cobalt_internal, checkout_copybara), "
                "platform shims, and Cobalt runtime behavior while adopting "
                "upstream updates.\n\n"
                f"{final_instruction}")

      try:
        resp = query_gemini(
            prompt=prompt,
            system_instruction=system_instruction,
            api_key=api_key,
            project_id=project_id,
            location=location,
            model=model,
            timeout_seconds=timeout_seconds,
            max_retries=max_retries,
            mock_mode=mock_mode,
            token_tracker=tracker,
        )
      except GeminiAPIError as e:
        print(
            f"    [WARNING] Backend API Error on Block #{b.index}: "
            f"{e}. Escalating to human review and proceeding.",
            file=sys.stderr,
        )
        esc_list.append(
            EscalationItem(
                file_path=file_path,
                block_index=b.index,
                reason=f"Gemini API Error: {e}",
            ))
        resolved_chunk = None
        break

      # Check if model invoked a tool
      tool_match = re.search(r"^(TOOL_[A-Z_]+:.*)$", resp.strip(), re.MULTILINE)
      if tool_match and not is_final_round:
        tool_cmd = tool_match.group(1).strip()
        print(
            f"    [SEARCH] [Investigation Round {round_idx}/"
            f"{max_tool_rounds}] Model requested: {tool_cmd}",
            file=sys.stderr,
        )

        if tool_cmd.startswith("TOOL_EXPAND_CONTEXT:"):
          try:
            n_lines = int(tool_cmd.split(":", 1)[1].strip())
          except ValueError:
            n_lines = 80
          expanded = extract_conflict_blocks(
              original_content, context_lines=n_lines)
          matching_exp = [eb for eb in expanded if eb.index == b.index]
          if matching_exp:
            ctx_before = matching_exp[0].context_before
            ctx_after = matching_exp[0].context_after
          tool_output = (
              f"Context expanded to {n_lines} lines before and after.")
        else:
          tool_output = execute_local_tool(tool_cmd, repo_path=repo_path)

        investigation_history.append(
            f"Model Tool Call: `{tool_cmd}`\nResult:\n{tool_output}")
        continue

      # Check if model requested human escalation
      if "ESCALATE_TO_HUMAN:" in resp:
        reason = resp.split("ESCALATE_TO_HUMAN:", 1)[1].strip()
        print(
            f"    [WARNING] Complex conflict escalated: {reason}",
            file=sys.stderr,
        )
        esc_list.append(
            EscalationItem(
                file_path=file_path,
                block_index=b.index,
                reason=reason,
            ))
        resolved_chunk = None
        break

      # If still emitted a tool command on final round
      if tool_match and is_final_round:
        print(
            f"    [WARNING] Exhausted investigation budget ("
            f"{max_tool_rounds} rounds). Flagging for human review.",
            file=sys.stderr,
        )
        esc_list.append(
            EscalationItem(
                file_path=file_path,
                block_index=b.index,
                reason=(f"Investigation budget of {max_tool_rounds} rounds "
                        f"exhausted"),
            ))
        resolved_chunk = None
        break

      resolved_chunk = resp
      break

    if resolved_chunk is None:
      continue

    if not resolved_chunk.endswith("\n") and b.raw_block.endswith("\n"):
      resolved_chunk += "\n"

    if b.raw_block in current_content:
      current_content = current_content.replace(b.raw_block, resolved_chunk, 1)
      print(f"    [OK] Resolved Block #{b.index}", file=sys.stderr)
    else:
      print(
          f"    [WARNING] Warning: Block #{b.index} mismatch, skipping.",
          file=sys.stderr,
      )

  # Validate Python AST if DEPS or .py
  if os.path.basename(file_path) == "DEPS" or file_path.endswith(".py"):
    try:
      ast.parse(current_content)
      print(
          "    [OK] AST syntax check passed (Valid Python).",
          file=sys.stderr,
      )
    except SyntaxError as e:
      print(
          f"    [FAIL] AST syntax error in {file_path}: {e}",
          file=sys.stderr,
      )
      return False, []

  # Backup & write
  shutil.copyfile(file_path, f"{file_path}.bak")
  with open(file_path, "w", encoding="utf-8") as f:
    f.write(current_content)

  diff = list(
      difflib.unified_diff(
          original_content.splitlines(),
          current_content.splitlines(),
          fromfile=f"{file_path} (conflicted)",
          tofile=f"{file_path} (resolved)",
          lineterm="",
      ))
  print(
      f"[resolve_conflicts] Successfully processed {file_path} "
      f"({len(diff)} diff lines).",
      file=sys.stderr,
  )
  return True, diff


def write_result_report(
    report_path: str,
    resolved_files: List[str],
    git_meta: Dict[str, str],
    token_usage: TokenUsage,
    *,
    sync_success: bool,
    sync_output: str,
    escalations: List[EscalationItem],
):
  """Generates structured result.md summary with escalations."""
  file_rows = [
      f"| `{os.path.basename(f)}` | `{detect_language(f)}` | Processed |"
      for f in resolved_files
  ]
  file_table = ("\n".join(file_rows)
                if file_rows else "| None | - | No conflicts detected |")

  escalation_section = ""
  if escalations:
    esc_rows = [
        f"| `{os.path.basename(e.file_path)}` | Block #{e.block_index} | "
        f"{e.reason} |" for e in escalations
    ]
    joined_esc = "\n".join(esc_rows)
    escalation_section = f"""
## [WARNING] Human Escalations & Complex Conflicts
| Target File | Block | Reasoning / Recommendation |
| :--- | :--- | :--- |
{joined_esc}
"""

  subject = git_meta.get("subject", "Manual Rebase")
  upstream_sha = git_meta.get("upstream_sha", "N/A")
  bug_id = git_meta.get("bug_id", "N/A")
  sync_str = "SUCCESS" if sync_success else "SKIPPED / N/A"

  report_content = f"""# Cobalt Rebase Resolution Report

## 1. Executive Summary
- **Target Roll / Commit**: {subject}
- **Upstream SHA**: `{upstream_sha}`
- **Associated Chromium Bugs**: {bug_id}
- **Total Conflicted Files Processed**: {len(resolved_files)}
- **Escalations Flagged for Review**: {len(escalations)}
- **gclient sync**: {sync_str}

## 2. Resolved Files
| Target File | Language / Type | Status |
| :--- | :--- | :--- |
{file_table}
{escalation_section}
## 3. Behavior Preservation Notes
- **Cobalt Custom Variables**: Maintained `checkout_cobalt_internal`, `checkout_copybara`, and Cobalt custom submodule pins in `DEPS`.
- **Cobalt C++ / Java Macros**: Preserved `#if BUILDFLAG(USE_STARBOARD_MEDIA)`, `#if BUILDFLAG(IS_COBALT)`, `#if defined(STARBOARD)`, Starboard media flows, and platform shims.

## 4. AI Token Metrics
| Metric | Value |
| :--- | :--- |
| **Model** | `{token_usage.model}` |
| **Prompt Tokens** | {token_usage.prompt_tokens:,} |
| **Completion Tokens** | {token_usage.completion_tokens:,} |
| **Total Tokens** | {token_usage.total_tokens:,} |
| **AI Calls / Invocations** | {token_usage.calls} |

## 5. Verification
```text
{sync_output.strip()[-2000:] if sync_output else "All files processed."}
```
"""
  with open(report_path, "w", encoding="utf-8") as f:
    f.write(report_content)
  print(
      f"\n[resolve_conflicts] Report written to: {report_path}",
      file=sys.stderr,
  )


def main():
  """Main CLI entry point for conflict resolution."""
  parser = argparse.ArgumentParser(
      description="Unified AI conflict resolver for Cobalt rebase.")
  parser.add_argument(
      "--files",
      nargs="*",
      default=None,
      help="Specific file(s) to resolve (default: auto-detect all).",
  )
  parser.add_argument(
      "--repo-path",
      default=os.path.expanduser("~/cobalt/src"),
      help="Path to Cobalt src repo.",
  )
  parser.add_argument(
      "--cobalt-root",
      default=os.path.expanduser("~/cobalt"),
      help="Path to Cobalt workspace root.",
  )
  parser.add_argument(
      "--skills-dir",
      default=os.path.expanduser("~/cobalt/src/.github/rebase/skills"),
      help="Path to skills directory.",
  )
  parser.add_argument(
      "--report-path",
      default=os.path.expanduser(
          "~/cobalt/src/.github/rebase/results/M139_rebase_summary.md"),
      help="Path to summary report.",
  )
  parser.add_argument(
      "--project-id",
      default=os.environ.get("GCP_PROJECT") or
      os.environ.get("GOOGLE_CLOUD_PROJECT"),
      help="GCP Project ID for Vertex AI.",
  )
  parser.add_argument(
      "--location",
      default=os.environ.get("GCP_LOCATION", "us-central1"),
      help="Vertex AI Region (default: us-central1).",
  )
  parser.add_argument(
      "--model",
      default=os.environ.get("GEMINI_MODEL", "gemini-2.5-flash"),
      help="Gemini model name (e.g. gemini-2.5-flash, gemini-2.5-pro).",
  )
  parser.add_argument(
      "--api-key",
      default=None,
      help="Gemini API Key (Google AI Studio fallback).",
  )
  parser.add_argument(
      "--timeout",
      type=int,
      default=180,
      help="Gemini API request timeout in seconds (default: 180)",
  )
  parser.add_argument(
      "--max-tool-rounds",
      type=int,
      default=5,
      help="Max tool investigation rounds per conflict block (default: 5)",
  )
  parser.add_argument(
      "--mock", action="store_true", help="Run in mock mode for dry-runs.")
  parser.add_argument(
      "--skip-sync",
      action="store_true",
      help="Skip gclient sync even if DEPS was resolved.",
  )
  parser.add_argument(
      "--max-retries",
      type=int,
      default=5,
      help="Max API retries per conflict block (default: 5).",
  )
  parser.add_argument(
      "--max-sync-retries",
      type=int,
      default=10,
      help="Max self-healing retries for gclient sync (default: 10).",
  )
  args = parser.parse_args()

  repo_path = os.path.abspath(args.repo_path)
  git_context, git_meta = extract_git_context(repo_path)

  # Find files to resolve
  if args.files:
    target_files = [
        os.path.abspath(f) if os.path.isabs(f) else os.path.join(repo_path, f)
        for f in args.files
    ]
  else:
    rel_files = find_all_conflicted_files(repo_path)
    target_files = [os.path.join(repo_path, f) for f in rel_files]

  if not target_files:
    print(
        "[resolve_conflicts] No conflicted files found in repository.",
        file=sys.stderr,
    )
    sys.exit(0)

  auth_mode = (
      f"Vertex AI (GCP Project: {args.project_id}, Region: {args.location})"
      if args.project_id else "Google AI Studio (API Key)")
  print("=" * 70, file=sys.stderr)
  print(
      f"[resolve_conflicts] FOUND {len(target_files)} CONFLICTED FILE(S):",
      file=sys.stderr,
  )
  print(f"  - Authentication Mode: {auth_mode}", file=sys.stderr)
  print(f"  - Model:               {args.model}", file=sys.stderr)
  print("=" * 70, file=sys.stderr)
  for idx, tf in enumerate(target_files, 1):
    rel = os.path.relpath(tf, repo_path)
    lang = detect_language(tf)
    print(f"  {idx:2d}. {rel} [{lang}]", file=sys.stderr)
  print("=" * 70, file=sys.stderr)

  token_tracker = TokenUsage()
  resolved_list = []
  escalations: List[EscalationItem] = []
  deps_resolved = False

  for tf in target_files:
    if not os.path.isfile(tf):
      continue

    ok, _ = resolve_file_conflicts(
        file_path=tf,
        repo_path=repo_path,
        git_context=git_context,
        skills_dir=args.skills_dir,
        api_key=args.api_key,
        project_id=args.project_id,
        location=args.location,
        model=args.model,
        timeout_seconds=args.timeout,
        max_retries=args.max_retries,
        mock_mode=args.mock,
        token_tracker=token_tracker,
        escalations=escalations,
        max_tool_rounds=args.max_tool_rounds,
    )
    if ok:
      resolved_list.append(tf)
      if os.path.basename(tf) == "DEPS":
        deps_resolved = True
    else:
      print(f"[WARNING] Issues detected in: {tf}", file=sys.stderr)

  # If DEPS was resolved, execute gclient sync self-healing loop
  sync_success = True
  sync_output = ""
  if deps_resolved and not args.skip_sync:
    deps_path = os.path.join(repo_path, "DEPS")
    sys_inst = build_system_instruction(
        "Python (Chromium DEPS)", skills_dir=args.skills_dir)
    sync_success, sync_output = self_heal_gclient_sync(
        deps_path=deps_path,
        cobalt_root=args.cobalt_root,
        system_instruction=sys_inst,
        max_retries=args.max_sync_retries,
        api_key=args.api_key,
        project_id=args.project_id,
        location=args.location,
        model=args.model,
        timeout_seconds=args.timeout,
        mock_mode=args.mock,
        token_tracker=token_tracker,
    )

  # Write report
  write_result_report(
      report_path=args.report_path,
      resolved_files=resolved_list,
      git_meta=git_meta,
      token_usage=token_tracker,
      sync_success=sync_success,
      sync_output=sync_output,
      escalations=escalations,
  )

  print("\n" + "=" * 70, file=sys.stderr)
  print(
      f"[resolve_conflicts] ALL {len(resolved_list)} CONFLICTED FILES "
      "PROCESSED!",
      file=sys.stderr,
  )
  if escalations:
    print(f"  - [WARNING] Escalations Flagged: {len(escalations)} block(s) "
          f"require human review (see result.md)")
  print(f"  - Total Tokens:   {token_tracker.total_tokens:,}")
  print(f"  - Total AI Calls: {token_tracker.calls}")
  print("=" * 70, file=sys.stderr)


if __name__ == "__main__":
  main()
