#!/usr/bin/env vpython3
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
"""Script to assess the difficulty of disabling a feature in Cobalt.

This script provides a unified tool to analyze difficulty at different levels:
- By Symbol: Finds a specific symbol and its target.
- By Path: Analyzes a whole directory (fast, uses `gn ls`).
- By Namespace: Analyzes a C++ namespace (uses SuperSize to map to files).
- By Component: Analyzes a SuperSize component (derived from DIR_METADATA).
"""

import argparse
import json
import os
import subprocess
import sys
import traceback

# Calculate repository root relative to this script's location
_REPO_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "..", "..", ".."))

# Add libsupersize to python path
sys.path.append(os.path.join(_REPO_ROOT, "tools/binary_size"))
sys.path.append(os.path.join(_REPO_ROOT, "tools/binary_size/libsupersize"))
# pylint: disable=wrong-import-position
import libsupersize.archive

# ============================================================================
# Group 1: Low-Level I/O and Symbol Helpers
# ============================================================================


def load_size_file(size_file, verbose):
  """Parses the SuperSize .size file to load symbol information."""
  print(f"\n[Log] Loading size file: {size_file}...")
  print("[Log] This may take a minute as SuperSize parses all symbols.")
  try:
    return libsupersize.archive.LoadAndPostProcessSizeInfo(size_file)
  except Exception as e:  # pylint: disable=broad-exception-caught
    print(f"[Error] Failed to load size file: {e}")
    if verbose:
      traceback.print_exc()
    return None


def query_feature_symbols(size_info, parsed_args):
  """Queries SuperSize symbols based on the provided CLI search mode."""
  print("\n[Log] Querying feature symbols...")
  if parsed_args.symbol:
    return size_info.symbols.WhereNameMatches(parsed_args.symbol)
  elif parsed_args.path:
    return size_info.symbols.WherePathMatches(parsed_args.path)
  elif parsed_args.namespace:
    return size_info.symbols.WhereNameMatches(parsed_args.namespace)
  elif parsed_args.component:
    return size_info.symbols.WhereComponentMatches(parsed_args.component)
  return None


def extract_feature_files(syms, parsed_args):
  """Extracts unique source/object files compiled by matched symbols.

  Files are sorted by PSS contribution descending.
  """
  print("[Log] Extracting feature files...")
  file_sizes = {}
  for s in syms:
    path = s.source_path or s.object_path
    if path:
      file_sizes[path] = file_sizes.get(path, 0.0) + s.pss
  feature_files = sorted(
      file_sizes.keys(), key=lambda f: file_sizes[f], reverse=True)

  if parsed_args.symbol:
    print(f"[Log] Found {len(syms)} symbols matching "
          f"'{parsed_args.symbol}' spanning {len(feature_files)} files.")
    if len(feature_files) > 100:
      print(f"[Warning] The symbol you are searching for is too generic, "
            f"it matched {len(feature_files)} source files.")
      print("          Truncating search to the first 100 "
            "size-contributing source files for safety.")
      feature_files = feature_files[:100]
  elif parsed_args.path:
    print(f"[Log] Found {len(syms)} symbols in path '{parsed_args.path}' "
          f"spanning {len(feature_files)} files.")
  elif parsed_args.namespace or parsed_args.component:
    print(f"[Log] Found {len(syms)} symbols spanning "
          f"{len(feature_files)} files.")

  return feature_files


def deduce_feature_keyword(parsed_args):
  """Deduces the core feature name keyword from the provided search mode."""
  if parsed_args.symbol:
    return parsed_args.symbol.lower()
  elif parsed_args.path:
    return os.path.basename(parsed_args.path.strip("/")).lower()
  elif parsed_args.namespace:
    return parsed_args.namespace.split("::")[-1].lower()
  elif parsed_args.component:
    return parsed_args.component.split(">")[-1].lower()
  return ""


def calculate_feature_size(syms):
  """Calculates and prints the total size (PSS) of matched symbols."""
  total_pss = sum(s.pss for s in syms)
  print(f"[Result] Total PSS of matched symbols: {total_pss} bytes")
  return total_pss


# ============================================================================
# Group 2: GN Build Graph Helpers
# ============================================================================


def discover_feature_targets(feature_files, gn_build_directory, root_target,
                             command_runner_function):
  """Maps matched feature files to GN targets and filters by reachability."""
  print("[Log] Discovering feature targets...")

  gn_targets = set()

  def add_targets_from_output(stdout):
    for line in stdout.split("\n"):
      if line.strip():
        gn_targets.add(line.strip().split("(")[0])

  chunk_size = 100
  for i in range(0, len(feature_files), chunk_size):
    chunk = feature_files[i:i + chunk_size]
    res = command_runner_function(["gn", "refs", gn_build_directory] + chunk)
    if res.returncode == 0:
      add_targets_from_output(res.stdout)
    else:
      print(f"[Warning] 'gn refs' failed for a chunk of files: {res.stderr}")

  print(f"[Result] Found {len(gn_targets)} unique feature targets.")
  if len(gn_targets) <= 20:
    for t in gn_targets:
      print(f"  - {t}")

  return filter_reachable_targets(gn_targets, gn_build_directory, root_target,
                                  command_runner_function)


def filter_reachable_targets(gn_targets, gn_build_directory, root_target,
                             command_runner_function):
  """Filters reachable targets from the build root."""
  # Step A: Run a single bulk query to get all targets compiled in Cobalt
  print("[Log] Fetching all linked targets via bulk 'gn desc deps'...")
  res = command_runner_function(
      ["gn", "desc", gn_build_directory, root_target, "deps", "--all"])
  if res.returncode != 0:
    print(f"[Warning] Bulk reachability check failed: {res.stderr}")
    return []

  linked_targets = set()
  for line in res.stdout.split("\n"):
    line = line.strip()
    if not line:
      continue
    # Normalize: strip any toolchain modifier suffix
    # (e.g., "//path:target(//toolchain)")
    target = line.split("(")[0].strip()
    linked_targets.add(target)

  # The root target itself is by definition part of the build graph
  linked_targets.add(root_target)

  reachable_targets = [t for t in gn_targets if t in linked_targets]
  print(f"[Result] {len(reachable_targets)} out of {len(gn_targets)} targets "
        f"are reachable from {root_target}.")
  if len(reachable_targets) <= 20:
    for t in reachable_targets:
      print(f"  - {t}")
  return reachable_targets


def analyze_feature_flags(feature_keyword, build_dir, command_runner):
  """Queries GN args to find flags matching the feature keyword."""
  print(
      f"[Log] Querying GN args to find flags related to '{feature_keyword}'...")
  if not feature_keyword:
    return []

  gn_args_res = command_runner(["gn", "args", build_dir, "--list"])
  gn_args_output = gn_args_res.stdout if gn_args_res.returncode == 0 else ""

  matching_flags = {}
  current_flag = None
  current_value = None
  matched = False

  for line in gn_args_output.split("\n"):
    if line and not line.startswith(" "):
      if current_flag and matched:
        matching_flags[current_flag] = current_value
      current_flag = line.strip()
      current_value = None
      matched = False
      if feature_keyword in current_flag.lower():
        matched = True
    elif current_flag and matched:
      if "=" in line:
        if "Default value" in line:
          if current_value is None:
            current_value = line.split("=")[-1].strip()
        elif "Current value" in line:
          current_value = line.split("=")[-1].strip()

  if current_flag and matched:
    matching_flags[current_flag] = current_value

  flags = list(matching_flags.items())
  print(f"[Result] Found {len(flags)} potential feature flags.")
  for flag, val in flags:
    print(f"  - {flag} = {val}")

  return flags


# pylint: disable=too-many-positional-arguments
def classify_target(feature_target, feature_files, feature_keyword,
                    gn_build_directory, command_runner_function):
  """Classifies a target as DEDICATED or SHARED based on source file ratio."""
  # 1. Path segment keyword matching: if the feature keyword is a full
  # directory segment or name segment, it is DEDICATED by definition.
  if feature_keyword:
    normalized_target = feature_target.lower()
    feature_path_patterns = [
        f"/{feature_keyword}/", f"/{feature_keyword}:", f":{feature_keyword}"
    ]
    if any(p in normalized_target for p in feature_path_patterns):
      print(f"[Log] Target '{feature_target}' matches segment keyword "
            f"'{feature_keyword}'. Classified as DEDICATED.")
      return "DEDICATED", [], []

  res = command_runner_function(
      ["gn", "desc", gn_build_directory, feature_target, "sources"])
  if res.returncode != 0:
    print(f"[Warning] 'gn desc' failed for {feature_target}: {res.stderr}")
    return "UNKNOWN", [], []

  all_sources = set(line.strip().lstrip("/")
                    for line in res.stdout.split("\n")
                    if line.strip())

  if not all_sources:
    print("[Result] Target classified as: ASSEMBLY")
    return "ASSEMBLY", [], []

  matched_sources = [f for f in all_sources if f in feature_files]
  # SuperSize only tracks compiled sources (.cc), missing headers (.h).
  # Pair them so downstream removal can surgically gate both in BUILD.gn.
  matched_set = set(matched_sources)
  for f in matched_sources:
    base, ext = os.path.splitext(f)
    if ext in (".cc", ".cpp", ".mm", ".c"):
      for header_ext in (".h", ".hpp"):
        header_file = base + header_ext
        if header_file in all_sources:
          matched_set.add(header_file)
  matched_files = list(matched_set)

  ratio = len(matched_files) / len(all_sources)
  print(f"[Log] Target contains {len(matched_files)} feature files out of "
        f"{len(all_sources)} total files.")

  if ratio >= 0.7:
    print("[Result] Target classified as: DEDICATED")
    return "DEDICATED", [], list(all_sources)
  print("[Result] Target classified as: SHARED")
  return "SHARED", matched_files, list(all_sources)


# ============================================================================
# Group 3: C++ Dependency Auditing Helpers
# ============================================================================


def audit_cpp_integration_points(files_to_scan, feature_headers,
                                 enable_verbose_logging):
  """Scans C++ files to identify direct include references."""
  audit_reports = []

  for f in files_to_scan:
    clean_path = f[2:] if f.startswith("//") else f
    if not os.path.exists(clean_path):
      continue

    try:
      with open(clean_path, "r", encoding="utf-8", errors="ignore") as fh:
        content = fh.read()
    except Exception as e:  # pylint: disable=broad-exception-caught
      if enable_verbose_logging:
        print(f"[Warning] Failed to read file {clean_path}: {e}")
      continue

    # Find referencing includes
    referenced = []
    for line in content.split("\n"):
      line_strip = line.strip()
      if line_strip.startswith("#include") and '"' in line_strip:
        parts = line_strip.split('"')
        if len(parts) >= 2:
          include_path = parts[1]
          for h in feature_headers:
            # Match if the feature header path ends with the included path
            if h.endswith(include_path):
              referenced.append(line_strip)

    if referenced:
      audit_reports.append({
          "file": clean_path,
          "referenced_headers": list(set(referenced)),
      })

  # Print results immediately
  if audit_reports:
    print(
        f"[Result] Found {len(audit_reports)} C++ include integration points:")
    for f in audit_reports[:20]:
      file_path = f["file"]
      print(f"  - {file_path}")
    if len(audit_reports) > 20:
      print(
          f"  - ... and {len(audit_reports) - 20} more C++ integration files.")
  else:
    print("[Result] No C++ include integration points found.")
    print("         (Note: Watch out for indirect coupling like Mojo/Mojom\n"
          "                generated headers, dynamic interface binders, or\n"
          "                factory registration systems.)")

  return audit_reports


# ============================================================================
# Group 4: Difficulty Scoring, Output & Reporting Helpers
# ============================================================================


def score_target_difficulty(feature_files_count, integration_files_count):
  """Calculates the overall target difficulty for SHARED targets."""
  # 1. Calculate complexity points
  include_points = integration_files_count * 5
  file_points = feature_files_count * 1
  points = include_points + file_points

  print("[Result] Gating Complexity Points Breakdown:")
  print(f"  - C++ Include Gating: {integration_files_count} files "
        f"x 5 = {include_points} pts")
  print(f"  - Shared Gating Files: {feature_files_count} files "
        f"x 1 = {file_points} pts")
  print(f"  - Total Complexity: {points} pts")

  # 2. Map points to difficulty tiers
  if points <= 15:
    difficulty = "LOW"
    action = (f"Low complexity ({points} pts): Few calling files and low "
              "build coupling. Quick include-gating/swapping.")
  elif points <= 50:
    difficulty = "MEDIUM"
    action = (f"Moderate complexity ({points} pts): Requires include-gating "
              "and build-graph swap or file subtraction.")
  else:
    difficulty = "HIGH"
    action = (f"High complexity ({points} pts): Deeply integrated C++ "
              "callers and high build coupling. Broad refactoring required.")

  return difficulty, action


def suggest_related_feature_paths(size_info, feature_keyword,
                                  current_query_path):
  """Finds other directories in the codebase containing feature symbols."""
  if not feature_keyword:
    return []

  matched_syms = size_info.symbols.WherePathMatches(feature_keyword)
  related_dirs = set()
  for s in matched_syms:
    path = s.source_path or s.object_path
    if path:
      parent_dir = os.path.dirname(path)
      if parent_dir:
        related_dirs.add(parent_dir)

  norm_current = current_query_path.strip("/") if current_query_path else ""
  filtered_dirs = []
  for d in related_dirs:
    norm_d = d.strip("/")
    if not norm_current or (norm_current not in norm_d):
      filtered_dirs.append(d)

  return sorted(list(set(filtered_dirs)))


def calculate_overall_difficulty(target_assessments):
  """Propagates target difficulties to get the overall score."""
  overall_difficulty = "LOW"
  for a in target_assessments:
    diff = a["difficulty"]
    if diff == "HIGH":
      overall_difficulty = "HIGH"
    elif diff == "MEDIUM" and overall_difficulty != "HIGH":
      overall_difficulty = "MEDIUM"
    elif diff == "LOW/MEDIUM" and overall_difficulty not in ("HIGH", "MEDIUM"):
      overall_difficulty = "LOW/MEDIUM"
  return overall_difficulty


def print_unreachable_feature_assessment(root_target):
  """Prints LOW overall assessment if target is unreachable."""
  print("\n============================================================")
  print("CONSOLIDATED FINAL DIFFICULTY ASSESSMENT")
  print("============================================================")
  print("Difficulty: LOW")
  print(f"Reason: Feature is not used by the specified root target "
        f"{root_target}.")
  print("Action: Safe to remove or ignore for this build configuration.")
  print("============================================================")


def print_final_assessment(target_assessments, overall_difficulty,
                           has_feature_flag, related_paths):
  """Prints a consolidated difficulty assessment report to stdout."""
  print("\n============================================================")
  print("CONSOLIDATED FINAL DIFFICULTY ASSESSMENT")
  print("============================================================")

  shared_assessments = [a for a in target_assessments if a["type"] == "SHARED"]
  dedicated_assessments = [
      a for a in target_assessments if a["type"] == "DEDICATED"
  ]
  assembly_assessments = [
      a for a in target_assessments if a["type"] == "ASSEMBLY"
  ]

  # Detailed report for SHARED targets
  if shared_assessments:
    print(f"\n⚡ SHARED Targets ({len(shared_assessments)} targets) "
          "(Require BUILD.gn Gating & C++ Preprocessor):")
    for a in shared_assessments:
      target = a["target"]
      diff = a["difficulty"]
      action = a["action"]
      print(f"  - {target}:")
      print(f"    Difficulty: {diff}")
      print(f"    Action: {action}")

  # Consolidated summary for DEDICATED targets
  if dedicated_assessments:
    print(f"\n📦 DEDICATED Targets ({len(dedicated_assessments)} targets):")
    print("  Isolated/removed automatically by severing connections "
          "at SHARED boundaries.")
    print(
        "  No file-level or C++ code edits are required inside these targets.")
    if len(dedicated_assessments) <= 10:
      for a in dedicated_assessments:
        target_name = a["target"]
        print(f"  - {target_name}")
    else:
      for a in dedicated_assessments[:5]:
        target_name = a["target"]
        print(f"  - {target_name}")
      print("  - ... [and other DEDICATED targets] ...")
      for a in dedicated_assessments[-5:]:
        target_name = a["target"]
        print(f"  - {target_name}")

  # Consolidated summary for ASSEMBLY targets
  if assembly_assessments:
    print(f"\n📦 ASSEMBLY Targets ({len(assembly_assessments)} targets):")
    print("  High-level containers or group targets containing no "
          "direct C++ sources.")
    print("  Excludable strictly at dependency/graph level inside BUILD.gn.")
    for a in assembly_assessments:
      target_name = a["target"]
      print(f"  - {target_name}")

  print(f"\nOverall Difficulty: {overall_difficulty}")
  print("Reason: Based on individual target classifications and "
        "difficulty scores.")

  if has_feature_flag:
    print(
        "\n[Note] Existing GN flag(s) were found. If these flags are effective")
    print("       kill switches, the actual difficulty might be lower "
          "than estimated.")

  if related_paths:
    print(
        "\n[Directory Expansion Guide for Automation & Related Component Audit]"
    )
    print("  This feature has other compiled components codebase-wide.")
    print(
        "  To perform a complete audit or wider eradication, consider running")
    print("  this script in \"--path\" mode on these related directories:")
    for d in related_paths:
      print(f"    - {d}")

  print("\n⚠️ KNOWN ALGORITHMIC LIMITATIONS OF THIS ASSESSMENT TOOL:")
  print("------------------------------------------------------------")
  print("  1. Static Source Code Scanning & Include matching:")
  print("     This tool statically scans C++ includes. It cannot identify")
  print("     indirect coupling through Mojo IPC binders or runtime dependency")
  print("     injection (e.g. abstract factory registrars), which must be")
  print(
      "     manually gated (e.g., inside modules_initializer.cc or Mojo maps).")
  print("  2. Web IDL & Code Generation Systems Omission:")
  print("     Web IDL files (.idl) and bindings configurations (.gni) are")
  print("     omitted. If the feature exports JS Web APIs, V8 generated")
  print(
      "     bindings must be manually excluded in bindings/idl_in_modules.gni.")
  print("  3. Unit Test and Mocking Targets Omission:")
  print("     The tool only traces reachability from production root targets.")
  print("     Test files (*_unittest.cc) and mocks are ignored, meaning test")
  print("     suite targets (e.g., blink_unittests) must be manually pruned.")
  print("  4. Transitive GN Graph Dependencies Omission:")
  print("     If a parent target depends on this feature's target for config")
  print("     propagation or headers without direct C++ #include lines, the")
  print("     script will not flag it. Run 'gn refs' to double check.")
  print("  5. Header-Only Files Omission:")
  print("     Header-only C++ classes (.h without a compiling .cc file) might")
  print(
      "     be missing from SuperSize records, creating slight indexing gaps.")
  print("------------------------------------------------------------")


def export_automation_report_json(target_assessments, feature_flag_names,
                                  related_paths, estimated_size_bytes,
                                  json_output_path):
  """Exports automation JSON report for downstream scripts."""
  report_data = {
      "estimated_size_bytes": estimated_size_bytes,
      "feature_flags": feature_flag_names,
      "related_paths": related_paths,
      "targets": target_assessments
  }
  try:
    with open(json_output_path, "w", encoding="utf-8") as jf:
      json.dump(report_data, jf, indent=2)
    print("\n[Log] Structured automation JSON report written to: "
          f"{json_output_path}")
  except Exception as e:  # pylint: disable=broad-exception-caught
    print(f"\n[Error] Failed to write JSON report: {e}")


# ============================================================================
# Group 5: Orchestration & Execution
# ============================================================================


# pylint: disable=too-many-positional-arguments
def analyze_feature(parsed_args):
  """Main orchestrator that sequences all C++ build-graph audit steps."""
  search_modes = [
      parsed_args.symbol, parsed_args.path, parsed_args.namespace,
      parsed_args.component
  ]
  if sum(1 for m in search_modes if m) != 1:
    print("[Error] Exactly one search mode (--symbol, --path, "
          "--namespace, or --component) must be provided.")
    return

  if not (parsed_args.size_file and parsed_args.build_dir and
          parsed_args.root_target):
    print("[Error] Missing required arguments to analyze the feature.")
    return

  def run_command(cmd, **kwargs):
    if parsed_args.verbose:
      print("[Verbose] Running command: " + " ".join(cmd))
    try:
      return subprocess.run(
          cmd, capture_output=True, text=True, check=False, **kwargs)
    except Exception as e:  # pylint: disable=broad-exception-caught

      class DummyResult:

        def __init__(self, err_msg):
          self.returncode = -1
          self.stdout = ""
          self.stderr = err_msg

      return DummyResult(str(e))

  # Step 0: Check for Existing GN Feature Flags
  print("\n--- Step 0: Check for Existing GN Feature Flags ---")
  feature_keyword = deduce_feature_keyword(parsed_args)
  feature_flags = analyze_feature_flags(feature_keyword, parsed_args.build_dir,
                                        run_command)
  feature_flag_names = [f[0] for f in feature_flags]
  has_feature_flag = len(feature_flags) > 0

  # Step 1: Size Calculation
  print("\n--- Step 1: Size Calculation ---")
  size_info = load_size_file(parsed_args.size_file, parsed_args.verbose)
  if not size_info:
    return

  syms = query_feature_symbols(size_info, parsed_args)
  if not syms:
    print("[Result] Do not find matched symbols, exit the script.")
    return

  estimated_size = calculate_feature_size(syms)

  # Step 2: GN Feature Targets Discovery & Reachability Filtering
  print(
      "\n--- Step 2: GN Feature Targets Discovery & Reachability Filtering ---")
  feature_files = extract_feature_files(syms, parsed_args)
  feature_targets = discover_feature_targets(feature_files,
                                             parsed_args.build_dir,
                                             parsed_args.root_target,
                                             run_command)
  if not feature_targets:
    print_unreachable_feature_assessment(parsed_args.root_target)
    return

  # Prepare Orchestrator Context
  target_assessments = []
  feature_files_set = set(feature_files)

  # target-by-target C++ Audits & Gating analysis
  for feature_target in feature_targets:
    print("\n============================================================")
    print(f"Analyzing Feature Target: {feature_target}")
    print("============================================================")

    # Step 3: GN Target Classification (DEDICATED, ASSEMBLYvs SHARED)
    print("\n--- Step 3: GN Target Classification ---")
    target_type, matched_files, all_sources = classify_target(
        feature_target, feature_files_set, feature_keyword,
        parsed_args.build_dir, run_command)

    if target_type in ("DEDICATED", "ASSEMBLY"):
      target_assessments.append({
          "target": feature_target,
          "type": target_type,
          "difficulty": "LOW",
          "action":
              f"Low complexity (0 pts): {target_type.capitalize()} target; "
              "gated cleanly at dependency/graph level in BUILD.gn.",
          "matched_files": [],
          "cpp_integration_audit": [],
      })
      continue

    # If we get here, it is a SHARED target! Print headers and run deep
    # analysis.
    # Step 4: Deep C++ Code Auditing (Surgically scan includes)
    print("\n--- Step 4: Deep C++ Code Auditing "
          "(Scan direct C++ #include dependencies) ---")
    feature_headers = [f for f in matched_files if f.endswith(".h")]
    non_feature_files = [f for f in all_sources if f not in matched_files]
    cpp_integration_audit = audit_cpp_integration_points(
        non_feature_files,
        feature_headers,
        parsed_args.verbose,
    )

    # Step 5: Target Gating Difficulty Assessment (Score removal difficulty
    # based on all evaluated metrics)
    print("\n--- Step 5: Target Gating Difficulty Assessment ---")
    difficulty, action = score_target_difficulty(
        len(matched_files),
        len(cpp_integration_audit),
    )

    target_assessments.append({
        "target": feature_target,
        "type": "SHARED",
        "difficulty": difficulty,
        "action": action,
        "matched_files": matched_files,
        "cpp_integration_audit": cpp_integration_audit,
    })

  current_query_path = parsed_args.path if parsed_args.path else ""
  related_paths = suggest_related_feature_paths(size_info, feature_keyword,
                                                current_query_path)

  overall_difficulty = calculate_overall_difficulty(target_assessments)
  print_final_assessment(target_assessments, overall_difficulty,
                         has_feature_flag, related_paths)

  if parsed_args.json:
    export_automation_report_json(target_assessments, feature_flag_names,
                                  related_paths, estimated_size,
                                  parsed_args.json)


if __name__ == "__main__":
  parser = argparse.ArgumentParser(
      description="Analyze difficulty of disabling a feature based on a "
      "symbol, path, namespace, or component.")
  parser.add_argument("--size_file", help="Path to .size file")
  parser.add_argument(
      "--build_dir",
      default="out/android-arm_gold",
      help="Path to build directory")
  parser.add_argument(
      "--root_target",
      default="//cobalt:gn_all",
      help="Root target to check paths from")
  parser.add_argument(
      "--verbose", action="store_true", help="Print verbose logs")
  parser.add_argument(
      "--json", help="Path to write structured JSON automation report")

  group = parser.add_mutually_exclusive_group(required=True)
  group.add_argument("--symbol", help="Single symbol name to analyze")
  group.add_argument(
      "--path", help="Source directory path (e.g., third_party/dawn)")
  group.add_argument(
      "--namespace", help="C++ namespace (e.g., v8::internal::maglev)")
  group.add_argument(
      "--component", help="SuperSize component name (e.g., Blink>Bluetooth)")

  args = parser.parse_args()
  analyze_feature(args)
