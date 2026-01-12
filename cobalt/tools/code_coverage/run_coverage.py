#!/usr/bin/env python3
# Copyright 2025 The Cobalt Authors. All Rights Reserved.
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
"""
This script orchestrates the code coverage process by:
1. Running gn.py with the --coverage flag to generate build arguments.
2. Running the code_coverage_tool.py to perform the coverage scan.
"""

import argparse
import concurrent.futures
import json
import multiprocessing
import os
import subprocess
import sys

# Absolute path to the root of the checkout.
SRC_ROOT_PATH = os.path.join(
    os.path.abspath(os.path.dirname(__file__)), os.path.pardir, os.path.pardir,
    os.path.pardir)
GN_PY_PATH = os.path.join(SRC_ROOT_PATH, 'cobalt', 'build', 'gn.py')
COVERAGE_TOOL_PATH = os.path.join(SRC_ROOT_PATH, 'cobalt', 'tools',
                                  'code_coverage',
                                  'cobalt_code_coverage_bridge.py')
TEST_TARGETS_DIR = os.path.join(SRC_ROOT_PATH, 'cobalt', 'build', 'testing',
                                'targets')


def discover_targets(platform):
  """
  Discover test targets for a given platform by scanning the build testing
  targets directory.
  """
  discovered_targets = []
  print(f'Scanning for targets in: {TEST_TARGETS_DIR}')
  target_file = os.path.join(TEST_TARGETS_DIR, platform, 'test_targets.json')
  print(f'Checking for target file: {target_file}')
  if os.path.exists(target_file):
    with open(target_file, 'r', encoding='utf-8') as f:
      data = json.load(f)
      if 'test_targets' in data:
        discovered_targets.extend(data['test_targets'])
  return discovered_targets


def parse_args():
  parser = argparse.ArgumentParser(
      description='Run the end-to-end code coverage process.')

  # Arguments for gn.py
  parser.add_argument(
      '-p',
      '--platform',
      required=True,
      help='The platform to build for (e.g., android-x86).')

  # Arguments for code_coverage_tool.py
  parser.add_argument(
      'targets',
      nargs='*',
      help='The test targets to run. If not specified, targets are '
      'auto-discovered.')
  parser.add_argument(
      '-o',
      '--output-dir',
      required=True,
      help='Output directory for generated artifacts.')
  parser.add_argument(
      '-f',
      '--filters',
      action='append',
      help='Directories or files to get code coverage for.')
  parser.add_argument(
      '-j',
      '--jobs',
      type=int,
      default=multiprocessing.cpu_count(),
      help='The number of parallel jobs to run.')

  return parser.parse_args()


def run_coverage_for_target(target, args, build_dir, discovery_platform):
  """
  Run code coverage for a single target.
  """
  print(f'--- Running coverage for target: {target} ---')
  sanitized_target = target.replace(':', '_').replace('/', '_')
  target_output_dir = os.path.join(args.output_dir, sanitized_target)
  os.makedirs(target_output_dir, exist_ok=True)

  executable_name = target.split(':')[-1]
  command = os.path.join(build_dir, executable_name)

  # Add the test filters to the command.
  filter_file = os.path.join(SRC_ROOT_PATH, 'cobalt', 'testing', 'filters',
                             discovery_platform,
                             f'{executable_name}_filter.json')
  if os.path.exists(filter_file):
    with open(filter_file, 'r', encoding='utf-8') as f:
      data = json.load(f)
      if 'failing_tests' in data and data['failing_tests']:
        gtest_filter = f'--gtest_filter=-{":".join(data["failing_tests"])}'  # pylint: disable=inconsistent-quotes
        command += f' {gtest_filter}'

  coverage_command = [
      sys.executable,
      COVERAGE_TOOL_PATH,
      '-b',
      build_dir,
      '-o',
      target_output_dir,
      '-c',
      command,
  ]
  if args.filters:
    for f in args.filters:
      coverage_command.extend(['-f', f])
  coverage_command.extend(['--format=lcov', executable_name])

  print(f'Running command: {" ".join(coverage_command)}')  # pylint: disable=inconsistent-quotes
  try:
    subprocess.check_call(coverage_command)
    return True
  except subprocess.CalledProcessError as e:
    print(f'Error running code_coverage_tool.py for target {target}: {e}')
    return False


def main():
  """
  Main function to run the end-to-end code coverage process.
  """
  args = parse_args()

  discovery_platform = args.platform
  if discovery_platform == 'android-x86':
    discovery_platform = 'android-arm'
  elif discovery_platform == 'android-x64':
    discovery_platform = 'android-arm64'

  targets = args.targets
  if not targets:
    print('No targets specified, auto-discovering targets...')
    targets = discover_targets(discovery_platform)
    if not targets:
      print(f'No test targets found for platform {args.platform}.')
      return 1
    print(f'Discovered targets: {", ".join(targets)}')  # pylint: disable=inconsistent-quotes

  # 1. Run gn.py with --coverage
  build_dir = os.path.join('out', f'{args.platform}_devel')
  gn_command = [sys.executable, GN_PY_PATH, '-p', args.platform, '--coverage']
  print(f"Running command: {' '.join(gn_command)}")
  try:
    subprocess.check_call(gn_command)
  except subprocess.CalledProcessError as e:
    print(f'Error running gn.py: {e}')
    return 1

  # 2. Run code_coverage_tool.py for each target in parallel
  print(f'Running coverage for {len(targets)} targets with {args.jobs} jobs...')
  with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as executor:
    futures = [
        executor.submit(run_coverage_for_target, target, args, build_dir,
                        discovery_platform) for target in targets
    ]
    results = [future.result() for future in futures]

  if all(results):
    print('Code coverage process completed successfully.')
  else:
    print('Code coverage process completed with some errors.')

  return 0


if __name__ == '__main__':
  sys.exit(main())
