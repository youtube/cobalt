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
"""Parses and analyzes processed smaps logs."""

import argparse
from collections import OrderedDict
import json
import os
import re


class ParsingError(Exception):
  """Custom exception for parsing errors."""
  pass


def parse_smaps_file(filepath):
  """Parses a single processed smaps file and returns memory data."""
  memory_data = OrderedDict()
  total_data = {}
  with open(filepath, 'r', encoding='utf-8') as f:
    lines = f.readlines()

  # Find the header line to get field order
  header_line_index = -1
  for i, line in enumerate(lines):
    # A header line should contain 'name', 'pss', and 'rss'
    if 'name' in line and 'pss' in line and 'rss' in line:
      header_line_index = i
      break

  if header_line_index == -1:
    raise ParsingError(f'Could not find header in {filepath}')

  header_parts = [p.strip() for p in lines[header_line_index].split('|')]
  # The first part is 'name', subsequent parts are memory fields
  fields = [h for h in header_parts if h and h != 'name']

  # Parse data rows
  for line in lines[header_line_index + 1:]:
    stripped_line = line.strip()

    if stripped_line.startswith('Output saved to'):
      break  # Stop if we hit the footer

    if '|' in stripped_line:
      parts = [p.strip() for p in stripped_line.split('|')]
      name = parts[0]

      if name == 'total':
        for i, field in enumerate(fields):
          total_data[field] = int(parts[i + 1])
        continue  # Continue to next line after parsing total

      # Ensure there are enough parts to parse (name + all fields)
      if len(parts) == len(fields) + 1:
        if name:
          try:
            region_data = {}
            for i, field in enumerate(fields):
              region_data[field] = int(parts[i + 1])
            memory_data[name] = region_data
          except ValueError:
            # This will skip non-integer lines, like the repeated header
            continue

  return memory_data, total_data


def extract_timestamp(filename):
  """Extracts the timestamp (YYYYMMDD_HHMMSS) from the filename for sorting."""
  match = re.search(r'smaps_(\d{8}_\d{6}).*?_processed\.txt$', filename)
  if match:
    return match.group(1)
  return None  # Return None if no timestamp is found


def get_top_consumers(memory_data, metric='pss', top_n=10):
  """Returns the top N memory consumers by a given metric."""
  if not memory_data:
    return []
  sorted_consumers = sorted(
      memory_data.items(),
      key=lambda item: item[1].get(metric, 0),
      reverse=True)
  return sorted_consumers[:top_n]


def analyze_logs(log_dir, json_output_filepath=None):
  """Analyzes a directory of processed smaps logs."""
  all_files_with_ts = []
  for f in os.listdir(log_dir):
    if f.endswith('_processed.txt'):
      filepath = os.path.join(log_dir, f)
      timestamp = extract_timestamp(os.path.basename(filepath))
      if timestamp:
        all_files_with_ts.append((timestamp, filepath))

  if not all_files_with_ts:
    print(f'No processed smaps files with valid timestamps found in {log_dir}')
    return

  # Sort files based on the extracted timestamp
  all_files_with_ts.sort(key=lambda x: x[0])
  all_files = [filepath for _, filepath in all_files_with_ts]

  print(f'Analyzing {len(all_files)} processed smaps files...')

  # List to store structured data for JSON output
  analysis_data = []

  last_timestamp = None
  last_memory_data = None
  last_total_data = None

  for filepath in all_files:
    filename = os.path.basename(filepath)
    timestamp = extract_timestamp(filename)
    last_timestamp = timestamp

    try:
      memory_data, total_data = parse_smaps_file(filepath)
    except ParsingError as e:
      print(f'Warning: {e}')
      continue

    last_memory_data = memory_data
    last_total_data = total_data

    # Still collect all data for the JSON output
    current_snapshot = {
        'timestamp':
            timestamp,
        'total_memory':
            total_data,
        'regions': [{
            'name': name,
            'pss': data.get('pss', 0),
            'rss': data.get('rss', 0)
        } for name, data in memory_data.items()]
    }
    analysis_data.append(current_snapshot)

  print('\n' + '=' * 50)
  print(f'Analysis of the last log: {last_timestamp}')
  print('=' * 50)

  # Output JSON data if requested
  if json_output_filepath:
    with open(json_output_filepath, 'w', encoding='utf-8') as f:
      json.dump(analysis_data, f, indent=2)
    print(f'JSON analysis saved to {json_output_filepath}')

  # 1. Top 10 Consumers from the final log
  if last_memory_data:
    print('\nTop 10 Largest Consumers by PSS:')
    top_pss = get_top_consumers(last_memory_data, metric='pss', top_n=10)
    for name, data in top_pss:
      print(f"  - {name}: {data.get('pss', 0)} kB PSS")

    print('\nTop 10 Largest Consumers by RSS:')
    top_rss = get_top_consumers(last_memory_data, metric='rss', top_n=10)
    for name, data in top_rss:
      print(f"  - {name}: {data.get('rss', 0)} kB RSS")

  # 2. Detailed breakdown from the final log
  if last_memory_data:
    print('\n' + '-' * 50)
    print('Full Memory Breakdown:')
    for name, data in last_memory_data.items():
      print(f"  - {name}: {data.get('pss', 0)} kB PSS, "
            f"{data.get('rss', 0)} kB RSS")

  if last_total_data:
    print('\n' + '-' * 50)
    print('Total Memory:')
    for metric, value in last_total_data.items():
      print(f'  - {metric.upper()}: {value} kB')


def run_smaps_analysis_tool(argv=None):
  """Parses arguments and runs the smaps log analysis."""
  parser = argparse.ArgumentParser(
      description='Analyzes processed smaps logs to display the memory '
      'breakdown of the final log file.')
  parser.add_argument(
      'log_dir',
      type=str,
      help='Path to the directory containing processed smaps log files.')

  parser.add_argument(
      '--json_output',
      type=str,
      help='Optional: Path to a file where JSON output will be saved for '
      'use with other tools like visualize_smaps_analysis.py.')
  args = parser.parse_args(argv)
  analyze_logs(args.log_dir, args.json_output)


def main():
  """Main entry point."""
  run_smaps_analysis_tool()


if __name__ == '__main__':
  main()
