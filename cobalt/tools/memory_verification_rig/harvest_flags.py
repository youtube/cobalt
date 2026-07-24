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
"""CLI switches & Blink/content Features static harvester for Cobalt."""

import argparse
import logging
import os
import re

# Known C++ files declaring features and CLI switches in Chromium/Cobalt
TARGET_SOURCE_FILES = [
    "cc/base/switches.cc",
    "cc/base/features.cc",
    "gpu/command_buffer/service/gpu_switches.cc",
    "content/public/common/content_switches.cc",
    "content/public/common/content_features.cc",
    "third_party/blink/common/features.cc",
    "third_party/blink/common/switches.cc",
    "cobalt/shell/common/shell_switches.cc",
    "cobalt/browser/switches.cc",
    "cobalt/browser/features.cc",
]


def scan_file_for_switches_and_features(file_path):
  """Uses regex parsing to extract switches and features from C++ files."""
  switches = []
  features = []

  if not os.path.exists(file_path):
    return switches, features

  try:
    with open(file_path, "r", encoding="utf-8") as f:
      content = f.read()

      # 1. Parse switches: const char kName[] = "switch-string";
      # Captures both variable name and string switch value for filtering
      switch_pattern = r'const char (k[A-Za-z0-9_]+)\[\]\s*=\s*"([^"]+)";'
      raw_switches = re.findall(switch_pattern, content)
      for var_name, switch_val in raw_switches:
        # A. Ignore purely numeric values (e.g. 0, 1)
        if switch_val.isdigit():
          continue
        # B. Ignore constant switch values (containing underscore '_')
        if "_" in var_name:
          continue
        # C. Require standard switches structure (contains at least one
        # hyphen '-')
        if "-" not in switch_val:
          continue
        switches.append(switch_val)

      # 2. Parse features: BASE_FEATURE(kName, "FeatureName", ...)
      feature_pattern = r'BASE_FEATURE\(k[A-Za-z0-9_]+,\s*"([^"]+)"'
      features = re.findall(feature_pattern, content)
  except OSError as e:
    logging.warning("  ⚠️ Warning: Failed to parse C++ file %s: %s", file_path,
                    e)

  return switches, features


def main():
  parser = argparse.ArgumentParser(
      description="Cobalt CLI switches & Feature flags Harvester")
  parser.add_argument(
      "--repo-root",
      default="/usr/local/google/home/avvall/cobalt/src",
      help="Path to repository root directory")
  parser.add_argument(
      "--output-file",
      default="scanned_flags.txt",
      help="Output file path to write the discovered switches")
  args = parser.parse_args()

  logging.basicConfig(level=logging.INFO, format="%(message)s")

  logging.info("=" * 65)
  logging.info("🔍 COBALT CONFIGURATION HARVESTER: SCANNING CODEBASE")
  logging.info("=" * 65)

  all_switches = set()
  all_features = set()

  for relative_path in TARGET_SOURCE_FILES:
    full_path = os.path.join(args.repo_root, relative_path)
    logging.info("Scanning file: %s...", relative_path)
    switches, features = scan_file_for_switches_and_features(full_path)
    all_switches.update(switches)
    all_features.update(features)

  # Filter out noisy, non-toggleable, or debugging specific switches
  noise_keywords = [
      "help",
      "version",
      "log",
      "vmodule",
      "trace",
      "crash",
      "parent",
      "child",
      "ipc",
  ]
  filtered_switches = []
  for sw in sorted(all_switches):
    if not any(k in sw for k in noise_keywords):
      filtered_switches.append(sw)

  logging.info("-" * 65)
  logging.info(" discovered: %s switches | %s features", len(filtered_switches),
               len(all_features))
  logging.info("-" * 65)

  # Write to output file in standard verify_stat_sig format
  output_path = os.path.join(args.repo_root, args.output_file)
  try:
    with open(output_path, "w", encoding="utf-8") as f:
      f.write("# === SCOPIED CLI SWITCHES ===\n")
      for sw in filtered_switches:
        f.write(f"switch: --{sw}\n")
      f.write("\n# === SCOPIED FEATURE FLAGS ===\n")
      for feat in sorted(all_features):
        f.write(f"feature: {feat}\n")
    logging.info("🟢 DISCOVERED SWITCHES WRITTEN SUCCESSFULLY TO: %s",
                 output_path)
  except OSError as e:
    logging.error("❌ Error: Failed to write scanned flags: %s", e)

  logging.info("%s\n", "=" * 65)


if __name__ == "__main__":
  main()
