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
"""Statistical significance verification engine for Cobalt."""

# pylint: disable=too-many-positional-arguments,too-many-arguments,wrong-import-position,logging-fstring-interpolation,logging-not-lazy

import argparse
import logging
import json
import math
import os
import random
import subprocess
import tempfile
import time

import sys

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
from cuj_definitions import CUJS

# Dynamic repository root detection
DEFAULT_REPO_ROOT = (
    "/usr/local/google/home/avvall/secondary_cobalt_checkout/src")
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))


def find_repository_root():
  """Walks up parents to dynamically identify Git workspace repository root."""
  current = SCRIPT_DIR
  while current != os.path.dirname(current):
    if os.path.exists(os.path.join(current, "cobalt")) and os.path.exists(
        os.path.join(current, "build")):
      return current
    current = os.path.dirname(current)
  return DEFAULT_REPO_ROOT


REPO_ROOT = find_repository_root()

GRANULAR_METRICS = [
    "Media", "Graphics", "Script", "Unknown", "GraphicsCanvas",
    "GraphicsCompositor", "GraphicsGlyphs", "ScriptHeap", "ScriptJIT",
    "ScriptBindings", "NetworkLoader", "NetworkCache", "BlinkDOM", "BlinkStyle",
    "BlinkParser", "PlatformIPC", "PlatformStarboard"
]

GRANULAR_METRICS = [
    "Media", "Graphics", "Script", "Unknown", "GraphicsCanvas",
    "GraphicsCompositor", "GraphicsGlyphs", "ScriptHeap", "ScriptJIT",
    "ScriptBindings", "NetworkLoader", "NetworkCache", "BlinkDOM", "BlinkStyle",
    "BlinkParser", "PlatformIPC", "PlatformStarboard"
]


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


def force_stop_app(device, package, platform, proc=None):
  """Forcefully terminates Cobalt process on Android or Linux."""
  if platform == "android":
    run_command(["adb", "-s", device, "shell", "am", "force-stop", package])
  elif platform == "linux" and proc:
    try:
      proc.terminate()
      proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
      proc.kill()
      proc.wait()
    except ProcessLookupError:
      pass
  time.sleep(2)


def flush_system_caches(device, platform):
  """Forces filesystem sync and drops page/inode caches on target host."""
  if platform == "android":
    run_command(["adb", "-s", device, "shell", "sync"])
    run_command(
        ["adb", "-s", device, "shell", "echo 3 > /proc/sys/vm/drop_caches"])
  elif platform == "linux":
    # Running drop_caches on Linux host usually requires root/sudo,
    # so we just flush standard user sync cleanly.
    run_command(["sync"])
  time.sleep(2)


def launch_app(device, package, activity, args, platform, url, bin_path=None):
  """Launches Cobalt process on Android (intent) or Linux (binary Popen)."""
  if platform == "android":
    cmd = (f"adb -s {device} shell \"am start -n {package}/{activity} "
           f"--esa url '{url}' --esa commandLineArgs '{args}'\"")
    run_command(cmd, shell=True)
  elif platform == "linux":
    flags = args.split()
    cmd = [bin_path] + flags + [package, url]
    proc_log_f = open("/tmp/cobalt_process.log", "a", encoding="utf-8")  # pylint: disable=consider-using-with
    # pylint: disable=consider-using-with
    proc = subprocess.Popen(cmd, stdout=proc_log_f, stderr=proc_log_f)
    return proc
  return None


def capture_memory_rss(device, package, platform, pid=None):
  """Extracts Resident Set Size (RSS) footprint in MB from dumpsys or procfs."""
  if platform == "android":
    stdout, stderr = run_command(
        ["adb", "-s", device, "shell", "dumpsys", "meminfo", package])
    if not stdout:
      if stderr:
        logging.warning(f"  ⚠️ Warning: dumpsys meminfo failed: {stderr}")
      return None

    for line in stdout.split("\n"):
      line = line.strip()
      if "TOTAL:" in line or "TOTAL" in line:
        parts = [p for p in line.split() if p.isdigit()]
        if parts:
          val_kb = int(parts[0])
          val_mb = val_kb / 1024.0
          return val_mb
  elif platform == "linux" and pid:
    try:
      with open(f"/proc/{pid}/status", "r", encoding="utf-8") as f:
        for line in f:
          if "VmRSS:" in line:
            parts = line.split()
            if len(parts) > 1:
              return float(parts[1]) / 1024.0  # Convert KB to MB!
    except FileNotFoundError:
      pass
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
    logging.warning(f"  ⚠️ Warning: Metric JSON file not found: {json_file}")
  except json.JSONDecodeError:
    logging.warning(f"  ⚠️ Warning: Failed to parse UMA JSON file: {json_file}")
  except KeyError as e:
    logging.warning(f"  ⚠️ Warning: UMA metrics format mismatch: {e}")
  return None


def execute_profiling_run(device,
                          package,
                          activity,
                          args,
                          duration,
                          run_id,
                          tmp_json,
                          histograms_txt,
                          platform,
                          bin_path,
                          url,
                          port=9222,
                          is_random_nav=False):
  """Performs a single end-to-end benchmark run and scrapes RSS & UMA."""
  logging.info(f"  [Run {run_id}] Stopping package/process...")
  force_stop_app(device, package, platform)

  if os.path.exists("/tmp/cobalt_process.log"):
    try:
      os.remove("/tmp/cobalt_process.log")
    except OSError:
      pass

  logging.info(f"  [Run {run_id}] Flashing system page caches...")
  flush_system_caches(device, platform)

  logging.info(f"  [Run {run_id}] Launching Cobalt with experimental flags...")
  proc = launch_app(device, package, activity, args, platform, url, bin_path)

  # Wait 6 seconds for webview and DevTools loopback port to initialize
  time.sleep(6)

  # Setup loopback forwarder if Android
  if platform == "android":
    run_command([
        "adb", "-s", device, "forward", f"tcp:{port}",
        "localabstract:content_shell_devtools_remote"
    ])
    time.sleep(1)

  # Launch background raw-websocket CDP page scroller only if randomized
  # navigation is required.
  scroll_proc = None
  if is_random_nav:
    logging.info(
        f"  [Run {run_id}] Launching background D-pad page scroller...")
    scroller_path = os.path.join(
        REPO_ROOT, "cobalt/tools/memory_verification_rig/scroll_cdp.py")
    scroll_cmd = [
        "python3", scroller_path, f"--port={port}",
        f"--duration={duration - 8}", "--interval=1.5"
    ]
    try:
      # pylint: disable=consider-using-with
      scroll_proc = subprocess.Popen(
          scroll_cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    except Exception as e:  # pylint: disable=broad-exception-caught
      logging.warning(f"  ⚠️ Warning: Failed to start background scroller: {e}")

  logging.info(f"  [Run {run_id}] Actively scrolling page for {duration - 8}s "
               f"to trigger offscreen thumbnail decodes...")
  time.sleep(duration - 8)

  # Terminate background scroller cleanly
  if scroll_proc:
    try:
      scroll_proc.terminate()
      scroll_proc.wait(timeout=3)
    except Exception:  # pylint: disable=broad-exception-caught
      pass

  # 1. Capture system-wide RSS memory
  logging.info(f"  [Run {run_id}] Dumping system PSS/RSS memory...")
  pid = proc.pid if proc else None
  rss_mb = capture_memory_rss(device, package, platform, pid)

  # 2. Attempt to capture granular UMA breakdown metrics via CDP
  uma_captured = False
  logging.info(f"  [Run {run_id}] Attempting to pull UMA breakdown metrics...")

  if os.path.exists("/tmp/uma_scraper.log"):
    try:
      os.remove("/tmp/uma_scraper.log")
    except OSError:
      pass

  if os.path.exists(tmp_json):
    try:
      os.remove(tmp_json)
    except OSError:
      pass

  scrape_cmd = [
      "python3", "-u",
      f"{REPO_ROOT}/cobalt/tools/uma/pull_uma_histogram_set_via_cdp.py",
      f"--platform={platform}", f"--device={device}",
      f"--package-name={package}", "--no-manage-cobalt",
      f"--histogram-file={histograms_txt}", "--poll-interval-s=2",
      f"--output-file={tmp_json}", f"--port={port}"
  ]

  scrape_proc = None
  try:
    with open("/tmp/uma_scraper.log", "a", encoding="utf-8") as log_f:
      # pylint: disable=consider-using-with
      scrape_proc = subprocess.Popen(scrape_cmd, stdout=log_f, stderr=log_f)
      time.sleep(6)
  finally:
    if scrape_proc:
      try:
        scrape_proc.terminate()
        scrape_proc.wait(timeout=5)
      except subprocess.TimeoutExpired:
        scrape_proc.kill()
        scrape_proc.wait()
      except ProcessLookupError:
        pass

  # Check if UMA was successfully captured via CDP.
  # If not, or if file is empty, fallback to parsing C++ process logs
  uma_captured = False
  if os.path.exists(tmp_json) and os.path.getsize(tmp_json) > 0:
    with open(tmp_json, "r", encoding="utf-8") as jf:
      content = jf.read()
      if '"histogram"' in content:
        uma_captured = True

  if not uma_captured:
    # Fallback: Parse C++ process logs directly
    process_log = "/tmp/cobalt_process.log"
    if os.path.exists(process_log):
      try:
        parsed_uma = {}
        with open(process_log, "r", encoding="utf-8") as lf:
          for line in lf:
            if "[UMA DIAGNOSTIC]" in line:
              parts = line.split("[UMA DIAGNOSTIC]")[-1].strip().split("|")
              category = None
              bytes_val = 0
              for part in parts:
                part = part.strip()
                if part.startswith("Category:"):
                  category = part.split("Category:")[-1].strip()
                elif part.startswith("Raw Allocated Bytes:"):
                  val_str = part.split("Raw Allocated Bytes:")[-1].strip()
                  bytes_val = int(val_str)
              if category:
                parsed_uma[category] = bytes_val

        if parsed_uma:
          with open(tmp_json, "w", encoding="utf-8") as jf:
            for cat, bytes_val in parsed_uma.items():
              val_kb = bytes_val / 1024.0
              cdp_json = (f'{{"id": 999, "result": '
                          f'{{"histogram": {{"sum": {val_kb}}}}}}}')
              cdp_line = (
                  f"2026-05-20 05:00:00,Memory.Cobalt.AllocationVolume.{cat},"
                  f"{cdp_json}\n")
              jf.write(cdp_line)
          uma_captured = True
      except Exception as e:  # pylint: disable=broad-exception-caught
        logging.warning(
            f"  ⚠️ Warning: Failed to parse C++ process logs fallback: {e}")

  logging.info(f"  [Run {run_id}] System RSS: {rss_mb:.2f} MB"
               if rss_mb else f"  [Run {run_id}] System RSS: FAILED")
  logging.info(
      f"  [Run {run_id}] Granular UMA Telemetry: ACTIVE" if uma_captured else
      f"  [Run {run_id}] Granular UMA Telemetry: NOT PRESENT")

  force_stop_app(device, package, platform, proc)
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

  diff = b_mean - e_mean
  reduction_pct = (diff / b_mean) * 100.0 if b_mean > 0 else 0.0

  # Check if the metric name represents a memory metric to display MB,
  # otherwise raw units.
  is_memory = ("Memory" in metric_name or "Malloc" in metric_name or "Heap"
               in metric_name or "GC" in metric_name or "Skia" in metric_name or
               "PartitionAlloc" in metric_name) and "Reason" not in metric_name
  unit = "MB" if is_memory else "Units"

  p_val = permutation_test_p_value(baseline, experiment)
  directional_sig = check_directional_consistency(baseline, experiment)
  stat_sig = p_val < sig_threshold

  logging.info(f"\n📈 METRIC ANALYZED: {metric_name}")
  logging.info("-" * 50)
  logging.info(
      f"   • Baseline Mean:   {b_mean:.2f} {unit} (StdDev: {b_std:.2f} {unit})")
  logging.info(
      f"   • Experiment Mean: {e_mean:.2f} {unit} (StdDev: {e_std:.2f} {unit})")
  logging.info(
      f"   • Mean Reduction:  {diff:.2f} {unit} ({reduction_pct:.1f}%)")
  dir_str = "PASS" if directional_sig else "FAIL"
  logging.info(f"   • p-value:         {p_val:.4f} "
               f"(Directional Consistency: {dir_str})")
  if stat_sig:
    confidence = (1 - p_val) * 100.0
    logging.info(f"   🎉 CONCLUSION: STATISTICALLY SIGNIFICANT "
                 f"(Confidence: {confidence:.1f}%) 🟢")
  else:
    logging.warning("   ⚠️ CONCLUSION: NOT STATISTICALLY SIGNIFICANT 🔴")


def main():
  logging.basicConfig(level=logging.INFO, format="%(message)s")
  parser = argparse.ArgumentParser(
      description="Dual-Telemetry Statistical Significance Profiler")
  parser.add_argument(
      "--platform",
      default="android",
      choices=["android", "linux"],
      help="Target platform to profile (android or linux)")
  parser.add_argument(
      "--device", default="localhost:38879", help="ADB device serial ID")
  parser.add_argument(
      "--bin-path",
      default="./out/linux-x64x11_qa/cobalt",
      help="Local path to Cobalt monolithic binary (Linux only)")
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
      "--enable-granular-memory",
      action="store_true",
      help="Enable detailed Cobalt granular memory metrics collection")
  parser.add_argument(
      "--port",
      type=int,
      default=9222,
      help="CDP port for DevTools remote debugging")
  parser.add_argument(
      "--cuj",
      default="watch",
      choices=["all", "browse", "watch", "baseline", "combined"],
      help="Specific CUJ to run profiling campaigns on (default: watch)")

  parser.add_argument(
      "--baseline-args",
      default="--remote-allow-origins=*,--enable-metrics,"
      "--force-enable-metrics-reporting,--memory-metrics-interval=30",
      help="Baseline launch arguments comma-separated list")
  parser.add_argument(
      "--experiment-args",
      default="--remote-allow-origins=*,--enable-metrics,"
      "--force-enable-metrics-reporting,--memory-metrics-interval=30",
      help="Experiment launch arguments comma-separated list")

  args = parser.parse_args()

  if args.runs < 3:
    logging.warning("⚠️ Warning: N < 3 runs is statistically too small "
                    "to compute reliable significance. Suggest >= 5 runs.")

  # Determine the list of CUJs to execute
  active_cuj_keys = []
  if args.cuj == "all":
    active_cuj_keys = list(CUJS.keys())
  else:
    active_cuj_keys = [args.cuj]

  # Print overall profiling configuration header
  logging.info("=" * 80)
  logging.info(
      "🚀 STARTING DUAL-TELEMETRY SIGNIFICANCE PROFILING CAMPAIGN".center(80))
  logging.info(f"   Platform: {args.platform} | "
               f"Runs: {args.runs} per config | "
               f"Duration: {args.duration}s per run".center(80))
  logging.info(f"   Detected Repo Root: {REPO_ROOT}".center(80))
  logging.info("=" * 80)

  for cuj_key in active_cuj_keys:
    cuj_spec = CUJS[cuj_key]
    cuj_url = cuj_spec["url"]
    cuj_random_nav = cuj_spec["is_random_nav"]

    cuj_name = cuj_spec["name"]
    logging.info("\n" + "=" * 80)
    logging.info(f"🔥 RUNNING SIGNIFICANCE CAMPAIGN FOR: {cuj_name}".center(80))
    logging.info("=" * 80 + "\n")

    baseline_rss = []
    experiment_rss = []

    # Granular UMA databases dynamically populated depending on flag
    monitored_categories = []
    if args.enable_granular_memory:
      monitored_categories = GRANULAR_METRICS

    baseline_uma = {cat: [] for cat in monitored_categories}
    experiment_uma = {cat: [] for cat in monitored_categories}

    # Safe dynamic temporary file paths using secure tempfile API
    tmp_json_fd, tmp_json = tempfile.mkstemp(suffix="_tmp_uma.json")
    os.close(
        tmp_json_fd)  # close descriptor immediately so scraper can write to it

    # Create dynamic target histograms text file securely
    histograms_fd, histograms_txt = tempfile.mkstemp(
        suffix="_target_histograms.txt")
    with os.fdopen(histograms_fd, "w", encoding="utf-8") as f:
      for cat in monitored_categories:
        f.write(f"Memory.Cobalt.AllocationVolume.{cat}\n")
      f.write("Playback.DroppedFrames\n")

    # Format space-separated flags and append remote-debugging-port dynamically
    b_args_clean = args.baseline_args.replace(",", " ")
    e_args_clean = args.experiment_args.replace(",", " ")
    b_args = f"{b_args_clean} --remote-debugging-port={args.port}"
    e_args = f"{e_args_clean} --remote-debugging-port={args.port}"

    logging.info("\n====== SECTION 1/2: BASELINE RUNS ======")
    for i in range(1, args.runs + 1):
      rss, uma_active = execute_profiling_run(args.device, args.package,
                                              args.activity, b_args,
                                              args.duration, i, tmp_json,
                                              histograms_txt, args.platform,
                                              args.bin_path, cuj_url, args.port,
                                              cuj_random_nav)
      if rss:
        baseline_rss.append(rss)
      if uma_active:
        for cat, value_list in baseline_uma.items():
          val = parse_uma_sum_metric(tmp_json,
                                     f"Memory.Cobalt.AllocationVolume.{cat}")
          if val is not None:
            value_list.append(val)
      time.sleep(3)

    logging.info("\n====== SECTION 2/2: EXPERIMENT RUNS ======")
    for i in range(1, args.runs + 1):
      rss, uma_active = execute_profiling_run(args.device, args.package,
                                              args.activity, e_args,
                                              args.duration, i, tmp_json,
                                              histograms_txt, args.platform,
                                              args.bin_path, cuj_url, args.port,
                                              cuj_random_nav)
      if rss:
        experiment_rss.append(rss)
      if uma_active:
        for cat, value_list in experiment_uma.items():
          val = parse_uma_sum_metric(tmp_json,
                                     f"Memory.Cobalt.AllocationVolume.{cat}")
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

    logging.info("\n" + "=" * 60)
    logging.info(f"📊 STATISTICAL EVALUATION REPORT: {cuj_name}")
    logging.info("=" * 60)

    # 1. Always evaluate System RSS
    if len(baseline_rss) >= 2 and len(experiment_rss) >= 2:
      analyze_and_report_metric("System RSS Memory (VmRSS/meminfo)",
                                baseline_rss, experiment_rss)
    else:
      logging.error("❌ Error: Insufficient System RSS data points captured "
                    "to run significance engine.")

    # 2. Auto-detect and evaluate Granular UMA categories dynamically
    for cat, value_list in baseline_uma.items():
      b_list = value_list
      e_list = experiment_uma[cat]
      if len(b_list) >= 2 and len(e_list) >= 2:
        analyze_and_report_metric(
            f"Granular Allocation: k{cat} (UMA Histogram Sum)", b_list, e_list)

    logging.info("\n" + "=" * 80)
    logging.info(f"🏁 CAMPAIGN FOR {cuj_name} COMPLETE!".center(80))
    logging.info("=" * 80 + "\n")


if __name__ == "__main__":
  main()
