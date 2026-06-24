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
"""PartitionAlloc subregion UMA verification script for Cobalt.

Verifies that the sum of the subregion Resident UMA metrics equals the total
PartitionAllocRss UMA metric, and compares this total with OS smaps metrics.
"""

import argparse
import json
import os
import subprocess


def parse_uma_histos(file_path):
  """Parses UMA snapshots file mapping metric name to count/sum snapshots."""
  # Maps metric_name -> last observed sum
  metrics_data = {}

  if not os.path.exists(file_path):
    return metrics_data

  with open(file_path, "r", encoding="utf-8") as f:
    for line in f:
      line = line.strip()
      if not line:
        continue

      parts = line.split(",", 2)
      if len(parts) < 3:
        continue

      _, json_str = parts[1], parts[2]

      try:
        payload = json.loads(json_str)
        result = payload.get("result", {})
        histograms = result.get("histograms", [])
        for h in histograms:
          name = h.get("name")
          count = h.get("count", 0)
          s = h.get("sum", 0)
          if name and count > 0:
            # We just want the latest reported sum (e.g. latest resident size)
            metrics_data[name] = s
      except json.JSONDecodeError:
        continue

  return metrics_data


def get_smaps_partition_alloc_size(platform, pid=None):
  """Retrieves the total PSS/RSS for anon:partition_alloc from smaps."""
  smaps_output = ""
  if platform == "linux":
    if not pid:
      cmd = ["pgrep", "-n", "cobalt"]
      try:
        pid = subprocess.check_output(cmd).decode().strip()
      except subprocess.CalledProcessError:
        return 0
    try:
      with open(f"/proc/{pid}/smaps", "r", encoding="utf-8") as f:
        smaps_output = f.read()
    except FileNotFoundError:
      return 0
  elif platform == "android":
    cmd = ["adb", "shell", "dumpsys", "meminfo", "dev.cobalt.coat"]
    try:
      _ = subprocess.check_output(cmd).decode()
      # Dumpsys parsing is complex; alternatively adb shell cat /proc/pid/smaps
      # For simplicity, returning 0 if we can't parse properly without root
      return 0
    except subprocess.CalledProcessError:
      return 0

  total_kb = 0
  in_pa_region = False
  for line in smaps_output.split("\n"):
    if "[anon:partition_alloc]" in line:
      in_pa_region = True
    elif line.startswith("VmFlags:") or line.startswith("Size:"):
      pass  # Keep reading block
    elif in_pa_region and line.startswith("Rss:"):
      parts = line.split()
      if len(parts) >= 2:
        total_kb += int(parts[1])
      in_pa_region = False  # Block ended
    elif not line.startswith(" ") and ":" in line:
      in_pa_region = False  # Reached a new mapping block

  return total_kb


def verify_partition_alloc(file_path, platform):
  metrics = parse_uma_histos(file_path)

  # 1. Collect UMAs for subregions
  # Assuming subregions are emitted under Cobalt.Memory.Profiler.Resident.*
  # Note: Using a placeholder prefix depending on final metric names.
  subregion_prefix = "Cobalt.Memory.Profiler.Resident."
  subregions = {
      k: v for k, v in metrics.items() if k.startswith(subregion_prefix)
  }

  if not subregions:
    print(f"ERROR: No subregion metrics found matching \"{subregion_prefix}\" "
          f"in {file_path}")
    # Fallback to display what memory metrics DO exist to help debugging
    print("Available memory metrics:")
    for k in metrics:
      if "Memory" in k:
        print(f" - {k}")
    return False

  subregion_total = sum(subregions.values())

  # 2. Collect total PartitionAlloc UMA
  pa_rss_uma = metrics.get(
      "Memory.Browser.PartitionAllocRss",
      0) * 1024  # Convert MB/KB to bytes as needed, assuming KB here.

  # 3. Collect smaps metric
  smaps_kb = get_smaps_partition_alloc_size(platform)

  print("=" * 80)
  print(" PARTITION ALLOC SUBREGION VERIFICATION ".center(80))
  print("=" * 80)

  print("\n[ Subregion UMA Breakdown ]")
  for name, val in subregions.items():
    print(f"  {name:<50}: {val} KB")
  print("-" * 80)
  label1 = "SUM OF SUBREGIONS"
  label2 = "Memory.Browser.PartitionAllocRss"
  label3 = "OS smaps [anon:partition_alloc] RSS"
  print(f"  {label1:<50}: {subregion_total} KB")
  print(f"  {label2:<50}: {pa_rss_uma} KB")
  print(f"  {label3:<50}: {smaps_kb} KB")
  print("=" * 80)

  # Evaluate if subregions sum up approximately to PartitionAllocRss
  # Using a 5% tolerance window
  tolerance = 0.05
  diff = abs(subregion_total - pa_rss_uma)
  if pa_rss_uma > 0 and diff / pa_rss_uma <= tolerance:
    print("[SUCCESS] Subregion UMAs accurately add up to the total "
          "PartitionAlloc UMA.")
    return True
  else:
    print("[FAIL] Subregion UMAs do NOT match the total PartitionAlloc UMA.")
    return False


if __name__ == "__main__":
  parser = argparse.ArgumentParser()
  parser.add_argument("file_path", nargs="?", default="uma_histos.txt")
  parser.add_argument(
      "--platform", default="linux", choices=["linux", "android"])
  args = parser.parse_args()

  verify_partition_alloc(args.file_path, args.platform)
