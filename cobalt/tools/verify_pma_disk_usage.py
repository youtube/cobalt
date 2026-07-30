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
"""Tool to inspect and verify PMA disk usage on Android TV."""

import argparse
import os
import re
import subprocess
import sys
import time
from typing import Any, Dict, List, Optional, Tuple

MAX_ALLOWED_PMA_DIR_BYTES = 512 * 1024  # 512 KiB
DEFAULT_PACKAGE = "dev.cobalt.coat"
DEFAULT_METRICS_DIR_NAME = "BrowserStabilityMetrics"
DEFAULT_ACTIVITY = "dev.cobalt.app.MainActivity"


def get_adb_command(device_id: Optional[str] = None) -> List[str]:
  """Returns the base adb command with optional device serial."""
  cmd = ["adb"]
  if device_id:
    cmd.extend(["-s", device_id])
  return cmd


def run_adb_shell(cmd_str: str,
                  device_id: Optional[str] = None,
                  package: Optional[str] = None) -> Tuple[int, str]:
  """Executes a shell command on the device with run-as fallback."""
  base_cmd = get_adb_command(device_id)

  full_cmd = base_cmd + ["shell", cmd_str]
  try:
    result = subprocess.run(
        full_cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        check=False,
        timeout=30)
  except subprocess.TimeoutExpired:
    return -1, "Error: ADB shell command timed out."
  except FileNotFoundError:
    return -1, "Error: 'adb' executable not found in PATH."

  if result.returncode == 0:
    return result.returncode, result.stdout

  # Attempt run-as fallback if permission denied
  if package and ("Permission denied" in result.stderr or
                  "Permission denied" in result.stdout):
    run_as_cmd = base_cmd + ["shell", f"run-as {package} {cmd_str}"]
    try:
      run_as_result = subprocess.run(
          run_as_cmd,
          stdout=subprocess.PIPE,
          stderr=subprocess.PIPE,
          text=True,
          check=False,
          timeout=30)
      return run_as_result.returncode, run_as_result.stdout
    except subprocess.TimeoutExpired:
      return -1, "Error: ADB run-as command timed out."

  return result.returncode, result.stdout + result.stderr


def resolve_metrics_dir(
    device_id: Optional[str],
    package: str,
    metrics_dir_override: Optional[str] = None
) -> Tuple[Optional[str], Optional[str]]:
  """Discovers and resolves the active metrics directory path.

  Returns (resolved_dir_path, error_message).
  """
  if metrics_dir_override:
    code, out = run_adb_shell(
        f"test -d {metrics_dir_override} && echo EXISTS",
        device_id=device_id,
        package=package)
    if code != 0 or "EXISTS" not in out:
      return None, (f"Specified metrics directory '{metrics_dir_override}' "
                    f"does not exist or is not accessible (ADB code {code}).")
    return metrics_dir_override, None

  candidate_dirs = [
      f"/data/user/0/{package}/{DEFAULT_METRICS_DIR_NAME}",
      f"/data/data/{package}/{DEFAULT_METRICS_DIR_NAME}",
      f"/data/user/0/{package}/app_content_shell/{DEFAULT_METRICS_DIR_NAME}",
      f"/data/data/{package}/app_content_shell/{DEFAULT_METRICS_DIR_NAME}",
      f"/data/user/0/{package}/files/{DEFAULT_METRICS_DIR_NAME}",
      f"/data/data/{package}/files/{DEFAULT_METRICS_DIR_NAME}",
  ]

  for target_dir in candidate_dirs:
    code, out = run_adb_shell(
        f"test -d {target_dir} && echo EXISTS",
        device_id=device_id,
        package=package)
    if code == 0 and "EXISTS" in out:
      return target_dir, None

  return None, (f"No BrowserStabilityMetrics directory found for package "
                f"'{package}' among candidate paths: {candidate_dirs}")


def parse_pma_files_from_stat(output: str) -> Tuple[int, List[Dict[str, Any]]]:
  """Parses output from 'stat -c \"%s %n\" *.pma'."""
  files = []
  total_bytes = 0

  for line in output.strip().splitlines():
    line = line.strip()
    if not line or "No such file" in line:
      continue
    parts = line.split(None, 1)
    if len(parts) != 2:
      continue
    size_str, filepath = parts
    if not size_str.isdigit():
      continue
    if not filepath.endswith(".pma"):
      continue

    size = int(size_str)
    filename = os.path.basename(filepath)
    total_bytes += size
    files.append({
        "name": filename,
        "path": filepath,
        "size_bytes": size,
        "size_kb": size / 1024.0,
    })

  return total_bytes, files


def parse_pma_files_from_ls(output: str) -> Tuple[int, List[Dict[str, Any]]]:
  """Fallback parser for 'ls -l' output."""
  files = []
  total_bytes = 0

  # Standard ls -l: -rw------- 1 user group 131072 2026-09-16 12:00 file.pma
  pma_regex = re.compile(
      r"^[-a-z]{10}\s+\d+\s+\S+\s+\S+\s+(\d+)\s+.*?\s+(\S+\.pma)$")

  for line in output.strip().splitlines():
    line = line.strip()
    match = pma_regex.search(line)
    if match:
      size = int(match.group(1))
      filename = os.path.basename(match.group(2))
      total_bytes += size
      files.append({
          "name": filename,
          "path": match.group(2),
          "size_bytes": size,
          "size_kb": size / 1024.0,
      })

  return total_bytes, files


def list_pma_files(
    device_id: Optional[str],
    package: str,
    metrics_dir_override: Optional[str] = None
) -> Tuple[Optional[int], Optional[List[Dict[str, Any]]], Optional[str]]:
  """Lists .pma files and returns (total_bytes, file_list, resolved_dir).

  Fails closed by returning (None, None, error_msg) on errors.
  """
  resolved_dir, error_msg = resolve_metrics_dir(device_id, package,
                                                metrics_dir_override)
  if not resolved_dir:
    return None, None, error_msg

  # Primary query using stat -c "%s %n"
  stat_cmd = f"stat -c '%s %n' {resolved_dir}/*.pma 2>/dev/null"
  code, out = run_adb_shell(stat_cmd, device_id=device_id, package=package)

  if code == 0 and out.strip() and "No such file" not in out:
    total_bytes, files = parse_pma_files_from_stat(out)
    return total_bytes, files, resolved_dir

  # Check if directory is simply empty
  test_empty_cmd = f"ls -1 {resolved_dir} 2>/dev/null"
  code_ls, out_ls = run_adb_shell(
      test_empty_cmd, device_id=device_id, package=package)
  if code_ls == 0:
    pma_names = [
        l.strip()
        for l in out_ls.strip().splitlines()
        if l.strip().endswith(".pma")
    ]
    if not pma_names:
      # Directory exists and legitimately has 0 PMA files
      return 0, [], resolved_dir

    # Fallback to ls -l parsing if stat failed
    ls_l_cmd = f"ls -la {resolved_dir} 2>/dev/null"
    _, out_ls_l = run_adb_shell(ls_l_cmd, device_id=device_id, package=package)
    total_bytes, files = parse_pma_files_from_ls(out_ls_l)
    return total_bytes, files, resolved_dir

  return None, None, f"Failed to list directory '{resolved_dir}': {out_ls}"


def get_dir_total_disk_usage_kb(device_id: Optional[str], package: str,
                                target_dir: str) -> Optional[float]:
  """Returns total directory disk usage in KiB using du -s -k."""
  code, out = run_adb_shell(
      f"du -s -k {target_dir} 2>/dev/null",
      device_id=device_id,
      package=package)
  if code == 0 and out.strip():
    parts = out.strip().split()
    if parts and parts[0].isdigit():
      return float(parts[0])
  return None


def verify_disk_budget(total_bytes: int,
                       files: List[Dict[str, Any]],
                       resolved_dir: str,
                       *,
                       max_bytes: int = MAX_ALLOWED_PMA_DIR_BYTES,
                       total_dir_kb: Optional[float] = None) -> bool:
  """Verifies that the PMA total size does not exceed allowed quota."""
  max_kb = max_bytes / 1024.0
  total_kb = total_bytes / 1024.0
  percent = (total_bytes / max_bytes) * 100.0 if max_bytes > 0 else 0.0

  dir_info = ""
  if total_dir_kb is not None:
    dir_info = f" (Total Directory: {total_dir_kb:.1f} KB)"

  print(f"Directory: {resolved_dir}{dir_info}")
  print(f"PMA Disk Usage: {total_kb:.2f} KB / {max_kb:.2f} KB "
        f"({percent:.1f}% quota, {len(files)} files)")

  for idx, f in enumerate(files, start=1):
    name = f["name"]
    kb = f["size_kb"]
    b = f["size_bytes"]
    print(f"  [{idx}] {name} - {kb:.2f} KB ({b} bytes)")

  if total_bytes > max_bytes:
    print(f"❌ VIOLATION: PMA exceeds budget! {total_bytes} > {max_bytes}")
    return False

  print("✅ OK: PMA directory usage is strictly within budget (<= 512 KB).")
  return True


def poll_pma_usage(device_id: Optional[str],
                   package: str,
                   interval_sec: float,
                   duration_sec: float,
                   *,
                   max_bytes: int = MAX_ALLOWED_PMA_DIR_BYTES,
                   metrics_dir_override: Optional[str] = None) -> bool:
  """Polls PMA directory usage over a period of time."""
  start_time = time.time()
  end_time = start_time + duration_sec if duration_sec > 0 else float("inf")
  peak_bytes = 0
  poll_count = 0
  violations = 0

  print(f"Starting PMA disk usage monitor for package '{package}'...")
  print(f"Interval: {interval_sec}s | Max Quota: {max_bytes / 1024.0:.1f} KB")
  print("-" * 60)

  try:
    while time.time() < end_time:
      poll_count += 1
      total_bytes, files, resolved_dir = list_pma_files(
          device_id, package, metrics_dir_override=metrics_dir_override)

      if total_bytes is None:
        print(f"⚠️ Error polling PMA usage: {resolved_dir}")
        time.sleep(interval_sec)
        continue

      peak_bytes = max(peak_bytes, total_bytes)
      timestamp_str = time.strftime("%H:%M:%S")
      total_kb = total_bytes / 1024.0
      quota_pct = (total_bytes / max_bytes) * 100.0

      status_icon = "✅" if total_bytes <= max_bytes else "❌"
      print(f"[{timestamp_str}] #{poll_count:04d} | "
            f"{total_kb:6.2f} KB / {max_bytes / 1024.0:.1f} KB "
            f"({quota_pct:5.1f}%) | Files: {len(files)} {status_icon}")

      if total_bytes > max_bytes:
        violations += 1
        print(f"  ⚠️ Quota violation detected at {timestamp_str}!")
        for f in files:
          name = f["name"]
          kb = f["size_kb"]
          print(f"     - {name}: {kb:.2f} KB")

      time.sleep(interval_sec)
  except KeyboardInterrupt:
    print("\nMonitoring stopped by user.")

  print("=" * 60)
  print("Monitoring Summary:")
  print(f"  Total Polls: {poll_count}")
  print(f"  Peak Usage:  {peak_bytes / 1024.0:.2f} KB "
        f"({peak_bytes} bytes / {max_bytes / 1024.0:.1f} KB)")
  print(f"  Violations:  {violations}")

  if violations > 0:
    print("❌ FAILED: PMA directory exceeded quota during monitoring.")
    return False

  print("✅ PASSED: PMA directory remained under 512 KB throughout run.")
  return True


def run_stress_launches(device_id: Optional[str],
                        package: str,
                        num_launches: int,
                        *,
                        max_bytes: int = MAX_ALLOWED_PMA_DIR_BYTES,
                        metrics_dir_override: Optional[str] = None,
                        activity: Optional[str] = None) -> bool:
  """Performs launch-and-kill cycles to test PMA pruning under stress."""
  print(f"Running {num_launches} rapid launch-and-kill stress cycles...")
  launch_activity = activity or f"{package}/{DEFAULT_ACTIVITY}"

  for i in range(1, num_launches + 1):
    print(f"\n--- Launch Cycle {i}/{num_launches} ---")

    # Start app synchronously with -W
    start_cmd = f"am start -W -n {launch_activity}"
    code, _ = run_adb_shell(start_cmd, device_id=device_id)
    if code != 0:
      run_adb_shell(f"am start -n {launch_activity}", device_id=device_id)

    # Poll up to 3s for PMA file creation
    pma_ready = False
    resolved_path = None
    for _ in range(6):
      total_bytes, files, resolved_dir = list_pma_files(
          device_id, package, metrics_dir_override=metrics_dir_override)
      if total_bytes is not None and files:
        pma_ready = True
        resolved_path = resolved_dir
        break
      time.sleep(0.5)

    if not pma_ready or total_bytes is None or resolved_path is None:
      print("❌ Error: PMA file not created after app launch.")
      return False

    total_dir_kb = get_dir_total_disk_usage_kb(device_id, package,
                                               resolved_path)
    if not verify_disk_budget(
        total_bytes,
        files,
        resolved_path,
        max_bytes=max_bytes,
        total_dir_kb=total_dir_kb):
      return False

    # Stop app
    run_adb_shell(f"am force-stop {package}", device_id=device_id)
    time.sleep(0.5)

  print(f"\n✅ Stress test passed across {num_launches} launches (<= 512 KB)!")
  return True


def main():
  parser = argparse.ArgumentParser(
      description="Verify Cobalt PMA disk space usage on Android TV.")
  parser.add_argument(
      "-s", "--device-id", help="ADB device serial ID (optional)")
  parser.add_argument(
      "-p",
      "--package",
      default=DEFAULT_PACKAGE,
      help=f"Package name (default: {DEFAULT_PACKAGE})")
  parser.add_argument(
      "-a",
      "--activity",
      help=f"Launch activity (default: {DEFAULT_PACKAGE}/{DEFAULT_ACTIVITY})")
  parser.add_argument(
      "-d",
      "--metrics-dir",
      help="Custom metrics directory path on device (optional)")
  parser.add_argument(
      "--max-bytes",
      type=int,
      default=MAX_ALLOWED_PMA_DIR_BYTES,
      help="Max allowed total bytes (default: 524288 = 512 KiB)")
  parser.add_argument(
      "--poll",
      action="store_true",
      help="Continuously poll directory size over a duration")
  parser.add_argument(
      "--interval",
      type=float,
      default=1.0,
      help="Polling interval in seconds (default: 1.0)")
  parser.add_argument(
      "--duration",
      type=float,
      default=30.0,
      help="Polling duration in seconds (default: 30.0, 0 = infinite)")
  parser.add_argument(
      "--stress-launches",
      type=int,
      default=0,
      help="Run N rapid launch-and-kill cycles to test PMA pruning")

  args = parser.parse_args()

  if args.stress_launches > 0:
    success = run_stress_launches(
        args.device_id,
        args.package,
        args.stress_launches,
        max_bytes=args.max_bytes,
        metrics_dir_override=args.metrics_dir,
        activity=args.activity)
    sys.exit(0 if success else 1)

  if args.poll:
    success = poll_pma_usage(
        args.device_id,
        args.package,
        args.interval,
        args.duration,
        max_bytes=args.max_bytes,
        metrics_dir_override=args.metrics_dir)
    sys.exit(0 if success else 1)

  total_bytes, files, resolved_dir = list_pma_files(
      args.device_id, args.package, metrics_dir_override=args.metrics_dir)

  if total_bytes is None or resolved_dir is None:
    print(f"❌ Error: {resolved_dir}", file=sys.stderr)
    sys.exit(2)

  total_dir_kb = get_dir_total_disk_usage_kb(args.device_id, args.package,
                                             resolved_dir)
  success = verify_disk_budget(
      total_bytes,
      files,
      resolved_dir,
      max_bytes=args.max_bytes,
      total_dir_kb=total_dir_kb)
  sys.exit(0 if success else 1)


if __name__ == "__main__":
  main()
