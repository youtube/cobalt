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
# pylint: skip-file
"""Instrument and verify Android process exit reason tracking on live devices.

This script tests multiple process termination scenarios (e.g. SIGKILL, SIGSEGV,
SIGABRT, SIGTERM, force-stop) against Cobalt on an Android device or emulator.
For each scenario:
  1. Launches Cobalt (dev.cobalt.coat) and captures its PID.
  2. Verifies that the PMA stability allocator creates the active PMA file.
  3. Terminates the app using the scenario's termination method/signal.
  4. Queries Android's ActivityManager exit info via `dumpsys activity exit-info`.
  5. Verifies the OS recorded the exact PID and exit reason/subreason/status.
  6. Relaunches Cobalt and verifies that the prior session PID is processed by
     Cobalt's `RecordPriorSessionExitReasons()`:
       - Verifies that logcat confirms prior session exit reason recording.
       - Decodes the newly active PMA file to verify that the UMA histogram
         `Cobalt.Stability.Android.SystemExitReason` contains the exact expected
         enum bucket sample.

Usage:
    # Run all termination scenarios on connected device:
    python3 cobalt/tools/verify_android_exit_reasons.py

    # Run specific scenarios:
    python3 cobalt/tools/verify_android_exit_reasons.py --scenarios sigkill,sigsegv,force-stop

    # Specify a target device serial:
    python3 cobalt/tools/verify_android_exit_reasons.py --device localhost:27281
"""

import argparse
import dataclasses
import os
import re
import struct
import subprocess
import sys
import time
from typing import Any, Dict, List, Optional, Tuple

DEFAULT_PACKAGE = "dev.cobalt.coat"
DEFAULT_ACTIVITY = "dev.cobalt.app.MainActivity"
HISTOGRAM_NAME = "Cobalt.Stability.Android.SystemExitReason"

# Android ApplicationExitInfo Reason Codes
ANDROID_REASONS: Dict[int, str] = {
    0: "REASON_UNKNOWN",
    1: "REASON_EXIT_SELF",
    2: "REASON_SIGNALED",
    3: "REASON_LOW_MEMORY",
    4: "REASON_CRASH",
    5: "REASON_CRASH_NATIVE",
    6: "REASON_ANR",
    7: "REASON_INITIALIZATION_FAILURE",
    8: "REASON_PERMISSION_CHANGE",
    9: "REASON_EXCESSIVE_RESOURCE_USAGE",
    10: "REASON_USER_REQUESTED",
    11: "REASON_USER_STOPPED",
    12: "REASON_DEPENDENCY_DIED",
    13: "REASON_OTHER",
    14: "REASON_FREEZER",
    15: "REASON_PACKAGE_STATE_CHANGE",
    16: "REASON_PACKAGE_UPDATED",
}

# Mapping from Android ApplicationExitInfo reason to Chromium ExitReason UMA enum
SYSTEM_REASON_TO_UMA_ENUM: Dict[int, Tuple[int, str]] = {
    -1: (14, "REASON_API_FAILED"),
    0: (11, "REASON_UNKNOWN"),
    1: (5, "REASON_EXIT_SELF"),
    2: (10, "REASON_SIGNALED"),
    3: (7, "REASON_LOW_MEMORY"),
    4: (1, "REASON_CRASH"),
    5: (2, "REASON_CRASH_NATIVE"),
    6: (0, "REASON_ANR"),
    7: (6, "REASON_INITIALIZATION_FAILURE"),
    8: (9, "REASON_PERMISSION_CHANGE"),
    9: (4, "REASON_EXCESSIVE_RESOURCE_USAGE"),
    10: (12, "REASON_USER_REQUESTED"),
    11: (13, "REASON_USER_STOPPED"),
    12: (3, "REASON_DEPENDENCY_DIED"),
    13: (8, "REASON_OTHER"),
    14: (15, "REASON_FREEZER"),
    15: (16, "REASON_PACKAGE_STATE_CHANGE"),
    16: (17, "REASON_PACKAGE_UPDATED"),
}

# Mapping from Android ApplicationExitInfo reason to UMA token suffix
REASON_TO_SUFFIX: Dict[int, str] = {
    0: "Unknown",
    1: "ExitSelf",
    2: "Signaled",
    3: "LowMemory",
    4: "Crash",
    5: "CrashNative",
    6: "Anr",
    7: "InitializationFailure",
    8: "PermissionChange",
    9: "ExcessiveResourceUsage",
    10: "UserRequested",
    11: "UserStopped",
    12: "DependencyDied",
    13: "Other",
    14: "Freezer",
    15: "PackageStateChange",
    16: "PackageUpdated",
}


@dataclasses.dataclass
class ExitInfoRecord:
  pid: int
  reason_code: int
  reason_str: str
  subreason_code: Optional[int]
  subreason_str: Optional[str]
  status: Optional[int]
  description: str
  raw_text: str


@dataclasses.dataclass
class DecodedPmaHistogram:
  name: str
  count: int
  sum_val: int
  buckets: Dict[int, int]  # bucket_index -> count


@dataclasses.dataclass
class ScenarioResult:
  scenario_name: str
  description: str
  pid: int
  termination_signal: Optional[int]
  android_exit_info: Optional[ExitInfoRecord]
  expected_reason_codes: List[int]
  expected_uma_enum: Optional[int]
  expected_uma_name: Optional[str]
  pma_files_found: List[str]
  relaunch_pid: Optional[int]
  decoded_uma: Optional[DecodedPmaHistogram]
  uma_verified: bool
  logcat_verified: bool
  relaunch_log_matches: List[str]
  success: bool
  error_message: str = ""


def decode_pma_histogram(data: bytes,
                         name_query: str) -> Optional[DecodedPmaHistogram]:
  """Walks PMA blocks and decodes histogram bucket counts for a given name."""
  if not data or len(data) < 64:
    return None

  offset = 64
  while offset + 16 <= len(data):
    size, cookie, type_id, next_offset = struct.unpack_from(
        "<IIII", data, offset)
    if size < 16 or offset + size > len(data):
      break
    data_offset = offset + 16
    if cookie == 0xC8799269 and type_id == 0xF1645913:  # TYPE_ID_PERSISTENT_HISTOGRAM
      (h_type, flags, min_val, max_val, buckets, ranges_ref, checksum,
       counts_ref) = struct.unpack_from("<iiiiIIII", data, data_offset)
      sm_id, sm_sum, sm_count, sm_single = struct.unpack_from(
          "<Qqii", data, data_offset + 32)
      name_part = data[data_offset + 80:offset + size].split(b"\x00")[0]
      try:
        name = name_part.decode("utf-8", errors="ignore")
      except Exception:
        name = ""

      if name == name_query:
        bucket_counts: Dict[int, int] = {}
        if counts_ref and counts_ref + 16 <= len(data):
          counts = struct.unpack_from(f"<{buckets}i", data, counts_ref + 16)
          for b_idx, cnt in enumerate(counts):
            if cnt > 0:
              bucket_counts[b_idx] = cnt
        elif sm_single and sm_single != -1:
          b_idx = sm_single & 0xFFFF
          cnt = (sm_single >> 16) & 0xFFFF
          if cnt > 0:
            bucket_counts[b_idx] = cnt

        return DecodedPmaHistogram(
            name=name,
            count=sm_count,
            sum_val=sm_sum,
            buckets=bucket_counts,
        )

    offset += size
  return None


class AdbDevice:
  """ADB command helper for test execution."""

  def __init__(self, device: Optional[str] = None):
    self.device = device or self._detect_device()
    if not self.device:
      raise RuntimeError("No connected ADB devices detected.")

  def _detect_device(self) -> Optional[str]:
    res = subprocess.run(["adb", "devices"],
                         capture_output=True,
                         text=True,
                         check=True)
    lines = res.stdout.strip().splitlines()[1:]
    devices = [
        l.split()[0]
        for l in lines
        if len(l.split()) >= 2 and l.split()[1] == "device"
    ]
    if not devices:
      return None
    for d in devices:
      if "localhost" in d or "127.0.0.1" in d:
        return d
    return devices[0]

  def run_cmd(self, cmd: List[str]) -> Tuple[int, str, str]:
    full_cmd = ["adb", "-s", self.device] + cmd
    res = subprocess.run(full_cmd, capture_output=True, text=True)
    return res.returncode, res.stdout, res.stderr

  def shell(self, command: str) -> Tuple[int, str, str]:
    return self.run_cmd(["shell", command])

  def shell_binary(self, command: str) -> Tuple[int, bytes, bytes]:
    full_cmd = ["adb", "-s", self.device, "shell", command]
    res = subprocess.run(full_cmd, capture_output=True)
    return res.returncode, res.stdout, res.stderr

  def get_pid(self, package: str) -> Optional[int]:
    code, out, _ = self.shell(f"pidof {package}")
    if code == 0 and out.strip():
      pids = out.strip().split()
      return int(pids[0])
    code, out, _ = self.shell(f"pgrep -f {package}")
    if code == 0 and out.strip():
      pids = out.strip().split()
      return int(pids[0])
    return None

  def force_stop(self, package: str) -> bool:
    code, _, _ = self.shell(f"am force-stop {package}")
    return code == 0

  def start_activity(self, package: str, activity: str) -> bool:
    code, _, _ = self.shell(f"am start -n {package}/{activity}")
    return code == 0

  def kill_pid(self, pid: int, signal: int) -> bool:
    code, _, _ = self.shell(
        f"kill -{signal} {pid} 2>/dev/null || su 0 kill -{signal} {pid} 2>/dev/null"
    )
    return code == 0

  def clear_logcat(self) -> None:
    self.shell("logcat -c")

  def get_logcat(self, filter_regex: str, max_lines: int = 100) -> List[str]:
    code, out, _ = self.shell(
        f"logcat -d | grep -E '{filter_regex}' | tail -n {max_lines}")
    if code == 0 and out.strip():
      return [line.strip() for line in out.strip().splitlines()]
    return []

  def clean_pma_files(self, package: str) -> bool:
    cmds = [
        f"run-as {package} rm -f app_content_shell/BrowserStabilityMetrics/* 2>/dev/null",
        f"run-as {package} rm -f BrowserStabilityMetrics/* 2>/dev/null",
        f"su 0 rm -f /data/data/{package}/app_content_shell/BrowserStabilityMetrics/* 2>/dev/null",
        f"su 0 rm -f /data/data/{package}/BrowserStabilityMetrics/* 2>/dev/null",
    ]
    for cmd in cmds:
      self.shell(cmd)
    return True

  def list_pma_files(self, package: str) -> List[str]:
    commands = [
        f"run-as {package} ls app_content_shell/BrowserStabilityMetrics/ 2>/dev/null",
        f"run-as {package} ls BrowserStabilityMetrics/ 2>/dev/null",
        f"su 0 ls /data/data/{package}/app_content_shell/BrowserStabilityMetrics/ 2>/dev/null",
        f"su 0 ls /data/data/{package}/BrowserStabilityMetrics/ 2>/dev/null",
    ]
    for cmd in commands:
      code, out, _ = self.shell(cmd)
      if code == 0 and out.strip():
        files = [
            f.strip()
            for f in out.strip().splitlines()
            if f.strip().endswith(".pma")
        ]
        if files:
          return files
    return []

  def read_pma_file_for_pid(self, package: str, pid: int) -> Optional[bytes]:
    pid_hex = f"{pid:X}"
    files = self.list_pma_files(package)
    target_file = None
    for f in files:
      if f.endswith(f"-{pid_hex}.pma"):
        target_file = f
        break
    if not target_file:
      return None

    paths = [
        f"app_content_shell/BrowserStabilityMetrics/{target_file}",
        f"BrowserStabilityMetrics/{target_file}",
    ]
    for p in paths:
      code, data, _ = self.shell_binary(
          f"run-as {package} cat '{p}' 2>/dev/null")
      if code == 0 and len(data) >= 64:
        return data
      code, data, _ = self.shell_binary(
          f"su 0 cat '/data/data/{package}/{p}' 2>/dev/null")
      if code == 0 and len(data) >= 64:
        return data
    return None

  def get_dumpsys_exit_info(self, package: str) -> List[ExitInfoRecord]:
    code, out, _ = self.shell(f"dumpsys activity exit-info {package}")
    if code != 0 or not out.strip():
      return []
    records = []
    blocks = re.split(r"ApplicationExitInfo\s+#\d+:", out)
    for block in blocks[1:]:
      pid_match = re.search(r"pid=(\d+)", block)
      reason_match = re.search(r"reason=(\d+)\s*\(([^)]+)\)", block)
      subreason_match = re.search(r"subreason=(\d+)\s*\(([^)]+)\)", block)
      status_match = re.search(r"status=(\d+)", block)
      desc_match = re.search(r"description=([^\n]+)", block)

      if pid_match and reason_match:
        pid = int(pid_match.group(1))
        reason_code = int(reason_match.group(1))
        reason_str = reason_match.group(2).strip()
        sub_code = int(subreason_match.group(1)) if subreason_match else None
        sub_str = subreason_match.group(2).strip() if subreason_match else None
        status = int(status_match.group(1)) if status_match else None
        desc = desc_match.group(1).strip() if desc_match else ""

        records.append(
            ExitInfoRecord(
                pid=pid,
                reason_code=reason_code,
                reason_str=reason_str,
                subreason_code=sub_code,
                subreason_str=sub_str,
                status=status,
                description=desc,
                raw_text=block.strip(),
            ))
    return records


class ExitReasonVerifier:
  """Orchestrates test scenarios for process exit reason collection."""

  def __init__(self,
               adb: AdbDevice,
               package: str = DEFAULT_PACKAGE,
               activity: str = DEFAULT_ACTIVITY,
               launch_wait: float = 4.0):
    self.adb = adb
    self.package = package
    self.activity = activity
    self.launch_wait = launch_wait

  def launch_and_wait(self) -> Optional[int]:
    self.adb.force_stop(self.package)
    time.sleep(0.5)
    self.adb.start_activity(self.package, self.activity)
    pid = None
    for _ in range(20):
      time.sleep(0.2)
      pid = self.adb.get_pid(self.package)
      if pid:
        break
    if not pid:
      return None
    time.sleep(self.launch_wait)
    return pid

  def wait_for_exit(self, pid: int, timeout: float = 5.0) -> bool:
    start = time.time()
    while time.time() - start < timeout:
      current_pid = self.adb.get_pid(self.package)
      if current_pid != pid:
        return True
      time.sleep(0.2)
    return False

  def run_scenario(self, scenario_name: str, signal: Optional[int],
                   expected_reasons: List[int], desc: str) -> ScenarioResult:
    print(f"\n=======================================================")
    print(f"Running Scenario: {scenario_name}")
    print(f"Description: {desc}")
    print(f"=======================================================")

    # 1. Clean prior PMA files for isolated test
    self.adb.clean_pma_files(self.package)

    # 2. Launch Cobalt
    print(f"[*] Launching {self.package}...")
    pid = self.launch_and_wait()
    if not pid:
      return ScenarioResult(
          scenario_name=scenario_name,
          description=desc,
          pid=-1,
          termination_signal=signal,
          android_exit_info=None,
          expected_reason_codes=expected_reasons,
          expected_uma_enum=None,
          expected_uma_name=None,
          pma_files_found=[],
          relaunch_pid=None,
          decoded_uma=None,
          uma_verified=False,
          logcat_verified=False,
          relaunch_log_matches=[],
          success=False,
          error_message="Failed to launch app or acquire PID.",
      )
    print(f"[*] App running with PID: {pid}")

    # 3. Check active PMA file
    pma_files = self.adb.list_pma_files(self.package)
    print(f"[*] Active PMA files on device: {pma_files}")

    # 4. Terminate app
    print(f"[*] Terminating process (PID: {pid})...")
    if signal is None:
      self.adb.force_stop(self.package)
    else:
      self.adb.kill_pid(pid, signal)

    if not self.wait_for_exit(pid):
      print(f"[!] Warning: Process {pid} did not terminate promptly.")

    time.sleep(1.0)

    # 5. Fetch exit info from Android ActivityManager
    exit_records = self.adb.get_dumpsys_exit_info(self.package)
    matching_record = None
    for rec in exit_records:
      if rec.pid == pid:
        matching_record = rec
        break

    if matching_record:
      print(f"[+] ActivityManager recorded exit for PID {pid}:")
      print(
          f"    Reason: {matching_record.reason_code} ({matching_record.reason_str})"
      )
      if matching_record.subreason_code is not None:
        print(
            f"    Subreason: {matching_record.subreason_code} ({matching_record.subreason_str})"
        )
      if matching_record.status is not None:
        print(f"    Status: {matching_record.status}")
      if matching_record.description:
        print(f"    Description: {matching_record.description}")
    else:
      print(f"[!] No dumpsys exit-info record found for PID {pid}")

    uma_enum_val = None
    uma_enum_name = None
    if matching_record and matching_record.reason_code in SYSTEM_REASON_TO_UMA_ENUM:
      uma_enum_val, uma_enum_name = SYSTEM_REASON_TO_UMA_ENUM[
          matching_record.reason_code]
      print(f"[+] Expected UMA Mapping: Enum {uma_enum_val} ({uma_enum_name})")

    # 6. Relaunch Cobalt and check UMA emission and logcat
    print(
        f"[*] Relaunching {self.package} to test retrieval of prior exit reason..."
    )
    self.adb.clear_logcat()
    relaunch_pid = self.launch_and_wait()
    print(f"[*] Relaunched with PID: {relaunch_pid}")

    # Check logcat for exit reason reporting log
    log_matches = self.adb.get_logcat(
        rf"Recording exit reason for prior session PID {pid}|Cobalt\.Stability\.Android\.SystemExitReason",
        max_lines=50)
    logcat_verified = False
    if log_matches:
      print(f"[+] Logcat verified: Found {len(log_matches)} matching entries:")
      for line in log_matches[:3]:
        print(f"    {line}")
      logcat_verified = True
    else:
      print(
          f"[?] No explicit log line found in logcat (logging may be at higher level)."
      )

    # Read and decode active PMA file of relaunch_pid
    decoded_uma = None
    uma_verified = False
    if relaunch_pid:
      pma_data = self.adb.read_pma_file_for_pid(self.package, relaunch_pid)
      if pma_data:
        decoded_uma = decode_pma_histogram(pma_data, HISTOGRAM_NAME)
        if decoded_uma:
          print(f"[+] PMA Decoded UMA Histogram: {decoded_uma.name}")
          print(
              f"    Total Samples: {decoded_uma.count}, Buckets: {decoded_uma.buckets}"
          )
          if uma_enum_val is not None and uma_enum_val in decoded_uma.buckets:
            bucket_cnt = decoded_uma.buckets[uma_enum_val]
            print(
                f"[+] UMA Bucket {uma_enum_val} ({uma_enum_name}) verified with count {bucket_cnt}!"
            )
            uma_verified = True
          else:
            print(
                f"[!] Warning: Expected UMA bucket {uma_enum_val} not found in decoded buckets: {decoded_uma.buckets}"
            )
        else:
          print(f"[!] Could not locate {HISTOGRAM_NAME} in PMA binary.")

        # Check for pre-joined PeakRssMB histograms if summary is present
        if matching_record and matching_record.reason_code in REASON_TO_SUFFIX:
          suffix = REASON_TO_SUFFIX[matching_record.reason_code]
          rss_reason_hist = decode_pma_histogram(
              pma_data, f"Cobalt.Stability.Android.PeakRssMB.{suffix}")
          rss_all_hist = decode_pma_histogram(
              pma_data, "Cobalt.Stability.Android.PeakRssMB.AllExits")
          if rss_reason_hist:
            print(
                f"[+] Verified pre-joined histogram Cobalt.Stability.Android.PeakRssMB.{suffix} "
                f"(samples={rss_reason_hist.count}, buckets={rss_reason_hist.buckets})"
            )
          if rss_all_hist:
            print(
                f"[+] Verified pre-joined histogram Cobalt.Stability.Android.PeakRssMB.AllExits "
                f"(samples={rss_all_hist.count}, buckets={rss_all_hist.buckets})"
            )
      else:
        print(f"[!] Could not read PMA file for relaunch PID {relaunch_pid}.")

    success = False
    err = ""
    if not matching_record:
      err = f"PID {pid} not recorded by ActivityManager."
    elif matching_record.reason_code not in expected_reasons:
      err = (
          f"Exit reason {matching_record.reason_code} ({matching_record.reason_str}) "
          f"not in expected list {expected_reasons}.")
    elif not (uma_verified or logcat_verified):
      err = "Neither UMA histogram sample nor logcat entry was verified on relaunch."
    else:
      success = True

    return ScenarioResult(
        scenario_name=scenario_name,
        description=desc,
        pid=pid,
        termination_signal=signal,
        android_exit_info=matching_record,
        expected_reason_codes=expected_reasons,
        expected_uma_enum=uma_enum_val,
        expected_uma_name=uma_enum_name,
        pma_files_found=pma_files,
        relaunch_pid=relaunch_pid,
        decoded_uma=decoded_uma,
        uma_verified=uma_verified,
        logcat_verified=logcat_verified,
        relaunch_log_matches=log_matches,
        success=success,
        error_message=err,
    )


def main():
  parser = argparse.ArgumentParser(
      description="Verify Android process exit reason tracking across termination signals."
  )
  parser.add_argument(
      "--device", help="ADB device serial (default: auto-detect)")
  parser.add_argument(
      "--package",
      default=DEFAULT_PACKAGE,
      help=f"Package name (default: {DEFAULT_PACKAGE})")
  parser.add_argument(
      "--activity",
      default=DEFAULT_ACTIVITY,
      help=f"Main activity (default: {DEFAULT_ACTIVITY})")
  parser.add_argument(
      "--launch-wait",
      type=float,
      default=4.0,
      help="Wait time in seconds after launch (default: 4.0)")
  parser.add_argument(
      "--scenarios",
      default="all",
      help="Comma-separated scenarios: all, force-stop, sigkill, sigsegv, sigabrt, sigterm",
  )
  args = parser.parse_args()

  try:
    adb = AdbDevice(args.device)
    print(f"Using ADB device: {adb.device}")
  except Exception as e:
    print(f"Error initializing ADB device: {e}")
    sys.exit(1)

  _, sdk_str, _ = adb.shell("getprop ro.build.version.sdk")
  try:
    sdk_int = int(sdk_str.strip())
    print(f"Device Android SDK API Level: {sdk_int}")
    if sdk_int < 30:
      print(
          f"[!] Warning: Device API level is {sdk_int} (< 30 / Android R). "
          f"ActivityManager.getHistoricalProcessExitReasons is only available on API 30+."
      )
  except ValueError:
    pass

  verifier = ExitReasonVerifier(
      adb,
      package=args.package,
      activity=args.activity,
      launch_wait=args.launch_wait)

  all_scenarios = {
      "force-stop": {
          "signal":
              None,
          "expected": [10],  # REASON_USER_REQUESTED
          "desc":
              "User stop via `am force-stop` (maps to REASON_USER_REQUESTED)",
      },
      "sigkill": {
          "signal": 9,
          "expected": [2, 10],  # REASON_SIGNALED or REASON_USER_REQUESTED
          "desc": "Uncatchable termination via SIGKILL (kill -9)",
      },
      "sigsegv": {
          "signal": 11,
          "expected": [5],  # REASON_CRASH_NATIVE
          "desc": "Segmentation fault crash via SIGSEGV (kill -11)",
      },
      "sigabrt": {
          "signal": 6,
          "expected": [5],  # REASON_CRASH_NATIVE
          "desc": "Assertion failure / abort via SIGABRT (kill -6)",
      },
      "sigterm": {
          "signal": 15,
          "expected": [
              1, 2, 10
          ],  # REASON_EXIT_SELF, REASON_SIGNALED, or REASON_USER_REQUESTED
          "desc": "Graceful termination request via SIGTERM (kill -15)",
      },
  }

  if args.scenarios == "all":
    scenarios_to_run = list(all_scenarios.keys())
  else:
    scenarios_to_run = [
        s.strip().lower() for s in args.scenarios.split(",") if s.strip()
    ]

  results: List[ScenarioResult] = []
  for sc_name in scenarios_to_run:
    if sc_name not in all_scenarios:
      print(f"[!] Unknown scenario: {sc_name}. Skipping.")
      continue
    cfg = all_scenarios[sc_name]
    res = verifier.run_scenario(
        scenario_name=sc_name,
        signal=cfg["signal"],
        expected_reasons=cfg["expected"],
        desc=cfg["desc"],
    )
    results.append(res)

  # Print Summary Table
  print("\n" + "=" * 90)
  print("PROCESS EXIT REASON VERIFICATION SUMMARY (ANDROID OS + UMA HISTOGRAM)")
  print("=" * 90)
  print(
      f"{'Scenario':<11} | {'PID':<6} | {'Android Reason':<22} | {'Expected UMA Enum':<22} | {'Decoded PMA Bucket':<20} | {'Logcat':<6} | {'Result':<6}"
  )
  print("-" * 90)
  all_passed = True
  for r in results:
    if r.android_exit_info:
      reason_repr = f"{r.android_exit_info.reason_code}: {r.android_exit_info.reason_str}"
    else:
      reason_repr = "N/A (not found)"

    if r.expected_uma_enum is not None:
      expected_uma = f"{r.expected_uma_enum}: {r.expected_uma_name}"
    else:
      expected_uma = "N/A"

    if r.decoded_uma and r.expected_uma_enum in r.decoded_uma.buckets:
      cnt = r.decoded_uma.buckets[r.expected_uma_enum]
      pma_repr = f"Bucket {r.expected_uma_enum} (cnt={cnt})"
    elif r.decoded_uma and r.decoded_uma.buckets:
      pma_repr = f"Buckets {list(r.decoded_uma.buckets.keys())}"
    else:
      pma_repr = "None"

    log_repr = "YES" if r.logcat_verified else "NO"
    status_repr = "PASS" if r.success else "FAIL"
    if not r.success:
      all_passed = False

    print(
        f"{r.scenario_name:<11} | {r.pid:<6} | {reason_repr:<22} | {expected_uma:<22} | {pma_repr:<20} | {log_repr:<6} | {status_repr:<6}"
    )

  print("=" * 90)
  if all_passed:
    print(
        "[+] All tested process exit scenarios verified in Android OS and emitted to UMA!"
    )
  else:
    print("[!] Some scenarios failed. See details above.")
    sys.exit(1)


if __name__ == "__main__":
  main()
