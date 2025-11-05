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
"""A tool to run the full smaps analysis pipeline."""

import argparse
import os
import shutil
import tempfile

from read_smaps_batch import run_smaps_batch_tool
from analyze_smaps_logs import run_smaps_analysis_tool
from visualize_smaps_analysis import create_visualization


def main():
  """Main entry point."""
  parser = argparse.ArgumentParser(
      description='Run the full smaps analysis pipeline.')
  parser.add_argument(
      'raw_logs_dir',
      type=str,
      help='Path to the directory containing raw smaps log files.')
  parser.add_argument(
      '--output_image',
      type=str,
      default='smaps_analysis.png',
      help='Path to save the output PNG image file.')
  parser.add_argument(
      '--platform',
      choices=['android', 'linux'],
      default='android',
      help='Specify the platform for platform-specific aggregations.')
  args = parser.parse_args()

  # Create a temporary directory for processed logs
  processed_logs_dir = tempfile.mkdtemp()
  json_output_path = os.path.join(processed_logs_dir, 'analysis.json')

  try:
    # Step 1: Run read_smaps_batch.py
    print('--- Running read_smaps_batch.py ---')
    batch_args = [
        args.raw_logs_dir, '-o', processed_logs_dir, '--platform', args.platform
    ]
    if args.platform == 'android':
      batch_args.append('-d')  # Only append -d for android platform
    run_smaps_batch_tool(batch_args)

    # Step 2: Run analyze_smaps_logs.py
    print('\n--- Running analyze_smaps_logs.py ---')
    analysis_args = [processed_logs_dir, '--json_output', json_output_path]
    run_smaps_analysis_tool(analysis_args)

    # Step 3: Run visualize_smaps_analysis.py
    print('\n--- Running visualize_smaps_analysis.py ---')
    create_visualization(json_output_path, args.output_image)

  finally:
    # Clean up the temporary directory
    shutil.rmtree(processed_logs_dir)


if __name__ == '__main__':
  main()
