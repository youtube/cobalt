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

_GENERIC_NAMES = {
    "shared", "common", "base", "core", "all", "test", "tests", "unittests",
    "support", "helpers", "impl", "util", "utils", "proto", "static", "browser",
    "network", "content", "service", "renderer", "gpu", "media", "utility",
    "features", "switches", "ui", "cpp", "constants", "mojom", "public",
    "platform", "v8"
}

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
  print("\n[Log] Querying SuperSize symbols for your feature...")
  if parsed_args.symbol:
    return size_info.symbols.WhereNameMatches(parsed_args.symbol)
  elif parsed_args.path:
    return size_info.symbols.WherePathMatches(parsed_args.path)
  elif parsed_args.namespace:
    return size_info.symbols.WhereNameMatches(parsed_args.namespace)
  elif parsed_args.component:
    return size_info.symbols.WhereComponentMatches(parsed_args.component)
  return None


def extract_feature_files(syms):
  """Extracts unique source/object files compiled by matched symbols."""
  feature_files = set()
  if syms:
    for s in syms:
      if s.source_path:
        feature_files.add(s.source_path)
      elif s.object_path:
        feature_files.add(s.object_path)
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


def calculate_feature_size(syms, parsed_args):
  """Calculates and prints the total size (PSS) of matched symbols."""
  if not syms:
    print("[Result] Total PSS of matched symbols: 0 bytes")
    return 0

  total_pss = sum(s.pss for s in syms)
  print(f"[Result] Total PSS of matched symbols: {total_pss} bytes")

  if parsed_args.symbol:
    sorted_syms = syms.Sorted(key=lambda s: s.pss)
    if sorted_syms:
      print(f"[Result] Size of largest symbol: {sorted_syms[-1].pss} bytes")

  return total_pss


# ============================================================================
# Group 2: GN Build Graph Helpers
# ============================================================================


def discover_gn_targets(syms, parsed_args, gn_build_directory,
                        command_runner_function):
  """Maps matched symbols or paths to their corresponding GN build targets."""
  gn_targets = set()
  if not syms and not parsed_args.path:
    return gn_targets

  def add_targets_from_output(stdout):
    for line in stdout.split("\n"):
      if line.strip():
        gn_targets.add(line.strip().split("(")[0])

  if parsed_args.symbol:
    print(f"[Log] Searching for symbol matching '{parsed_args.symbol}'...")
    syms = syms.Sorted(key=lambda s: s.pss)
    if not syms:
      print(f"[Result] No symbols found matching '{parsed_args.symbol}'")
      return gn_targets
    symbol_obj = syms[-1]
    source_path = symbol_obj.source_path or symbol_obj.object_path
    if not source_path:
      print("[Error] No source or object path found for this symbol.")
      return gn_targets
    print(f"[Result] Largest symbol: {symbol_obj.full_name}")
    print(f"[Result] Source file: {source_path}")

    if not os.path.exists(source_path):
      print(f"[Error] Source path does not exist: {source_path}")
      return gn_targets

    res = command_runner_function(
        ["gn", "refs", gn_build_directory, source_path])
    if res.returncode == 0:
      add_targets_from_output(res.stdout)
    else:
      print(f"[Error] 'gn refs' failed for file: {res.stderr}")

  elif parsed_args.path:
    print(f"[Log] Finding all targets under path '{parsed_args.path}'...")
    res = command_runner_function(
        ["gn", "ls", gn_build_directory, f"//{parsed_args.path}/*"])
    if res.returncode == 0:
      add_targets_from_output(res.stdout)
    else:
      print(f"[Error] 'gn ls' failed: {res.stderr}")

  elif parsed_args.namespace or parsed_args.component:
    print("[Log] Batch querying GN targets for all feature source files...")
    source_files = set()
    for s in syms:
      if s.source_path:
        source_files.add(s.source_path)
      elif s.object_path:
        source_files.add(s.object_path)

    file_list = list(source_files)
    chunk_size = 100
    for i in range(0, len(file_list), chunk_size):
      chunk = file_list[i:i + chunk_size]
      res = command_runner_function(["gn", "refs", gn_build_directory] + chunk)
      if res.returncode == 0:
        add_targets_from_output(res.stdout)
      else:
        print(f"[Warning] 'gn refs' failed for a chunk of files: {res.stderr}")

  print(f"[Result] Found {len(gn_targets)} unique targets.")
  return gn_targets


def filter_and_cache_paths(gn_targets, gn_build_directory, root_target,
                           command_runner_function):
  """Filters reachable targets and caches their dependency paths."""
  print(f"[Log] Verifying reachability from {root_target} and caching paths...")

  # Step A: Run a single bulk query to get all targets compiled in Cobalt
  print("[Log] Fetching all linked targets via bulk 'gn desc deps'...")
  res = command_runner_function(
      ["gn", "desc", gn_build_directory, root_target, "deps", "--all"])
  if res.returncode != 0:
    print(f"[Warning] Bulk reachability check failed: {res.stderr}")
    return [], {}

  linked_targets = set(line.strip().split("(")[0]
                       for line in res.stdout.split("\n")
                       if line.strip())

  # Step B: Intersect to keep only reachable targets (takes milliseconds!)
  reachable_targets = [t for t in gn_targets if t in linked_targets]
  print(f"[Result] {len(reachable_targets)} out of {len(gn_targets)} targets "
        f"are reachable from {root_target}.")

  # Step C: Only run slow 'gn path' on reachable targets to cache their paths
  feature_targets = []
  cached_paths = {}
  for t in reachable_targets:
    res = command_runner_function(
        ["gn", "path", gn_build_directory, root_target, t, "--all"])
    output = res.stdout.strip()
    if res.returncode == 0 and output and "No path found" not in output:
      feature_targets.append(t)
      cached_paths[t] = output
  return feature_targets, cached_paths


def analyze_target_flags(feature_target, feature_keyword, gn_args_output):
  """Queries GN args to find if the target has feature gates (flags)."""
  print(
      f"[Log] Querying GN args to find flags related to '{feature_target}'...")

  keyword = feature_target.split(":")[-1].lower()
  t_kw = keyword if keyword not in _GENERIC_NAMES else None

  keywords = [feature_keyword]
  if t_kw:
    keywords.append(t_kw)

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
      for kw in keywords:
        if kw in current_flag.lower():
          matched = True
          break
    elif current_flag and matched:
      if "=" in line:
        if "Default value" in line:
          if current_value is None:
            current_value = line.split("=")[-1].strip()
        elif "Current value" in line:
          current_value = line.split("=")[-1].strip()

  if current_flag and matched:
    matching_flags[current_flag] = current_value

  flags_from_symbol = []
  flags_from_target = []

  for flag_name, value in matching_flags.items():
    lower_flag = flag_name.lower()
    if feature_keyword in lower_flag:
      flags_from_symbol.append((flag_name, value))
    elif t_kw and t_kw in lower_flag:
      flags_from_target.append((flag_name, value))

  # Log found flags in this step
  total_flags = len(flags_from_symbol) + len(flags_from_target)
  print(f"[Result] Found {total_flags} potential flags.")
  if flags_from_symbol:
    print(f"  Flags matching symbol/feature keyword '{feature_keyword}':")
    for flag, val in flags_from_symbol:
      print(f"    - {flag} = {val}")
  if flags_from_target:
    print(f"  Flags matching target keyword '{t_kw}':")
    for flag, val in flags_from_target:
      print(f"    - {flag} = {val}")

  return flags_from_symbol, flags_from_target


def analyze_target_dependents(feature_target, cached_output,
                              enable_verbose_logging):
  """Calculates the number of unique direct dependents on paths to Cobalt."""
  print(f"[Log] Using cached 'gn path' results for {feature_target}...")

  output = cached_output
  paths_count = 0
  unique_preceding_targets = set()

  if output and "No path found" not in output:
    paths = [p for p in output.split("\n\n") if p.strip()]
    paths_count = len(paths)

    preceding_targets = []
    for p in paths:
      lines = [line.strip() for line in p.split("\n") if line.strip()]
      if len(lines) >= 2:
        preceding_targets.append(lines[-2])

    unique_preceding_targets = set(preceding_targets)

  print(f"[Result] Target dependency paths count: {paths_count}")
  print("[Result] Unique Direct Dependents count: "
        f"{len(unique_preceding_targets)}")

  if enable_verbose_logging and len(unique_preceding_targets) > 0:
    print("[Verbose] Unique Direct Dependents on Paths:")
    for t in unique_preceding_targets:
      print(f"  - {t}")

  return list(unique_preceding_targets)


# pylint: disable=too-many-positional-arguments
def classify_target(feature_target, feature_files, feature_keyword,
                    cli_arguments, gn_build_directory, command_runner_function):
  """Classifies a target as DEDICATED or SHARED based on source file ratio."""
  res = command_runner_function(
      ["gn", "desc", gn_build_directory, feature_target, "sources"])
  if res.returncode != 0:
    print(f"[Warning] 'gn desc' failed for {feature_target}: {res.stderr}")
    return "UNKNOWN", [], []

  all_sources = set(
      line.strip() for line in res.stdout.split("\n") if line.strip())

  # Name-matching override
  target_name = feature_target.split(":")[-1].lower()
  if feature_keyword in target_name:
    print(f"[Log] Target name '{target_name}' matches feature keyword "
          f"'{feature_keyword}'. Classified as DEDICATED.")
    return "DEDICATED", [], [f.lstrip("/") for f in all_sources]

  matched_files = []
  if cli_arguments.path:
    sanitized_path = cli_arguments.path.strip("/")
    prefix = f"//{sanitized_path}"
    # Strip leading slashes for consistency
    matched_files = [f.lstrip("/") for f in all_sources if f.startswith(prefix)]
  else:
    # Strip leading slashes when comparing with SuperSize paths
    matched_sources = [f for f in all_sources if f.lstrip("/") in feature_files]
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
    matched_files = [f.lstrip("/") for f in matched_set]

  if not all_sources:
    return "UNKNOWN", [], []

  ratio = len(matched_files) / len(all_sources)
  print(f"[Log] Target contains {len(matched_files)} feature files out of "
        f"{len(all_sources)} total files.")

  if ratio >= 0.7:
    print("[Result] Target classified as: DEDICATED")
    return "DEDICATED", [], [f.lstrip("/") for f in all_sources]
  print("[Result] Target classified as: SHARED")
  return "SHARED", matched_files, [f.lstrip("/") for f in all_sources]


# ============================================================================
# Group 3: C++ Dependency Auditing Helpers
# ============================================================================


def audit_files_utilization(files_to_scan, feature_headers,
                            enable_verbose_logging):
  """Scans C++ files to identify direct include references."""
  audit_reports = []
  header_basenames = {os.path.basename(h) for h in feature_headers}

  for f in files_to_scan:
    clean_path = f.lstrip("/")
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
      if line.strip().startswith("#include"):
        for hb in header_basenames:
          if hb in line:
            referenced.append(hb)

    if referenced:
      audit_reports.append({
          "file": clean_path,
          "referenced_headers": list(set(referenced)),
      })

  return audit_reports


# pylint: disable=too-many-positional-arguments
def perform_dependent_audit(feature_target_type, dependent_funnel_targets,
                            target_compiled_sources, matched_feature_files,
                            target_sources_cache, gn_build_directory,
                            command_runner_function, enable_verbose_logging):
  """Audits C++ include references across dependents."""
  # Formulate feature headers to audit based on target classification
  feature_headers = [
      f for f in matched_feature_files if f.endswith(".h") or f.endswith(".hpp")
  ]
  if feature_target_type == "DEDICATED":
    feature_headers = [
        f for f in target_compiled_sources
        if f.endswith(".h") or f.endswith(".hpp")
    ]

  # Perform External Dependents Audit
  external_files = []
  for dependent in dependent_funnel_targets:
    if dependent in target_sources_cache:
      external_files.extend(target_sources_cache[dependent])
    else:
      res = command_runner_function(
          ["gn", "desc", gn_build_directory, dependent, "sources"])
      if res.returncode == 0:
        dep_sources = [
            line.strip().lstrip("/")
            for line in res.stdout.split("\n")
            if line.strip()
        ]
        target_sources_cache[dependent] = dep_sources
        external_files.extend(dep_sources)
  external_audit = audit_files_utilization(external_files, feature_headers,
                                           enable_verbose_logging)

  # Perform Internal Dependents Audit (SHARED targets only)
  internal_audit = []
  if feature_target_type == "SHARED":
    internal_files = [
        f for f in target_compiled_sources if f not in matched_feature_files
    ]
    internal_audit = audit_files_utilization(internal_files, feature_headers,
                                             enable_verbose_logging)

  combined_audit = external_audit + internal_audit

  # Print Step 5 results immediately at the audit execution site
  if combined_audit:
    if len(combined_audit) < 10:
      print("[Result] Found "
            f"{len(combined_audit)} C++ include integration points:")
      for f in combined_audit:
        f_path = f["file"]
        print(f"  - {f_path}")
    else:
      print("[Result] Found "
            f"{len(combined_audit)} C++ include integration points "
            "(listing skipped as count is >= 10).")
  else:
    print("[Result] No C++ include integration points found.")

  return combined_audit


# ============================================================================
# Group 4: Difficulty Scoring, Output & Reporting Helpers
# ============================================================================


# pylint: disable=too-many-positional-arguments
def score_target_difficulty(feature_target_type, has_flag, dependents_count,
                            matched_files_count, referencing_files_count):
  """Calculates the overall target difficulty based on Proposal B (Points)."""
  # 1. Calculate complexity points
  include_points = referencing_files_count * 5
  dep_points = dependents_count * 2
  file_points = matched_files_count * 1
  points = include_points + dep_points + file_points

  print("[Result] Gating Complexity Points Breakdown:")
  print(f"  - C++ Include Gating: {referencing_files_count} files x 5 = "
        f"{include_points} pts")
  print(f"  - Build dependents Coupling: {dependents_count} targets x 2 = "
        f"{dep_points} pts")
  if feature_target_type == "SHARED":
    print(f"  - Shared Gating Files: {matched_files_count} files x 1 = "
          f"{file_points} pts")

  # 2. Apply Dedicated Target Discount
  # Dedicated targets can be cleanly swapped using stubs in BUILD.gn,
  # which is easier than shared file-by-file gating.
  if feature_target_type == "DEDICATED":
    print("  - Dedicated Target Discount: -5 pts")
    points = max(0, points - 5)

  print(f"  - Total Complexity: {points} pts")

  # 3. Map points to difficulty tiers
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

  if has_flag:
    action += " (Note: GN flag(s) found; must be verified for effectiveness.)"

  return difficulty, action


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


def print_final_assessment(target_assessments, feature_targets,
                           overall_difficulty):
  """Prints a consolidated difficulty assessment report to stdout."""
  print("\n============================================================")
  print("CONSOLIDATED FINAL DIFFICULTY ASSESSMENT")
  print("============================================================")

  print(f"Analyzed {len(feature_targets)} targets:")
  overall_has_flag = False
  # Accumulates parent directories of matched feature files to suggest
  # full-folder path removals at the end of the consolidated report.
  unique_dirs = set()

  for a in target_assessments:
    target = a["target"]
    has_flags = len(a.get("flags", [])) > 0
    deps = len(a.get("funnel_targets", []))
    t_type = a["type"]
    diff = a["difficulty"]
    action = a["action"]
    matched_files = a.get("matched_files", [])

    print(f"  - {target}: Type={t_type}, Flags={has_flags}, Funnel Deps={deps}")
    print(f"    Difficulty: {diff}")
    print(f"    Action: {action}")

    # Gather unique directories for the final guide
    for f in matched_files:
      parent_dir = os.path.dirname(f)
      if parent_dir:
        unique_dirs.add(parent_dir)

    if has_flags:
      overall_has_flag = True

  print(f"\nOverall Difficulty: {overall_difficulty}")
  print("Reason: Based on individual target classifications and "
        "difficulty scores.")

  if overall_has_flag:
    print(
        "\n[Note] Existing GN flag(s) were found. If these flags are effective")
    print("       kill switches, the actual difficulty might be lower "
          "than estimated.")

  if unique_dirs:
    print("\n[Directory Expansion Guide for Automation]")
    print("  Matched files live in these parent folders. To ensure complete")
    print("  eradication (including database, SQLite storage, or untracked ")
    print("  headers), consider expanding your automation to '--path' mode ")
    print("  on these paths:")
    for d in sorted(unique_dirs):
      print(f"    - {d}")


def export_automation_report_json(target_assessments, json_output_path):
  """Exports automation JSON report for downstream scripts."""
  report_data = {"targets": []}
  for a in target_assessments:
    target_dict = {
        "target": a["target"],
        "type": a["type"],
        "flags": a.get("flags", []),
        "funnel_targets": a.get("funnel_targets", []),
        "matched_files": a.get("matched_files", []),
        "dependents_audit": []
    }
    # Prune dependents_audit to keep only file and referenced_headers
    for da in a.get("dependents_audit", []):
      if len(da["referenced_headers"]) > 0:
        target_dict["dependents_audit"].append({
            "file": da["file"],
            "referenced_headers": da["referenced_headers"]
        })
    if a["type"] == "DEDICATED":
      target_dict["all_sources"] = a.get("all_sources", [])
    report_data["targets"].append(target_dict)
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
  size_info = load_size_file(parsed_args.size_file, parsed_args.verbose)
  if not size_info:
    return

  def run_command(cmd, **kwargs):
    if parsed_args.verbose:
      print("[Verbose] Running command: " + " ".join(cmd))
    return subprocess.run(
        cmd, capture_output=True, text=True, check=False, **kwargs)

  # Step 0: Query Symbols & Size Calculation
  syms = query_feature_symbols(size_info, parsed_args)
  print("\n--- Step 0: Size Calculation ---")
  calculate_feature_size(syms, parsed_args)

  feature_files = extract_feature_files(syms)

  # Step 1: GN Feature Targets Discovery & Reachability Filtering
  print("\n--- Step 1: GN Feature Targets Discovery ---")
  gn_targets = discover_gn_targets(syms, parsed_args, parsed_args.build_dir,
                                   run_command)
  if not gn_targets:
    return

  feature_targets, cached_paths = filter_and_cache_paths(
      gn_targets, parsed_args.build_dir, parsed_args.root_target, run_command)

  if not feature_targets:
    print_unreachable_feature_assessment(parsed_args.root_target)
    return

  # Prepare Orchestrator Context
  target_assessments = []
  # Cache mapping target labels to their compiled sources lists (GN desc
  # sources), preventing redundant slow 'gn desc' subprocess queries for
  # shared dependents.
  target_sources_cache = {}
  feature_keyword = deduce_feature_keyword(parsed_args)

  # Fetch active GN build arguments list once to prevent slow redundant
  # subprocess calls in loop
  print("\n[Log] Fetching active GN build arguments list...")
  gn_args_res = run_command(["gn", "args", parsed_args.build_dir, "--list"])
  gn_args_output = gn_args_res.stdout if gn_args_res.returncode == 0 else ""

  # target-by-target C++ Audits & Gating analysis
  for feature_target in feature_targets:
    print("\n============================================================")
    print(f"Analyzing Feature Target: {feature_target}")
    print("============================================================")

    # Step 2: Check for Existing GN Feature Flags
    print("\n--- Step 2: Check for Existing GN Feature Flags ---")
    flags_from_symbol, flags_from_target = analyze_target_flags(
        feature_target, feature_keyword, gn_args_output)
    all_found_flags = [f[0] for f in flags_from_symbol + flags_from_target]

    # Step 3: GN Target Classification (DEDICATED vs SHARED)
    print("\n--- Step 3: GN Target Classification ---")
    target_type, matched_files, all_sources = classify_target(
        feature_target, feature_files, feature_keyword, parsed_args,
        parsed_args.build_dir, run_command)

    # Step 4: Quantify Build-Level Coupling (Find preceding dependents)
    print("\n--- Step 4: Quantify Build-Level Coupling "
          "(Identify unique direct dependents) ---")
    funnel_targets = analyze_target_dependents(
        feature_target,
        cached_paths.get(feature_target),
        parsed_args.verbose,
    )

    # Step 5: Deep C++ Code Auditing (Surgically scan includes)
    print("\n--- Step 5: Deep C++ Code Auditing "
          "(Scan direct C++ #include dependencies) ---")
    combined_audit = perform_dependent_audit(
        target_type,
        funnel_targets,
        all_sources,
        matched_files,
        target_sources_cache,
        parsed_args.build_dir,
        run_command,
        parsed_args.verbose,
    )

    # Step 6: Target Gating Difficulty Assessment (Score removal difficulty
    # based on all evaluated metrics)
    print("\n--- Step 6: Target Gating Difficulty Assessment ---")
    total_ref_files = len(combined_audit)
    difficulty, action = score_target_difficulty(
        target_type,
        len(all_found_flags) > 0,
        len(funnel_targets),
        len(matched_files),
        total_ref_files,
    )

    target_assessments.append({
        "target": feature_target,
        "flags": all_found_flags,
        "funnel_targets": funnel_targets,
        "type": target_type,
        "difficulty": difficulty,
        "action": action,
        "matched_files": matched_files,
        "all_sources": all_sources,
        "dependents_audit": combined_audit,
    })

  overall_difficulty = calculate_overall_difficulty(target_assessments)
  print_final_assessment(target_assessments, feature_targets,
                         overall_difficulty)

  if parsed_args.json:
    export_automation_report_json(target_assessments, parsed_args.json)


if __name__ == "__main__":
  parser = argparse.ArgumentParser(
      description="Analyze difficulty of disabling a feature based on a "
      "symbol, path, namespace, or component.")
  parser.add_argument(
      "--size_file", default="cobalt27.size", help="Path to .size file")
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
