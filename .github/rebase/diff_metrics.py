#!/usr/bin/env python3
"""Diff parsing helpers for comparing two rebase attempts.

Provides file inventory extraction, functional-line normalization, and
per-file diff slicing. There is deliberately no similarity score here:
the pipeline measures divergence as a *count of functional differences*
enumerated by the expert model, not as a string-overlap ratio. Exact
line overlap punished semantically equivalent fixes that happened to be
written differently, which made the number hard to act on.

These helpers exist to give the expert model a focused, pre-filtered
view of what actually differs, and to keep that filtering deterministic
and testable.

Every function here is pure: no subprocess, no filesystem, no network.
"""

from typing import Dict, List, Set

# Preprocessor directives are functional code, not comments, even though
# they begin with '#'. #error and #warning are included deliberately:
# they change compilation behavior, so their presence or absence is a
# real difference between two rebase attempts.
CPP_DIRECTIVES = (
    "#include",
    "#if",
    "#ifdef",
    "#ifndef",
    "#else",
    "#elif",
    "#endif",
    "#define",
    "#undef",
    "#pragma",
    "#error",
    "#warning",
)


def strip_diff_marker(line: str) -> str:
  """Removes a leading '+'/'-' diff marker, preserving file headers."""
  stripped = line.strip()
  if stripped.startswith(("+++", "---")):
    return stripped
  if stripped[:1] in ("+", "-"):
    return stripped[1:].strip()
  return stripped


def is_comment_or_whitespace(line: str) -> bool:
  """Reports whether a diff line carries no functional code.

  Treats blank lines, diff metadata, and comments as non-functional.
  Preprocessor directives count as functional code.
  """
  stripped = line.strip()
  if not stripped or stripped in ("+", "-"):
    return True
  if stripped.startswith(("+++", "---", "@@", "diff --git", "index ")):
    return True

  content = strip_diff_marker(line)
  if not content:
    return True

  # Python and shell comments, minus C/C++ preprocessor directives.
  if content.startswith("#"):
    return not content.startswith(CPP_DIRECTIVES)

  # Python docstring delimiters are treated as non-functional.
  if content.startswith(('"""', "'''")):
    return True

  if content.startswith(("//", "/*", "*/")):
    return True

  # A bare '*' is a block-comment continuation only when followed by
  # whitespace or nothing. Without this check a pointer dereference such
  # as "*ptr = 5;" would be silently dropped from the comparison.
  if content.startswith("*"):
    return len(content) == 1 or content[1].isspace()

  return False


def extract_functional_lines(diff_text: str) -> Set[str]:
  """Returns normalized added/removed code lines from a unified diff.

  Whitespace runs are collapsed so pure reformatting does not register
  as a change. The '+'/'-' marker is retained so that adding a line and
  removing it are not conflated.
  """
  functional_lines = set()
  for line in diff_text.splitlines():
    if not line.startswith(("+", "-")):
      continue
    if line.startswith(("+++", "---")):
      continue
    if is_comment_or_whitespace(line):
      continue
    prefix = line[0]
    normalized = " ".join(line[1:].strip().split())
    functional_lines.add(f"{prefix} {normalized}")
  return functional_lines


def _header_paths(line: str) -> List[str]:
  """Returns the a/ and b/ paths declared by a 'diff --git' header."""
  parts = line.split()
  paths = []
  for token in parts[2:4]:
    if token.startswith(("a/", "b/")):
      paths.append(token[2:])
    else:
      paths.append(token)
  return paths


def extract_modified_files(diff_text: str) -> Set[str]:
  """Returns the set of repository paths touched by a unified diff.

  Reads 'diff --git' headers rather than '+++ b/' lines. A deleted file
  has '+++ /dev/null' and a pure rename has no '+++' line at all, so a
  '+++'-only reader silently loses both.
  """
  files = set()
  for line in diff_text.splitlines():
    if line.startswith("diff --git "):
      paths = _header_paths(line)
      if paths:
        files.add(paths[-1])
    elif line.startswith("+++ b/"):
      files.add(line[6:].strip())
  return files


def split_diff_by_file(diff_text: str) -> Dict[str, str]:
  """Splits a multi-file unified diff into {path: diff_section}.

  Sections are keyed by the post-image ('b/') path. Splitting once and
  indexing is both faster and safer than repeatedly scanning for a
  substring, which previously let a request for 'foo.h' match
  'ui/foo.html'.
  """
  sections: Dict[str, str] = {}
  current_path = ""
  current: List[str] = []

  for line in diff_text.splitlines():
    if line.startswith("diff --git "):
      if current_path and current:
        sections[current_path] = "\n".join(current).strip()
      paths = _header_paths(line)
      current_path = paths[-1] if paths else ""
      current = [line]
    elif current_path:
      current.append(line)

  if current_path and current:
    sections[current_path] = "\n".join(current).strip()

  return sections


def resolve_diff_path(sections: Dict[str, str], target: str) -> List[str]:
  """Returns section keys matching target, exact match preferred.

  A bare filename that matches several paths returns all of them so the
  caller can report the ambiguity rather than silently pick one.
  """
  if target in sections:
    return [target]
  suffix_matches = [p for p in sections if p.endswith("/" + target)]
  return sorted(suffix_matches)


def files_with_functional_differences(reference_diff: str,
                                      candidate_diff: str) -> List[str]:
  """Returns shared files whose functional content actually differs.

  Files touched by both sides with identical normalized content carry no
  difference to explain, so excluding them keeps the expert model's
  attention (and the token budget) on the parts that matter.
  """
  ref_sections = split_diff_by_file(reference_diff)
  cand_sections = split_diff_by_file(candidate_diff)

  differing = []
  for path in sorted(set(ref_sections) & set(cand_sections)):
    ref_lines = extract_functional_lines(ref_sections[path])
    cand_lines = extract_functional_lines(cand_sections[path])
    if ref_lines != cand_lines:
      differing.append(path)
  return differing


def build_file_inventory(reference_diff: str,
                         candidate_diff: str) -> Dict[str, List[str]]:
  """Returns the factual file-level inventory of the two diffs."""
  ref_files = extract_modified_files(reference_diff)
  cand_files = extract_modified_files(candidate_diff)
  return {
      "shared":
          sorted(ref_files & cand_files),
      "reference_only":
          sorted(ref_files - cand_files),
      "candidate_only":
          sorted(cand_files - ref_files),
      "shared_differing":
          files_with_functional_differences(reference_diff, candidate_diff),
  }
