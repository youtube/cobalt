#!/usr/bin/env python3
"""Structured representation of Human vs AI rebase differences.

The expert model is asked to enumerate differences as discrete fenced
blocks rather than as prose. The pipeline then parses and counts them.

This matters for trust: if the model simply stated "there are 7
differences", the number would be an unverifiable assertion that could
drift between runs. Counting parsed blocks makes the number a direct
function of enumerated, individually reviewable items, so a reviewer can
always see exactly what was counted.

Expected block format:

  ```difference
  FILE: media/mojo/mojom/BUILD.gn
  CATEGORY: DIVERGENT
  SEVERITY: HIGH
  HUMAN: Added enabled_features to the new media_types target.
  AI: Left media_types unchanged and edited the caller instead.
  IMPACT: kStarboard is never emitted, so the build fails downstream.
  QUESTION: Was the target split the intended place to declare this?
  ```
"""

import dataclasses
import re
from typing import Dict, List

VALID_CATEGORIES = ("MISSED", "EXTRA", "DIVERGENT")
VALID_SEVERITIES = ("HIGH", "MEDIUM", "LOW")

_BLOCK_PATTERN = re.compile(r"```difference\s*\n(.*?)```", re.DOTALL)
_FIELD_PATTERN = re.compile(
    r"^(FILE|CATEGORY|SEVERITY|HUMAN|AI|IMPACT|QUESTION)\s*:\s*(.*)$",
    re.IGNORECASE)


@dataclasses.dataclass
class Difference:
  """One functional difference between the human and AI rebase."""

  file: str = ""
  category: str = "DIVERGENT"
  severity: str = "MEDIUM"
  human: str = ""
  ai: str = ""
  impact: str = ""
  question: str = ""

  def is_valid(self) -> bool:
    """Reports whether this item carries enough signal to count.

    A block naming no file, or describing neither side's behavior, is
    treated as malformed rather than counted. Counting empty shells
    would inflate the headline number.
    """
    return bool(self.file) and bool(self.human or self.ai)

  def to_markdown(self, index: int) -> str:
    """Renders one reviewer-facing entry."""
    no_change = "(no change)"
    lines = [
        f"### {index}. [{self.severity}] `{self.file}`",
        f"- **Category**: {self.category}",
        f"- **Human**: {self.human or no_change}",
        f"- **AI**: {self.ai or no_change}",
    ]
    if self.impact:
      lines.append(f"- **Impact**: {self.impact}")
    if self.question:
      lines.append(f"- **Question for reviewer**: {self.question}")
    return "\n".join(lines)


def parse_differences(text: str) -> List[Difference]:
  """Extracts difference blocks from an expert model response.

  Unparseable or empty blocks are dropped rather than counted.
  """
  differences = []
  for raw_block in _BLOCK_PATTERN.findall(text or ""):
    item = Difference()
    current_field = ""
    for line in raw_block.splitlines():
      match = _FIELD_PATTERN.match(line.strip())
      if match:
        current_field = match.group(1).upper()
        value = match.group(2).strip()
        _assign_field(item, current_field, value)
      elif current_field and line.strip():
        # Continuation of a multi-line field value.
        _append_field(item, current_field, line.strip())

    item.category = _normalize(item.category, VALID_CATEGORIES, "DIVERGENT")
    item.severity = _normalize(item.severity, VALID_SEVERITIES, "MEDIUM")
    if item.is_valid():
      differences.append(item)

  return differences


def _assign_field(item: Difference, field: str, value: str) -> None:
  """Sets a parsed field on a Difference."""
  mapping = {
      "FILE": "file",
      "CATEGORY": "category",
      "SEVERITY": "severity",
      "HUMAN": "human",
      "AI": "ai",
      "IMPACT": "impact",
      "QUESTION": "question",
  }
  attr = mapping.get(field)
  if attr:
    setattr(item, attr, value)


def _append_field(item: Difference, field: str, value: str) -> None:
  """Appends a continuation line to an existing field."""
  mapping = {
      "HUMAN": "human",
      "AI": "ai",
      "IMPACT": "impact",
      "QUESTION": "question",
  }
  attr = mapping.get(field)
  if attr:
    existing = getattr(item, attr)
    setattr(item, attr, f"{existing} {value}".strip())


def _normalize(value: str, allowed: tuple, default: str) -> str:
  """Coerces a field to an allowed enum value."""
  upper = (value or "").strip().upper()
  return upper if upper in allowed else default


def count_by(differences: List[Difference], attr: str) -> Dict[str, int]:
  """Tallies differences by an attribute such as severity or category."""
  counts: Dict[str, int] = {}
  for item in differences:
    key = getattr(item, attr, "") or "UNKNOWN"
    counts[key] = counts.get(key, 0) + 1
  return counts


def format_summary(differences: List[Difference], human_label: str,
                   ai_label: str, inventory: Dict[str, List[str]]) -> str:
  """Renders the reviewer-facing difference summary.

  Ordered by severity so a reviewer with limited time reads the
  important items first.
  """
  total = len(differences)
  by_sev = count_by(differences, "severity")
  by_cat = count_by(differences, "category")

  lines = [
      "# Rebase Difference Report",
      "",
      f"- **Human PR**: {human_label}",
      f"- **AI PR**: {ai_label}",
      "",
      f"## Functional differences found: {total}",
      "",
      "| Severity | Count |",
      "| :--- | ---: |",
  ]
  for sev in VALID_SEVERITIES:
    lines.append(f"| {sev} | {by_sev.get(sev, 0)} |")

  n_missed = by_cat.get("MISSED", 0)
  n_extra = by_cat.get("EXTRA", 0)
  n_divergent = by_cat.get("DIVERGENT", 0)
  n_shared = len(inventory.get("shared", []))
  n_differing = len(inventory.get("shared_differing", []))
  n_reference_only = len(inventory.get("reference_only", []))
  n_candidate_only = len(inventory.get("candidate_only", []))

  lines.extend([
      "",
      "| Category | Count | Meaning |",
      "| :--- | ---: | :--- |",
      f"| MISSED | {n_missed} | Human changed it, AI did not |",
      f"| EXTRA | {n_extra} | AI changed it, human did not |",
      f"| DIVERGENT | {n_divergent} | Both changed it, differently |",
      "",
      "### File inventory",
      f"- Touched by both: {n_shared} ({n_differing} differ functionally)",
      f"- Human only: {n_reference_only}",
      f"- AI only: {n_candidate_only}",
      "",
  ])

  if not differences:
    lines.append(
        "No functional differences were identified. Note this means the "
        "expert model found none, not that none exist.")
    return "\n".join(lines)

  lines.append("## Differences, highest severity first")
  lines.append("")
  order = {sev: i for i, sev in enumerate(VALID_SEVERITIES)}
  ranked = sorted(differences, key=lambda d: order.get(d.severity, 99))
  for idx, item in enumerate(ranked, start=1):
    lines.append(item.to_markdown(idx))
    lines.append("")

  return "\n".join(lines)
