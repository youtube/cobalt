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
"""Runs on-device tests with retry logic."""

import argparse
import json
import logging
import os
import pathlib
import subprocess
import sys
from typing import Dict, List

import junit_mini_parser
import on_device_tests_gateway_client

# Add .github/scripts to path to import generate_crash_report
script_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.append(
    os.path.abspath(os.path.join(script_dir, "../../.github/scripts")))
import generate_crash_report  # pylint: disable=wrong-import-position

DEFAULT_JOB_TIMEOUT_SEC = '2700'
DEFAULT_TEST_TIMEOUT_SEC = '1800'
DEFAULT_START_TIMEOUT_SEC = '900'


def download_results_from_gcs(gcs_path: str, local_dir: str) -> bool:
  """Downloads test results from GCS using gsutil."""
  logging.info("Downloading results from %s to %s", gcs_path, local_dir)
  cmd = ['gsutil', '-q', 'cp', '-r', gcs_path + '/', local_dir]
  try:
    result = subprocess.run(cmd, check=False)
    return result.returncode == 0
  except Exception as error:  # pylint: disable=broad-exception-caught
    logging.error("Failed to download results from GCS: %s", error)
    return False


def check_failures_in_xml(xml_files: List[str]) -> bool:
  """Checks if any of the XML files contain failures or errors."""
  for xml_file in xml_files:
    if not os.path.exists(xml_file):
      continue
    fails = junit_mini_parser.find_failing_tests([xml_file])
    if fails:
      return True
  return False


def get_failed_targets(test_output_dir: str, targets: List[str]) -> List[str]:
  """Identifies which targets failed by inspecting XML results."""
  failed_targets = []

  # List all XML files once outside the loop (Performance Optimization)
  all_xml_files = list(pathlib.Path(test_output_dir).rglob('*.xml'))

  for target_with_path in targets:
    # Typical target: //cobalt/dom:dom -> dom
    target = target_with_path.split(':')[-1]

    # Filter XML files for this target in memory
    target_xml_files = [
        xml_file for xml_file in all_xml_files
        if xml_file.name.startswith(target)
    ]

    if not target_xml_files:
      logging.warning("No XML results found for %s", target)

      # Try to find log file to generate crash report
      log_files = list(pathlib.Path(test_output_dir).rglob(f'{target}_log.txt'))
      if log_files:
        logging.info("Found log for %s, generating crash report.", target)
        log_path = log_files[0]
        xml_path = log_path.parent / f'{target}_testoutput.xml'

        crash_info = generate_crash_report.extract_crash(log_path)
        if crash_info:
          generate_crash_report.write_junit_xml(xml_path, *crash_info)
          target_xml_files = [xml_path]

      if not target_xml_files:
        failed_targets.append(target_with_path)
        continue

    if check_failures_in_xml([str(xml_file) for xml_file in target_xml_files]):
      failed_targets.append(target_with_path)

  return failed_targets


def main() -> int:
  """Main function to orchestrate on-device tests with retries."""
  logging.basicConfig(level=logging.INFO, format='%(levelname)s: %(message)s')

  parser = argparse.ArgumentParser(description='Run device tests with retries.')
  parser.add_argument(
      '--token', required=True, help='GitHub token for authentication')
  parser.add_argument('--test_type', required=True, help='Type of test to run')
  parser.add_argument('--device_family', required=True, help='Device family')
  parser.add_argument('--targets', required=True, help='JSON string of targets')
  parser.add_argument(
      '--filter_json_dir', required=True, help='Directory for filters')
  parser.add_argument('--label', action='append', help='Labels for the test')
  parser.add_argument('--dimensions', required=True, help='Dimensions JSON')
  parser.add_argument(
      '--gcs_archive_path', required=True, help='GCS archive path')
  parser.add_argument(
      '--gcs_results_path', required=True, help='GCS results path')
  parser.add_argument('--event_name', required=True, help='GitHub event name')
  parser.add_argument(
      '--max_attempts', type=int, default=3, help='Max attempts')

  args = parser.parse_args()

  try:
    targets_list = json.loads(args.targets)
  except json.JSONDecodeError as error:
    logging.error("Failed to parse targets JSON: %s", error)
    return 1

  local_results_dir = 'results'
  os.makedirs(local_results_dir, exist_ok=True)

  failed_targets = targets_list
  any_run_failed = False

  for attempt in range(1, args.max_attempts + 1):
    logging.info("Starting Attempt %d", attempt)

    if attempt > 1:
      if args.event_name == 'pull_request' and not failed_targets:
        logging.info('All tests passed, skipping further attempts.')
        break

    current_targets = []
    if attempt == 1 or args.event_name == 'push':
      current_targets = targets_list
    else:  # attempt > 1 and pull_request
      current_targets = failed_targets

    # Prepare args for gateway client (using public method now)
    client_args = argparse.Namespace(
        token=args.token,
        test_type=args.test_type,
        device_family=args.device_family,
        targets=json.dumps(current_targets),
        filter_json_dir=args.filter_json_dir,
        label=args.label,
        test_attempts='0',  # We handle retries here
        retry_level=None,
        dimensions=args.dimensions,
        job_timeout_sec=DEFAULT_JOB_TIMEOUT_SEC,
        test_timeout_sec=DEFAULT_TEST_TIMEOUT_SEC,
        start_timeout_sec=DEFAULT_START_TIMEOUT_SEC,
        gcs_archive_path=args.gcs_archive_path,
        gcs_result_path=f'{args.gcs_results_path}/attempt_{attempt}',
        cobalt_path=None,
        artifact_name=None,
        action='trigger')

    logging.info("Triggering tests for targets: %s", current_targets)
    # Use public method
    test_requests = on_device_tests_gateway_client.process_test_requests(
        client_args)
    client = on_device_tests_gateway_client.OnDeviceTestsGatewayClient()

    try:
      client.run_trigger_command(client_args.token, client_args.label,
                                 test_requests)
    except Exception as error:  # pylint: disable=broad-exception-caught
      logging.error("Failed to trigger tests: %s", error)
      any_run_failed = True
      continue

    # Download results
    attempt_gcs_path = f'{args.gcs_results_path}/attempt_{attempt}'
    attempt_local_dir = os.path.join(local_results_dir, f'attempt_{attempt}')
    os.makedirs(attempt_local_dir, exist_ok=True)

    if download_results_from_gcs(attempt_gcs_path, attempt_local_dir):
      # Check for failures
      current_failed = get_failed_targets(attempt_local_dir, current_targets)
      if current_failed:
        any_run_failed = True
        failed_targets = current_failed
        logging.warning("Attempt %d failed for: %s", attempt, failed_targets)
      else:
        failed_targets = []
        logging.info("Attempt %d passed.", attempt)
    else:
      logging.error("Failed to download results for attempt %d", attempt)
      any_run_failed = True
      failed_targets = current_targets  # Assume failed if we can't get results

  if args.event_name == 'pull_request':
    if failed_targets:
      logging.error("Tests failed after %d attempts.", args.max_attempts)
      return 1
    else:
      logging.info('Tests passed (possibly after retries).')
      return 0
  elif args.event_name == 'push':
    if any_run_failed:
      logging.error('Some test runs failed on Push event.')
      return 1
    return 0

  return 0


if __name__ == '__main__':
  sys.exit(main())
