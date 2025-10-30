#!/usr/bin/env python3
#
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

"""Tool to batch process smaps files."""

import argparse
import os
import sys

from read_smaps import read_smap, get_analysis_parser


def run_smaps_batch_tool(argv=None):
  """Parses arguments and runs the batch processing of smaps files."""
  parser = argparse.ArgumentParser(
      description='A tool to batch process smaps files.',
      parents=[get_analysis_parser()])
  parser.add_argument(
      'smaps_files', nargs='+', help='One or more smaps files to process.')
  parser.add_argument(
      '-o',
      '--output_dir',
      type=str,
      default='processed_smaps',
      help='The directory to save processed smaps files.')

  args = parser.parse_args(argv)

  os.makedirs(args.output_dir, exist_ok=True)

  for smaps_file in args.smaps_files:
    base_name = os.path.basename(smaps_file)
    name, ext = os.path.splitext(base_name)
    output_name = f'{name}_processed{ext}'
    output_path = os.path.join(args.output_dir, output_name)

    # Create a new namespace object for each file, inheriting the other args
    process_args = argparse.Namespace(**vars(args))
    process_args.smaps_file = smaps_file

    original_stdout = sys.stdout
    with open(output_path, 'w', encoding='utf-8') as f:
      sys.stdout = f
      print(f'Processing {smaps_file}...')
      read_smap(process_args)
      print(f'\nOutput saved to {output_path}')

    sys.stdout = original_stdout
    print(f'Successfully processed {smaps_file} -> {output_path}')

def main():
  """Main entry point for batch processing."""
  run_smaps_batch_tool()


if __name__ == '__main__':
  main()
