#!/usr/bin/env python3
"""One git commit per fix, with the reason in the message.

Reviewers who take over an AI rebase PR work the way the Chromium roll
fixes are landed by hand: one commit per resolved conflict file or fixed
build error, whose message says what was wrong and why the change fixes it.
This module turns the change records a phase collects (trigger, both sides
of a conflict, the model's rationale) into such commits.

For every commit the workhorse model is asked to write the subject and body
under explicit rules (judge_commit in the reasoning engine, which for a
build fix also decides whether the pending patches are one complete fix).
A record-derived trailer (Trigger / Blocks / Files / Model) is appended, and
when there is no engine or its answer is unreadable the subject and body
are built from the records instead, so the message is never empty.
"""

import json
import logging
import re
import subprocess
import textwrap
from typing import Any, Dict, List, Optional, Sequence, Tuple

from change_record import NO_RATIONALE, AgentChangeRecord

log = logging.getLogger(__name__)

# Identity for commits made by the pipeline, matching the Phase 1
# checkpoint commit it has always made.
GIT_IDENTITY = [
    "-c", "user.name=Cobalt Rebase Agent", "-c",
    "user.email=cobalt-rebase-agent@google.com"
]

SUBJECT_MAX = 72


def _one_line(text: str, limit: int) -> str:
  first = (text or "").strip().splitlines()
  line = first[0].strip() if first else ""
  return line if len(line) <= limit else line[:limit - 3].rstrip() + "..."


_MD_NOISE_RE = re.compile(r"^\s*([-*]\s+|\d+\.\s+|>+\s*)|\*\*", re.MULTILINE)


def _excerpt(text: str, limit: int = 300) -> str:
  """The first `limit` characters of prose in `text`, without markdown.

  Expert plans are markdown written for the worker ("### Root Cause",
  bullets, code fences); this drops heading lines, code fences and their
  contents, and list/bold markup, joins the remaining lines, and cuts at a
  sentence end when one falls in the second half of the limit.
  """
  if not text:
    return ""
  kept = []
  in_fence = False
  for line in text.splitlines():
    if line.strip().startswith("```"):
      in_fence = not in_fence
      continue
    if in_fence or line.lstrip().startswith("#"):
      continue
    line = _MD_NOISE_RE.sub("", line).strip()
    if line:
      kept.append(line)
  flat = " ".join(kept)
  if len(flat) <= limit:
    return flat
  cut = flat[:limit]
  stop = cut.rfind(". ")
  if stop > limit // 2:
    return cut[:stop + 1]
  return cut.rstrip() + " [...]"


def _git(repo_path: str, *args: str) -> subprocess.CompletedProcess:
  return subprocess.run(["git", *GIT_IDENTITY, *args],
                        cwd=repo_path,
                        capture_output=True,
                        text=True,
                        check=False)


def head_sha(repo_path: str) -> str:
  """Full sha of HEAD, "" outside a git repository."""
  return _git(repo_path, "rev-parse", "--verify", "HEAD").stdout.strip()


def commit_files(repo_path: str,
                 files: Optional[Sequence[str]],
                 subject: str,
                 body: str = "") -> Optional[str]:
  """Stages `files` (every tracked change when None) and commits them.

  Returns the new short sha, or None when there was nothing to commit for
  these paths (the working tree already matched HEAD, for example after an
  anti-loop revert) or git failed, which is logged. Only the given paths
  are staged, so edits to other files made in the same iteration stay in
  the working tree for a later commit.
  """
  paths = sorted(set(f for f in files if f)) if files is not None else None
  if paths == []:
    return None
  pathspec = ["--", *paths] if paths else []
  add = _git(repo_path, "add", *(pathspec or ["-u"]))
  if add.returncode != 0:
    log.warning("[commit] git add failed for %s: %s", paths, add.stderr.strip())
    return None
  if _git(repo_path, "diff", "--cached", "--quiet", *pathspec).returncode == 0:
    return None
  message = subject.strip()
  if body.strip():
    message += "\n\n" + body.strip() + "\n"
  only = ["--only"] if paths else []
  commit = _git(repo_path, "commit", "-q", *only, "-m", message, *pathspec)
  if commit.returncode != 0:
    log.warning("[commit] git commit failed: %s", commit.stderr.strip())
    return None
  sha = _git(repo_path, "rev-parse", "--short=12", "HEAD").stdout.strip()
  log.info("[commit] %s %s", sha, subject.strip())
  return sha


def diff_for(repo_path: str,
             files: Sequence[str],
             max_chars: int = 60000) -> str:
  """Working-tree diff against HEAD for `files`, capped for a prompt."""
  paths = sorted(set(f for f in files if f))
  if not paths:
    return ""
  proc = _git(repo_path, "diff", "--no-color", "HEAD", "--", *paths)
  text = proc.stdout if proc.returncode == 0 else ""
  if len(text) > max_chars:
    text = text[:max_chars] + "\n[... diff truncated ...]"
  return text


def _files(rec: AgentChangeRecord) -> str:
  return ", ".join(rec.modified_files)


def _why(rec: AgentChangeRecord, limit: int = 300) -> str:
  """An excerpt of the record's rationale, or a visible placeholder."""
  if rec.rationale and rec.rationale != NO_RATIONALE:
    return _excerpt(rec.rationale, limit) or "(no written reason)"
  return "(no written reason)"


def _wrap(body: str, width: int = 72) -> str:
  """Re-wraps the model's prose paragraphs at `width`; keeps indented text."""
  paragraphs = re.split(r"\n\s*\n", body.strip())
  out = []
  for para in paragraphs:
    if any(line.startswith((" ", "\t")) for line in para.splitlines()):
      out.append(para)
    else:
      out.append(textwrap.fill(" ".join(para.split()), width))
  return "\n\n".join(out)


def _short_trigger(trigger: str, repo_path: str = "") -> str:
  """First line of a trigger, with the repository prefix cut off paths."""
  line = _one_line(trigger, 300)
  if repo_path:
    line = line.replace(repo_path.rstrip("/") + "/", "")
  return line


def request_message(engine: Any,
                    *,
                    phase: str,
                    trigger: str,
                    outcome: str,
                    diff: str,
                    rationales: str,
                    decide: bool = False) -> Dict[str, Any]:
  """Asks the engine for a commit message (and, with decide, a verdict).

  Returns parse_judgement's dict, {} when there is no engine, the call
  fails or the answer is unreadable; callers then fall back to the
  record-built message. Without decide the decision defaults to COMMIT.
  """
  if engine is None:
    return {}
  try:
    res = engine.judge_commit(
        phase=phase,
        trigger=trigger,
        outcome=outcome,
        diff=diff,
        rationales=rationales,
        decide=decide,
    )
  except Exception as e:  # pylint: disable=broad-exception-caught
    log.info("[%s] Notice: judge_commit query failed: %s", phase, e)
    return {}
  if not isinstance(res, dict):
    return {}
  return parse_judgement(res.get("judgement", ""), require_decision=decide)


def _block_label(rec: AgentChangeRecord) -> str:
  """"Conflict block #1 in <file> (lines 4-10)" -> "#1 (lines 4-10)"."""
  where = rec.trigger or f"Conflict block #{rec.iteration}"
  return re.sub(r"^Conflict block (#\d+) in \S+ ", r"\1 ", where)


def _upstream_refs(records: Sequence[AgentChangeRecord]) -> List[str]:
  """Deduplicated upstream CL references across `records`."""
  refs: List[str] = []
  seen = set()
  for rec in records:
    for ref in rec.upstream_refs:
      if ref and ref not in seen:
        seen.add(ref)
        refs.append(ref)
  return refs


def conflict_rationales(records: Sequence[AgentChangeRecord]) -> str:
  """One line per resolved block with the expert's reason, for the model."""
  lines = [
      f"- {_block_label(r)}: {_why(r, 1500)}" for r in records
      if r.applied_cleanly
  ]
  for ref in _upstream_refs(records):
    lines.append(f"- Upstream CL: {ref}")
  return "\n".join(lines)


def conflict_commit_message(rel_path: str,
                            records: Sequence[AgentChangeRecord],
                            proposed_subject: str = "",
                            proposed_body: str = "") -> Tuple[str, str]:
  """Subject and body for a Phase 1 commit of one resolved file.

  The model's subject/body are used when present. Otherwise the subject
  is "<file>: resolve merge conflict" and the body gives each block's
  reason. The trailer lists the blocks with their line numbers, upstream
  CLs and the model either way.
  """
  blocks = [r for r in records if r.applied_cleanly]
  n = len(blocks)
  subject = _one_line(
      proposed_subject.strip() or f"{rel_path}: resolve merge conflict",
      SUBJECT_MAX)
  parts: List[str] = []
  if proposed_body.strip():
    parts.append(_wrap(proposed_body))
  else:
    parts.extend(f"Block {_block_label(r)}: {_why(r)}" for r in blocks)
  parts.append("")
  plural = "s" if n != 1 else ""
  parts.append(f"Conflict block{plural}: " +
               ", ".join(_block_label(r) for r in blocks))
  for ref in _upstream_refs(blocks):
    parts.append(f"Upstream: {ref}")
  models = sorted(set(r.model for r in blocks if r.model))
  if models:
    parts.append("Model: " + ", ".join(models))
  return subject, "\n".join(parts)


def fix_rationales(records: Sequence[AgentChangeRecord]) -> str:
  """One line per pending patch with its rationale, for the model."""
  lines = [
      f"- iteration {r.iteration} ({_files(r)}): {r.rationale or NO_RATIONALE}"
      for r in records
  ]
  for ref in _upstream_refs(records):
    lines.append(f"- Upstream CL: {ref}")
  return "\n".join(lines)


def fix_commit_message(records: Sequence[AgentChangeRecord],
                       phase: str,
                       proposed_subject: str = "",
                       proposed_body: str = "",
                       repo_path: str = "") -> Tuple[str, str]:
  """Subject and body for a commit of one fixed build error.

  The model's subject/body (from judge_commit) are used when present;
  otherwise the subject is "<changed file>: <error>" and the body is an
  excerpt of each patch's rationale. The trailer with the trigger, the
  changed files (when more than one), upstream CLs and the model is always
  appended.
  """
  applied = [r for r in records if r.applied_cleanly] or list(records)
  first = applied[0]
  files: List[str] = []
  for rec in applied:
    for f in rec.modified_files:
      if f not in files:
        files.append(f)
  trigger_line = _short_trigger(first.trigger or first.error or "", repo_path)
  # The subject names the changed file, so the diagnostic's own location
  # prefixes go: "<path>:<line>:<col>: error:", and for gn the second one
  # inside the message, "ERROR at //path:8:1:".
  short_error = trigger_line
  for _ in range(2):
    short_error = re.sub(r"^(ERROR at )?\S+?:\d+(?::\d+)?:\s*", "", short_error)
  short_error = re.sub(r"^(error|fatal error):\s*", "", short_error)
  changed = files[0] if files else first.target_file
  subject = _one_line(proposed_subject.strip() or f"{changed}: {short_error}",
                      SUBJECT_MAX)
  parts: List[str] = []
  if proposed_body.strip():
    parts.append(_wrap(proposed_body))
  else:
    for rec in applied:
      label = (f"Why ({phase} iteration {rec.iteration})"
               if len(applied) > 1 else "Why")
      parts.append(f"{label}: {_why(rec)}")
  parts.append("")
  parts.append(f"Trigger: {trigger_line}")
  if len(files) > 1:
    parts.append("Files: " + ", ".join(files))
  for ref in _upstream_refs(applied):
    parts.append(f"Upstream: {ref}")
  models = sorted(set(r.model for r in applied if r.model))
  if models:
    parts.append("Model: " + ", ".join(models))
  return subject, "\n".join(parts)


def parse_judgement(text: str, require_decision: bool = True) -> Dict[str, Any]:
  """Reads the JSON object judge_commit returns; tolerant of fences.

  Returns {} when nothing usable can be read, which callers treat as "no
  opinion" and fall back to the deterministic rule and the record-built
  message. Without require_decision a missing decision means COMMIT.
  """
  if not text or not isinstance(text, str):
    return {}
  match = re.search(r"\{.*\}", text, re.DOTALL)
  if not match:
    return {}
  try:
    data = json.loads(match.group(0))
  except json.JSONDecodeError:
    return {}
  if not isinstance(data, dict):
    return {}
  decision = str(data.get("decision", "")).strip().upper()
  if decision not in ("COMMIT", "HOLD"):
    if require_decision or not str(data.get("subject", "")).strip():
      return {}
    decision = "COMMIT"
  return {
      "decision": decision,
      "subject": str(data.get("subject", "") or ""),
      "body": str(data.get("body", "") or ""),
      "reason": str(data.get("reason", "") or ""),
  }


class FixCommitter:
  """Turns the patches a build loop applies for one error into one commit.

  Patches are added as they are applied; the first one names the error the
  fix answers (`trigger`). After each build the loop reports the new error
  summary: while it is the triggering error the patches are held, once it
  changes they are committed, with the engine's judge_commit deciding (when
  there is an engine) whether the change really is one complete fix or the
  same problem with a new message. Whatever is still held when the loop
  ends is committed as unverified, so the branch carries every change.
  """

  def __init__(self, repo_path: str, phase: str, engine: Any = None):
    self.repo_path = repo_path
    self.phase = phase
    self.engine = engine
    # HEAD when this loop began; the diff shown to the model is taken
    # against it so earlier fix commits of this loop stay visible.
    self.base_sha = head_sha(repo_path)
    self.pending: List[AgentChangeRecord] = []
    self.trigger = ""

  def add(self, record: AgentChangeRecord, trigger: str) -> None:
    """Adds an applied patch; `trigger` is the error it answers."""
    if record in self.pending or not record.applied_cleanly:
      return
    if not self.pending:
      self.trigger = trigger
    self.pending.append(record)

  def drop_file(self, rel_path: str) -> None:
    """Forgets held patches that only touched `rel_path` (it was reverted)."""
    self.pending = [
        r for r in self.pending if set(r.modified_files) != {rel_path}
    ]
    if not self.pending:
      self.trigger = ""

  def after_build(self, error_summary: str, outcome: str) -> None:
    """Commits the held patches if the error they answered is gone.

    `outcome` is the new diagnostic in full, for the judge. An unreadable
    or failed judgement counts as COMMIT, so a flaky model cannot pile
    fixes up into one commit.
    """
    if not self.pending or error_summary == self.trigger:
      return
    verdict = self._ask(outcome, decide=True)
    if verdict.get("decision") == "HOLD":
      log.info("[%s] [commit] holding %d patch(es): %s", self.phase,
               len(self.pending), verdict.get("reason", ""))
      return
    self.flush(
        outcome=outcome,
        subject=verdict.get("subject", ""),
        body=verdict.get("body", ""))

  def flush(self,
            *,
            outcome: str,
            subject: str = "",
            body: str = "",
            unverified: bool = False) -> None:
    """Commits the held patches as one fix and clears them.

    `outcome` is what happened after the patches (the new diagnostic, or
    "build passed"); it goes into the trailer. When no subject was given
    (the build passed, or the loop ended) the engine is asked to write the
    message for the pending diff.
    """
    if not self.pending:
      return
    if not subject:
      message = self._ask(outcome, decide=False)
      subject, body = message.get("subject", ""), message.get("body", "")
    records, self.pending, self.trigger = self.pending, [], ""
    subject, body = fix_commit_message(records, self.phase, subject, body,
                                       self.repo_path)
    if unverified:
      subject = _one_line("WIP " + subject, SUBJECT_MAX)
      body = ("Unverified: the loop stopped before the triggering error "
              "went away.\n\n" + body)
    else:
      body += f"\nOutcome: {_short_trigger(outcome, self.repo_path)}"
    files = [f for r in records for f in r.modified_files]
    sha = commit_files(self.repo_path, files, subject, body)
    if sha:
      for rec in records:
        rec.commit = sha

  def _ask(self, outcome: str, *, decide: bool) -> Dict[str, Any]:
    files = [f for r in self.pending for f in r.modified_files]
    return request_message(
        self.engine,
        phase=self.phase,
        trigger=self.pending[0].trigger or self.trigger,
        outcome=outcome,
        diff=diff_for(self.repo_path, files),
        rationales=fix_rationales(self.pending),
        decide=decide)
