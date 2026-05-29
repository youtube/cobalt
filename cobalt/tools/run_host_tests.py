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
"""Runs on-host tests with retry logic, handling PR and Push events."""

import argparse
import json
import logging
import os
import pathlib
import subprocess
import sys
from typing import Dict, List, Optional, Set, Tuple

import junit_mini_parser

# Add .github/scripts to path to import generate_crash_report
script_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.append(
    os.path.abspath(os.path.join(script_dir, "../../.github/scripts")))
import generate_crash_report

XVFB_ARGS = "-screen 0 1920x1080x24i +render +extension GLX -noreset"


def run_test_binary(entrypoint: str,
                    shard_index: int,
                    total_shards: int,
                    xml_output: str,
                    log_output: str,
                    gtest_filter: Optional[str] = None,
                    xvfb_run_path: str = "xvfb-run") -> int:
  """Runs a test entrypoint with gtest flags, redirecting output to a log file."""
  cmd = [
      xvfb_run_path, "-a", f"--server-args={XVFB_ARGS}", "stdbuf", "-i0", "-o0",
      "-e0", entrypoint, "--single-process-tests",
      f"--gtest_shard_index={shard_index}",
      f"--gtest_total_shards={total_shards}", f"--gtest_output=xml:{xml_output}"
  ]
  if gtest_filter:
    cmd.append(f"--gtest_filter={gtest_filter}")

  logging.info(f"Running: {' '.join(cmd)}")
  try:
    with open(log_output, 'w', encoding='utf-8') as log_file:
      return_code = subprocess.run(
          cmd, stdout=log_file, stderr=subprocess.STDOUT,
          check=False).returncode
    return return_code
  except Exception as error:
    logging.error(f"Failed to execute {entrypoint}: {error}")
    return 1


def get_initial_filter(binary_name: str, filter_json_dir: Optional[str]) -> str:
  """Loads initial filter from JSON file if available."""
  if not filter_json_dir:
    return "*"
  filter_file = os.path.join(filter_json_dir, f"{binary_name}_filter.json")
  if os.path.exists(filter_file):
    try:
      with open(filter_file, 'r', encoding='utf-8') as file:
        data = json.load(file)
        failing_tests = data.get('failing_tests', [])
        if failing_tests:
          return "-" + ":".join(failing_tests)
    except Exception as error:
      logging.warning(f"Failed to parse filter file {filter_file}: {error}")
  return "*"


def handle_run(
    binaries: List[str], attempt: int, args: argparse.Namespace,
    failed_tests_map: Dict[str, List[str]],
    initial_filters: Dict[str, str]) -> Tuple[Set[str], Dict[str, str]]:
  """Handles a single run (attempt) of tests."""
  failed_binaries = set()
  xml_files_created = {}

  for binary_path in binaries:
    binary_name = os.path.basename(binary_path)

    # Calculate filter
    if attempt == 1 or args.event_name == "push":
      gtest_filter = initial_filters.get(binary_name, "*")
    else:  # attempt > 1 and pull_request
      failed_tests = failed_tests_map.get(binary_name, [])
      if not failed_tests:
        logging.info(
            f"No specific failed tests found for {binary_name}, retrying all.")
        gtest_filter = "*"
      else:
        gtest_filter = ":".join(failed_tests)

    xml_path = os.path.join(args.results_dir,
                            f"{binary_name}_result_attempt_{attempt}.xml")
    log_path = os.path.join(args.results_dir,
                            f"{binary_name}_log_attempt_{attempt}.txt")
    xml_files_created[binary_name] = xml_path

    # Check for wrapper script (Functional Regression Fix)
    binary_dir = os.path.dirname(binary_path)
    wrapper_path = os.path.join(binary_dir, "bin", f"run_{binary_name}")
    entrypoint = binary_path
    if os.path.exists(wrapper_path) and os.access(wrapper_path, os.X_OK):
      entrypoint = wrapper_path
      logging.info(f"Using wrapper script: {entrypoint}")

    return_code = run_test_binary(entrypoint, args.shard, args.num_gtest_shards,
                                  xml_path, log_path, gtest_filter,
                                  args.xvfb_run_path)

    if return_code != 0:
      logging.warning(f"{binary_name} failed with exit code {return_code}")
      failed_binaries.add(binary_path)

      # Generate crash report if XML is missing (likely crash)
      if not os.path.exists(xml_path):
        logging.info(
            f"XML output missing for {binary_name}. Attempting to generate crash report."
        )
        crash_info = generate_crash_report.extract_crash(pathlib.Path(log_path))
        if crash_info:
          generate_crash_report.write_junit_xml(
              pathlib.Path(xml_path), *crash_info)

  return failed_binaries, xml_files_created


def main() -> int:
  logging.basicConfig(level=logging.INFO, format='%(levelname)s: %(message)s')

  parser = argparse.ArgumentParser(description="Run host tests with retries.")
  parser.add_argument(
      "--test_targets_json", required=True, help="Path to test_targets.json")
  parser.add_argument("--shard", type=int, required=True, help="Shard index")
  parser.add_argument(
      "--num_gtest_shards", type=int, required=True, help="Total shards")
  parser.add_argument(
      "--results_dir", required=True, help="Directory to save results")
  parser.add_argument(
      "--event_name",
      required=True,
      help="GitHub event name (pull_request or push)")
  parser.add_argument(
      "--max_attempts", type=int, default=3, help="Maximum number of attempts")
  parser.add_argument(
      "--xvfb_run_path", default="xvfb-run", help="Path to xvfb-run")
  parser.add_argument(
      "--filter_json_dir", help="Directory containing filter JSON files")

  args = parser.parse_args()

  if not os.path.exists(args.test_targets_json):
    logging.error(f"File not found: {args.test_targets_json}")
    return 1

  with open(args.test_targets_json, 'r', encoding='utf-8') as file:
    targets = json.load(file)

  test_binaries = [
      target['executable'] for target in targets if target.get('executable')
  ]

  os.makedirs(args.results_dir, exist_ok=True)

  failed_binaries = set(test_binaries)
  failed_tests_map = {}
  any_run_failed = False

  # Cache initial filters to avoid redundant I/O (Dead Code Fix)
  initial_filters = {}
  for binary_path in test_binaries:
    binary_name = os.path.basename(binary_path)
    initial_filters[binary_name] = get_initial_filter(binary_name,
                                                      args.filter_json_dir)

  for attempt in range(1, args.max_attempts + 1):
    logging.info(f"Starting Attempt {attempt}")

    if attempt == 1 or args.event_name == "push":
      binaries_to_run = test_binaries
    else:  # attempt > 1 and pull_request
      if not failed_binaries:
        logging.info("All tests passed, skipping further attempts.")
        break
      binaries_to_run = list(failed_binaries)

    current_failed_binaries, xml_files_created = handle_run(
        binaries_to_run, attempt, args, failed_tests_map, initial_filters)

    if current_failed_binaries:
      any_run_failed = True

    # Update failed tests map for next attempt (only relevant for PR)
    if args.event_name == "pull_request":
      xml_files = list(xml_files_created.values())
      # Batch process XML files
      fails = junit_mini_parser.find_failing_tests(xml_files)
      failed_tests_map = {}
      for path, test_list in fails.items():
        filename = os.path.basename(path)
        binary_name = filename.split('_result')[0]
        # Index 0 is the test name
        failed_tests_map[binary_name] = [
            test_case[0] for test_case in test_list
        ]

      # Update failed_binaries to include those that failed to run OR have failed tests
      failed_binaries = set()
      for binary in current_failed_binaries:
        binary_name = os.path.basename(binary)
        if binary_name in failed_tests_map and failed_tests_map[binary_name]:
          failed_binaries.add(binary)
        else:
          # If it failed but we found no specific tests (maybe crashed),
          # keep it in failed_binaries to retry the whole binary.
          logging.info(
              f"{binary_name} failed but no specific tests found. Retrying all."
          )
          failed_binaries.add(binary)

  if args.event_name == "pull_request":
    if failed_binaries:
      logging.error(f"Tests failed after {args.max_attempts} attempts.")
      return 1
    else:
      logging.info("Tests passed (possibly after retries).")
      return 0
  elif args.event_name == "push":
    if any_run_failed:
      logging.error("Some test runs failed on Push event.")
      return 1
    return 0

  return 0


if __name__ == "__main__":
  sys.exit(main())
