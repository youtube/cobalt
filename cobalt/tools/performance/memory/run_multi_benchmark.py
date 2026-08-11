#!/usr/bin/env python3
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
"""Multi-run Memory Benchmark Orchestrator for Cobalt."""

import argparse
import csv
import math
import os
import subprocess
import time


def parse_args():
  """Parse command line arguments."""
  parser = argparse.ArgumentParser(
      description="Run N Cobalt memory benchmark iterations.")
  parser.add_argument(
      "--build_dir",
      required=True,
      help="Path to build output directory (e.g. out/evergreen-x64_qa)",
  )
  parser.add_argument(
      "--iterations",
      type=int,
      default=25,
      help="Number of benchmark iterations (default: 25)",
  )
  parser.add_argument(
      "--url",
      default="https://www.youtube.com/tv",
      help="Target URL (default: https://www.youtube.com/tv)",
  )
  parser.add_argument(
      "--warmup",
      type=int,
      default=20,
      help="Warmup duration in seconds (default: 20)",
  )
  parser.add_argument(
      "--measure",
      type=int,
      default=10,
      help="Sampling duration in seconds per state (default: 10)",
  )
  parser.add_argument(
      "--output_csv",
      default="/tmp/cobalt_memory_25runs.csv",
      help="CSV log file path",
  )
  return parser.parse_args()


def get_smaps_rollup(pid):
  """Read smaps_rollup memory metrics for pid."""
  rollup_file = f"/proc/{pid}/smaps_rollup"
  if not os.path.exists(rollup_file):
    return 0, 0, 0
  rss, pss, pdirty = 0, 0, 0
  try:
    with open(rollup_file, "r", encoding="utf-8") as f:
      for line in f:
        if line.startswith("Rss:"):
          rss = int(line.split()[1])
        elif line.startswith("Pss:"):
          pss = int(line.split()[1])
        elif line.startswith("Private_Dirty:"):
          pdirty = int(line.split()[1])
  except (OSError, IOError):
    pass
  return rss, pss, pdirty


def get_vmdata(pid):
  """Read VmData heap size from /proc/pid/status."""
  status_file = f"/proc/{pid}/status"
  if not os.path.exists(status_file):
    return 0
  try:
    with open(status_file, "r", encoding="utf-8") as f:
      for line in f:
        if line.startswith("VmData:"):
          return int(line.split()[1])
  except (OSError, IOError):
    pass
  return 0


def sample_state(pid, duration):
  """Sample memory statistics over duration seconds."""
  rss_list, pss_list, pdirty_list, vmdata_list = [], [], [], []
  for _ in range(duration):
    if not os.path.exists(f"/proc/{pid}"):
      break
    rss, pss, pdirty = get_smaps_rollup(pid)
    vmdata = get_vmdata(pid)
    rss_list.append(rss / 1024.0)
    pss_list.append(pss / 1024.0)
    pdirty_list.append(pdirty / 1024.0)
    vmdata_list.append(vmdata / 1024.0)
    time.sleep(1)

  def median(vals):
    if not vals:
      return 0.0
    s = sorted(vals)
    mid = len(s) // 2
    return s[mid]

  return (
      median(rss_list),
      median(pss_list),
      median(pdirty_list),
      median(vmdata_list),
  )


def run_single_iteration(build_dir, url, warmup, measure):
  """Execute a single benchmark iteration across lifecycle states."""
  loader_app = os.path.join(build_dir, "loader_app")
  env = os.environ.copy()
  env["DISPLAY"] = env.get("DISPLAY", ":100")

  with subprocess.Popen(
      [loader_app, f"--url={url}"],
      stdout=subprocess.DEVNULL,
      stderr=subprocess.DEVNULL,
      env=env,
  ) as proc:
    pid = proc.pid
    time.sleep(3)

    if proc.poll() is not None:
      raise RuntimeError("loader_app process terminated immediately")

    try:
      time.sleep(warmup)
      fg_rss, fg_pss, fg_pdirty, fg_vmdata = sample_state(pid, measure)

      os.kill(pid, 12)  # SIGUSR2 (Conceal)
      time.sleep(2)
      bg_rss, bg_pss, bg_pdirty, bg_vmdata = sample_state(pid, measure)

      os.kill(pid, 18)  # SIGCONT (Reveal)
      time.sleep(2)
      res_rss, res_pss, res_pdirty, res_vmdata = sample_state(pid, measure)

      return {
          "fg_rss": fg_rss,
          "fg_pss": fg_pss,
          "fg_pdirty": fg_pdirty,
          "fg_vmdata": fg_vmdata,
          "bg_rss": bg_rss,
          "bg_pss": bg_pss,
          "bg_pdirty": bg_pdirty,
          "bg_vmdata": bg_vmdata,
          "res_rss": res_rss,
          "res_pss": res_pss,
          "res_pdirty": res_pdirty,
          "res_vmdata": res_vmdata,
      }
    finally:
      if proc.poll() is None:
        proc.kill()
        proc.wait()


def stats(vals):
  """Calculate mean, median, min, max, stddev."""
  if not vals:
    return 0, 0, 0, 0, 0
  mean = sum(vals) / len(vals)
  s_vals = sorted(vals)
  med = s_vals[len(s_vals) // 2]
  mn, mx = s_vals[0], s_vals[-1]
  variance = sum((x - mean)**2 for x in vals) / len(vals)
  stddev = math.sqrt(variance)
  return mean, med, mn, mx, stddev


def main():
  """Main entrypoint for multi-run benchmark orchestrator."""
  args = parse_args()
  print("==========================================================")
  print(f" Starting {args.iterations}-Run Memory Benchmark Orchestrator")
  print(f" Build:  {args.build_dir}")
  print(f" URL:    {args.url}")
  print(f" Warmup: {args.warmup}s | Measure: {args.measure}s per state")
  print(f" CSV:    {args.output_csv}")
  print("==========================================================")

  fieldnames = [
      "iteration",
      "build_dir",
      "fg_rss",
      "fg_pss",
      "fg_pdirty",
      "fg_vmdata",
      "bg_rss",
      "bg_pss",
      "bg_pdirty",
      "bg_vmdata",
      "res_rss",
      "res_pss",
      "res_pdirty",
      "res_vmdata",
      "pdirty_savings",
  ]

  csv_exists = os.path.exists(args.output_csv)
  with open(args.output_csv, "a", newline="", encoding="utf-8") as f_csv:
    writer = csv.DictWriter(f_csv, fieldnames=fieldnames)
    if not csv_exists:
      writer.writeheader()

    records = []
    for i in range(1, args.iterations + 1):
      print(
          f"[{i:02d}/{args.iterations:02d}] Running iteration...",
          end="",
          flush=True,
      )
      try:
        res = run_single_iteration(args.build_dir, args.url, args.warmup,
                                   args.measure)
        savings = res["fg_pdirty"] - res["bg_pdirty"]
        row = {
            "iteration": i,
            "build_dir": args.build_dir,
            **res,
            "pdirty_savings": savings,
        }
        writer.writerow(row)
        f_csv.flush()
        records.append(res)
        fg_m = res["fg_pdirty"]
        bg_m = res["bg_pdirty"]
        print(f" Done! FG: {fg_m:.2f}MB | BG: {bg_m:.2f}MB"
              f" | Savings: {savings:.2f}MB")
      except (RuntimeError, OSError) as e:
        print(f" ERROR: {e}")

  fg_pdirty_vals = [r["fg_pdirty"] for r in records]
  bg_pdirty_vals = [r["bg_pdirty"] for r in records]
  fg_pss_vals = [r["fg_pss"] for r in records]
  bg_pss_vals = [r["bg_pss"] for r in records]
  fg_rss_vals = [r["fg_rss"] for r in records]
  bg_rss_vals = [r["bg_rss"] for r in records]

  print("\n==========================================================")
  print(f"           STATISTICAL REPORT ({len(records)} Iterations)")
  print(f" Build: {args.build_dir}")
  print("==========================================================")
  for name, vals in [
      ("FG Private Dirty", fg_pdirty_vals),
      ("BG Private Dirty", bg_pdirty_vals),
      ("FG PSS         ", fg_pss_vals),
      ("BG PSS         ", bg_pss_vals),
      ("FG RSS         ", fg_rss_vals),
      ("BG RSS         ", bg_rss_vals),
  ]:
    mean, med, mn, mx, std = stats(vals)
    print(
        f" {name} | Mean: {mean:6.2f} MB | Med: {med:6.2f} MB | Min: {mn:6.2f}"
        f" MB | Max: {mx:6.2f} MB | Std: {std:5.2f} MB")
  print("==========================================================")


if __name__ == "__main__":
  main()
