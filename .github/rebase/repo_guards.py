"""Repository path resolution and patch-target guards.

Keeps model-supplied paths inside the repository, resolves compiler output
paths to source files, and decides which files the agent must never patch
(generated build output and unmodified third-party sources).
"""

import os
import re
import subprocess
from typing import Dict, List, Optional, Tuple


def get_clean_build_env(
    depot_tools_path: Optional[str] = None,) -> Dict[str, str]:
  """Builds the environment used to invoke gclient, gn and autoninja.

  Prepending depot_tools to PATH is the only change from the ambient
  environment: those tools live there and are not otherwise on PATH.
  """
  depot_tools = depot_tools_path or os.path.expanduser("~/depot_tools")
  clean_env = dict(os.environ)
  if os.path.isdir(depot_tools):
    orig_path = clean_env.get("PATH", "")
    clean_env["PATH"] = f"{depot_tools}:{orig_path}"
  # Sets PYTHONNOUSERSITE=1: CI installs the agent's own requirements with
  # `pip install --user` (e.g. protobuf 6.x for google-cloud-aiplatform).
  # Without this, Chromium build scripts such as
  # build/android/gyp/compile_resources.py import those user-site packages
  # and fail ("Descriptors cannot be created directly"). Chromium build
  # scripts must only see the system/vendored Python packages.
  clean_env["PYTHONNOUSERSITE"] = "1"
  return clean_env


def is_within_repo(path: str, repo_path: str) -> bool:
  """Returns True if path (after normalization) is inside repo_path."""
  if not path:
    return False
  repo_abs = os.path.abspath(repo_path)
  path_abs = os.path.abspath(path)
  try:
    return os.path.commonpath([repo_abs, path_abs]) == repo_abs
  except ValueError:  # e.g. different drives on Windows
    return False


def resolve_repo_file_path(raw_path: str, repo_path: str) -> str:
  """Resolves command / compiler output paths into a path inside repo_path.

  Returns "" when the resolved path falls outside repo_path, so AI tool calls
  and patches cannot read or modify files elsewhere on the host (e.g.
  /etc/passwd or ~/.config credentials) via absolute paths or ../ traversal.
  """
  if ((resolved := _resolve_repo_file_path_unchecked(raw_path, repo_path)) and
      is_within_repo(resolved, repo_path)):
    return os.path.abspath(resolved)
  return ""


def _resolve_repo_file_path_unchecked(raw_path: str, repo_path: str) -> str:
  """Best-effort resolution of a raw path; may point outside repo_path."""
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

  # 5. Fallback: look the path up in the git index, but only accept a unique
  # match. Prefer the longest known suffix (e.g. "browser/foo.cc") and fall
  # back to the bare basename; an ambiguous name like BUILD.gn resolves to
  # nothing rather than to an arbitrary file.
  for suffix in dict.fromkeys((stripped, os.path.basename(clean))):
    if unique := _find_unique_tracked_file(suffix, repo_path):
      return unique

  return direct


_GITLINK_CACHE: Dict[str, List[str]] = {}


def _git_ls_files(pathspec: str, cwd: str) -> List[str]:
  """Runs `git ls-files -z -- pathspec` in cwd; [] on any failure."""
  try:
    res = subprocess.run(
        ["git", "ls-files", "-z", "--", pathspec],
        cwd=cwd,
        capture_output=True,
        text=True,
        check=False,
        timeout=60,
    )
  except (OSError, subprocess.SubprocessError):
    return []
  if res.returncode != 0:
    return []
  return [p for p in res.stdout.split("\0") if p]


def _dependency_checkouts(repo_path: str) -> List[str]:
  """Gitlink directories that are separate git checkouts (gclient deps)."""
  key = os.path.abspath(repo_path)
  if key in _GITLINK_CACHE:
    return _GITLINK_CACHE[key]

  try:
    res = subprocess.run(
        ["git", "ls-files", "-s"],
        cwd=repo_path,
        capture_output=True,
        text=True,
        check=False,
        timeout=60,
    )
    lines = res.stdout.splitlines() if res.returncode == 0 else []
  except (OSError, subprocess.SubprocessError):
    lines = []

  _GITLINK_CACHE[key] = [
      l.split("\t", 1)[1]
      for l in lines
      if l.startswith("160000 ") and "\t" in l and
      os.path.exists(os.path.join(repo_path,
                                  l.split("\t", 1)[1], ".git"))
  ]
  return _GITLINK_CACHE[key]


def _find_unique_tracked_file(suffix: str, repo_path: str) -> str:
  """Returns the only tracked file whose path ends with `suffix`, else "".

  Searches the main repository index and every gclient dependency checkout
  (git index only, so it is fast and ignores out/ and untracked files).
  """
  if not suffix or any(c in suffix for c in "*?[]"):
    return ""
  pathspec = f":(glob)**/{suffix}"
  matches = _git_ls_files(pathspec, repo_path)
  if len(matches) > 1:
    return ""
  for dep in _dependency_checkouts(repo_path):
    matches += [
        f"{dep}/{p}"
        for p in _git_ls_files(pathspec, os.path.join(repo_path, dep))
    ]
    if len(matches) > 1:
      return ""
  if len(matches) != 1:
    return ""
  return os.path.abspath(os.path.join(repo_path, matches[0]))


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


_FORKED_THIRD_PARTY_PREFIXES = ("third_party/jni_zero/",)

# Repository metadata is never a valid patch target, even inside a forked
# dependency.
_NEVER_PATCHABLE_BASENAMES = (
    "DEPS",
    "DIR_METADATA",
    "LICENSE",
    "OWNERS",
    "PRESUBMIT.py",
    "README.chromium",
)


def is_unmodified_third_party(file_path: str, repo_path: str) -> bool:
  """Checks if a file is pure third-party source code without Cobalt changes."""
  rel = os.path.relpath(file_path, repo_path)
  if not rel.startswith("third_party/"):
    return False
  if any(rel.startswith(p) for p in _FORKED_THIRD_PARTY_PREFIXES):
    return os.path.basename(rel) in _NEVER_PATCHABLE_BASENAMES
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


def patch_target_rejection(target_file: str, rel_file: str,
                           repo_path: str) -> str:
  """Returns reasons that target_file cannot be patched"""
  if not target_file:
    return f"{rel_file} is outside the repository"
  if (rel_file.endswith((".apk", ".ninja", ".so", ".a", ".o")) or
      rel_file in ("cobalt_apk", "all")):
    return (f"{rel_file} is a build target or binary; patch the source "
            "(.cc/.h) or BUILD.gn that produces it")
  if is_generated_build_artifact(target_file, repo_path):
    return (f"{rel_file} is a generated build artifact; patch the source "
            "or generator that produces it")
  if rel_file.startswith("cobalt/build/configs/") or rel_file.endswith(
      "args.gn"):
    return (f"{rel_file} is a global build config file; change the "
            "component BUILD.gn or source code instead")
  if (not target_file.endswith((".gn", ".gni", ".star")) and
      is_unmodified_third_party(target_file, repo_path)):
    return (f"{rel_file} is an upstream third-party file that Cobalt does "
            "not modify; do not edit it, adapt the Cobalt code or BUILD.gn "
            "that uses it instead")
  return ""
