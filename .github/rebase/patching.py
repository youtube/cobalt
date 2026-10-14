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
  Levels 3 and 4 ignore blank lines at the start and end of `search` but
  keep blank lines inside it, which must match blank lines in the file.
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
    wanted = [key(l) for l in search.splitlines()]
    while wanted and not wanted[0]:
      wanted.pop(0)
    while wanted and not wanted[-1]:
      wanted.pop()
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


def _has_line_numbers(text: str) -> bool:
  """True if every non-empty line starts with a "<digits>:" prefix."""
  lines = [l for l in text.splitlines() if l.strip()]
  return bool(lines) and all(_LINE_NUMBER_RE.match(l) for l in lines)


def _block_result(content: str, search: str, replace: str) -> Optional[str]:
  """Content after replacing `search` with `replace`; None if not found.

  Rogue conflict marker lines are dropped from `replace`: models sometimes
  echo git 3-way merge syntax into flat files, which would break GN and
  compiler parsing. Line-number prefixes are stripped only when the whole
  SEARCH block is numbered (copied from numbered file context), so code
  such as `100: "Continue",` is never rewritten.
  """
  replace_lines = [
      l for l in replace.splitlines()
      if not l.startswith(_CONFLICT_MARKER_PREFIXES)
  ]
  if _has_line_numbers(search):
    search = "\n".join(_LINE_NUMBER_RE.sub("", l) for l in search.splitlines())
    replace_lines = [_LINE_NUMBER_RE.sub("", l) for l in replace_lines]
  span = _find_search_span(content, search, "\n".join(replace_lines))
  if span is None:
    return None
  start, end, replacement = span
  return content[:start] + replacement + content[end:]


def _read_text(path: str) -> str:
  with open(path, encoding="utf-8", errors="replace") as f:
    return f.read()


def _write_text(path: str, content: str) -> None:
  with open(path, "w", encoding="utf-8", newline="") as f:
    f.write(content)


def apply_search_replace(file_path: str, search_block: str,
                         replace_block: str) -> bool:
  """Applies one SEARCH/REPLACE edit to a file; False if it does not match."""
  if not os.path.isfile(file_path):
    return False
  if new := _block_result(_read_text(file_path), search_block, replace_block):
    _write_text(file_path, new)
    return True
  return False


def _unified_diff_target(diff_text: str) -> str:
  """Repo-relative file named by the first ---/+++ header ("" if none).

  Skips /dev/null (the old side of a new file) and strips only a whole
  "a/" or "b/" prefix, so paths such as base/logging.cc stay intact.
  """
  for m in re.finditer(r"^(?:---|\+\+\+)\s+(\S+)", diff_text, re.MULTILINE):
    if m.group(1) != "/dev/null":
      return re.sub(r"^[ab]/", "", m.group(1))
  return ""


_HUNK_RE = re.compile(r"^@@\s+-(\d+)(?:,(\d+))?\s+\+(\d+)(?:,(\d+))?\s+@@")


def _unified_diff_result(diff_text: str, content: str) -> Optional[str]:
  """Content after applying the diff's hunks; None if any hunk misfits."""
  lines = diff_text.splitlines()
  new_lines = content.splitlines(keepends=True)
  offset = 0
  hunks = 0
  i = 0
  while i < len(lines):
    hm = _HUNK_RE.match(lines[i])
    i += 1
    if not hm:
      continue
    src: List[str] = []
    dst: List[str] = []
    while i < len(lines) and not lines[i].startswith("@@"):
      line = lines[i]
      if line.startswith(("-", " ")):
        src.append(line[1:] + "\n")
      if line.startswith(("+", " ")):
        dst.append(line[1:] + "\n")
      i += 1
    pos = int(hm.group(1)) - 1 + offset
    if not 0 <= pos <= len(new_lines) or new_lines[pos:pos + len(src)] != src:
      return None
    new_lines[pos:pos + len(src)] = dst
    offset += len(dst) - len(src)
    hunks += 1
  return "".join(new_lines) if hunks else None


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
  """Parses DELETE and SEARCH/REPLACE blocks in the order they appear.

  A block without a FILE header edits the previous block's file, or
  default_file for the first block.
  """
  text = patch_text.strip()
  text = re.sub(r"^```[a-zA-Z0-9_-]*\n", "", text)
  text = re.sub(r"\n```$", "", text)

  matches = sorted([(m.start(), "DELETE", m.group(1), m.group(2), "")
                    for m in _DELETE_BLOCK_RE.finditer(text)] +
                   [(m.start(), "SEARCH", m.group(1), m.group(2), m.group(3))
                    for m in _SEARCH_REPLACE_BLOCK_RE.finditer(text)])
  blocks: List[PatchBlock] = []
  rel = default_file or ""
  for _, kind, header, search, replace in matches:
    rel = (header or "").strip() or rel
    if not rel:
      continue
    # Strip a trailing ``` that leaked into the REPLACE section.
    replace = re.sub(r"\n```\s*$", "", replace)
    blocks.append(
        PatchBlock(kind, rel, resolve_repo_file_path(rel, repo_path), search,
                   replace))
  return ParsedPatch(text, blocks)


_EXEC_SCRIPT_RE = re.compile(r"\bexec_script\s*\(")


def _blank_gn_comments(content: str) -> str:
  """GN source with # comments replaced by spaces (offsets unchanged)."""
  out = list(content)
  in_string = False
  i = 0
  while i < len(content):
    c = content[i]
    if in_string:
      if c == "\\":
        i += 1
      elif c == '"':
        in_string = False
    elif c == '"':
      in_string = True
    elif c == "#":
      while i < len(content) and content[i] != "\n":
        out[i] = " "
        i += 1
      continue
    i += 1
  return "".join(out)


def exec_script_spans(content: str) -> List[Tuple[int, int]]:
  """(start, end) offsets of every exec_script(...) call in GN source.

  A call runs from "exec_script(" to its matching ")", so a call written
  over several lines is one span. Comments and parentheses inside string
  literals are ignored; an unclosed call extends to the end of the file.
  """
  code = _blank_gn_comments(content)
  spans = []
  for m in _EXEC_SCRIPT_RE.finditer(code):
    depth = 0
    in_string = False
    i = m.end() - 1
    while i < len(code):
      c = code[i]
      if in_string:
        if c == "\\":
          i += 1
        elif c == '"':
          in_string = False
      elif c == '"':
        in_string = True
      elif c == "(":
        depth += 1
      elif c == ")":
        depth -= 1
        if depth == 0:
          break
      i += 1
    spans.append((m.start(), min(i + 1, len(code))))
  return spans


def exec_script_calls(content: str) -> List[str]:
  """Whitespace-normalized text of every exec_script() call, sorted."""
  code = _blank_gn_comments(content)
  return sorted(
      " ".join(code[a:b].split()) for a, b in exec_script_spans(content))


def _exec_script_rejection(rel_file: str, old: str, new: str) -> str:
  """Rejects GN edits that add, change or remove exec_script() calls.

  gn gen runs exec_script() commands, so a model-written call would run
  arbitrary code on the runner. Edits elsewhere in the file are allowed.
  """
  if not rel_file.endswith((".gn", ".gni")):
    return ""
  if exec_script_calls(old) == exec_script_calls(new):
    return ""
  return (f"patch for {rel_file} adds, changes or removes an exec_script() "
          "call, which gn gen would run; fix the build without touching "
          "exec_script()")


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


def _planned_contents(parsed: ParsedPatch,
                      repo_path: str) -> Optional[Dict[str, Tuple[str, str]]]:
  """{absolute path: (old, new content)} that the patch would produce.

  Computed in memory, without writing, so guards can inspect the result and
  apply can write every file or none. None if any edit does not apply.
  """
  if not parsed.blocks:
    rel_file = _unified_diff_target(parsed.text)
    path = resolve_repo_file_path(rel_file, repo_path) if rel_file else ""
    if not os.path.isfile(path):
      return None
    old = _read_text(path)
    new = _unified_diff_result(parsed.text, old)
    return None if new is None else {path: (old, new)}
  plan: Dict[str, Tuple[str, str]] = {}
  for b in parsed.blocks:
    if not os.path.isfile(b.target_file):
      return None
    if b.target_file in plan:
      old, current = plan[b.target_file]
    else:
      old = current = _read_text(b.target_file)
    new = _block_result(current, b.search, b.replace)
    if new is None:
      return None
    plan[b.target_file] = (old, new)
  return plan


def patch_rejections(parsed: ParsedPatch, repo_path: str) -> List[str]:
  """Distinct guard rejection reasons for a parsed patch (empty if allowed).

  Checks each block (or the target of a unified diff), then the
  exec_script() calls of every GN file the patch would change.
  """
  if parsed.blocks:
    reasons = [_block_rejection(b, repo_path) for b in parsed.blocks]
  else:
    rel_file = _unified_diff_target(parsed.text)
    reasons = [
        patch_target_rejection(
            resolve_repo_file_path(rel_file, repo_path), rel_file, repo_path)
    ] if rel_file else []
  if not any(reasons):
    plan = _planned_contents(parsed, repo_path) or {}
    reasons = [
        _exec_script_rejection(os.path.relpath(path, repo_path), old, new)
        for path, (old, new) in plan.items()
    ]
  return list(dict.fromkeys(r for r in reasons if r))


def apply_parsed_patch(parsed: ParsedPatch, repo_path: str) -> List[str]:
  """Applies a parsed patch; returns the modified absolute paths.

  Every edit is computed in memory first, so either all files are written
  or none is (for example when one SEARCH block does not match).
  """
  if patch_rejections(parsed, repo_path):
    return []
  plan = _planned_contents(parsed, repo_path)
  if not plan:
    return []
  for path, (_, new) in plan.items():
    _write_text(path, new)
  return list(plan)


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
