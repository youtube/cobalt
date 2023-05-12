#!/usr/bin/python
#
# Copyright 2023 The Cobalt Authors. All Rights Reserved.
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
"""Generate coverage reports from llvm coverage data."""

import argparse
import glob
import logging
import os
import pathlib
import subprocess
import sys

from starboard.build import clang
from starboard.tools import build
from starboard.tools.util import SetupDefaultLoggingConfig


def _get_raw_reports(test_binaries, coverage_dir):
  """Get a list of raw coverage reports from the coverage directory."""
  raw_reports = []
  for target in test_binaries:
    # Find coverage reports from the supplied test binaries.
    profraw_file = os.path.join(coverage_dir, target + '.profraw')
    if not os.path.isfile(profraw_file):
      raise RuntimeError(f'Missing coverage report file: \'{profraw_file}\'')
    raw_reports.append(profraw_file)
  else:
    # If no test binaries were supplied we use all coverage data we find.
    raw_reports = glob.glob(os.path.join(coverage_dir, '*.profraw'))
  return raw_reports


def _get_test_binary_paths(binaries_dir, test_binaries, coverage_dir):
  if not test_binaries:
    # Use the coverage files to deduce the paths if no binaries were specified.
    raw_reports = _get_raw_reports(test_binaries, coverage_dir)
    test_binaries = [pathlib.Path(report).stem for report in raw_reports]
  return [os.path.join(binaries_dir, binary) for binary in test_binaries]


def _get_exclude_opts():
  """Get list of exclude options,"""
  # TODO(oxv): This requires the script to be invoked from the source root.
  all_dirs = next(os.walk(os.getcwd()))[1]
  # TODO(oxv): Make configurable?
  include_dirs = ['cobalt', 'starboard']
  exclude_dirs = [d for d in all_dirs if d not in include_dirs]
  return [
      r'--ignore-filename-regex=.*_test\.h$',
      r'--ignore-filename-regex=.*_test\.cc$',
      r'--ignore-filename-regex=.*_test_fixture\.h$',
      r'--ignore-filename-regex=.*_test_fixture\.cc$',
      r'--ignore-filename-regex=.*_test_internal\.h$',
      r'--ignore-filename-regex=.*_test_internal\.cc$',
      r'--ignore-filename-regex=.*_test_util\.h$',
      r'--ignore-filename-regex=.*_test_util\.cc$',
      r'--ignore-filename-regex=.*_unittest\.h$',
      r'--ignore-filename-regex=.*_unittest\.cc$',
  ] + [f'--ignore-filename-regex={d}/.*' for d in exclude_dirs]


def _generate_reports(target_paths, merged_data_path, report_dir, report_type):
  """Generate reports based on the combined coverage data."""
  toolchain_dir = build.GetClangBinPath(clang.GetClangSpecification())
  llvm_cov = os.path.join(toolchain_dir, 'llvm-cov')

  exclude_opts = _get_exclude_opts()

  test_binary_opts = [target_paths[0]] + \
    ['-object=' + target for target in target_paths[1:]]
  if report_type in ['html', 'both']:
    logging.info('Creating html coverage report')
    show_cmd_list = [
        llvm_cov, 'show', *exclude_opts, '-instr-profile=' + merged_data_path,
        '-format=html', '-output-dir=' + os.path.join(report_dir, 'html'),
        *test_binary_opts
    ]
    subprocess.check_call(show_cmd_list, text=True)

  if report_type in ['text', 'both']:
    logging.info('Creating text coverage report')
    report_cmd_list = [
        llvm_cov, 'show', *exclude_opts, '-instr-profile=' + merged_data_path,
        '-format=text', *test_binary_opts
    ]
    with open(os.path.join(report_dir, 'report.txt'), 'wb') as out:
      subprocess.check_call(report_cmd_list, stdout=out, text=True)


def _merge_coverage_data(targets, coverage_dir):
  """Create a combined coverage file from the raw reports."""
  toolchain_dir = build.GetClangBinPath(clang.GetClangSpecification())
  llvm_profdata = os.path.join(toolchain_dir, 'llvm-profdata')

  raw_reports = _get_raw_reports(targets, coverage_dir)
  if not raw_reports:
    raise RuntimeError('No coverage data found in \'coverage_dir\'')

  logging.info('Merging coverage files')
  merged_data_path = os.path.join(coverage_dir, 'report.profdata')
  merge_cmd_list = [
      llvm_profdata, 'merge', '-sparse=true', '-o', merged_data_path
  ] + raw_reports
  subprocess.check_call(merge_cmd_list, text=True)

  return merged_data_path


def create_report(binary_dir, test_binaries, coverage_dir, report_type='both'):
  """Generate source code coverage reports."""
  merged_data_path = _merge_coverage_data(test_binaries, coverage_dir)

  # TODO(oxv): Make configurable?
  report_dir = coverage_dir
  target_paths = _get_test_binary_paths(binary_dir, test_binaries, coverage_dir)
  _generate_reports(target_paths, merged_data_path, report_dir, report_type)


if __name__ == '__main__':
  SetupDefaultLoggingConfig()

  def report_type_checker(param):
    if param not in ['text', 'html', 'both']:
      raise argparse.ArgumentTypeError('invalid value: ' + param)
    return param

  arg_parser = argparse.ArgumentParser()
  arg_parser.add_argument(
      '--binaries_dir',
      required=True,
      help='Directory where the test binaries are located.')
  arg_parser.add_argument(
      '--coverage_dir',
      required=True,
      help='Directory where coverage files are located and where the reports '
      'will be generated.')
  arg_parser.add_argument(
      '--test_binaries',
      nargs='+',
      default=[],
      help='List of test binaries that should be used for the coverage report. '
      'If not passed all coverage files in --coverage_dir will be used.')
  arg_parser.add_argument(
      '--report_type',
      type=report_type_checker,
      default='both',
      help='Specifies the report type to create. Must be one of \'html\', '
      '\'text\', or \'both\'. Default is \'both\'.')

  args = arg_parser.parse_args()

  sys.exit(
      create_report(args.binaries_dir, args.targets, args.coverage_dir,
                    args.report_type))
