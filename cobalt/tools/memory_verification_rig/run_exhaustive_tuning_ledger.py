#!/usr/bin/env python3
# Copyright 2026 The Cobalt Authors. All Rights Reserved.
"""Master Memory Optimizations Ledger Automation Runner Rig for Cobalt.

Programmatically parses, regex-filters, and systematically profiles all
memory-related C++ BASE_FEATUREs, V8 flags, and Blink cache switches
sequentially across the 4 Critical User Journeys (CUJs).
"""

import os
import re
import sys
import time
import subprocess

REPO_ROOT = "/usr/local/google/home/avvall/secondary_cobalt_checkout/src"
LEDGER_PATH = os.path.join(REPO_ROOT, "memory_optimizations_ledger.md")

# Prohibited list (User constraints)
PROHIBITED = [
    "jitless", "max-old-space-size", "lite-mode", "single-threaded",
    "mse-video-buffer-size-limit-mb", "mse-audio-buffer-size-limit-mb",
    "gpu-rasterization-msaa-sample-count", "skia-resource-cache-limit-mb",
    "disk-cache-size", "disable-http-cache",
    "CobaltLocalTypefaceCacheSizeInBytes",
    "CobaltRemoteTypefaceCacheSizeInBytes", "SmallerInterestArea"
]


def parse_features_report():
  """Parses feature_flags_report.md and extracts all raw flag names."""
  report_path = os.path.join(REPO_ROOT, "feature_flags_report.md")
  if not os.path.exists(report_path):
    print(f"❌ Error: feature_flags_report.md not found at {report_path}")
    sys.exit(1)

  flags = []
  # Match bulleted flags: e.g. kAlignSurfaceLayerImplToPixelGrid
  flag_pattern = re.compile(r"^\s*-\s*`([^`]+)`")
  with open(report_path, "r", encoding="utf-8") as f:
    for line in f:
      match = flag_pattern.match(line)
      if match:
        flag_name = match.group(1).strip()
        flags.append(flag_name)
  return flags


def filter_memory_flags(raw_flags):
  """Filters memory-related flags, bypassing prohibited list."""
  pattern_str = (r"(?i)(memory|cache|evict|limit|purge|reclaim|"
                 r"size|thread|low_?end|gpu_?memory|buffer|"
                 r"allocator|tile|decoded|decoder|msaa|sdr|"
                 r"compression|saver)")
  memory_keywords = re.compile(pattern_str)
  filtered = []
  for flag in raw_flags:
    # Bypass prohibited list checks
    if any(p in flag for p in PROHIBITED):
      continue
    # Keep only memory-related matches
    if memory_keywords.search(flag):
      filtered.append(flag)
  return filtered


def run_campaign(flag):
  """Configures and executes verify_stat_sig.py campaigns for a flag."""
  experiment_args = (
      "--remote-allow-origins=*,--enable-metrics,"
      "--force-enable-metrics-reporting,--remote-debugging-port=9222,"
      "--memory-metrics-interval=30")
  if flag.startswith("--"):
    experiment_args += f",--js-flags={flag}"
  elif flag.startswith("k") and flag[1].isupper():
    experiment_args += f",--enable-features={flag}"
  else:
    experiment_args += f",--enable-blink-features={flag}"

  script_path = os.path.join(
      REPO_ROOT, "cobalt/tools/memory_verification_rig/verify_stat_sig.py")
  base_args_val = (
      "--remote-allow-origins=*,--enable-metrics,"
      "--force-enable-metrics-reporting,--remote-debugging-port=9222,"
      "--memory-metrics-interval=30")

  cmd = [
      "python3",
      script_path,
      "--platform=linux",
      "--bin-path=./out/linux-x64x11_qa/cobalt",
      "--runs=15",
      "--duration=180",  # 3-minute stress saturation!
      "--cuj=all",  # Profile all 4 golden CUJs sequentially!
      f"--baseline-args={base_args_val}",
      f"--experiment-args={experiment_args}",
      "--package=https://www.youtube.com/tv#/watch?v=AB-4pS2Og1g"
  ]

  print(f"\n🚀 [LEDGER LAUNCH] Campaign for Flag: {flag}")
  cmd_str = " ".join(cmd)
  print(f"   Running: {cmd_str}")

  # Execute significance loop and capture stdout logs
  proc = subprocess.run(cmd, capture_output=True, text=True, check=False)
  return proc.stdout


def parse_and_append_ledger(flag, stdout_logs):
  """Parses evaluation outputs and appends structured entry to ledger."""
  print(f"✍️ [LEDGER RECORD] Campaign completed for {flag}...")

  # Extract CUJ evaluation evaluation matches
  browse_sig = "NOT SIGNIGICANT 🔴"
  watch_sig = "NOT SIGNIGICANT 🔴"
  baseline_sig = "NOT SIGNIGICANT 🔴"
  combined_sig = "NOT SIGNIGICANT 🔴"

  # Graphics/Script reductions variables
  graphics_reduction = "0.00 MB"
  script_reduction = "0.00 MB"
  rss_reduction = "0.00 MB"

  # Parse lines in stdout
  lines = stdout_logs.split("\n")
  current_cuj = ""
  for line in lines:
    if "🏁 RUNNING SIGNIFICANCE CAMPAIGN:" in line:
      current_cuj = line.split("🏁 RUNNING SIGNIFICANCE CAMPAIGN:")[-1].strip()
    elif "CONCLUSION: STATISTICALLY SIGNIFICANT" in line:
      if "System RSS" in line:
        rss_reduction = "SIGNIFICANT 🟢"
      elif "kGraphics" in line:
        graphics_reduction = "SIGNIFICANT 🟢"
      elif "kScript" in line:
        script_reduction = "SIGNIFICANT 🟢"

      if current_cuj:
        if "1. BROWSE" in current_cuj:
          browse_sig = "SIGNIFICANT 🟢"
        elif "2. WATCH" in current_cuj:
          watch_sig = "SIGNIFICANT 🟢"
        elif "3. BASELINE" in current_cuj:
          baseline_sig = "SIGNIFICANT 🟢"
        elif "4. COMBINED" in current_cuj:
          combined_sig = "SIGNIFICANT 🟢"

  is_approved = "🟢" in (browse_sig + watch_sig + combined_sig)
  ledger_status = "[APPROVED] ✅" if is_approved else "[REJECTED] ❌"

  # Format structured markdown entry
  ledger_entry = f"""
## 🛠️ Flag: `{flag}`
*   **Browse CUJ Telemetry:** {browse_sig}
*   **Watch CUJ Telemetry:** {watch_sig}
*   **Baseline CUJ Telemetry:** {baseline_sig}
*   **Combined CUJ Telemetry:** {combined_sig}
*   **Graphics allocation Win:** {graphics_reduction}
*   **Script allocation Win:** {script_reduction}
*   **System RSS footprint Win:** {rss_reduction}
*   **LEDGER STATUS:** {ledger_status}

---
"""
  with open(LEDGER_PATH, "a", encoding="utf-8") as f:
    f.write(ledger_entry)


def main():
  print("============================================================")
  print("🛰️ MASTER MEMORY OPTIMIZATIONS LEDGER RUNNER INITIATING")
  print("============================================================")

  # Ensure ledger file exists with beautiful headers
  if not os.path.exists(LEDGER_PATH):
    with open(LEDGER_PATH, "w", encoding="utf-8") as f:
      f.write("# Cobalt Master Memory Optimizations Ledger\n\n")
      desc_str = ("This ledger keeps a historic, A/B resampled statistical "
                  "log of all memory optimization flags evaluated on "
                  "low-end Smart TV platforms.\n\n")
      f.write(desc_str)
      f.write("---\n")

  raw_flags = parse_features_report()
  memory_flags = filter_memory_flags(raw_flags)

  print(f"🔍 Total Base Flags found in report: {len(raw_flags)}")
  print(f"🎯 Filtered memory-related flags: {len(memory_flags)}")
  print(f"🚀 Initiating sequential profiling for {len(memory_flags)} flags...")

  for idx, flag in enumerate(memory_flags, 1):
    print(f"\n🔄 [PROGRESS] Queue Item {idx}/{len(memory_flags)}")
    try:
      logs = run_campaign(flag)
      parse_and_append_ledger(flag, logs)
    except Exception as e:  # pylint: disable=broad-exception-caught
      print(f"⚠️ Warning: Campaign failed for flag {flag}: {e}")
    time.sleep(5)

  print("\n🎉 EXHAUSTIVE MEMORY LEDGER COMPLETED!")


if __name__ == "__main__":
  main()
