#!/usr/bin/env python3
"""The record of one change the agent made, and helpers that read it.

AgentChangeRecord is what every phase appends to the shared session history;
change_history.json, the summary report, the commit messages and the model
prompts are all built from it. This module has no dependency on the
resolvers so that any of them, and fix_commits, can import it directly.
"""

import dataclasses
import re
import time
from typing import Any, Dict, List, Optional


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

  # The fields below are review context for the humans who take over the
  # PR. They are written to change_history.json and the summary report,
  # never to prompts. `error` above keeps its meaning: the command outcome
  # after the change.

  # What prompted the change: the compiler/gn diagnostic, or
  # "Conflict block #N in <file> (lines a-b)".
  trigger: str = ""
  # The model's own reasoning: the explanation text it wrote outside the
  # SEARCH/REPLACE blocks, or the expert pre-flight plan when the patch came
  # without one.
  rationale: str = ""
  # The model that produced the change ("fast-path" / "mock" when no model
  # was consulted).
  model: str = ""
  # For a conflict, the upstream side of the block as it was, so a reviewer
  # can judge the resolution without replaying the merge. Empty otherwise.
  upstream_side: str = ""
  # For a conflict, the Cobalt side of the block as it was. Empty otherwise.
  cobalt_side: str = ""
  # Short sha of the commit this change went into when the pipeline commits
  # per fix (see fix_commits); empty when not committed by the pipeline.
  commit: str = ""
  # Merged upstream Chromium/subrepo CLs that touched the file in this roll
  # (e.g. "https://crrev.com/c/7695593 (16777dd52227 Remove ...)").
  upstream_refs: List[str] = dataclasses.field(default_factory=list)

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
        "trigger": self.trigger,
        "rationale": self.rationale,
        "model": self.model,
        "upstream_side": self.upstream_side,
        "cobalt_side": self.cobalt_side,
        "commit": self.commit,
        "upstream_refs": list(self.upstream_refs),
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


# Pre-flight guidance has tool transcripts appended after the model's text.
_TOOL_RESULT_RE = re.compile(r"\n*Tool Call: `.*", re.DOTALL)
_TOOL_LINE_RE = re.compile(r"^\s*TOOL_[A-Z_]+:.*$", re.MULTILINE)

# Lengths of the review-context strings kept in a change record. They go to
# change_history.json and the summary report, not to prompts.
MAX_RATIONALE_CHARS = 1500
MAX_TRIGGER_CHARS = 2000


def guidance_summary(guidance: str, limit: int = MAX_RATIONALE_CHARS) -> str:
  """The expert's plan without the tool transcripts appended to it.

  Pre-flight guidance has "Tool Call: `...`\\nResult:\\n```...```" sections
  appended after the model's text, and may contain bare TOOL_ request
  lines. Both are dropped; what is left is the reasoning a reviewer wants.
  """
  if not guidance:
    return ""
  text = _TOOL_RESULT_RE.sub("", guidance)
  text = _TOOL_LINE_RE.sub("", text).strip()
  if len(text) > limit:
    text = text[:limit].rstrip() + " [...]"
  return text


# Placeholder rationale when the expert answered only with tool requests
# and the patch carried no explanation, so an empty record is visible as
# such rather than looking like a dropped field.
NO_RATIONALE = "(expert consulted tools; no written plan)"


def format_change_history(records: List[AgentChangeRecord],
                          window: int = 6) -> str:
  """Formats the last `window` change records, one line each, oldest first."""
  lines = []
  for rec in records[-window:]:
    if not rec.applied_cleanly:
      lines.append(f"- Iteration {rec.iteration}: patch for {rec.target_file} "
                   f"was not applied: {rec.error}")
    elif rec.error is None:
      lines.append(
          f"- Iteration {rec.iteration}: changed {rec.target_file} -> passed")
    else:
      lines.append(f"- Iteration {rec.iteration}: changed {rec.target_file} "
                   f"-> {rec.error}")
  return "\n".join(lines)
