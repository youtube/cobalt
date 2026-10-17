#!/usr/bin/env python3
# Copyright 2026 The Cobalt Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Automated Memory Win Hunter sweep campaign orchestrator for Cobalt."""

import argparse
import json
import logging
import os
import random
import re
import subprocess
import sys
import tempfile
import time


def find_repository_root():
  """Dynamically identifies Git workspace repository root."""
  script_dir = os.path.dirname(os.path.abspath(__file__))
  current = script_dir
  while current != os.path.dirname(current):
    if os.path.exists(os.path.join(current, "cobalt")) and os.path.exists(
        os.path.join(current, "build")):
      return current
    current = os.path.dirname(current)
  return "/usr/local/google/home/avvall/cobalt/src"


REPO_ROOT = find_repository_root()


def run_significance_campaign(args, flag_type, flag_name, results_json):
  """Spawns verify_stat_sig.py subprocess to run a sweep experiment."""
  cmd = [
      sys.executable,
      os.path.join(REPO_ROOT, "cobalt/tools/memory_verification_rig",
                   "verify_stat_sig.py"),
      f"--platform={args.platform}",
      f"--device={args.device}",
      f"--cuj={args.cuj}",
      f"--runs={args.runs}",
      f"--duration={args.duration}",
      f"--port={args.port}",
      f"--json-results-file={results_json}",
  ]
  if args.bin_path:
    cmd.append(f"--bin-path={args.bin_path}")
  if args.enable_granular_memory:
    cmd.append("--enable-granular-memory")

  # Baseline vs Experiment configurations mapping
  if flag_type == "feature":
    # Feature Flag toggling campaign
    cmd += [
        f"--baseline-args=--remote-allow-origins=*,--enable-metrics,"
        f"--force-enable-metrics-reporting,--disable-features={flag_name}",
        f"--experiment-args=--remote-allow-origins=*,--enable-metrics,"
        f"--force-enable-metrics-reporting,--enable-features={flag_name}",
    ]
  else:
    # CLI Switch campaign
    cmd += [
        "--baseline-args=--remote-allow-origins=*,--enable-metrics,"
        "--force-enable-metrics-reporting",
        f"--experiment-args=--remote-allow-origins=*,--enable-metrics,"
        f"--force-enable-metrics-reporting,{flag_name}",
    ]

  try:
    res = subprocess.run(cmd, capture_output=True, text=True, check=False)
    return res.stdout, res.stderr
  except OSError as e:
    return "", str(e)


def parse_campaign_results(results_json_path):
  """Reads the structured JSON results file from verify_stat_sig.py."""
  if not os.path.exists(results_json_path) or os.path.getsize(
      results_json_path) == 0:
    return None

  try:
    with open(results_json_path, "r", encoding="utf-8") as f:
      campaign_results = json.load(f)

    if not campaign_results:
      return None

    # Extract System RSS memory result or default to first metric
    rss_entry = next((r for r in campaign_results
                      if "System RSS" in r.get("metric_name", "")),
                     campaign_results[0])

    return {
        "mean_reduction_mb":
            rss_entry.get("mean_reduction_mb", 0.0),
        "p_value":
            rss_entry.get("p_value", 1.0),
        "directional_consistency":
            rss_entry.get("directional_consistency", "FAIL"),
        "is_significant":
            rss_entry.get("is_significant", False),
        "all_metrics":
            campaign_results
    }
  except (OSError, ValueError, json.JSONDecodeError) as e:
    logging.warning("  ⚠️ Warning: Failed to parse campaign results JSON: %s",
                    e)
  return None


def main():
  parser = argparse.ArgumentParser(
      description="Automated Memory Win Hunter Sweep Orchestrator")
  parser.add_argument(
      "--platform",
      default="android",
      choices=["android", "linux"],
      help="Target platform (android or linux)")
  parser.add_argument(
      "--device", default="localhost:38879", help="ADB device ID serial")
  parser.add_argument(
      "--bin-path", help="Path to Cobalt monolithic binary (Linux only)")
  parser.add_argument(
      "--runs",
      type=int,
      default=3,
      help="Number of runs per config (default: 3)")
  parser.add_argument(
      "--duration",
      type=int,
      default=45,
      help="Watch CUJ duration per run (default: 45s)")
  parser.add_argument(
      "--port", type=int, default=9222, help="Remote DevTools debugging port")
  parser.add_argument(
      "--cuj",
      default="watch",
      choices=["browse", "watch", "baseline", "combined"],
      help="Specific CUJ to utilize as sweep context (default: watch)")
  parser.add_argument(
      "--max-flags",
      type=int,
      default=10,
      help="Max number of flags to sweep in the hunt (default: 10)")
  parser.add_argument(
      "--shuffle",
      action="store_true",
      help="Exploration Sweep: Shuffles harvested config stack before slicing")
  parser.add_argument(
      "--filter", help="Regex query filter to sweep specific components")
  parser.add_argument(
      "--enable-granular-memory",
      action="store_true",
      help="Forward granular memory metrics collection to the campaign")
  args = parser.parse_args()

  logging.basicConfig(level=logging.INFO, format="%(message)s")

  # Execute Phase 1: Discover all toggleable switches/features via harvester
  logging.info("=" * 75)
  logging.info("🎯 COBALT MEMORY WIN HUNTER: HARVESTING CONFIGURATION SWITCHES")
  logging.info("=" * 75)

  harvester_cmd = [
      sys.executable,
      os.path.join(REPO_ROOT, "cobalt/tools/memory_verification_rig",
                   "harvest_flags.py"),
      f"--repo-root={REPO_ROOT}",
  ]
  subprocess.run(harvester_cmd, check=True)

  flags_file = os.path.join(REPO_ROOT, "scanned_flags.txt")
  if not os.path.exists(flags_file):
    logging.error("❌ Error: scanned_flags.txt not found. Aborting sweep.")
    sys.exit(1)

  # Read flagged configuration switches
  switches_to_sweep = []
  features_to_sweep = []
  with open(flags_file, "r", encoding="utf-8") as f:
    for line in f:
      line = line.strip()
      if line.startswith("switch:"):
        switches_to_sweep.append(line.split("switch:")[-1].strip())
      elif line.startswith("feature:"):
        features_to_sweep.append(line.split("feature:")[-1].strip())

  total_discovered = len(switches_to_sweep) + len(features_to_sweep)
  logging.info(
      "Loaded discovered configuration stack. Switches: %s | Features: %s",
      len(switches_to_sweep), len(features_to_sweep))

  # Filter switches and features if --filter is specified
  if args.filter:
    r_pattern = re.compile(args.filter, re.IGNORECASE)
    switches_to_sweep = [s for s in switches_to_sweep if r_pattern.search(s)]
    features_to_sweep = [f for f in features_to_sweep if r_pattern.search(f)]
    logging.info(
        "Filtered configuration stack (query=%s). Switches: %s | Features: %s",
        args.filter, len(switches_to_sweep), len(features_to_sweep))

  # Shuffle the lists if --shuffle is requested
  if args.shuffle:
    logging.info("Exploration Sweep: Shuffling harvested config flags stack...")
    random.shuffle(switches_to_sweep)
    random.shuffle(features_to_sweep)

  # Slice the hunt to limit runs
  target_switches = switches_to_sweep[:args.max_flags // 2]
  target_features = features_to_sweep[:args.max_flags - len(target_switches)]
  logging.info(
      "Hunting memory wins across target slice: %s total flags to sweep...",
      len(target_switches) + len(target_features))

  # Execute Phase 2: Systematic sweep experiments profiling campaigns
  logging.info("\n%s", "=" * 75)
  logging.info("🔬 LAUNCHING SYSTEMATIC SIGNIFICANCE SWEEPS CAMPAIGN")
  logging.info("%s\n", "=" * 75)

  leaderboard = []
  index = 1
  total_sweep = len(target_switches) + len(target_features)

  # 1. Sweep Switches
  for sw in target_switches:
    logging.info("[%s/%s] Sweeping CLI Switch: %s...", index, total_sweep, sw)
    tmp_fd, tmp_json = tempfile.mkstemp(suffix="_sweep_results.json")
    os.close(tmp_fd)

    run_significance_campaign(args, "switch", sw, tmp_json)
    res = parse_campaign_results(tmp_json)
    if res:
      res["flag"] = sw
      res["type"] = "Switch"
      leaderboard.append(res)
      logging.info(
          "   -> Observed Win: %.2f MB | p-value: %.4f | Significant: %s",
          res["mean_reduction_mb"], res["p_value"], res["is_significant"])
    else:
      logging.warning("   ⚠️ Failed to collect valid profiling data for: %s",
                      sw)
    if os.path.exists(tmp_json):
      os.remove(tmp_json)
    index += 1
    time.sleep(2)

  # 2. Sweep Features
  for feat in target_features:
    logging.info("[%s/%s] Sweeping Blink Feature: %s...", index, total_sweep,
                 feat)
    tmp_fd, tmp_json = tempfile.mkstemp(suffix="_sweep_results.json")
    os.close(tmp_fd)

    run_significance_campaign(args, "feature", feat, tmp_json)
    res = parse_campaign_results(tmp_json)
    if res:
      res["flag"] = feat
      res["type"] = "Feature"
      leaderboard.append(res)
      logging.info(
          "   -> Observed Win: %.2f MB | p-value: %.4f | Significant: %s",
          res["mean_reduction_mb"], res["p_value"], res["is_significant"])
    else:
      logging.warning("   ⚠️ Failed to collect valid profiling data for: %s",
                      feat)
    if os.path.exists(tmp_json):
      os.remove(tmp_json)
    index += 1
    time.sleep(2)

  # Sort leaderboard by mean reduction (highest memory savings first!)
  leaderboard.sort(key=lambda x: x["mean_reduction_mb"], reverse=True)

  # Execute Phase 3: Structured Memory Win Leaderboard generation
  logging.info("\n%s", "=" * 75)
  logging.info("📊 GENERATING RANKED MEMORY WIN LEADERBOARD REPORT")
  logging.info("%s\n", "=" * 75)

  leaderboard_path = os.path.join(REPO_ROOT, "win_leaderboard.md")
  try:
    with open(leaderboard_path, "w", encoding="utf-8") as f:
      f.write("# 🏆 Cobalt Memory Optimization Win Leaderboard\n\n")
      f.write(
          "Ranked list of configuration switches and Blink feature flags "
          "evaluated systematically via the statistical significance rig.\n\n")
      f.write("## 📊 Sweep Configurations\n")
      f.write(f"- **Platform:** {args.platform}\n")
      f.write(f"- **Runs per Group:** {args.runs}\n")
      f.write(f"- **CUJ Context:** {args.cuj}\n")
      f.write(f"- **Duration per Run:** {args.duration}s\n")
      total_sw = (f"- **Total Discoveries Swept:** {total_discovered} "
                  f"(Slice={total_sweep})\n\n")
      f.write(total_sw)

      f.write("## 🏆 Memory Optimization Leaderboard\n\n")
      headers = ("| Rank | Optimization switch / feature | Type | p-value | "
                 "paired consistency | Status | **Mean Memory savings** |\n")
      f.write(headers)
      f.write("| :---: | :--- | :---: | :---: | :---: | :---: | :--- |\n")

      for rank, entry in enumerate(leaderboard, 1):
        flag = entry["flag"]
        type_str = entry["type"]
        p_val = entry["p_value"]
        dir_str = entry["directional_consistency"]
        emoji = "🟢 SIGNIFICANT" if entry["is_significant"] else "🔴 NOT SIG"
        reduction_mb = entry["mean_reduction_mb"]
        f.write(f"| {rank} | `{flag}` | {type_str} | {p_val:.4f} | "
                f"{dir_str} | {emoji} | **{reduction_mb:.2f} MB** |\n")

      f.write("\n---\n\n")
      f.write("> [!NOTE]\n")
      note_str = ("> Memory optimizations yielding positive savings "
                  "(e.g. `> 0.00 MB`) decrease memory footprint. "
                  "Optimizations showing negative values represent memory "
                  "regressions (increase footprint).\n")
      f.write(note_str)

    logging.info("🟢 LEADERBOARD WRITTEN SUCCESSFULLY TO: %s", leaderboard_path)
  except OSError as e:
    logging.error("❌ Error: Failed to generate leaderboard report: %s", e)

  logging.info("%s\n", "=" * 75)


if __name__ == "__main__":
  main()
