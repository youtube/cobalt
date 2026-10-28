#!/usr/bin/env python3
"""End-to-end automated Cobalt Chromium rebase pipeline runner.

Executes all rebase phases in sequence:
  Phase 1: Conflict Resolution (prioritizing DEPS & toolchain build files first,
           then GN build configs, then C++/Java source files).
  Phase 2: Toolchain & Dependency Sync (gclient sync -D).
  Phase 3: GN Build Generation & Verification (cobalt/build/gn.py).
  Phase 4: autoninja Compiler Self-Healing Loop (up to 100 iterations).
  Phase 5: Comprehensive MXXX_rebase_summary.md generation with metrics.
"""

import argparse
import collections
import json
import logging
import os
import shutil
import subprocess
import sys
import tempfile
import time
from typing import Dict, List, Optional
import warnings

from autoninja import AutoninjaResolver
from change_record import AgentChangeRecord
from conflicts import ConflictResolver
from gclient_sync import GClientSyncResolver
from gn_gen import GNGenResolver
from engine_client import ReasoningEngineClient
from fix_commits import commit_files, head_sha
from patching import NEEDS_HUMAN_PREFIX
from token_usage import TokenUsage

log = logging.getLogger(__name__)

# Suppress google.auth UserWarning about ADC quota project on Cloudtop
warnings.filterwarnings("ignore", category=UserWarning, module="google.auth")


def get_chromium_milestone(repo_path: Optional[str] = None) -> str:
  """Reads the Chromium major milestone from chrome/VERSION (e.g. 'M138')."""
  base = repo_path or os.path.expanduser("~/cobalt/src")
  version_file = os.path.join(base, "chrome", "VERSION")
  if os.path.isfile(version_file):
    try:
      with open(version_file, "r", encoding="utf-8") as f:
        for line in f:
          if line.startswith("MAJOR="):
            major_ver = line.strip().split("=")[1]
            return f"M{major_ver}"
    except OSError:
      pass
  return "M_Unknown"


_PHASES = (
    ("Phase 1", "Conflict Resolution", "Unified DEPS & source conflict repair"),
    ("Phase 2", "Toolchain Sync", "`gclient sync -D` toolchain & CIPD sync"),
    ("Phase 3", "GN Config Check", "`cobalt/build/gn.py --check` validation"),
    ("Phase 4", "autoninja Loop", "autoninja compiler healing"),
)


def _one_line(text: str, limit: int) -> str:
  """First line of `text`, cut to `limit` characters."""
  first = (text or "").strip().splitlines()
  line = first[0].strip() if first else ""
  return line if len(line) <= limit else line[:limit].rstrip() + " [...]"


def _squash(text: str, limit: int) -> str:
  """`text` with whitespace runs collapsed, cut to `limit` characters."""
  flat = " ".join((text or "").split())
  return flat if len(flat) <= limit else flat[:limit].rstrip() + " [...]"


def format_change_log(records: List[AgentChangeRecord]) -> str:
  """Markdown "what changed and why" for the applied changes, per file.

  This is the section a reviewer taking over the PR reads first: for each
  file, the conflict or build error that prompted the change, the model's
  reasoning, and the upstream commits/CLs it consulted. Attempts that were
  not applied are left out; they are in change_history.json.
  """
  applied = [r for r in records if r.applied_cleanly and r.modified_files]
  if not applied:
    return ""
  by_file: Dict[str, List[AgentChangeRecord]] = collections.OrderedDict()
  for rec in applied:
    for path in rec.modified_files:
      by_file.setdefault(path, []).append(rec)

  lines = [
      "", "## 5. Change Log (what changed and why)",
      "Per file, oldest first. `trigger` is the conflict or error the change "
      "answers; `why` is the model's reasoning. Full diffs, and both sides of "
      "each conflict, are in change_history.json.", ""
  ]
  for path, recs in by_file.items():
    lines.append(f"### `{path}`")
    for rec in recs:
      outcome = ("ok"
                 if rec.error is None else f"then: {_one_line(rec.error, 160)}")
      model = f", {rec.model}" if rec.model else ""
      commit = f", `{rec.commit}`" if rec.commit else ""
      lines.append(
          f"- **{rec.phase}** iteration {rec.iteration}{model}{commit} "
          f"({outcome})")
      if rec.trigger:
        lines.append(f"  - trigger: {_one_line(rec.trigger, 220)}")
      if rec.rationale:
        lines.append(f"  - why: {_squash(rec.rationale, 400)}")
      if rec.upstream_refs:
        lines.append("  - upstream: " + ", ".join(rec.upstream_refs[:5]))
    lines.append("")
  return "\n".join(lines)


def write_rebase_report(
    results_dir: str,
    platform: str,
    build_type: str,
    *,
    target: str,
    model: str,
    expert_model: str,
    status: str,
    elapsed_seconds: float,
    phase_status: Optional[Dict[str, str]] = None,
    repo_path: Optional[str] = None,
    session_changes: Optional[List[AgentChangeRecord]] = None,
    token_usage: Optional[TokenUsage] = None,
) -> str:
  """Generates the final comprehensive rebase summary report.

  Written to results_dir (out/rebase_results next to change_history.json),
  outside the source tree, so it is never committed with the resolution.
  phase_status maps "Phase N" to its outcome; phases not in it show
  "Not run".
  """
  milestone = get_chromium_milestone(repo_path)
  os.makedirs(results_dir, exist_ok=True)
  report_filename = f"{milestone}_rebase_summary.md"
  report_path = os.path.join(results_dir, report_filename)
  statuses = {phase: "Not run" for phase, _, _ in _PHASES}
  statuses.update(phase_status or {})
  phase_rows = "\n".join(
      f"| **{phase}** | {stage} | {desc} | {statuses[phase]} |"
      for phase, stage, desc in _PHASES)

  changes_section = ""
  if session_changes:
    phase_counts = collections.Counter(c.phase for c in session_changes)
    applied = [c for c in session_changes if c.applied_cleanly]
    not_applied = len(session_changes) - len(applied)
    applied_then_failed = sum(1 for c in applied if c.error is not None)
    phase_breakdown = ", ".join(f"`{k}`: {v}" for k, v in phase_counts.items())
    changes_section = f"""
## 3. Autonomous Change Trajectory Summary
- **Total Changes Recorded**: `{len(session_changes)}`
- **Patches Applied**: `{len(applied)}`
- **Applied Patches Still Followed by an Error**: `{applied_then_failed}`
- **Patches Not Applied** (rejected or not matching): `{not_applied}`
- **Changes by Phase**: {phase_breakdown}
"""

  needs_human = [
      c.error[len(NEEDS_HUMAN_PREFIX):]
      for c in session_changes or []
      if c.error and c.error.startswith(NEEDS_HUMAN_PREFIX)
  ]
  human_section = ""
  if needs_human:
    human_section = ("\n## Needs Human Decision\n" +
                     "".join(f"- {r}\n" for r in needs_human))

  usage_section = ""
  if token_usage is not None:
    usage_section = ("\n## 4. Token Usage\n" +
                     token_usage.format_summary_table() + "\n")

  change_log_section = format_change_log(session_changes or [])

  content = f"""# Cobalt {milestone} Rebase Resolution & Verification Report

## 1. Executive Summary
- **Status**: **{status}**
- **Milestone**: `{milestone}`
- **Platform**: `{platform}`
- **Build Type**: `{build_type}`
- **Target**: `{target}`
- **Workhorse Model**: `{model}`
- **Expert Model**: `{expert_model}`
- **Total Execution Time**: `{elapsed_seconds:.1f}s`
{human_section}
## 2. Rebase Pipeline Stages
| Phase | Stage | Description | Status |
| :--- | :--- | :--- | :--- |
{phase_rows}
{changes_section}{usage_section}{change_log_section}"""
  try:
    with open(report_path, "w", encoding="utf-8") as f:
      f.write(content)
    log.info("[pipeline] [REPORT] Report written to: %s", report_path)
  except OSError as e:
    log.warning("[pipeline] [WARNING] Could not write report: %s", e)
  return report_path


def build_arg_parser() -> argparse.ArgumentParser:
  """Constructs the CLI argument parser for the rebase pipeline."""
  # This file lives in src/cobalt/devinfra/rebase/.
  default_src_dir = os.path.abspath(
      os.path.join(os.path.dirname(__file__), "..", "..", ".."))
  default_cobalt_root = os.path.abspath(os.path.join(default_src_dir, ".."))

  parser = argparse.ArgumentParser(
      description="Automated Cobalt Chromium Rebase Pipeline Runner.")
  parser.add_argument(
      "--repo-path",
      default=default_src_dir,
      help="Path to Cobalt src repository.",
  )
  parser.add_argument(
      "--cobalt-root",
      default=default_cobalt_root,
      help="Path to Cobalt workspace root.",
  )
  parser.add_argument(
      "--platform",
      default="android-arm",
      help="Platform for cobalt/build/gn.py (e.g. android-arm, linux-x64x11)",
  )
  parser.add_argument(
      "--build-type",
      default="devel",
      help="Build type for cobalt/build/gn.py (default: devel)",
  )
  parser.add_argument(
      "--target",
      default="cobalt",
      help="Target executable to build (default: cobalt)",
  )
  parser.add_argument(
      "--project-id",
      default=os.environ.get("GCP_PROJECT") or
      os.environ.get("GOOGLE_CLOUD_PROJECT"),
      help="GCP Project ID for Vertex AI Reasoning Engine.",
  )
  parser.add_argument(
      "--location",
      default=os.environ.get("GCP_LOCATION", "global"),
      help="Vertex AI Region (default: global).",
  )
  parser.add_argument(
      "--reasoning-engine-id",
      default=os.environ.get("REASONING_ENGINE_ID") or
      os.environ.get("REASONING_ENGINE_RESOURCE_ID"),
      help="Hosted Vertex AI Reasoning Engine resource ID or full name.",
  )
  parser.add_argument(
      "--skills-dir",
      default=None,
      help=(
          "Directory path containing declarative rebase skills markdown files."
      ),
  )
  parser.add_argument(
      "--model",
      required=True,
      help="Workhorse model used for most fixes.",
  )
  parser.add_argument(
      "--expert-model",
      required=True,
      help="Tier-2 expert model used when a diagnostic repeats.",
  )
  parser.add_argument(
      "--skip-conflicts",
      action="store_true",
      help="Skip Phase 1 (Conflict resolution).",
  )
  parser.add_argument(
      "--skip-sync",
      action="store_true",
      help="Skip Phase 2 (gclient sync -D).",
  )
  parser.add_argument(
      "--skip-gn",
      action="store_true",
      help="Skip Phase 3 (cobalt/build/gn.py).",
  )
  parser.add_argument(
      "--skip-build",
      action="store_true",
      help="Skip Phase 4 (autoninja compiler loop).",
  )
  parser.add_argument(
      "--max-gn-iterations",
      type=int,
      default=50,
      help="Max GN self-healing iterations (default: 50)",
  )
  parser.add_argument(
      "--max-build-iterations",
      type=int,
      default=60,
      help="Max autoninja compiler self-healing iterations (default: 60)",
  )
  parser.add_argument(
      "--mode",
      choices=["resolve-conflicts", "full-pipeline", "gn-gen", "build-only"],
      default=None,
      help=("Execution mode: 'resolve-conflicts' (Phase 1 only, heals "
            "conflicts), 'gn-gen' (Phases 1-3), 'build-only' (Phase 4), "
            "or 'full-pipeline' (Phases 1-4)."),
  )
  parser.add_argument(
      "--local",
      action="store_true",
      default=os.environ.get("REBASE_LOCAL", "").lower() in ("1", "true"),
      help="Run Reasoning Engine in-process locally without hosted deployment.",
  )
  parser.add_argument(
      "--commit-per-fix",
      action="store_true",
      default=False,
      help=("Commit every resolved conflict file and every fixed build "
            "error on its own, with the reason in the commit message, the "
            "way roll fixes are landed by hand. Off by default so local "
            "runs leave the working tree uncommitted."),
  )
  return parser


class DependentPhaseError(RuntimeError):
  """gclient sync or gn gen, re-run after a patch, failed."""


def restore_files_outside_build(repo_path: str, base_sha: str, out_dir: str,
                                target: str) -> List[str]:
  """Puts changed files that `target` does not build back to base_sha.

  The agent may change files the built target never compiles, such as web
  test expectations. One `gn gen --ide=json` lists the files of every
  target; a changed file outside the dependency closure of `target` is
  checked out from base_sha. The agent only edits existing files, so
  `git diff` lists every change. GN files are kept (gn reads them, but they
  are no target's source), and so is DEPS (gclient sync reads it). Returns
  the restored paths.

  base_sha must be the commit at the end of Phase 1, not the start of the
  run: Phase 1 resolves conflicts in files the target does not build, and
  restoring those to the start would bring the conflict markers back.
  """

  def run(cmd: List[str]) -> subprocess.CompletedProcess:
    return subprocess.run(
        cmd, cwd=repo_path, capture_output=True, text=True, check=False)

  changed = [
      rel for rel in run([
          "git", "diff", "--name-only", "--ignore-submodules", base_sha
      ]).stdout.splitlines()
      if rel != "DEPS" and not rel.endswith((".gn", ".gni"))
  ]
  if not changed:
    return []
  # A scratch build dir with the same args, so out/<out_dir> is untouched.
  with tempfile.TemporaryDirectory(
      dir=os.path.join(repo_path, "out")) as build_dir:
    shutil.copy(os.path.join(repo_path, "out", out_dir, "args.gn"), build_dir)
    if run(["gn", "gen", build_dir, "--ide=json"]).returncode != 0:
      log.warning("[WARNING] gn gen --ide=json failed; keeping all files.")
      return []
    with open(os.path.join(build_dir, "project.json"), encoding="utf-8") as f:
      targets = json.load(f)["targets"]

  # Default-toolchain labels have no "(toolchain)" suffix, so this finds
  # e.g. //cobalt/android:cobalt_apk and not its copies in other toolchains.
  roots = [label for label in targets if label.endswith(":" + target)]
  if len(roots) != 1:
    log.warning("[WARNING] No unique GN label for %s; keeping all files.",
                target)
    return []
  built, seen, pending = set(), set(), roots
  while pending:
    label = pending.pop()
    if label in seen:
      continue
    seen.add(label)
    info = targets[label]
    for key in ("sources", "inputs", "public"):
      # "public" is "*" when the target declares no public headers.
      if isinstance(info.get(key), list):
        built.update(info[key])
    pending.extend(info.get("deps", []))

  restored = [rel for rel in changed if "//" + rel not in built]
  if restored:
    run(["git", "checkout", base_sha, "--", *restored])
  return restored


def run_pipeline(args: argparse.Namespace) -> int:
  """Executes the end-to-end multi-phase Cobalt rebase pipeline."""
  if args.mode == "resolve-conflicts":
    args.skip_sync = True
    args.skip_gn = True
    args.skip_build = True
  elif args.mode == "gn-gen":
    args.skip_build = True
  elif args.mode == "build-only":
    args.skip_conflicts = True
    args.skip_sync = True
    args.skip_gn = True

  results_dir = os.path.join(args.repo_path, "out", "rebase_results")
  out_dir = f"{args.platform}_{args.build_type}"
  effective_target = args.target
  if effective_target == "cobalt" and args.platform.startswith("android"):
    effective_target = "cobalt_apk"

  start_time = time.time()
  # Files the agent changed but the target does not build are put back to
  # this commit after Phase 4. It is moved to the end of Phase 1 below, so
  # that conflicts resolved there stay resolved.
  restore_sha = head_sha(args.repo_path)
  log.info("=" * 80)
  log.info("[START] STARTING AUTOMATED COBALT CHROMIUM REBASE PIPELINE")
  if args.reasoning_engine_id:
    log.info("  - Reasoning Engine: %s", args.reasoning_engine_id)
  else:
    log.info("  - Workhorse Model: %s", args.model)
    log.info("  - Expert Model:    %s", args.expert_model)
  log.info("  - Platform:   %s", args.platform)
  log.info("  - Config:     %s", args.build_type)
  log.info("  - Out Dir:    out/%s", out_dir)
  log.info("  - Target:     %s", effective_target)
  log.info("  - Commit per fix: %s", args.commit_per_fix)
  log.info("=" * 80)

  # -------------------------------------------------------------------------
  # REASONING ENGINE & RESOLVER SETUP
  # -------------------------------------------------------------------------
  reasoning_engine = ReasoningEngineClient(
      resource_id=args.reasoning_engine_id,
      project_id=args.project_id,
      location=args.location,
      flash_model=args.model,
      expert_model=args.expert_model,
      skills_dir=args.skills_dir,
      local=args.local,
  )

  # Omniscient change trajectory tracked across all phases
  shared_session_changes: List[AgentChangeRecord] = []

  # Phase 1: Conflict Resolver
  conflict_resolver = ConflictResolver(
      repo_path=args.repo_path,
      engine=reasoning_engine,
      session_changes=shared_session_changes,
      commit_per_fix=args.commit_per_fix,
  )

  # Phase 2: Shared Sync Resolver
  sync_resolver = GClientSyncResolver(
      repo_path=args.repo_path,
      engine=reasoning_engine,
      session_changes=shared_session_changes,
      max_iterations=10,
      commit_per_fix=args.commit_per_fix,
  )

  def rerun(resolver, failure: str) -> None:
    """Re-runs an earlier phase after a patch to a file it depends on.

    Its patches are part of the fix that caused the re-run, so it does not
    commit on its own: the outer loop collects them into that fix's commit.
    """
    prev, resolver.commit_per_fix = resolver.commit_per_fix, False
    try:
      if not resolver.run_resolution_loop():
        raise DependentPhaseError(failure)
    finally:
      resolver.commit_per_fix = prev

  def on_gn_patch_applied(modified_files: List[str]) -> None:
    """Triggered if GN healing touches DEPS or other dependency files."""
    if any(os.path.basename(f) == "DEPS" for f in modified_files):
      log.info("[Phase 3] DEPS was modified by GN fix. Re-running gclient "
               "sync...")
      rerun(sync_resolver, "gclient sync failed after a GN fix")

  # Phase 3: Shared GN Resolver
  gn_resolver = GNGenResolver(
      repo_path=args.repo_path,
      platform=args.platform,
      build_type=args.build_type,
      gn_check=True,
      max_iterations=args.max_gn_iterations,
      engine=reasoning_engine,
      session_changes=shared_session_changes,
      on_patch_applied_fn=on_gn_patch_applied,
      commit_per_fix=args.commit_per_fix,
  )

  def on_build_patch_applied(modified_files: List[str]) -> None:
    """Triggered if compiler loop touches DEPS or GN build files."""
    if any(os.path.basename(f) == "DEPS" for f in modified_files):
      log.info("[Phase 4] DEPS modified by compiler fix. Re-running "
               "gclient sync...")
      rerun(sync_resolver, "gclient sync failed after a build fix")

    if any(f.endswith((".gn", ".gni", ".star")) for f in modified_files):
      log.info("[Phase 4] Build files modified. Re-running GN "
               "generation...")
      rerun(gn_resolver, "gn gen failed after a build fix")

  # Phase 4: autoninja Compiler Resolver
  autoninja_resolver = AutoninjaResolver(
      repo_path=args.repo_path,
      out_dir=out_dir,
      target=effective_target,
      max_iterations=args.max_build_iterations,
      engine=reasoning_engine,
      session_changes=shared_session_changes,
      on_patch_applied_fn=on_build_patch_applied,
      commit_per_fix=args.commit_per_fix,
  )

  def dump_change_history() -> None:
    history_file = os.path.join(results_dir, "change_history.json")
    try:
      os.makedirs(os.path.dirname(history_file), exist_ok=True)
      with open(history_file, "w", encoding="utf-8") as f:
        json.dump([rec.to_dict() for rec in shared_session_changes],
                  f,
                  indent=2)
      log.info("  - Change History: %s (%s records)", history_file,
               len(shared_session_changes))
    except OSError as e:
      log.info("  - Warning: Failed to write change history: %s", e)

  phase_status: Dict[str, str] = {
      phase: "Skipped" for phase, skipped in (
          ("Phase 1", args.skip_conflicts),
          ("Phase 2", getattr(args, "skip_sync", False)),
          ("Phase 3", args.skip_gn),
          ("Phase 4", args.skip_build),
      ) if skipped
  }

  def failure_status(phase: str) -> str:
    stopped = any(
        c.error and c.error.startswith(NEEDS_HUMAN_PREFIX)
        for c in shared_session_changes)
    label = "NEEDS HUMAN" if stopped else "FAILED"
    phase_status[phase.split(":")[0]] = f"[{label}]"
    return f"{label} ({phase})"

  def run_phase(phase: str, label: str, resolver) -> bool:
    """Runs one phase loop; on failure writes the report and returns False.

    A DependentPhaseError (gclient sync or gn gen re-run after a patch
    failed) fails the phase instead of letting it continue against a
    broken checkout or build graph.
    """
    try:
      ok = resolver.run_resolution_loop()
      detail = label
    except DependentPhaseError as e:
      ok, detail = False, f"{label}: {e}"
    if not ok:
      log.warning("[FAIL] %s %s failed.", phase, label)
      write_report(failure_status(f"{phase}: {detail}"))
      return False
    phase_status[phase] = "[OK] Completed"
    log.info("[OK] %s Completed Successfully.", phase)
    return True

  def write_report(status_str: str) -> str:
    dump_change_history()
    return write_rebase_report(
        results_dir=results_dir,
        platform=args.platform,
        build_type=args.build_type,
        target=effective_target,
        model=args.model,
        expert_model=args.expert_model,
        session_changes=shared_session_changes,
        status=status_str,
        elapsed_seconds=time.time() - start_time,
        phase_status=phase_status,
        repo_path=args.repo_path,
        token_usage=reasoning_engine.token_usage,
    )

  # -------------------------------------------------------------------------
  # PHASE 1: Unified Conflict Resolution (DEPS + Source)
  # -------------------------------------------------------------------------
  if not args.skip_conflicts:
    log.info("\n%s", "=" * 80)
    log.info("[PHASE] PHASE 1: Unified Conflict Resolution (DEPS + "
             "Source)")
    log.info("=" * 80)
    if not run_phase("Phase 1", "Conflict Resolution", conflict_resolver):
      return 1
    # Checkpoint Phase 1 resolutions into a clean commit so HEAD is valid.
    # With --commit-per-fix each file was already committed and this only
    # catches anything left over.
    if commit_files(args.repo_path, None,
                    "[AI] Checkpoint: Resolved Phase 1 merge conflicts"):
      log.info("  [OK] Created clean baseline checkpoint commit for Phase 1.")
    restore_sha = head_sha(args.repo_path)

  # -------------------------------------------------------------------------
  # PHASE 2: Toolchain & Dependency Sync: gclient sync -D
  # -------------------------------------------------------------------------
  if not getattr(args, "skip_sync", False):
    log.info("\n%s", "=" * 80)
    log.info("[PHASE] PHASE 2: Toolchain & Dependency Sync (gclient sync "
             "-D)")
    log.info("=" * 80)
    if not run_phase("Phase 2", "gclient sync", sync_resolver):
      return 1

  # -------------------------------------------------------------------------
  # PHASE 3: GN Generation & Header Verification (cobalt/build/gn.py)
  # -------------------------------------------------------------------------
  if not args.skip_gn:
    log.info("\n%s", "=" * 80)
    log.info("[PHASE] PHASE 3: GN Build Generation (cobalt/build/gn.py)")
    log.info("=" * 80)
    if not run_phase("Phase 3", "GN Generation", gn_resolver):
      return 1

  # -------------------------------------------------------------------------
  # PHASE 4: autoninja Compiler Self-Healing Loop
  # -------------------------------------------------------------------------
  if not args.skip_build:
    log.info("\n%s", "=" * 80)
    log.info("[PHASE] PHASE 4: autoninja Compiler Loop (Target: %s)",
             effective_target)
    log.info("=" * 80)
    if not run_phase("Phase 4", "Compiler Loop", autoninja_resolver):
      return 1
    restored = restore_files_outside_build(args.repo_path, restore_sha, out_dir,
                                           effective_target)
    log.info("[pipeline] Restored %d changed file(s) that %s does not build:",
             len(restored), effective_target)
    for rel in restored:
      log.info("  - %s", rel)
    if restored and args.commit_per_fix:
      # The edits being undone are already in fix commits, so the undo
      # needs a commit of its own.
      commit_files(
          args.repo_path, restored,
          f"Revert agent edits to files {effective_target} does not build",
          "These files were changed by a build fix but are not in the "
          f"dependency closure of {effective_target}, so the change was "
          "never compiled. They are put back to the end of Phase 1.\n\n"
          "Files: " + ", ".join(restored))

  elapsed = time.time() - start_time
  summary_path = write_report("SUCCESS (All Phases Complete)")

  log.info("\n%s", "=" * 80)
  log.info("[SUCCESS] PIPELINE COMPLETED CLEANLY in %.1fs!", elapsed)
  log.info("[REPORT] Summary Report: %s", summary_path)
  log.info("=" * 80)
  return 0


def main():
  """Main CLI entry point for Cobalt rebase pipeline."""
  # Resolver modules log through `logging`; keep their output identical to
  # the previous plain stderr prints.
  logging.basicConfig(
      level=logging.INFO, format="%(message)s", stream=sys.stderr)
  parser = build_arg_parser()
  args = parser.parse_args()
  sys.exit(run_pipeline(args))


if __name__ == "__main__":
  main()
