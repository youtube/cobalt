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
"""Merge C++ and Rust archives into a single static library."""

import argparse
import os
import subprocess
import sys


def main():
  parser = argparse.ArgumentParser(description='Merge C++ and Rust archives.')
  parser.add_argument(
      '--rust-libs-file',
      required=True,
      help='File containing list of Rust libs.')
  parser.add_argument(
      '--cpp-lib', required=True, help='Path to C++ static library.')
  parser.add_argument(
      '--output', required=True, help='Path to output merged library.')
  args = parser.parse_args()

  try:
    with open(args.rust_libs_file, 'r', encoding='utf-8') as f:
      # Deduplicate while preserving order to avoid duplicate inputs to libtool
      rust_libs = list(
          dict.fromkeys([line.strip() for line in f if line.strip()]))
  except IOError as e:
    print(f'Error reading Rust libs file: {e}', file=sys.stderr)
    return 1

  # Convert paths to absolute to ensure compatibility with xcrun/libtool
  abs_output = os.path.abspath(args.output)
  abs_cpp_lib = os.path.abspath(args.cpp_lib)
  abs_rust_libs = [os.path.abspath(lib) for lib in rust_libs]

  # Use xcrun to guarantee we get Apple's libtool.
  # We assume this script only runs on macOS since tvOS builds require macOS.
  cmd = ['xcrun', 'libtool', '-static', '-o', abs_output, abs_cpp_lib
        ] + abs_rust_libs

  cmd_str = ' '.join(cmd)
  print(f'Running: {cmd_str}')
  result = subprocess.run(cmd, capture_output=True, text=True, check=False)
  if result.returncode != 0:
    print(
        f'libtool failed:\nstdout:\n{result.stdout}\nstderr:\n{result.stderr}',
        file=sys.stderr)
    return result.returncode

  return 0


if __name__ == '__main__':
  sys.exit(main())
