# Copyright 2026 The Cobalt Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Memory accounting coverage verification script for Cobalt.

Calculates dynamic allocation deltas by subtracting static startup
baseline snapshots from periodic UMA metric snapshots.
"""

# pylint: disable=inconsistent-quotes

import argparse
import datetime
import json
import os
import sys
import time
from collections import defaultdict


def parse_uma_histos(file_path):
  """Parses UMA snapshots file mapping metric name to count snapshots."""
  # Maps metric_name -> dict of { count -> sum }
  metrics_data = defaultdict(dict)

  if not os.path.exists(file_path):
    return metrics_data

  with open(file_path, "r", encoding="utf-8") as f:
    for line in f:
      line = line.strip()
      if not line:
        continue

      # Format: timestamp,metric_name,json_payload
      parts = line.split(",", 2)
      if len(parts) < 3:
        continue

      metric_name, json_str = parts[1], parts[2]
      if not metric_name.startswith("Memory.Cobalt.ResidentFootprint."):
        continue

      try:
        payload = json.loads(json_str)
        result = payload.get("result", {})
        histograms = result.get("histograms", [])
        for h in histograms:
          name = h.get("name")
          count = h.get("count", 0)
          s = h.get("sum", 0)
          if name and count > 0:
            if count == 1:
              # Reset tracking for this metric on new session
              metrics_data[name] = {}
            metrics_data[name][count] = s
      except json.JSONDecodeError:
        continue

  return metrics_data


def verify_accounting(file_path,
                      is_monitoring=False,
                      cuj_name=None,
                      iteration=None):
  """Evaluates granular memory tracking coverage milestones."""
  metrics_data = parse_uma_histos(file_path)

  if not metrics_data:
    if is_monitoring:
      now_str = datetime.datetime.now().strftime("%H:%M:%S")
      print(f"[{now_str}] Waiting for UMA metrics in {file_path}...")
      return False
    else:
      print(f"ERROR: No ResidentFootprint metrics in {file_path}")
      sys.exit(1)

  print("=" * 80)
  print(" COBALT GRANULAR MEMORY ACCOUNTING COVERAGE ANALYSIS".center(80))
  print("=" * 80)
  if is_monitoring:
    now_str = datetime.datetime.now().strftime("%H:%M:%S")
    print(f"Live Dashboard | Last Updated: {now_str} | Polling {file_path}")
    print("=" * 80)

  # Calculate baseline (min count) and final (max count)
  baselines = {}
  finals = {}
  deltas = {}

  for metric, counts in metrics_data.items():
    min_count = min(counts.keys())
    max_count = max(counts.keys())
    baselines[metric] = counts[min_count]
    finals[metric] = counts[max_count]
    deltas[metric] = finals[metric] - baselines[metric]

  unknown_delta = deltas.get("Memory.Cobalt.ResidentFootprint.Unknown", 0)
  subsystems_delta = sum(d for m, d in deltas.items() if "Unknown" not in m)
  total_delta = unknown_delta + subsystems_delta

  total_final = sum(finals.values())
  unknown_final = finals.get("Memory.Cobalt.ResidentFootprint.Unknown", 0)
  unknown_final_pct = (unknown_final / total_final *
                       100.0) if total_final > 0 else 0.0

  tbl_hdr = (f"{'Subsystem / Region':<28} | {'Base (MB)':<9} | "
             f"{'Final (MB)':<10} | {'Delta (MB)':<10} | "
             f"{'% Churn':<8} | {'% Total':<7}")
  print(tbl_hdr)
  print("-" * 80)

  for metric in sorted(deltas.keys()):
    subsystem = metric.replace("Memory.Cobalt.ResidentFootprint.", "")
    base_val = int(baselines[metric] / 1024.0)
    final_val = int(finals[metric] / 1024.0)
    delta_val = int(deltas[metric] / 1024.0)
    churn_pct = (deltas[metric] / total_delta *
                 100.0) if total_delta > 0 else 0.0
    total_pct = (finals[metric] / total_final *
                 100.0) if total_final > 0 else 0.0
    row = (f"{subsystem:<28} | {base_val:<9} | {final_val:<10} | "
           f"{delta_val:<10} | {churn_pct:>5.1f}%   | {total_pct:>5.1f}%")
    print(row)

  print("-" * 80)
  total_final_mb = int(total_final / 1024.0)
  total_delta_mb = int(total_delta / 1024.0)
  total_row = (
      f"{'TOTAL ALLOCATIONS / CHURN':<28} | {'-':<9} | "
      f"{total_final_mb:<10} | {total_delta_mb:<10} | 100.0%   | 100.0%")
  print(total_row)
  print("=" * 80)

  # Evaluate Milestones: 15s (3), 60s (12), 5m (60), 15m (180), etc.
  milestones = {
      "15 Seconds": 3,
      "60 Seconds": 12,
      "5 Minutes": 60,
      "15 Minutes": 180,
      "30 Minutes": 360,
      "45 Minutes": 540,
      "60 Minutes": 720
  }

  print("\n" + "=" * 80)
  print(" LONG-TERM ENDURANCE VERIFICATION MILESTONES (Goal: kUnknown < 20%)"
        .center(80))
  print("=" * 80)
  tbl_hdr_ms = (
      f"{'Milestone':<15} | {'Poll Count':<12} | {'Total (MB)':<12} | "
      f"{'kUnknown (MB)':<15} | {'% kUnknown':<12} | {'Status':<10}")
  print(tbl_hdr_ms)
  print("-" * 80)

  failed_milestones = []
  has_failed_milestones = False
  for label, target_count in milestones.items():
    # Find nearest available poll count <= target_count
    available_counts = [
        c for c in metrics_data.get("Memory.Cobalt.ResidentFootprint.Unknown",
                                    {}).keys() if c <= target_count
    ]
    if not available_counts or max(available_counts) < int(target_count * 0.8):
      print(f"{label:<15} | {target_count:<12} | {'-':<12} | "
            f"{'-':<15} | {'-':<12} | [PENDING]")
      continue

    closest_count = max(available_counts)
    m_unknown = metrics_data.get("Memory.Cobalt.ResidentFootprint.Unknown",
                                 {}).get(closest_count, 0)
    m_total = 0
    for counts in metrics_data.values():
      avail = [c for c in counts.keys() if c <= target_count]
      if avail:
        m_total += counts[max(avail)]
    m_unknown_pct = (m_unknown / m_total * 100.0) if m_total > 0 else 0.0
    status = "[PASS]" if m_unknown_pct < 20.0 else "[FAIL]"
    if m_unknown_pct >= 20.0:
      has_failed_milestones = True
      failed_milestones.append(label)
    m_total_mb = int(m_total / 1024.0)
    m_unknown_mb = int(m_unknown / 1024.0)
    row = (f"{label:<15} | {closest_count:<12} | {m_total_mb:<12} | "
           f"{m_unknown_mb:<15} | {m_unknown_pct:>5.1f}%      | {status:<10}")
    print(row)

  print("=" * 80 + "\n")

  coverage_pct = (subsystems_delta / total_delta *
                  100.0) if total_delta > 0 else 0.0
  print(f"DYNAMIC ACCOUNTING COVERAGE: {coverage_pct:.2f}% (Goal: >= 80.0%)")
  print(
      f"TOTAL UNKNOWN ALLOCATIONS:   {unknown_final_pct:.2f}% (Goal: < 20.0%)\n"
  )

  if not has_failed_milestones and unknown_final_pct < 20.0:
    print("[SUCCESS] kUnknown allocations account for < 20% at milestones!")
    print("WE ARE DONE!!!")
    return True
  else:
    print(f"[ITERATION REQUIRED] kUnknown at {unknown_final_pct:.2f}%, "
          f"or milestone failed.")
    if cuj_name and iteration and failed_milestones:
      print(f"\n[OVERALL FAILURE] CUJ: {cuj_name} | Iteration: {iteration} | "
            f"Failed Milestones: {', '.join(failed_milestones)}\n")
    if not is_monitoring:
      print("Opportunities for further exploration:")
      print("1. V8 Garbage Collector / Scavenger / Compaction worker threads.")
      print("2. Skia / Ganesh GPU Texture Uploads & Command Buffer pools.")
      print("3. Starboard / FFmpeg internal video decoding worker threads.")
    return False


def monitor_accounting(file_path, interval_seconds, p_process=None):
  """Runs live tracking metrics coverage monitoring loop."""
  print(f"Starting live monitoring dashboard for {file_path}...")
  print("Press Ctrl+C to exit.")
  time.sleep(1)

  while True:
    try:
      # Clear screen for live dashboard effect
      os.system("cls" if os.name == "nt" else "clear")
      verify_accounting(file_path, is_monitoring=True)
      time.sleep(interval_seconds)
    except KeyboardInterrupt:
      print("\nMonitoring dashboard stopped by user.")
      if p_process:
        print("Terminating UMA pulling script...")
        p_process.terminate()
      break


if __name__ == "__main__":
  parser = argparse.ArgumentParser(
      description="Verify granular memory accounting coverage in Cobalt.")
  parser.add_argument(
      "file_path",
      nargs="?",
      default="uma_histos.txt",
      help="Path to uma_histos.txt (default: uma_histos.txt)")
  parser.add_argument(
      "--monitor",
      "-m",
      type=int,
      nargs="?",
      const=10,
      metavar="SECONDS",
      help="Run in live monitoring dashboard mode (default: 10)")
  parser.add_argument(
      "--pull",
      action="store_true",
      help="Simultaneously invoke pull script in background")
  parser.add_argument(
      "--platform",
      default="linux",
      choices=["linux", "android"],
      help="Platform for pull script (default: linux)")
  parser.add_argument(
      "--port",
      type=int,
      default=9222,
      help="DevTools port for pull script (default: 9222)")
  parser.add_argument(
      "--histogram-file",
      default="cobalt_uma_histograms.txt",
      help="Target histogram list file (default: cobalt_uma_histograms.txt)")
  parser.add_argument("--cuj-name", help="Name of the CUJ being verified")
  parser.add_argument("--iteration", help="Iteration identifier (e.g. 1/4)")
  args = parser.parse_args()

  pull_process = None
  if args.pull:
    import subprocess
    script_path = os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "..", "uma",
        "pull_uma_histogram_set_via_cdp.py")
    cmd = [
        sys.executable, script_path, "--platform", args.platform, "--port",
        str(args.port), "--histogram-file",
        os.path.join(
            os.path.dirname(os.path.abspath(__file__)), args.histogram_file),
        "--output-file", args.file_path, "--poll-interval-s", "60", "-q"
    ]
    print(f"Launching UMA pulling script in background: {' '.join(cmd)}")
    # pylint: disable=consider-using-with
    pull_process = subprocess.Popen(
        cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

  if args.monitor is not None or args.pull:
    monitor_interval = args.monitor if args.monitor is not None else 10
    monitor_accounting(args.file_path, monitor_interval, pull_process)
  else:
    success = verify_accounting(
        args.file_path,
        is_monitoring=False,
        cuj_name=args.cuj_name,
        iteration=args.iteration)
    if not success:
      sys.exit(1)
