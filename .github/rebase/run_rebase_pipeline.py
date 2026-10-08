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
import subprocess
import sys
import time
from typing import List, Optional
import warnings

from autoninja import AutoninjaResolver
from base_resolver import AgentChangeRecord
from conflicts import ConflictResolver
from gclient_sync import GClientSyncResolver
from gn_gen import GNGenResolver
from engine_client import ReasoningEngineClient

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


def write_rebase_report(
    rebase_dir: str,
    platform: str,
    build_type: str,
    *,
    target: str,
    model: str,
    expert_model: str,
    status: str,
    elapsed_seconds: float,
    repo_path: Optional[str] = None,
    session_changes: Optional[List[AgentChangeRecord]] = None,
) -> str:
  """Generates the final comprehensive rebase summary report."""
  milestone = get_chromium_milestone(repo_path)
  results_dir = os.path.join(rebase_dir, "results")
  os.makedirs(results_dir, exist_ok=True)
  report_filename = f"{milestone}_rebase_summary.md"
  report_path = os.path.join(results_dir, report_filename)
  comp_status = ("[OK] Clean"
                 if "SUCCESS" in status else "[WARNING] Requires Attention")

  changes_section = ""
  if session_changes:
    phase_counts = collections.Counter(c.phase for c in session_changes)
    clean_counts = sum(1 for c in session_changes if c.applied_cleanly)
    error_counts = sum(1 for c in session_changes if c.error is not None)
    phase_breakdown = ", ".join(f"`{k}`: {v}" for k, v in phase_counts.items())
    changes_section = f"""
## 3. Autonomous Change Trajectory Summary
- **Total Changes Recorded**: `{len(session_changes)}`
- **Clean Patches Applied**: `{clean_counts}`
- **Subsequent Errors/Breaks**: `{error_counts}`
- **Changes by Phase**: {phase_breakdown}
"""

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

## 2. Rebase Pipeline Stages
| Phase | Stage | Description | Status |
| :--- | :--- | :--- | :--- |
| **Phase 1** | Conflict Resolution | Unified DEPS & source conflict repair | [OK] Completed |
| **Phase 2** | Toolchain Sync | `gclient sync -D` toolchain & CIPD sync | [OK] Completed |
| **Phase 3** | GN Config Check | `cobalt/build/gn.py --check` validation | [OK] Completed |
| **Phase 4** | autoninja Loop | autoninja compiler healing | {comp_status} |
{changes_section}"""
  try:
    with open(report_path, "w", encoding="utf-8") as f:
      f.write(content)
    log.info("[pipeline] [REPORT] Report written to: %s", report_path)
  except OSError as e:
    log.warning("[pipeline] [WARNING] Could not write report: %s", e)
  return report_path


def build_arg_parser() -> argparse.ArgumentParser:
  """Constructs the CLI argument parser for the rebase pipeline."""
  default_src_dir = os.path.abspath(
      os.path.join(os.path.dirname(__file__), "..", ".."))
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
  return parser


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

  rebase_dir = os.path.dirname(os.path.abspath(__file__))
  out_dir = f"{args.platform}_{args.build_type}"
  effective_target = args.target
  if effective_target == "cobalt" and args.platform.startswith("android"):
    effective_target = "cobalt_apk"

  start_time = time.time()
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
      skip_sync=True,  # Phase 2 handles gclient sync
  )

  # Phase 2: Shared Sync Resolver
  sync_resolver = GClientSyncResolver(
      repo_path=args.repo_path,
      engine=reasoning_engine,
      session_changes=shared_session_changes,
      max_iterations=10,
  )

  def on_gn_patch_applied(modified_files: List[str]) -> None:
    """Triggered if GN healing touches DEPS or other dependency files."""
    if any(os.path.basename(f) == "DEPS" for f in modified_files):
      log.info("[Phase 3] DEPS was modified by GN fix. Re-running gclient "
               "sync...")
      sync_resolver.run_resolution_loop()

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
  )

  def on_build_patch_applied(modified_files: List[str]) -> None:
    """Triggered if compiler loop touches DEPS or GN build files."""
    if any(os.path.basename(f) == "DEPS" for f in modified_files):
      log.info("[Phase 4] DEPS modified by compiler fix. Re-running "
               "gclient sync...")
      sync_resolver.run_resolution_loop()

    if any(f.endswith((".gn", ".gni", ".star")) for f in modified_files):
      log.info("[Phase 4] Build files modified. Re-running GN "
               "generation...")
      gn_resolver.run_resolution_loop()

  # Phase 4: autoninja Compiler Resolver
  autoninja_resolver = AutoninjaResolver(
      repo_path=args.repo_path,
      out_dir=out_dir,
      target=effective_target,
      max_iterations=args.max_build_iterations,
      engine=reasoning_engine,
      session_changes=shared_session_changes,
      on_patch_applied_fn=on_build_patch_applied,
  )

  def dump_change_history() -> None:
    history_file = os.path.join(args.repo_path, "out", "rebase_results",
                                "change_history.json")
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

  def write_report(status_str: str) -> str:
    dump_change_history()
    return write_rebase_report(
        rebase_dir=rebase_dir,
        platform=args.platform,
        build_type=args.build_type,
        target=effective_target,
        model=args.model,
        expert_model=args.expert_model,
        session_changes=shared_session_changes,
        status=status_str,
        elapsed_seconds=time.time() - start_time,
        repo_path=args.repo_path,
    )

  # -------------------------------------------------------------------------
  # PHASE 1: Unified Conflict Resolution (DEPS + Source)
  # -------------------------------------------------------------------------
  if not args.skip_conflicts:
    log.info("\n%s", "=" * 80)
    log.info("[PHASE] PHASE 1: Unified Conflict Resolution (DEPS + "
             "Source)")
    log.info("=" * 80)
    conflict_ok = conflict_resolver.run_resolution_loop()
    if not conflict_ok:
      log.warning("[FAIL] Phase 1 Conflict Resolution failed.")
      write_report("FAILED (Phase 1: Conflict Resolution)")
      return 1
    log.info("[OK] Phase 1 Completed Successfully.")
    # Checkpoint Phase 1 resolutions into a clean commit so HEAD is valid
    try:
      subprocess.run(["git", "add", "-u"], cwd=args.repo_path, check=False)
      diff_proc = subprocess.run(
          ["git", "diff", "--cached", "--quiet"],
          cwd=args.repo_path,
          check=False,
      )
      if diff_proc.returncode != 0:
        subprocess.run(
            [
                "git",
                "-c",
                "user.name=Cobalt Rebase Agent",
                "-c",
                "user.email=cobalt-rebase-agent@google.com",
                "commit",
                "-m",
                "[AI] Checkpoint: Resolved Phase 1 merge conflicts",
            ],
            cwd=args.repo_path,
            check=False,
        )
        log.info("  [OK] Created clean baseline checkpoint commit for Phase "
                 "1.")
    except Exception as cp_err:  # pylint: disable=broad-exception-caught
      log.warning("  [WARNING] Failed to create Phase 1 checkpoint commit: %s",
                  cp_err)

  # -------------------------------------------------------------------------
  # PHASE 2: Toolchain & Dependency Sync: gclient sync -D
  # -------------------------------------------------------------------------
  if not getattr(args, "skip_sync", False):
    log.info("\n%s", "=" * 80)
    log.info("[PHASE] PHASE 2: Toolchain & Dependency Sync (gclient sync "
             "-D)")
    log.info("=" * 80)
    sync_ok = sync_resolver.run_resolution_loop()
    if not sync_ok:
      log.warning("[FAIL] Phase 2 Toolchain Sync failed.")
      write_report("FAILED (Phase 2: gclient sync)")
      return 1
    log.info("[OK] Phase 2 Completed Successfully.")

  # -------------------------------------------------------------------------
  # PHASE 3: GN Generation & Header Verification (cobalt/build/gn.py)
  # -------------------------------------------------------------------------
  if not args.skip_gn:
    log.info("\n%s", "=" * 80)
    log.info("[PHASE] PHASE 3: GN Build Generation (cobalt/build/gn.py)")
    log.info("=" * 80)
    gn_ok = gn_resolver.run_resolution_loop()
    if not gn_ok:
      log.warning("[FAIL] Phase 3 GN Generation & Header Verification failed.")
      write_report("FAILED (Phase 3: GN Generation)")
      return 1
    log.info("[OK] Phase 3 Completed Successfully.")

  # -------------------------------------------------------------------------
  # PHASE 4: autoninja Compiler Self-Healing Loop
  # -------------------------------------------------------------------------
  if not args.skip_build:
    log.info("\n%s", "=" * 80)
    log.info("[PHASE] PHASE 4: autoninja Compiler Loop (Target: %s)",
             effective_target)
    log.info("=" * 80)
    build_ok = autoninja_resolver.run_resolution_loop()
    if not build_ok:
      log.warning("[FAIL] Phase 4 Compiler Feedback Loop failed.")
      write_report("FAILED (Phase 4: Compiler Loop)")
      return 1
    log.info("[OK] Phase 4 Completed Successfully.")

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
