"""Diagnostics reported by the rebase phases.

Every phase parses its failed command output into Diagnostic objects. The
phase-specific subclasses live here, next to their base class, so tooling
(pylint's dataclass support) sees each class's full field list.
"""

import dataclasses
from typing import Dict, List, Optional


@dataclasses.dataclass
class Diagnostic:
  """One failure reported by a phase command.

  A bare Diagnostic wraps raw command output that no phase parser could
  structure.
  """

  error_message: str
  file_path: str = ""  # Absolute path (or repo-relative for DEPS); "" if none.
  line_number: int = 1
  raw_snippet: str = ""
  notes: List[str] = dataclasses.field(default_factory=list)

  def trace(self) -> str:
    """Full error text for prompts: location, message, snippet and notes."""
    prefix = f"{self.file_path}:{self.line_number}: " if self.file_path else ""
    snippet = f"\nSnippet:\n{self.raw_snippet}" if self.raw_snippet else ""
    notes = "\n" + "\n".join(self.notes) if self.notes else ""
    return f"{prefix}{self.error_message}{snippet}{notes}"


@dataclasses.dataclass
class CompilerDiagnostic(Diagnostic):
  """A compiler / linker / action error parsed from ninja or siso output."""

  column: int = 0


@dataclasses.dataclass
class GNDiagnostic(Diagnostic):
  """A GN build error diagnostic parsed from gn gen output."""

  raw_output: str = ""
  target_files: Dict[str,
                     Optional[int]] = dataclasses.field(default_factory=dict)
  is_structural_break: bool = False


@dataclasses.dataclass
class GClientSyncDiagnostic(Diagnostic):
  """A gclient sync error diagnostic."""

  raw_output: str = ""
  diagnostic_trace: str = ""
  file_path: str = "DEPS"

  def trace(self) -> str:
    return self.diagnostic_trace or super().trace()
