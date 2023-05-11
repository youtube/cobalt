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
"""Cross-platform unit test runner."""

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


def _get_raw_reports(targets, coverage_dir):
  raw_reports = []
  for target in targets:
    profraw_file = os.path.join(coverage_dir, target + '.profraw')
    if os.path.isfile(profraw_file):
      raw_reports.append(profraw_file)
  else:
    raw_reports = glob.glob(os.path.join(coverage_dir, '*.profraw'))
  return raw_reports


def _get_target_paths(binaries_dir, targets, coverage_dir):
  raw_reports = _get_raw_reports(targets, coverage_dir)
  if not targets:
    targets = [pathlib.Path(report).stem for report in raw_reports]
  return [os.path.join(binaries_dir, target) for target in targets]


def _get_exclude_opts():
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


def _generate_reports(target_paths, profdata_path, report_dir, report_type):
  toolchain_dir = build.GetClangBinPath(clang.GetClangSpecification())
  llvm_cov = os.path.join(toolchain_dir, 'llvm-cov')

  exclude_opts = _get_exclude_opts()

  test_binary_opts = [target_paths[0]] + \
    ['-object=' + target for target in target_paths[1:]]
  if report_type in ['html', 'both']:
    show_cmd_list = [
        llvm_cov, 'show', *exclude_opts, '-instr-profile=' + profdata_path,
        '-format=html', '-output-dir=' + os.path.join(report_dir, 'html'),
        *test_binary_opts
    ]
    logging.info('Creating html coverage report')
    subprocess.check_call(show_cmd_list, text=True)

  if report_type in ['text', 'both']:
    report_cmd_list = [
        llvm_cov, 'report', *exclude_opts, '-instr-profile=' + profdata_path,
        *test_binary_opts
    ]
    logging.info('Creating text coverage report')
    with open(os.path.join(report_dir, 'report.txt'), 'wb') as out:
      subprocess.check_call(report_cmd_list, stdout=out, text=True)


def _merge_coverage_data(targets, coverage_dir):
  profdata_path = os.path.join(coverage_dir, 'report.profdata')

  toolchain_dir = build.GetClangBinPath(clang.GetClangSpecification())
  llvm_profdata = os.path.join(toolchain_dir, 'llvm-profdata')

  raw_reports = _get_raw_reports(targets, coverage_dir)
  if not raw_reports:
    logging.warning('Found no coverage reports')
    raise RuntimeError('No coverage data found in \'coverage_dir\'')

  merge_cmd_list = [
      llvm_profdata, 'merge', '-sparse=true', '-o', profdata_path, *raw_reports
  ]
  logging.info('Merging coverage files')
  subprocess.check_call(merge_cmd_list, text=True)

  return profdata_path


def create_report(binaries_dir, targets, coverage_dir, report_type='both'):
  """Generate the source code coverage report."""
  profdata_path = _merge_coverage_data(targets, coverage_dir)

  # TODO(oxv): Make configurable?
  report_dir = coverage_dir
  target_paths = _get_target_paths(binaries_dir, targets, coverage_dir)
  _generate_reports(target_paths, profdata_path, report_dir, report_type)


if __name__ == '__main__':
  SetupDefaultLoggingConfig()

  def type_checker(param):
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
      help='Directory where coverage files are located and where the report '
      'will be generated.')
  arg_parser.add_argument(
      '--targets',
      nargs='+',
      default=[],
      help='List of test binaries that should be used for the coverage report. '
      'If not passed all coverage files in --coverage_dir will be used.')
  arg_parser.add_argument(
      '--report_type',
      type=type_checker,
      default='both',
      help='Specifies the report type to create. Must be one of \'html\', '
      '\'text\', or \'both\'. Default is \'both\'')

  args = arg_parser.parse_args()

  sys.exit(
      create_report(args.binaries_dir, args.targets, args.coverage_dir,
                    args.report_type))
