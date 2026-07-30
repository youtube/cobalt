#!/usr/bin/env python3
"""Upstream Chromium and subrepository CL lookup via depot_tools Gerrit client.

During a Chromium autoroll, Cobalt has a squashed `Update to <milestone>.`
commit rather than individual upstream commits. To tell why upstream changed a
file (and to cite the exact upstream CL in change_history.json, summary reports
and fix commit messages), this module:

1. Reads the old and new `chrome/VERSION` (and `DEPS` for flattened subrepos
   such as `v8/` or `third_party/icu/`) from the roll commit in the local git
   repository.
2. Asks Gitiles for the log of the file between the two versions
   (`<project>/+log/<old>..<new>/<path>`), so the result is exactly the
   upstream commits in the roll that touched it; each commit's `Reviewed-on:`
   line is its CL. Requests go through depot_tools' `gerrit_util`, which
   authenticates like `git cl` (in CI, via the cookie file `gob_auth` sets
   up) so they are not throttled as anonymous traffic.
"""

import dataclasses
import logging
import os
import re
import shutil
import subprocess
import sys
from typing import Any, Dict, List, Optional, Sequence, Tuple
import urllib.parse


def _add_depot_tools_to_sys_path() -> None:
  """Makes depot_tools' Python modules importable.

  depot_tools is on PATH wherever Cobalt builds, but Python does not look up
  modules on PATH, so add the directory that holds `gclient`.
  """
  gclient = shutil.which("gclient")
  if not gclient:
    raise ImportError("depot_tools (gclient) is not on PATH")
  depot_tools = os.path.dirname(os.path.realpath(gclient))
  if depot_tools not in sys.path:
    sys.path.append(depot_tools)


_add_depot_tools_to_sys_path()
import gerrit_util  # pylint: disable=import-error,wrong-import-position

log = logging.getLogger(__name__)

# Maximum body lines kept per CL when formatting `TOOL_UPSTREAM_LOG` output.
_MAX_BODY_LINES = 12


@dataclasses.dataclass(frozen=True)
class UpstreamCL:
  """One merged upstream Gerrit CL that touched a file in the roll."""

  commit: str
  url: str
  subject: str
  message: str = ""
  project: str = "chromium/src"

  def ref_line(self) -> str:
    """One-line citation for change records and commit trailers."""
    short = self.commit[:12] if self.commit else ""
    if short and self.subject:
      return f"{self.url} ({short} {self.subject})"
    if self.subject:
      return f"{self.url} ({self.subject})"
    return self.url


@dataclasses.dataclass(frozen=True)
class _SubrepoEntry:
  """A subrepository flattened into `src/` that is defined in `DEPS`."""

  host: str
  project: str
  old_rev: str
  new_rev: str


@dataclasses.dataclass(frozen=True)
class _RollRange:
  """The upstream versions a Cobalt roll commit moved between."""

  roll_sha: str
  old_version: str
  new_version: str
  subrepos: Dict[str, _SubrepoEntry]


# In-process caches keyed by repo_path / (repo_path, rel_path).
_ROLL_RANGE_CACHE: Dict[str, Optional[_RollRange]] = {}
_CL_CACHE: Dict[Tuple[str, str], List[UpstreamCL]] = {}


def _gerrit_json(host: str, path: str) -> Any:
  """GETs JSON from a Gerrit or Gitiles `host` via `gerrit_util`.

  `gerrit_util` picks the credentials the same way `git cl` does (LUCI, GCE,
  the `http.cookiefile` that `gob_auth` sets in CI, or SSO), retries 429 and
  5xx with backoff, and sends an anonymous request when no credentials apply.
  Returns None if the request fails.
  """
  try:
    conn = gerrit_util.CreateHttpConn(host, path, timeout=30)
    return gerrit_util.ReadHttpJsonResponse(conn)
  except Exception as e:  # pylint: disable=broad-exception-caught
    log.warning("Gerrit request to %s/%s failed: %s", host, path, e)
    return None


def _git_show(repo_path: str, spec: str) -> str:
  """Returns `git show <spec>` stdout, or "" on failure."""
  try:
    res = subprocess.run(
        ["git", "show", "--no-color", "--no-ext-diff", spec],
        cwd=repo_path,
        capture_output=True,
        text=True,
        errors="replace",
        check=False,
        timeout=15,
    )
    return res.stdout if res.returncode == 0 else ""
  except (OSError, subprocess.SubprocessError):
    return ""


def _find_upstream_roll_sha(repo_path: str) -> Optional[str]:
  """Finds the `Update to <milestone>.` commit in `repo_path`."""
  try:
    res = subprocess.run(
        [
            "git",
            "log",
            "-n20",
            "--grep=^Update to [0-9]",
            "--format=%H %s",
        ],
        cwd=repo_path,
        capture_output=True,
        text=True,
        errors="replace",
        check=False,
        timeout=15,
    )
  except (OSError, subprocess.SubprocessError):
    return None
  for line in res.stdout.splitlines():
    parts = line.split(" ", 1)
    if len(parts) == 2 and not parts[1].startswith("CONFLICTED"):
      return parts[0]
  return None


def _parse_chrome_version(text: str) -> str:
  """Parses `chrome/VERSION` content into `'MAJOR.MINOR.BUILD.PATCH'`."""
  vals = dict(re.findall(r"^([A-Z]+)=(\d+)\s*$", text or "", re.MULTILINE))
  keys = ("MAJOR", "MINOR", "BUILD", "PATCH")
  if not all(k in vals for k in keys):
    return ""
  return ".".join(vals[k] for k in keys)


_DEPS_VAR_RE = re.compile(
    r"['\"]([a-zA-Z0-9_]+)['\"]\s*:\s*['\"]([^'\"]+)['\"]")
_DEPS_SUBREPO_RE = re.compile(
    r"['\"]src/([^'\":]+)['\"]\s*:\s*(?:\{[^}]*?)?"
    r"Var\(['\"]([a-z_]+_git)['\"]\)\s*\+\s*['\"](/[^'\"@]+)['\"]"
    r"(?:\s*\+\s*['\"]@['\"]\s*\+\s*"
    r"(?:['\"]([0-9a-f]{40})['\"]|Var\(['\"]([^'\"]+)['\"]\)))?",
    re.DOTALL,
)


def parse_deps_subrepos(deps_text: str) -> Dict[str, Tuple[str, str, str]]:
  """Maps `src/<prefix>` in `DEPS` to `(host_short, gerrit_project, rev)`."""
  if not deps_text:
    return {}
  vars_dict = dict(_DEPS_VAR_RE.findall(deps_text))
  subrepos: Dict[str, Tuple[str, str, str]] = {}
  for match in _DEPS_SUBREPO_RE.finditer(deps_text):
    prefix, git_var, proj_path, lit_rev, var_rev = match.groups()
    host_url = vars_dict.get(git_var, "https://chromium.googlesource.com")
    host = re.sub(r"^https://|\.googlesource\.com.*$", "", host_url)
    project = re.sub(r"\.git$", "", proj_path.lstrip("/"))
    rev = lit_rev or vars_dict.get(var_rev or "", "")
    subrepos[prefix.rstrip("/")] = (host, project, rev)
  return subrepos


def _get_roll_range(repo_path: str) -> Optional[_RollRange]:
  """Reads the roll's old/new `chrome/VERSION` and `DEPS` from local git."""
  abs_repo = os.path.abspath(repo_path)
  if abs_repo in _ROLL_RANGE_CACHE:
    return _ROLL_RANGE_CACHE[abs_repo]

  roll = None
  roll_sha = _find_upstream_roll_sha(abs_repo)
  if roll_sha:
    old_ver = _parse_chrome_version(
        _git_show(abs_repo, f"{roll_sha}~1:chrome/VERSION"))
    new_ver = _parse_chrome_version(
        _git_show(abs_repo, f"{roll_sha}:chrome/VERSION"))
    if old_ver and new_ver:
      old_deps = parse_deps_subrepos(_git_show(abs_repo, f"{roll_sha}~1:DEPS"))
      new_deps = parse_deps_subrepos(_git_show(abs_repo, f"{roll_sha}:DEPS"))
      subrepos: Dict[str, _SubrepoEntry] = {}
      for prefix, (host, proj, new_rev) in new_deps.items():
        old_rev = old_deps.get(prefix, (host, proj, ""))[2]
        subrepos[prefix] = _SubrepoEntry(
            host=host, project=proj, old_rev=old_rev, new_rev=new_rev)
      roll = _RollRange(
          roll_sha=roll_sha,
          old_version=old_ver,
          new_version=new_ver,
          subrepos=subrepos,
      )
  _ROLL_RANGE_CACHE[abs_repo] = roll
  return roll


def _resolve_target_repo(roll: _RollRange,
                         rel_path: str) -> Tuple[str, str, str, str, str]:
  """Maps a Cobalt path to `(host, project, sub_path, old_ref, new_ref)`.

  Files under a `DEPS` subrepo flattened into `src/` (e.g. `v8/`) map to that
  repo and its old/new pinned revisions; everything else maps to
  `chromium/src` between the two version tags.
  """
  norm = rel_path.strip().lstrip("./")
  for prefix in sorted(roll.subrepos, key=len, reverse=True):
    if norm.startswith(prefix + "/"):
      entry = roll.subrepos[prefix]
      return (entry.host, entry.project, norm[len(prefix) + 1:], entry.old_rev,
              entry.new_rev)
  return ("chromium", "chromium/src", norm, roll.old_version, roll.new_version)


_REVIEWED_ON_RE = re.compile(
    r"^Reviewed-on:\s*(https://(\S+?)-review\.googlesource\.com/c/\S+/\+/"
    r"(\d+))\s*$", re.MULTILINE)


def _cl_url(host: str, project: str, commit: str, message: str) -> str:
  """Returns the CL URL from `Reviewed-on:`, else the Gitiles commit URL."""
  m = _REVIEWED_ON_RE.search(message or "")
  if m:
    url, review_host, num = m.groups()
    return f"https://crrev.com/c/{num}" if review_host == "chromium" else url
  return f"https://{host}.googlesource.com/{project}/+/{commit}"


def lookup_upstream_cls(repo_path: str,
                        rel_path: str,
                        limit: int = 10) -> List[UpstreamCL]:
  """Lists the upstream commits in the roll that modified `rel_path`."""
  clean_path = (rel_path or "").strip().lstrip("./")
  if not clean_path or clean_path.startswith("cobalt/"):
    return []
  cache_key = (os.path.abspath(repo_path), clean_path)
  if cache_key in _CL_CACHE:
    return _CL_CACHE[cache_key][:limit]

  results: List[UpstreamCL] = []
  roll = _get_roll_range(repo_path)
  if roll is not None:
    host, project, sub_path, old_ref, new_ref = _resolve_target_repo(
        roll, clean_path)
    # An unchanged subrepo pin means upstream did not touch the file.
    if old_ref and new_ref and old_ref != new_ref:
      data = _gerrit_json(
          f"{host}.googlesource.com", f"{project}/+log/{old_ref}..{new_ref}/"
          f"{urllib.parse.quote(sub_path)}?format=JSON&n={max(limit, 10)}")
      for entry in (data or {}).get("log", []):
        commit = str(entry.get("commit", ""))
        message = str(entry.get("message", "")).strip()
        if not commit:
          continue
        results.append(
            UpstreamCL(
                commit=commit,
                url=_cl_url(host, project, commit, message),
                subject=message.splitlines()[0].strip() if message else "",
                message=message,
                project=project,
            ))

  _CL_CACHE[cache_key] = results
  return results[:limit]


def _clean_commit_body(message: str, max_lines: int = _MAX_BODY_LINES) -> str:
  """Drops trailer lines (`Bug:`, `Change-Id:`, `Reviewed-on:`, `Cr-*`)."""
  lines = (message or "").strip().splitlines()[1:]
  kept: List[str] = []
  for line in lines:
    stripped = line.strip()
    if re.match(
        r"^(Change-Id|Reviewed-on|Reviewed-by|Commit-Queue|Cr-Commit-Position|"
        r"Auto-Submit|Bot-Commit|Owners-Override):", stripped):
      continue
    kept.append(line.rstrip())
  while kept and not kept[0].strip():
    kept.pop(0)
  while kept and not kept[-1].strip():
    kept.pop()
  if len(kept) > max_lines:
    kept = kept[:max_lines] + ["  [...]"]
  return "\n".join(f"  {l}" if l else "" for l in kept)


def format_upstream_log(repo_path: str, rel_path: str, limit: int = 5) -> str:
  """Formats the roll's upstream CLs for `rel_path` (`TOOL_UPSTREAM_LOG`)."""
  clean_path = (rel_path or "").strip().lstrip("./")
  if not clean_path:
    return "[ERROR] No file path provided to TOOL_UPSTREAM_LOG."

  roll = _get_roll_range(repo_path)
  if roll is None:
    return ("Could not determine upstream Chromium version range from local "
            "roll commit ('Update to <milestone>.').")

  cls = lookup_upstream_cls(repo_path, clean_path, limit=limit)
  _, project, sub_path, _, _ = _resolve_target_repo(roll, clean_path)
  scope = (f"{project}:{sub_path}" if project != "chromium/src" else clean_path)
  header = (f"Upstream CLs for {scope} in {roll.old_version}.."
            f"{roll.new_version}:")
  if not cls:
    return f"{header}\nNo upstream commits touched {scope} in this roll."

  blocks = [header]
  for cl in cls:
    short = cl.commit[:12] if cl.commit else "unknown"
    blocks.append(f"- {short} | {cl.url} | {cl.subject}")
    body = _clean_commit_body(cl.message)
    if body:
      blocks.append(body)
  return "\n".join(blocks)


# Matches `- <sha> | <url> | <subject>` lines emitted by `format_upstream_log`
# inside tool transcripts, so CLs the model looked up for a related header or
# `.gni` file are also recorded on the change record.
_TRANSCRIPT_CL_RE = re.compile(
    r"^-\s+([0-9a-f]{7,40})\s+\|\s+(https://\S+)\s+\|\s+(.+)$", re.MULTILINE)


def upstream_ref_lines(repo_path: str,
                       paths: Sequence[str],
                       tool_transcript: str = "",
                       max_refs: int = 5) -> List[str]:
  """Collects concise upstream CL references for `paths` and tool transcripts.

  Only structured Gitiles lookups for `paths` and `TOOL_UPSTREAM_LOG` output in
  `tool_transcript` are included; free-form model prose is never scanned so
  hallucinated links cannot enter `change_history.json` or commit messages.
  """
  refs: List[str] = []
  seen_urls = set()

  for path in paths:
    if not path or path.strip().lstrip("./") == "DEPS":
      continue
    for cl in lookup_upstream_cls(repo_path, path, limit=max_refs):
      if cl.url not in seen_urls:
        seen_urls.add(cl.url)
        refs.append(cl.ref_line())
        if len(refs) >= max_refs:
          return refs

  if tool_transcript:
    for sha, url, subject in _TRANSCRIPT_CL_RE.findall(tool_transcript):
      if url not in seen_urls:
        seen_urls.add(url)
        refs.append(f"{url} ({sha[:12]} {subject.strip()})")
        if len(refs) >= max_refs:
          return refs

  return refs


def clear_caches() -> None:
  """Clears in-process caches (used by unit tests)."""
  _ROLL_RANGE_CACHE.clear()
  _CL_CACHE.clear()
