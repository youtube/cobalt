"""Parses and applies the patches the model returns.

Supports SEARCH/REPLACE blocks, DELETE blocks and unified diffs. A response
is parsed once into PatchBlocks that are shared by apply and record.
"""

import dataclasses
import logging
import os
import re
from typing import Dict, List, Optional, Tuple

from repo_guards import patch_target_rejection, resolve_repo_file_path

log = logging.getLogger(__name__)

_LINE_NUMBER_RE = re.compile(r"^\s*\d+:\s*")
_CONFLICT_MARKER_PREFIXES = ("=======", "<<<<<<<", ">>>>>>>")


def _collapse_whitespace(line: str) -> str:
  """Fuzzy line key: no line-number prefix, runs of whitespace collapsed."""
  return re.sub(r"\s+", " ", _LINE_NUMBER_RE.sub("", line)).strip()


def _find_search_span(content: str, search: str,
                      replace: str) -> Optional[Tuple[int, int, str]]:
  """Locates `search` in `content`; returns (start, end, replacement) or None.

  Match levels, tried in order:
    1. exact substring;
    2. whitespace-trimmed text spanning whole lines (the file's indentation
       is kept);
    3. window of whole lines equal after stripping each line;
    4. window of whole lines equal after collapsing whitespace and dropping
       line-number prefixes (for re-indented or numbered model output).
  Fuzzy levels never match a fragment in the middle of a line, so they
  cannot rewrite text the model did not quote.
  """
  if search in content:
    start = content.index(search)
    return start, start + len(search), replace
  trimmed = search.strip()
  if trimmed:
    m = re.search(r"^[ \t]*(" + re.escape(trimmed) + r")[ \t]*$", content,
                  re.MULTILINE)
    if m:
      return m.start(1), m.end(1), replace.strip()

  lines = content.splitlines(keepends=True)
  offsets = [0]
  for line in lines:
    offsets.append(offsets[-1] + len(line))
  replacement = "".join(l + "\n" for l in replace.splitlines())
  for key in (str.strip, _collapse_whitespace):
    wanted = [key(l) for l in search.splitlines() if key(l)]
    if not wanted:
      continue
    keys = [key(l) for l in lines]
    n = len(wanted)
    for i in range(len(lines) - n + 1):
      if keys[i:i + n] == wanted:
        end = offsets[i + n]
        # Keep a file that lacks a final newline that way.
        if end == len(content) and not content.endswith("\n"):
          return offsets[i], end, replacement[:-1]
        return offsets[i], end, replacement
  return None


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

  # Drop rogue conflict markers and pasted line numbers if prepended.
  clean_replace = "\n".join(
      _LINE_NUMBER_RE.sub("", l)
      for l in replace_block.splitlines()
      if not l.startswith(_CONFLICT_MARKER_PREFIXES))
  search = "\n".join(
      _LINE_NUMBER_RE.sub("", l) for l in search_block.splitlines())

  with open(file_path, "r", encoding="utf-8", errors="replace") as f:
    content = f.read().replace("\r\n", "\n")

  span = _find_search_span(content, search, clean_replace)
  if span is None:
    return False
  start, end, replacement = span
  with open(file_path, "w", encoding="utf-8") as f:
    f.write(content[:start] + replacement + content[end:])
  return True


def _unified_diff_target(diff_text: str) -> str:
  """Repo-relative file named by the first ---/+++ header ("" if none)."""
  file_match = re.search(
      r"^(?:---|\+\+\+)\s+[ab]?/?([a-zA-Z0-9_/\.\-\+]+)",
      diff_text,
      re.MULTILINE,
  )
  return file_match.group(1).strip() if file_match else ""


def _apply_unified_diff(diff_text: str, repo_path: str) -> List[str]:
  """Applies a unified diff patch to source files, returning modified paths."""
  rel_file = _unified_diff_target(diff_text)
  if not rel_file:
    return []
  file_path = resolve_repo_file_path(rel_file, repo_path)

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


# Matches optional file directive headers produced by LLMs
# preceding patch blocks: e.g., "FILE: foo.cc", "**FILE**: 'baz.gn'"
_FILE_HEADER_PREFIX = (
    r"(?:(?:#{1,6}\s*)?"  # Optional markdown header (### )
    r"\*{0,2}(?:FILE|TARGET FILE)\*{0,2}"  # Optional bold (**)
    r":\s*"  # Colon
    r"[`'\"]*([a-zA-Z0-9_/\.\-\+]+)[`'\"]*"  # Captured relative path (Group 1)
    r"\s*[\r\n]+)?"  # Trailing newline (entire header is optional)
)

_CODE_FENCE = r"(?:\s*```[a-zA-Z0-9_-]*\s*[\r\n]+)?"
_DELETE_BLOCK_RE = re.compile(
    _FILE_HEADER_PREFIX + _CODE_FENCE +
    r"<<<<<<<\s*DELETE\r?\n(.*?)\r?\n>>>>>>>\s*DELETE(?:\s*```)?",
    re.DOTALL | re.IGNORECASE,
)
_SEARCH_REPLACE_BLOCK_RE = re.compile(
    _FILE_HEADER_PREFIX + _CODE_FENCE + r"<<<<<<<\s*SEARCH\r?\n(.*?)\r?\n"
    r"=======\r?\n(.*?)\r?\n>>>>>>>\s*REPLACE(?:\s*```)?",
    re.DOTALL | re.IGNORECASE,
)


@dataclasses.dataclass(frozen=True)
class PatchBlock:
  """One DELETE or SEARCH/REPLACE block parsed from a model response."""
  kind: str  # "DELETE" or "SEARCH"
  rel_file: str  # As written by the model (or the default file).
  target_file: str  # Resolved absolute path; "" if outside the repository.
  search: str
  replace: str = ""

  def render(self) -> str:
    if self.kind == "DELETE":
      return f"<<<<<<< DELETE\n{self.search}\n>>>>>>> DELETE"
    return (f"<<<<<<< SEARCH\n{self.search}\n=======\n"
            f"{self.replace}\n>>>>>>> REPLACE")


@dataclasses.dataclass(frozen=True)
class ParsedPatch:
  """A model patch response, parsed once and shared by apply and record.

  Attributes:
    text: The model response with a surrounding ```lang ... ``` code fence
      removed. Applied as a unified diff when there are no blocks, and
      stored in the change record.
    blocks: DELETE or SEARCH/REPLACE blocks found in the response; empty
      when the response is a unified diff.
  """
  text: str
  blocks: List[PatchBlock]


def parse_patch(patch_text: str,
                repo_path: str,
                default_file: Optional[str] = None) -> ParsedPatch:
  """Parses DELETE or SEARCH/REPLACE blocks (DELETE wins if both appear)."""
  text = patch_text.strip()
  text = re.sub(r"^```[a-zA-Z0-9_-]*\n", "", text)
  text = re.sub(r"\n```$", "", text)

  def block(kind: str,
            rel_file: str,
            search: str,
            replace: str = "") -> Optional[PatchBlock]:
    rel = rel_file.strip() or default_file or ""
    if not rel:
      return None
    # Strip a trailing ``` that leaked into the REPLACE section.
    replace = re.sub(r"\n```\s*$", "", replace)
    return PatchBlock(kind, rel, resolve_repo_file_path(rel, repo_path), search,
                      replace)

  blocks: List[PatchBlock] = []
  if "<<<<<<< DELETE" in text and ">>>>>>> DELETE" in text:
    blocks = [
        b for rel, search in _DELETE_BLOCK_RE.findall(text)
        if (b := block("DELETE", rel, search))
    ]
  if not blocks and "<<<<<<< SEARCH" in text and "=======" in text:
    blocks = [
        b for rel, search, replace in _SEARCH_REPLACE_BLOCK_RE.findall(text)
        if (b := block("SEARCH", rel, search, replace))
    ]
  return ParsedPatch(text, blocks)


def _block_rejection(b: PatchBlock, repo_path: str) -> str:
  """Why this block must not be applied ("" if it may be)."""
  if b.kind == "SEARCH":
    if not b.replace.strip() and len(b.search.splitlines()) > 80:
      return (f"bulk empty REPLACE block ({len(b.search.splitlines())} "
              f"lines) in {b.rel_file}; use <<<<<<< DELETE ... >>>>>>> "
              "DELETE for intentional bulk removals")
    if re.search(r"^(?:FILE|Target File):", b.replace, re.MULTILINE):
      return f"REPLACE block in {b.rel_file} contains a nested FILE directive"
  return patch_target_rejection(b.target_file, b.rel_file, repo_path)


def patch_rejections(parsed: ParsedPatch, repo_path: str) -> List[str]:
  """Distinct guard rejection reasons for a parsed patch (empty if allowed).

  Covers both formats: each block, or the target file of a unified diff.
  """
  if not parsed.blocks:
    rel_file = _unified_diff_target(parsed.text)
    if not rel_file:
      return []
    reason = patch_target_rejection(
        resolve_repo_file_path(rel_file, repo_path), rel_file, repo_path)
    return [reason] if reason else []
  reasons = (_block_rejection(b, repo_path) for b in parsed.blocks)
  return list(dict.fromkeys(r for r in reasons if r))


def _apply_blocks_atomically(blocks: List[PatchBlock]) -> List[str]:
  """Applies blocks all-or-nothing.

  Blocks are applied in order (so several blocks may edit the same file).
  If any block fails to match, every touched file is restored to its exact
  original bytes and [] is returned, so a half-applied patch never leaves
  the working tree in a state the resolution loop does not know about.
  """
  originals: Dict[str, bytes] = {}
  modified_files: List[str] = []
  for b in blocks:
    if b.target_file not in originals and os.path.isfile(b.target_file):
      with open(b.target_file, "rb") as f:
        originals[b.target_file] = f.read()
    if not apply_search_replace(b.target_file, b.search, b.replace):
      for path, data in originals.items():
        with open(path, "wb") as f:
          f.write(data)
      return []
    if b.target_file not in modified_files:
      modified_files.append(b.target_file)
  return modified_files


def apply_parsed_patch(parsed: ParsedPatch, repo_path: str) -> List[str]:
  """Applies a parsed patch; returns the modified absolute paths.

  DELETE and SEARCH/REPLACE blocks are applied atomically: every block is
  validated first and either all of them apply or no file is changed.
  """
  if patch_rejections(parsed, repo_path):
    return []
  if not parsed.blocks:
    return _apply_unified_diff(parsed.text, repo_path)
  return _apply_blocks_atomically(parsed.blocks)


def patch_file_changes(parsed: ParsedPatch,
                       repo_path: str,
                       default_file: Optional[str] = None) -> Dict[str, str]:
  """Groups a parsed patch into {repo-relative path: patch text} records."""

  def rel_key(abs_path: str, fallback: str) -> str:
    return os.path.relpath(abs_path, repo_path) if abs_path else fallback

  file_changes: Dict[str, str] = {}
  for b in parsed.blocks:
    key = rel_key(b.target_file, b.rel_file)
    file_changes[key] = (
        file_changes[key] + "\n\n" +
        b.render() if key in file_changes else b.render())
  if file_changes:
    return file_changes

  diff_match = re.search(r"^(?:--- [ab]/(.+)|diff --git a/.* b/(.+))$",
                         parsed.text, re.MULTILINE)
  target_rel = ((diff_match.group(1) or diff_match.group(2) or "").strip()
                if diff_match else "") or default_file or ""
  if target_rel:
    file_changes[rel_key(
        resolve_repo_file_path(target_rel, repo_path),
        target_rel)] = parsed.text
  return file_changes
