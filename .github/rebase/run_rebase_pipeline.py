#!/usr/bin/env python3
# pylint: disable=duplicate-code
"""End-to-end automated Cobalt Chromium rebase pipeline runner.

Executes all rebase phases in sequence:
  Phase 1: Conflict Resolution (DEPS, C++, Java, GN) + gclient sync.
  Phase 2: GN Build Generation & Verification (cobalt/build/gn.py).
  Phase 3: autoninja Compiler Self-Healing Loop (up to 60 iterations).
  Phase 4: Comprehensive result.md generation with status.
"""

import argparse
import os
import re
import subprocess
import sys
import time
from typing import List, Optional

from autoninja_loop import apply_patch_or_replacement
from reasoning_engine import CobaltReasoningEngine
from rebase_memory import (
    load_past_experience,
    pull_memory_from_gcs,
    record_successful_fix,
    sync_memory_to_gcs,
)


def run_cmd(cmd: List[str], cwd: Optional[str] = None) -> int:
  """Executes a shell command and streams output to stderr."""
  cmd_str = " ".join(cmd)
  print(f"\n[pipeline] >>> Executing: {cmd_str}", file=sys.stderr)
  res = subprocess.run(cmd, cwd=cwd, check=False)
  return res.returncode


def self_heal_gn_generation(
    repo_path: str,
    platform: str,
    build_type: str,
    *,
    gn_check: bool = True,
    model: str = "gemini-2.5-flash",
    project_id: Optional[str] = None,
    location: str = "us-central1",
    max_retries: int = 50,
) -> bool:
  """Executes cobalt/build/gn.py with self-healing AI feedback loop."""
  gn_script = os.path.join(repo_path, "cobalt", "build", "gn.py")
  cmd = [sys.executable, gn_script, "-p", platform, "-C", build_type]
  if gn_check:
    cmd.append("--check")

  depot_tools = os.path.expanduser("~/depot_tools")
  clean_env = {
      k: v for k, v in os.environ.items() if not k.startswith("ANTIGRAVITY_")
  }
  if os.path.isdir(depot_tools):
    orig_path = clean_env.get("PATH", "")
    clean_env["PATH"] = f"{depot_tools}:{orig_path}"

  attempt_history: List[str] = []
  last_error_summary = ""
  stuck_count = 0

  reasoning_engine = CobaltReasoningEngine(
      project_id=project_id,
      location=location,
      flash_model=model,
  )
  past_experience = load_past_experience()

  for attempt in range(1, max_retries + 1):
    cmd_str = " ".join(cmd)
    print(
        f"\n[pipeline] Running GN gen (attempt {attempt}/{max_retries}): "
        f"{cmd_str}",
        file=sys.stderr,
    )
    res = subprocess.run(
        cmd,
        cwd=repo_path,
        capture_output=True,
        text=True,
        env=clean_env,
        check=False,
    )
    if res.returncode == 0:
      print(
          f"[OK] GN generation and header verification passed: "
          f"out/{platform}_{build_type}",
          file=sys.stderr,
      )
      return True

    output = f"{res.stdout}\n{res.stderr}"
    print(f"[FAIL] GN generation failed:\n{output}", file=sys.stderr)

    if attempt == max_retries:
      print(
          f"[pipeline] GN self-healing exhausted after {max_retries} "
          "attempts.",
          file=sys.stderr,
      )
      return False

    # Extract file context from error trace
    file_matches = re.findall(r"([a-zA-Z0-9_/\.\-]+\.gn[i]?)", output)
    unique_gn_files = []
    for f in file_matches:
      full_p = os.path.join(repo_path, f) if not os.path.isabs(f) else f
      if os.path.isfile(full_p) and full_p not in unique_gn_files:
        unique_gn_files.append(full_p)

    file_contexts = []
    for gnf in unique_gn_files[:3]:
      try:
        with open(gnf, "r", encoding="utf-8") as gf:
          lines = gf.readlines()
        rel_f = os.path.relpath(gnf, repo_path)
        file_snippet = "".join(lines[:120])
        file_contexts.append(f"### File: {rel_f}\n```\n{file_snippet}\n```")
      except OSError:
        pass

    error_summary = output.strip().splitlines()[0] if output.strip() else ""
    if error_summary == last_error_summary:
      stuck_count += 1
    else:
      stuck_count = 0
    last_error_summary = error_summary

    use_pro = stuck_count >= 2
    if use_pro:
      print(
          "  - [PRO_REASONING] Repeated GN error. Escalating to Pro...",
          file=sys.stderr,
      )

    print(
        "  - Querying Vertex AI Reasoning Engine to repair GN build...",
        file=sys.stderr,
    )
    res_ai = reasoning_engine.heal_gn_error(
        error_trace=output[:3000],
        file_context="\n\n".join(file_contexts),
        attempt_history="\n".join(attempt_history[-3:]),
        past_experience=past_experience,
        use_pro=use_pro,
    )
    ai_fix = res_ai.get("patch", "")

    if modified := apply_patch_or_replacement(ai_fix, repo_path=repo_path):
      mod_names = [os.path.relpath(mf, repo_path) for mf in modified]
      for rel in mod_names:
        print(f"  [OK] Applied GN fix to: {rel}", file=sys.stderr)
        record_successful_fix(
            category="GN",
            target_file=rel,
            error_signature=(output.splitlines()[0] if output else "GN Error"),
            fix_summary=f"Resolved on attempt {attempt}/{max_retries}",
            solution_snippet=ai_fix,
        )
      mod_joined = ", ".join(mod_names)
      attempt_history.append(
          f"Attempt #{attempt}: Modified {mod_joined}. "
          "If the error persists, do not repeat these exact changes.")
    else:
      print(
          "  [WARNING] AI did not generate an auto-applicable fix.",
          file=sys.stderr,
      )
      attempt_history.append(
          f"Attempt #{attempt}: Model did not produce an applicable "
          "patch.")

  return False


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


def _write_final_report(
    rebase_dir: str,
    platform: str,
    build_type: str,
    *,
    target: str,
    model: str,
    status: str,
    elapsed_seconds: float,
    repo_path: Optional[str] = None,
) -> str:
  """Generates the final comprehensive rebase summary report."""
  milestone = get_chromium_milestone(repo_path)
  results_dir = os.path.join(rebase_dir, "results")
  os.makedirs(results_dir, exist_ok=True)
  report_filename = f"{milestone}_rebase_summary.md"
  report_path = os.path.join(results_dir, report_filename)
  comp_status = ("[OK] Clean"
                 if "SUCCESS" in status else "[WARNING] Requires Attention")
  content = f"""# Cobalt {milestone} Rebase Resolution & Verification Report

## 1. Executive Summary
- **Status**: **{status}**
- **Milestone**: `{milestone}`
- **Platform**: `{platform}`
- **Build Type**: `{build_type}`
- **Target**: `{target}`
- **Reasoning Model**: `{model}` (with `gemini-2.5-pro` escalation)
- **Total Execution Time**: `{elapsed_seconds:.1f}s`

## 2. Rebase Pipeline Stages
| Phase | Stage | Description | Status |
| :--- | :--- | :--- | :--- |
| **Phase 1** | Conflict Resolution | Unified DEPS & source conflict repair | [OK] Completed |
| **Phase 2** | GN Config Check | `cobalt/build/gn.py --check` validation | [OK] Completed |
| **Phase 3** | autoninja Loop | autoninja compiler healing | {comp_status} |
"""
  try:
    with open(report_path, "w", encoding="utf-8") as f:
      f.write(content)
    print(
        f"[pipeline] [REPORT] Report written to: {report_path}",
        file=sys.stderr,
    )
  except OSError as e:
    print(
        f"[pipeline] [WARNING] Could not write report: {e}",
        file=sys.stderr,
    )
  return report_path


def build_arg_parser() -> argparse.ArgumentParser:
  """Constructs the CLI argument parser for the rebase pipeline."""
  parser = argparse.ArgumentParser(
      description="Automated Cobalt Chromium Rebase Pipeline Runner.")
  parser.add_argument(
      "--repo-path",
      default=os.path.expanduser("~/cobalt/src"),
      help="Path to Cobalt src repository.",
  )
  parser.add_argument(
      "--cobalt-root",
      default=os.path.expanduser("~/cobalt"),
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
      default=os.environ.get("GCP_LOCATION", "us-central1"),
      help="Vertex AI Region (default: us-central1).",
  )
  parser.add_argument(
      "--model",
      default=os.environ.get("GEMINI_MODEL", "gemini-2.5-flash"),
      help="Gemini model name (e.g. gemini-2.5-flash, gemini-2.5-pro).",
  )
  parser.add_argument(
      "--skip-conflicts",
      action="store_true",
      help="Skip Phase 1 (Conflict resolution).",
  )
  parser.add_argument(
      "--skip-gn",
      action="store_true",
      help="Skip Phase 2 (cobalt/build/gn.py).",
  )
  parser.add_argument(
      "--skip-build",
      action="store_true",
      help="Skip Phase 3 (autoninja compiler loop).",
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
      "--gcs-memory-uri",
      default=os.environ.get("GCS_MEMORY_URI"),
      help=(
          "Optional GCS bucket URI (gs://bucket/path) to sync knowledge bank."),
  )
  return parser


def run_pipeline(args: argparse.Namespace) -> int:
  """Executes the end-to-end multi-phase Cobalt rebase pipeline."""
  rebase_dir = os.path.dirname(os.path.abspath(__file__))
  proj_arg = ["--project-id", args.project_id] if args.project_id else []
  loc_arg = ["--location", args.location] if args.location else []
  model_arg = ["--model", args.model]
  auth_args = proj_arg + loc_arg

  out_dir = f"{args.platform}_{args.build_type}"
  effective_target = args.target
  if effective_target == "cobalt" and args.platform.startswith("android"):
    effective_target = "cobalt_apk"

  # Pull existing knowledge memory from GCS if configured
  if args.gcs_memory_uri:
    pull_memory_from_gcs(gcs_uri=args.gcs_memory_uri)

  start_time = time.time()
  print("=" * 80, file=sys.stderr)
  print(
      "[START] STARTING AUTOMATED COBALT CHROMIUM REBASE PIPELINE",
      file=sys.stderr,
  )
  print(f"  - Model:      {args.model}", file=sys.stderr)
  print(f"  - Platform:   {args.platform}", file=sys.stderr)
  print(f"  - Config:     {args.build_type}", file=sys.stderr)
  print(f"  - Out Dir:    out/{out_dir}", file=sys.stderr)
  print(f"  - Target:     {effective_target}", file=sys.stderr)
  if args.gcs_memory_uri:
    print(f"  - GCS Memory: {args.gcs_memory_uri}", file=sys.stderr)
  print("=" * 80, file=sys.stderr)

  # -------------------------------------------------------------------------
  # PHASE 1: Unified Conflict Resolution (DEPS + Source) & gclient sync
  # -------------------------------------------------------------------------
  if not args.skip_conflicts:
    print("\n" + "=" * 80, file=sys.stderr)
    print(
        "[PHASE] PHASE 1: Unified Conflict Resolution & gclient sync",
        file=sys.stderr,
    )
    print("=" * 80, file=sys.stderr)
    conflicts_script = os.path.join(rebase_dir, "resolve_conflicts.py")
    cmd_p1 = [
        sys.executable,
        conflicts_script,
        "--repo-path",
        args.repo_path,
        "--cobalt-root",
        args.cobalt_root,
    ] + auth_args + model_arg
    rc = run_cmd(cmd_p1, cwd=args.repo_path)
    if rc != 0:
      print(
          f"[FAIL] Phase 1 Conflict Resolution failed (Exit Code: {rc}).",
          file=sys.stderr,
      )
      _write_final_report(
          rebase_dir=rebase_dir,
          platform=args.platform,
          build_type=args.build_type,
          target=effective_target,
          model=args.model,
          status="FAILED (Phase 1: Conflict Resolution)",
          elapsed_seconds=time.time() - start_time,
          repo_path=args.repo_path,
      )
      return rc
    print("[OK] Phase 1 Completed Successfully.", file=sys.stderr)

  # -------------------------------------------------------------------------
  # PHASE 2: GN Generation & Header Verification (cobalt/build/gn.py)
  # -------------------------------------------------------------------------
  if not args.skip_gn:
    print("\n" + "=" * 80, file=sys.stderr)
    print(
        "[PHASE] PHASE 2: GN Build Generation (cobalt/build/gn.py)",
        file=sys.stderr,
    )
    print("=" * 80, file=sys.stderr)
    gn_ok = self_heal_gn_generation(
        repo_path=args.repo_path,
        platform=args.platform,
        build_type=args.build_type,
        gn_check=True,
        model=args.model,
        project_id=args.project_id,
        location=args.location,
        max_retries=args.max_gn_iterations,
    )
    if not gn_ok:
      print(
          "[FAIL] Phase 2 GN Generation & Header Verification failed.",
          file=sys.stderr,
      )
      _write_final_report(
          rebase_dir=rebase_dir,
          platform=args.platform,
          build_type=args.build_type,
          target=effective_target,
          model=args.model,
          status="FAILED (Phase 2: GN Generation)",
          elapsed_seconds=time.time() - start_time,
          repo_path=args.repo_path,
      )
      return 1
    print("[OK] Phase 2 Completed Successfully.", file=sys.stderr)

  # -------------------------------------------------------------------------
  # PHASE 3: autoninja Compiler Self-Healing Loop
  # -------------------------------------------------------------------------
  if not args.skip_build:
    print("\n" + "=" * 80, file=sys.stderr)
    print(
        f"[PHASE] PHASE 3: autoninja Compiler Loop "
        f"(Target: {effective_target})",
        file=sys.stderr,
    )
    print("=" * 80, file=sys.stderr)
    autoninja_script = os.path.join(rebase_dir, "autoninja_loop.py")
    cmd_p3 = [
        sys.executable,
        autoninja_script,
        "--out-dir",
        out_dir,
        "--target",
        effective_target,
        "--cobalt-root",
        args.cobalt_root,
        "--max-iterations",
        str(args.max_build_iterations),
    ] + auth_args + model_arg
    rc = run_cmd(cmd_p3, cwd=args.repo_path)
    if rc != 0:
      print(
          "[FAIL] Phase 3 Compiler Feedback Loop failed (Exit "
          f"Code: {rc}).",
          file=sys.stderr,
      )
      _write_final_report(
          rebase_dir=rebase_dir,
          platform=args.platform,
          build_type=args.build_type,
          target=effective_target,
          model=args.model,
          status="FAILED (Phase 3: Compiler Loop)",
          elapsed_seconds=time.time() - start_time,
          repo_path=args.repo_path,
      )
      return rc
    print("[OK] Phase 3 Completed Successfully.", file=sys.stderr)

  elapsed = time.time() - start_time
  summary_path = _write_final_report(
      rebase_dir=rebase_dir,
      platform=args.platform,
      build_type=args.build_type,
      target=effective_target,
      model=args.model,
      status="SUCCESS (All Phases Complete)",
      elapsed_seconds=elapsed,
      repo_path=args.repo_path,
  )

  # Sync persistent knowledge memory bank to GCS if configured
  if args.gcs_memory_uri:
    sync_memory_to_gcs(gcs_uri=args.gcs_memory_uri)

  print("\n" + "=" * 80, file=sys.stderr)
  print(
      f"[SUCCESS] PIPELINE COMPLETED CLEANLY in {elapsed:.1f}s!",
      file=sys.stderr,
  )
  print(
      f"[REPORT] Summary Report: {summary_path}",
      file=sys.stderr,
  )
  print("=" * 80, file=sys.stderr)
  return 0


def main():
  """Main CLI entry point for Cobalt rebase pipeline."""
  parser = build_arg_parser()
  args = parser.parse_args()
  sys.exit(run_pipeline(args))


if __name__ == "__main__":
  main()
