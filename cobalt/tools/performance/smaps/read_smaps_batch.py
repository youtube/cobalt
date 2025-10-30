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

# Add the script's directory to the Python path to allow importing
current_dir = os.path.dirname(os.path.abspath(__file__))
if current_dir not in sys.path:
    sys.path.insert(0, current_dir)

from read_smaps import read_smap

class ProcessArgs:
        def __init__(self, smaps_file, args):
            self.smaps_file = smaps_file
            for k, v in vars(args).items():
                setattr(self, k, v)


def main():
  """Main entry point for batch processing."""
  parser = argparse.ArgumentParser(
      description='A tool to batch process smaps files.')
  parser.add_argument(
      'smaps_files', nargs='+', help='One or more smaps files to process.')
  parser.add_argument(
      '-o',
      '--output_dir',
      type=str,
      default='processed_smaps',
      help='The directory to save processed smaps files.')
  # Add all the other arguments from read_smaps.py so they can be passed through
  parser.add_argument(
      '-k',
      '--sortkey',
      choices=['size', 'rss', 'pss', 'anonymous', 'name'],
      default='pss')
  parser.add_argument(
      '-s',
      '--strip_paths',
      action='store_true',
      help='Remove leading paths from binaries')
  parser.add_argument(
      '-r',
      '--remove_so_versions',
      action='store_true',
      help='Remove dynamic library versions')
  parser.add_argument(
      '-a',
      '--aggregate_solibs',
      action='store_true',
      help='Collapse solibs into single row')
  parser.add_argument(
      '-d',
      '--aggregate_android',
      action='store_true',
      help='Consolidate various Android allocations')
  parser.add_argument(
      '-z',
      '--aggregate_zeros',
      action='store_true',
      help='Consolidate rows that show zero RSS and PSS')
  parser.add_argument(
      '--no_anonhuge',
      action='store_true',
      help='Omit AnonHugePages column from output')
  parser.add_argument(
      '--no_shr_dirty',
      action='store_true',
      help='Omit Shared_Dirty column from output')

  args = parser.parse_args()

  os.makedirs(args.output_dir, exist_ok=True)

  for smaps_file in args.smaps_files:
    base_name = os.path.basename(smaps_file)
    name, ext = os.path.splitext(base_name)
    output_name = f"{name}_processed{ext}"
    output_path = os.path.join(args.output_dir, output_name)

    # The original read_smap function expects args.smaps_file, so we need to
    # create a mock object that has the smaps_file attribute.
    # We can use a simple Namespace object for this.
    # Create a new object that has all the original args,
    # but with smaps_file set to the current file in the loop.
    process_args = ProcessArgs(smaps_file, args)

    original_stdout = sys.stdout
    with open(output_path, 'w', encoding='utf-8') as f:
      sys.stdout = f
      print(f"Processing {smaps_file}...")
      read_smap(process_args)
      print(f"\nOutput saved to {output_path}")

    sys.stdout = original_stdout
    print(f"Successfully processed {smaps_file} -> {output_path}")


if __name__ == '__main__':
  main()
