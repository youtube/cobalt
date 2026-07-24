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

import argparse
import json
import logging
import os
import socket
import subprocess
import tempfile
import time

from cuj_definitions import CUJS
from stats_utils import calculate_stats, permutation_test_p_value

# Dynamic repository root detection
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
DEFAULT_REPO_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, "..", "..", ".."))


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


def wait_for_devtools_port(port, timeout_s=15):
  """Actively checks loopback connection on DevTools TCP port until open."""
  start_time = time.time()
  while time.time() - start_time < timeout_s:
    try:
      with socket.create_connection(("127.0.0.1", port), timeout=0.5):
        return True
    except (OSError, ConnectionRefusedError):
      time.sleep(0.5)
  return False


def launch_app(args, cmd_args, url):
  """Launches Cobalt process on Android (intent) or Linux (binary Popen)."""
  if args.platform == "android":
    cmd = [
        "adb", "-s", args.device, "shell", "am", "start", "-n",
        f"{args.package}/{args.activity}", "--esa", "url", url, "--esa",
        "commandLineArgs", cmd_args
    ]
    run_command(cmd, shell=False)
  elif args.platform == "linux":
    flags = cmd_args.split()
    cmd = [args.bin_path] + flags + [args.package, url]
    # Use with open context block so descriptor is closed immediately
    # after Popen returns
    with open("/tmp/cobalt_process.log", "a", encoding="utf-8") as proc_log_f:
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
        logging.warning("  ⚠️ Warning: dumpsys meminfo failed: %s", stderr)
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
    logging.warning("  ⚠️ Warning: Metric JSON file not found: %s", json_file)
  except json.JSONDecodeError:
    logging.warning("  ⚠️ Warning: Failed to parse UMA JSON file: %s",
                    json_file)
  except KeyError as e:
    logging.warning("  ⚠️ Warning: UMA metrics format mismatch: %s", e)
  return None


def execute_profiling_run(args, run_id, sweep_config):
  """Performs a single end-to-end benchmark run and scrapes RSS & UMA."""
  cmd_args = sweep_config["cmd_args"]
  tmp_json = sweep_config["tmp_json"]
  histograms_txt = sweep_config["histograms_txt"]
  url = sweep_config["url"]
  is_random_nav = sweep_config.get("is_random_nav", False)

  logging.info("  [Run %s] Stopping package/process...", run_id)
  force_stop_app(args.device, args.package, args.platform)

  if os.path.exists("/tmp/cobalt_process.log"):
    try:
      os.remove("/tmp/cobalt_process.log")
    except OSError:
      pass

  logging.info("  [Run %s] Flashing system page caches...", run_id)
  flush_system_caches(args.device, args.platform)

  logging.info("  [Run %s] Launching Cobalt with experimental flags...", run_id)
  proc = launch_app(args, cmd_args, url)

  # Setup loopback forwarder immediately if Android
  if args.platform == "android":
    run_command([
        "adb", "-s", args.device, "forward", f"tcp:{args.port}",
        "localabstract:content_shell_devtools_remote"
    ])

  # Actively poll DevTools TCP port to confirm loopback accepts connections
  logging.info("  [Run %s] Waiting for DevTools to initialize...", run_id)
  if not wait_for_devtools_port(args.port, timeout_s=15):
    logging.warning(
        "  ⚠️ Warning: DevTools connection handshake timed out on port %s",
        args.port)

  # Launch background raw-websocket CDP page scroller only if randomized
  # navigation is required
  scroll_proc = None
  if is_random_nav:
    logging.info("  [Run %s] Launching background D-pad page scroller...",
                 run_id)
    scroller_path = os.path.join(
        REPO_ROOT, "cobalt/tools/memory_verification_rig/scroll_cdp.py")
    scroll_cmd = [
        "python3", scroller_path, f"--port={args.port}",
        f"--duration={args.duration - 8}", "--interval=1.5"
    ]
    try:
      # pylint: disable=consider-using-with
      scroll_proc = subprocess.Popen(
          scroll_cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    except OSError as e:
      logging.warning("  ⚠️ Warning: Failed to start background scroller: %s",
                      e)

  logging.info(
      "  [Run %s] Actively scrolling page for %ss to trigger "
      "offscreen decodes...", run_id, args.duration - 8)
  time.sleep(args.duration - 8)

  # Terminate background scroller cleanly
  if scroll_proc:
    try:
      scroll_proc.terminate()
      scroll_proc.wait(timeout=3)
    except OSError:
      pass

  # 1. Capture system-wide RSS memory
  logging.info("  [Run %s] Dumping system PSS/RSS memory...", run_id)
  pid = proc.pid if proc else None
  rss_mb = capture_memory_rss(args.device, args.package, args.platform, pid)

  # 2. Attempt to capture granular UMA breakdown metrics via CDP
  uma_captured = False
  logging.info("  [Run %s] Attempting to pull UMA breakdown metrics...", run_id)

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
      f"--platform={args.platform}", f"--device={args.device}",
      f"--package-name={args.package}", "--no-manage-cobalt",
      f"--histogram-file={histograms_txt}", "--poll-interval-s=2",
      f"--output-file={tmp_json}", f"--port={args.port}"
  ]

  scrape_proc = None
  try:
    with open("/tmp/uma_scraper.log", "a", encoding="utf-8") as log_f:
      # pylint: disable=consider-using-with
      scrape_proc = subprocess.Popen(scrape_cmd, stdout=log_f, stderr=log_f)

      # Poll securely for output file to be populated
      start_time = time.time()
      timeout_limit = 10  # max seconds to wait
      while time.time() - start_time < timeout_limit:
        if os.path.exists(tmp_json) and os.path.getsize(tmp_json) > 0:
          break
        time.sleep(0.5)
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
    # Fallback: Parse C++ process logs directly from Linux log file or
    # Android logcat
    lines = []
    if args.platform == "android":
      logcat_cmd = ["adb", "-s", args.device, "logcat", "-d"]
      stdout, _ = run_command(logcat_cmd)
      if stdout:
        lines = stdout.splitlines()
    elif args.platform == "linux":
      process_log = "/tmp/cobalt_process.log"
      if os.path.exists(process_log):
        try:
          with open(process_log, "r", encoding="utf-8") as lf:
            lines = lf.readlines()
        except OSError as e:
          logging.warning("  ⚠️ Warning: Failed to read C++ process log: %s", e)

    if lines:
      try:
        parsed_uma = {}
        for line in lines:
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
              timestamp = time.strftime("%Y-%m-%d %H:%M:%S", time.localtime())
              cdp_json = (f'{{"id": 999, "result": {{"histogram": '
                          f'{{"sum": {val_kb}}}}}}}')
              jf.write(f"{timestamp}, Memory.Cobalt.AllocationVolume.{cat}, "
                       f"{cdp_json}\n")
          uma_captured = True
      except (ValueError, OSError) as e:
        logging.warning(
            "  ⚠️ Warning: Failed to parse C++ process logs fallback: %s", e)

  if rss_mb:
    logging.info("  [Run %s] System RSS: %s MB", run_id, rss_mb)
  else:
    logging.info("  [Run %s] System RSS: FAILED", run_id)

  if uma_captured:
    logging.info("  [Run %s] Granular UMA Telemetry: ACTIVE", run_id)
  else:
    logging.info("  [Run %s] Granular UMA Telemetry: NOT PRESENT", run_id)

  force_stop_app(args.device, args.package, args.platform, proc)
  return rss_mb, uma_captured


# --- Statistical Calculations Engine ---


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

  logging.info("\n📈 METRIC ANALYZED: %s", metric_name)
  logging.info("%s", "-" * 50)
  logging.info("   • Baseline Mean:   %.2f %s (StdDev: %.2f %s)", b_mean, unit,
               b_std, unit)
  logging.info("   • Experiment Mean: %.2f %s (StdDev: %.2f %s)", e_mean, unit,
               e_std, unit)
  logging.info("   • Mean Reduction:  %.2f %s (%.1f%%)", diff, unit,
               reduction_pct)
  dir_str = "PASS" if directional_sig else "FAIL"
  logging.info("   • p-value:         %.4f (Directional Consistency: %s)",
               p_val, dir_str)
  if stat_sig:
    confidence = (1 - p_val) * 100.0
    logging.info(
        "   🎉 CONCLUSION: STATISTICALLY SIGNIFICANT "
        "(Confidence: %.1f%%) 🟢", confidence)
  else:
    logging.warning("   ⚠️ CONCLUSION: NOT STATISTICALLY SIGNIFICANT 🔴")

  return {
      "metric_name": metric_name,
      "baseline_mean": b_mean,
      "experiment_mean": e_mean,
      "mean_reduction_mb": diff,
      "p_value": p_val,
      "directional_consistency": "PASS" if directional_sig else "FAIL",
      "is_significant": stat_sig
  }


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
      "--json-results-file",
      help="Optional output JSON file path to write structured campaign results"
  )
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

  campaign_results = []

  # Print overall profiling configuration header
  logging.info("%s", "=" * 80)
  logging.info(
      "%s",
      "🚀 STARTING DUAL-TELEMETRY SIGNIFICANCE PROFILING CAMPAIGN".center(80))
  info_str = (f"   Platform: {args.platform} | "
              f"Runs: {args.runs} per config | "
              f"Duration: {args.duration}s per run").center(80)
  logging.info("%s", info_str)
  repo_str = f"   Detected Repo Root: {REPO_ROOT}".center(80)
  logging.info("%s", repo_str)
  logging.info("%s", "=" * 80)

  for cuj_key in active_cuj_keys:
    cuj_spec = CUJS[cuj_key]
    cuj_url = cuj_spec["url"]
    cuj_random_nav = cuj_spec["is_random_nav"]

    cuj_name = cuj_spec["name"]
    logging.info("\n%s", "=" * 80)
    cuj_header = f"🔥 RUNNING SIGNIFICANCE CAMPAIGN FOR: {cuj_name}".center(80)
    logging.info("%s", cuj_header)
    logging.info("%s\n", "=" * 80)

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

    baseline_sweep_config = {
        "cmd_args": b_args,
        "tmp_json": tmp_json,
        "histograms_txt": histograms_txt,
        "url": cuj_url,
        "is_random_nav": cuj_random_nav
    }
    experiment_sweep_config = {
        "cmd_args": e_args,
        "tmp_json": tmp_json,
        "histograms_txt": histograms_txt,
        "url": cuj_url,
        "is_random_nav": cuj_random_nav
    }

    logging.info("\n====== SECTION 1/2: BASELINE RUNS ======")
    for i in range(1, args.runs + 1):
      rss, uma_active = execute_profiling_run(args, i, baseline_sweep_config)
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
      rss, uma_active = execute_profiling_run(args, i, experiment_sweep_config)
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

    logging.info("\n%s", "=" * 60)
    logging.info("📊 STATISTICAL EVALUATION REPORT: %s", cuj_name)
    logging.info("%s", "=" * 60)

    # 1. Always evaluate System RSS
    if len(baseline_rss) >= 2 and len(experiment_rss) >= 2:
      res = analyze_and_report_metric("System RSS Memory (VmRSS/meminfo)",
                                      baseline_rss, experiment_rss)
      if res:
        res["cuj"] = cuj_name
        campaign_results.append(res)
    else:
      logging.error("❌ Error: Insufficient System RSS data points captured "
                    "to run significance engine.")

    # 2. Auto-detect and evaluate Granular UMA categories dynamically
    for cat, value_list in baseline_uma.items():
      b_list = value_list
      e_list = experiment_uma[cat]
      if len(b_list) >= 2 and len(e_list) >= 2:
        res = analyze_and_report_metric(
            f"Granular Allocation: k{cat} (UMA Histogram Sum)", b_list, e_list)
        if res:
          res["cuj"] = cuj_name
          campaign_results.append(res)

    logging.info("\n%s", "=" * 80)
    cuj_done = f"🏁 CAMPAIGN FOR {cuj_name} COMPLETE!".center(80)
    logging.info("%s", cuj_done)
    logging.info("%s\n", "=" * 80)

    # Write structured JSON results output if requested
    if args.json_results_file and campaign_results:
      try:
        with open(args.json_results_file, "w", encoding="utf-8") as jf:
          json.dump(campaign_results, jf, indent=2)
        logging.info("🟢 Campaign results JSON successfully dumped to: %s",
                     args.json_results_file)
      except OSError as e:
        logging.error("❌ Error: Failed to write structured JSON results: %s", e)


if __name__ == "__main__":
  main()
