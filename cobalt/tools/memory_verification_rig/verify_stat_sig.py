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
"""Dynamic statistical significance verification engine for Cobalt."""

# pylint: disable=too-many-positional-arguments,too-many-arguments

import argparse
import json
import math
import os
import random
import subprocess
import tempfile
import time

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))


def find_repository_root():
  """Walks up parents to dynamically identify Git workspace repository root."""
  current = SCRIPT_DIR
  while current != os.path.dirname(current):
    if os.path.exists(os.path.join(current, "cobalt")) and os.path.exists(
        os.path.join(current, "build")):
      return current
    current = os.path.dirname(current)
  # Dynamic fallback to three directories up from script location
  return os.path.dirname(os.path.dirname(os.path.dirname(SCRIPT_DIR)))


REPO_ROOT = find_repository_root()


def run_command(cmd, shell=False, timeout=30):
  """Executes a shell command synchronously with safety timeout limits."""
  try:
    res = subprocess.run(
        cmd,
        shell=shell,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        timeout=timeout,
        check=True)
    return res.stdout.strip(), None
  except subprocess.TimeoutExpired:
    return "", "Command timed out."
  except subprocess.CalledProcessError as e:
    return e.stdout.strip(), e.stderr.strip()


def force_stop_app(device, package):
  """Forcefully kills the targeted app package on the Android device."""
  run_command(["adb", "-s", device, "shell", "am", "force-stop", package])
  time.sleep(2)


def flush_system_caches(device):
  """Forces filesystem sync and drops Android OS system caches cleanly."""
  run_command(["adb", "-s", device, "shell", "sync"])
  run_command(
      ["adb", "-s", device, "shell", "echo 3 > /proc/sys/vm/drop_caches"])
  time.sleep(2)


def launch_app(device, package, activity, args):
  """Launches Cobalt MainActivity with correct string array extras."""
  cmd = [
      "adb", "-s", device, "shell", "am", "start", "-n",
      f"{package}/{activity}", "--esa", "commandLineArgs", args
  ]
  run_command(cmd, shell=False)


def capture_memory_rss(device, package):
  """Extracts the Total PSS/RSS memory footprints from dumpsys meminfo."""
  stdout, stderr = run_command(
      ["adb", "-s", device, "shell", "dumpsys", "meminfo", package])
  if not stdout:
    if stderr:
      print(f"  ⚠️ Warning: dumpsys meminfo failed: {stderr}")
    return None

  for line in stdout.split("\n"):
    line = line.strip()
    if "TOTAL:" in line or "TOTAL" in line:
      parts = [p for p in line.split() if p.isdigit()]
      if parts:
        val_kb = int(parts[0])
        val_mb = val_kb / 1024.0
        return val_mb
  return None


def parse_uma_sum_metric(json_file, metric_name):
  """Safely extracts the sum field from the scraped UMA histogram JSON."""
  if not os.path.exists(json_file):
    return None
  try:
    with open(json_file, "r", encoding="utf-8") as f:
      for line in f:
        if metric_name in line:
          parts = line.split(",", 2)
          if len(parts) > 2:
            data = json.loads(parts[2])
            if "result" in data and "histogram" in data["result"]:
              hist = data["result"]["histogram"]
              if "sum" in hist:
                return float(hist["sum"]) / 1024.0  # Convert KB to MB!
  except FileNotFoundError:
    print(f"  ⚠️ Warning: Metric JSON file not found: {json_file}")
  except json.JSONDecodeError:
    print(f"  ⚠️ Warning: Failed to parse UMA JSON file: {json_file}")
  except KeyError as e:
    print(f"  ⚠️ Warning: UMA metrics format mismatch: {e}")
  return None


def execute_profiling_run(device,
                          package,
                          activity,
                          args,
                          duration,
                          run_id,
                          tmp_json,
                          histograms_txt,
                          port=9222):
  """Performs a single end-to-end benchmark run and scrapes RSS & UMA."""
  print(f"  [Run {run_id}] Stopping package {package}...")
  force_stop_app(device, package)

  print(f"  [Run {run_id}] Flashing system page caches...")
  flush_system_caches(device)

  print(f"  [Run {run_id}] Launching Cobalt with experimental flags...")
  launch_app(device, package, activity, args)

  print(f"  [Run {run_id}] Streaming video for {duration}s "
        f"to accumulate allocations...")
  time.sleep(duration)

  # 1. Capture system-wide RSS memory
  print(f"  [Run {run_id}] Dumping system PSS/RSS memory...")
  rss_mb = capture_memory_rss(device, package)

  # 2. Attempt to capture granular UMA breakdown metrics via CDP
  uma_captured = False
  print(f"  [Run {run_id}] Attempting to pull UMA breakdown metrics...")

  # Setup abstract socket port forwarding
  run_command([
      "adb", "-s", device, "forward", f"tcp:{port}",
      "localabstract:content_shell_devtools_remote"
  ])
  time.sleep(1)

  if os.path.exists(tmp_json):
    try:
      os.remove(tmp_json)
    except OSError:
      pass

  scrape_cmd = [
      "python3",
      f"{REPO_ROOT}/cobalt/tools/uma/pull_uma_histogram_set_via_cdp.py",
      "--platform=android", f"--device={device}", f"--package-name={package}",
      "--no-manage-cobalt", f"--histogram-file={histograms_txt}",
      "--poll-interval-s=2", f"--output-file={tmp_json}", f"--port={port}"
  ]

  proc = None
  try:
    # pylint: disable=consider-using-with
    proc = subprocess.Popen(
        scrape_cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    # Poll for the output file to be populated securely
    start_time = time.time()
    timeout_limit = 10  # max seconds to wait
    while time.time() - start_time < timeout_limit:
      if os.path.exists(tmp_json) and os.path.getsize(tmp_json) > 0:
        break
      time.sleep(0.5)
  finally:
    if proc:
      try:
        proc.terminate()
        proc.wait(timeout=5)
      except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait()
      except ProcessLookupError:
        pass

  if os.path.exists(tmp_json) and os.path.getsize(tmp_json) > 0:
    uma_captured = True

  print(f"  [Run {run_id}] System RSS: {rss_mb:.2f} MB"
        if rss_mb else f"  [Run {run_id}] System RSS: FAILED")
  print(f"  [Run {run_id}] Granular UMA Telemetry: ACTIVE" if uma_captured else
        f"  [Run {run_id}] Granular UMA Telemetry: NOT PRESENT")

  force_stop_app(device, package)
  return rss_mb, uma_captured


# --- Statistical Calculations Engine ---


def calculate_stats(data):
  """Calculates mean, median, and standard deviation of a numeric array."""
  n = len(data)
  if n == 0:
    return 0.0, 0.0, 0.0
  mean = sum(data) / n
  median = sorted(data)[n // 2]
  variance = sum((x - mean)**2 for x in data) / max(1, n - 1)
  std_dev = math.sqrt(variance)
  return mean, median, std_dev


def permutation_test_p_value(group1, group2, permutations=10000):
  """Performs robust non-parametric permutation resampling test for p-value."""
  n1 = len(group1)
  n2 = len(group2)
  if n1 == 0 or n2 == 0:
    return 1.0

  mean1 = sum(group1) / n1
  mean2 = sum(group2) / n2
  observed_diff = abs(mean1 - mean2)

  pooled = group1 + group2
  count = 0
  for _ in range(permutations):
    random.shuffle(pooled)
    perm_g1 = pooled[:n1]
    perm_g2 = pooled[n1:]
    perm_diff = abs((sum(perm_g1) / n1) - (sum(perm_g2) / n2))
    if perm_diff >= observed_diff:
      count += 1
  return count / permutations


def check_directional_consistency(baseline, experiment):
  """Performs standard sign-test on sequential paired runs."""
  if len(baseline) != len(experiment) or len(baseline) == 0:
    return False

  differences = [baseline[i] - experiment[i] for i in range(len(baseline))]
  positives = sum(1 for d in differences if d > 0)
  negatives = sum(1 for d in differences if d < 0)

  # Require at least 80% of paired differences to share the same direction
  threshold = max(1, int(len(baseline) * 0.8))
  return positives >= threshold or negatives >= threshold


# --- Report Generator ---


def analyze_and_report_metric(metric_name,
                              baseline,
                              experiment,
                              sig_threshold=0.05):
  """Evaluates metrics statistical distributions and outputs conclusions."""
  b_mean, _, b_std = calculate_stats(baseline)
  e_mean, _, e_std = calculate_stats(experiment)

  diff_mb = b_mean - e_mean
  reduction_pct = (diff_mb / b_mean) * 100.0 if b_mean > 0 else 0.0

  p_val = permutation_test_p_value(baseline, experiment)
  directional_sig = check_directional_consistency(baseline, experiment)
  stat_sig = p_val < sig_threshold

  print(f"\n📈 METRIC ANALYZED: {metric_name}")
  print("-" * 50)
  print(f"   • Baseline Mean:   {b_mean:.2f} MB (StdDev: {b_std:.2f} MB)")
  print(f"   • Experiment Mean: {e_mean:.2f} MB (StdDev: {e_std:.2f} MB)")
  print(f"   • Mean Reduction:  {diff_mb:.2f} MB ({reduction_pct:.1f}%)")
  dir_str = "PASS" if directional_sig else "FAIL"
  print(f"   • p-value:         {p_val:.4f} "
        f"(Directional Consistency: {dir_str})")
  if stat_sig:
    confidence = (1 - p_val) * 100.0
    print(f"   🎉 CONCLUSION: STATISTICALLY SIGNIFICANT "
          f"(Confidence: {confidence:.1f}%) 🟢")
  else:
    print("   ⚠️ CONCLUSION: NOT STATISTICALLY SIGNIFICANT 🔴")


def main():
  parser = argparse.ArgumentParser(
      description="Dual-Telemetry Statistical Significance Profiler")
  parser.add_argument(
      "--device", default="localhost:38879", help="ADB device serial ID")
  parser.add_argument(
      "--runs", type=int, default=5, help="Number of sample runs per config")
  parser.add_argument(
      "--duration", type=int, default=60, help="Watch CUJ duration in seconds")
  parser.add_argument(
      "--package", default="dev.cobalt.coat", help="Android Package name")
  parser.add_argument(
      "--activity",
      default="dev.cobalt.app.MainActivity",
      help="Android Activity name")
  parser.add_argument(
      "--port",
      type=int,
      default=9222,
      help="CDP port for DevTools remote debugging")

  parser.add_argument(
      "--baseline-args",
      default="--remote-allow-origins=*,--enable-features="
      "CobaltMemoryAttributionManager",
      help="Baseline launch arguments comma-separated list")
  parser.add_argument(
      "--experiment-args",
      default="--remote-allow-origins=*,--enable-features="
      "CobaltMemoryAttributionManager,--mse-video-buffer-size-limit-mb=15,"
      "--mse-audio-buffer-size-limit-mb=2,--js-flags=--jitless,"
      "--skia-font-cache-limit-mb=2",
      help="Experiment launch arguments comma-separated list")

  args = parser.parse_args()

  if args.runs < 3:
    print("⚠️ Warning: N < 3 runs is statistically too small "
          "to compute reliable significance. Suggest >= 5 runs.")

  print("=" * 60)
  print("🚀 STARTING DUAL-TELEMETRY SIGNIFICANCE PROFILING CAMPAIGN")
  print(f"   Runs: {args.runs} per config | Duration: {args.duration}s per run")
  print(f"   Detected Repo Root: {REPO_ROOT}")
  print("=" * 60)

  baseline_rss = []
  experiment_rss = []

  # Granular UMA databases
  baseline_uma = {"Media": [], "Graphics": [], "Script": [], "Unknown": []}
  experiment_uma = {"Media": [], "Graphics": [], "Script": [], "Unknown": []}

  # Safe dynamic temporary file paths using secure tempfile API
  tmp_json_fd, tmp_json = tempfile.mkstemp(suffix="_tmp_uma.json")
  os.close(
      tmp_json_fd)  # close descriptor immediately so scraper can write to it

  # Create dynamic target histograms text file securely
  histograms_fd, histograms_txt = tempfile.mkstemp(
      suffix="_target_histograms.txt")
  with os.fdopen(histograms_fd, "w", encoding="utf-8") as f:
    f.write("Memory.Cobalt.AllocationVolume.kMedia\n")
    f.write("Memory.Cobalt.AllocationVolume.kGraphics\n")
    f.write("Memory.Cobalt.AllocationVolume.kScript\n")
    f.write("Memory.Cobalt.AllocationVolume.kUnknown\n")
    f.write("Playback.DroppedFrames\n")

  print("\n====== SECTION 1/2: BASELINE RUNS ======")
  for i in range(1, args.runs + 1):
    rss, uma_active = execute_profiling_run(args.device, args.package,
                                            args.activity, args.baseline_args,
                                            args.duration, i, tmp_json,
                                            histograms_txt, args.port)
    if rss:
      baseline_rss.append(rss)
    if uma_active:
      for cat, value_list in baseline_uma.items():
        val = parse_uma_sum_metric(tmp_json,
                                   f"Memory.Cobalt.AllocationVolume.k{cat}")
        if val is not None:
          value_list.append(val)
    time.sleep(3)

  print("\n====== SECTION 2/2: EXPERIMENT RUNS ======")
  for i in range(1, args.runs + 1):
    rss, uma_active = execute_profiling_run(args.device, args.package,
                                            args.activity, args.experiment_args,
                                            args.duration, i, tmp_json,
                                            histograms_txt, args.port)
    if rss:
      experiment_rss.append(rss)
    if uma_active:
      for cat, value_list in experiment_uma.items():
        val = parse_uma_sum_metric(tmp_json,
                                   f"Memory.Cobalt.AllocationVolume.k{cat}")
        if val is not None:
          value_list.append(val)
    time.sleep(3)

  # Cleanup temporary workspace files
  for f_path in (tmp_json, histograms_txt):
    if os.path.exists(f_path):
      try:
        os.remove(f_path)
      except OSError:
        pass

  print("\n" + "=" * 60)
  print("📊 SIGNIFICANCE ENGINE STATISTICAL EVALUATION")
  print("=" * 60)

  # 1. Always evaluate System RSS
  if len(baseline_rss) >= 2 and len(experiment_rss) >= 2:
    analyze_and_report_metric("System RSS Memory (dumpsys meminfo)",
                              baseline_rss, experiment_rss)
  else:
    print("❌ Error: Insufficient System RSS data points captured "
          "to run significance engine.")

  # 2. Auto-detect and evaluate Granular UMA categories
  for cat, value_list in baseline_uma.items():
    b_list = value_list
    e_list = experiment_uma[cat]
    if len(b_list) >= 2 and len(e_list) >= 2:
      analyze_and_report_metric(
          f"Granular Allocation: k{cat} (UMA Histogram Sum)", b_list, e_list)

  print("\n" + "=" * 60)
  print("🎉 SIGNIFICANCE EVALUATION RUN COMPLETE!")
  print("=" * 60 + "\n")


if __name__ == "__main__":
  main()
