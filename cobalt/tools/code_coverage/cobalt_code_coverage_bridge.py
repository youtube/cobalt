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
This tool performs code coverage scans using the underlying tool at
tools/code_coverage/coverage.py, but prevents it from calling
update.py --package.
"""

import os
import subprocess
import sys

# Absolute path to the root of the checkout.
SRC_ROOT_PATH = os.path.join(
    os.path.abspath(os.path.dirname(__file__)), os.path.pardir, os.path.pardir,
    os.path.pardir)
COVERAGE_PY_PATH = os.path.join(SRC_ROOT_PATH, 'tools', 'code_coverage',
                                'coverage.py')
LLVM_RELEASE_ASSERTS_DIR = os.path.join(SRC_ROOT_PATH, 'third_party',
                                        'llvm-build', 'Release+Asserts')
LLVM_BIN_DIR = os.path.join(LLVM_RELEASE_ASSERTS_DIR, 'bin')


def main():
  """
  Main function for the code coverage tool.
  """
  args = sys.argv[1:]
  if '--coverage-tools-dir' not in args:
    expected_entries = [
        'bin', 'cr_build_revision', 'lib', 'llvmobjdump_build_revision'
    ]
    if not os.path.isdir(LLVM_RELEASE_ASSERTS_DIR):
      print(
          f'Error: LLVM build directory not found at {LLVM_RELEASE_ASSERTS_DIR}'
      )
      print('Please run `gclient sync --no-history -r $(git rev-parse @)` ' \
            'to install them.')
      return 1

    actual_entries = os.listdir(LLVM_RELEASE_ASSERTS_DIR)
    missing_entries = [
        entry for entry in expected_entries if entry not in actual_entries
    ]
    if missing_entries:
      print(
          f'Error: The LLVM build directory {LLVM_RELEASE_ASSERTS_DIR} ' \
          'is incomplete.'
      )
      print(f"Missing entries: {', '.join(missing_entries)}")
      print('Please run `gclient sync --no-history -r $(git rev-parse @)` ' \
            'to ensure a complete installation.')
      return 1

    args.extend(['--coverage-tools-dir', LLVM_BIN_DIR])

  # We explicitly do not want to call update.py --package, so we check if
  # the underlying tool is about to do that. The logic in coverage.py is that
  # it calls update.py if --coverage-tools-dir is not specified. So, we ensure
  # that --coverage-tools-dir is always specified.
  if any(arg.startswith('--package') for arg in args):
    print('Error: This tool should not be used to call update.py --package.')
    return 1

  subprocess.check_call([sys.executable, COVERAGE_PY_PATH] + args)


if __name__ == '__main__':
  sys.exit(main())
