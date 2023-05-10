#!/usr/bin/python
#
# Copyright 2017 The Cobalt Authors. All Rights Reserved.
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
#
"""Cross-platform unit test runner."""

import argparse
import logging
import os
import subprocess
import sys

from starboard.build import clang
from starboard.tools import build
from starboard.tools.util import SetupDefaultLoggingConfig


def get_raw_reports(targets, coverage_dir):
  raw_reports = []
  for target in sorted(targets):
    profraw_file = os.path.join(coverage_dir, target + '.profraw')
    if os.path.isfile(profraw_file):
      raw_reports.append(profraw_file)
  return raw_reports


def generate_report(binaries_dir, targets, coverage_dir):
  """Generate the source code coverage report."""

  # Collect the raw coverage reports.
  raw_reports = get_raw_reports(targets, coverage_dir)
  if not raw_reports:
    logging.warning('Found no coverage reports')
    return

  target_paths = [os.path.join(binaries_dir, target) for target in targets]

  toolchain_dir = build.GetClangBinPath(clang.GetClangSpecification())
  llvm_profdata = os.path.join(toolchain_dir, 'llvm-profdata')
  llvm_cov = os.path.join(toolchain_dir, 'llvm-cov')

  profdata_name = os.path.join(coverage_dir, 'report.profdata')
  merge_cmd_list = [
      llvm_profdata, 'merge', '-sparse=true', '-o', profdata_name, *raw_reports
  ]
  logging.info('Merging coverage files')
  subprocess.check_call(merge_cmd_list, text=True)

  # TODO(oxv): Don't use os.getcwd(). This currently works because
  # test_runner.py is invoked from the root of the source dir.
  all_dirs = next(os.walk(os.getcwd()))[1]
  include_dirs = ['cobalt', 'starboard']
  exclude_dirs = [d for d in all_dirs if d not in include_dirs]
  exclude_opts = ['--ignore-filename-regex=*_test.cc'] + \
    [f'--ignore-filename-regex={d}.*' for d in exclude_dirs]

  test_binary_opts = [target_paths[0]] + \
    ['-object=' + target for target in target_paths[1:]]

  show_cmd_list = [
      llvm_cov, 'show', *exclude_opts, '-instr-profile=' + profdata_name,
      '-format=html', '-output-dir=' + os.path.join(coverage_dir, 'html'),
      *test_binary_opts
  ]
  logging.info('Creating html coverage report')
  subprocess.check_call(show_cmd_list, text=True)

  report_cmd_list = [
      llvm_cov, 'report', *exclude_opts, '-instr-profile=' + profdata_name,
      *test_binary_opts
  ]
  logging.info('Creating text coverage report')
  with open(os.path.join(coverage_dir, 'report.txt'), 'wb') as out:
    subprocess.check_call(report_cmd_list, stdout=out, text=True)


if __name__ == '__main__':
  SetupDefaultLoggingConfig()

  arg_parser = argparse.ArgumentParser()
  arg_parser.add_argument(
      '--binaries_dir',
      required=True,
      help='Directory where the test binaries are located.')
  arg_parser.add_argument(
      '--targets',
      nargs='+',
      required=True,
      help='List of test binaries that should be used for the coverage report.')
  arg_parser.add_argument(
      '--coverage_dir',
      required=True,
      help='Directory where coverage files are located and where the report '
      'will be generated.')
  args = arg_parser.parse_args()

  sys.exit(generate_report(args.binaries_dir, args.targets, args.coverage_dir))
