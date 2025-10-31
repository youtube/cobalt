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
from collections import defaultdict, OrderedDict
import json
import os
import re
import sys


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

  # Second pass for aggregation
  aggregated_data = OrderedDict()
  shared_mem_total = defaultdict(int)
  for name, data in memory_data.items():
    if name.startswith('mem/shared_memory'):
      for field, value in data.items():
        shared_mem_total[field] += value
    else:
      aggregated_data[name] = data

  if shared_mem_total:
    aggregated_data['[mem/shared_memory]'] = dict(shared_mem_total)

  return aggregated_data, total_data


def extract_timestamp(filename):
  """Extracts the timestamp (YYYYMMDD_HHMMSS) from the filename for sorting."""
  match = re.search(r'_(\d{8})_(\d{6})_\d{4}_processed\.txt$', filename)
  if match:
    return f'{match.group(1)}_{match.group(2)}'

  print(
      f"Warning: Could not extract timestamp from '{filename}'. "
      'File will be sorted last.',
      file=sys.stderr)
  return '00000000_000000'  # Default for files without a clear timestamp


def get_top_consumers(memory_data, metric='pss', top_n=5):
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
  all_files = [
      os.path.join(log_dir, f)
      for f in os.listdir(log_dir)
      if f.endswith('_processed.txt')
  ]
  all_files.sort(key=extract_timestamp)

  if not all_files:
    print(f'No processed smaps files found in {log_dir}')
    return

  print(f'Analyzing {len(all_files)} processed smaps files...')

  # List to store structured data for JSON output
  analysis_data = []

  # Store data over time for each memory region
  total_history = defaultdict(list)

  first_timestamp = None
  last_timestamp = None
  last_memory_data = None
  first_memory_data = None

  for filepath in all_files:
    filename = os.path.basename(filepath)
    timestamp = extract_timestamp(filename)
    if not first_timestamp:
      first_timestamp = timestamp
    last_timestamp = timestamp

    try:
      memory_data, total_data = parse_smaps_file(filepath)
    except ParsingError as e:
      print(f'Warning: {e}')
      continue

    if first_memory_data is None:
      first_memory_data = memory_data
    last_memory_data = memory_data  # Keep track of the last data

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

    for metric, value in total_data.items():
      total_history[metric].append(value)

  print('\n' + '=' * 50)
  print(f'Analysis from {first_timestamp} to {last_timestamp}')
  print('=' * 50)

  # Output JSON data if requested
  if json_output_filepath:
    with open(json_output_filepath, 'w', encoding='utf-8') as f:
      json.dump(analysis_data, f, indent=2)
    print(f'JSON analysis saved to {json_output_filepath}')

  # 1. Largest Consumers by the end log
  print('\nOverall Total Memory Change:')
  print('\nTop 10 Largest Consumers by the End Log (PSS):')
  top_pss = get_top_consumers(last_memory_data, metric='pss', top_n=10)
  for name, data in top_pss:
    print(f"  - {name}: {data.get('pss', 0)} kB PSS, "
          f"{data.get('rss', 0)} kB RSS")

  print('\nTop 10 Largest Consumers by the End Log (RSS):')
  top_rss = get_top_consumers(last_memory_data, metric='rss', top_n=10)
  for name, data in top_rss:
    print(f"  - {name}: {data.get('rss', 0)} kB RSS, "
          f"{data.get('pss', 0)} kB PSS")

  # 2. Top 10 Increases in Memory Over Time
  print('\nTop 10 Memory Increases Over Time (PSS):')
  pss_growth = []
  if last_memory_data and first_memory_data:
    all_keys = set(first_memory_data.keys()) | set(last_memory_data.keys())
    for r_name in all_keys:
      initial_pss = first_memory_data.get(r_name, {}).get('pss', 0)
      final_pss = last_memory_data.get(r_name, {}).get('pss', 0)
      growth = final_pss - initial_pss
      if growth > 0:
        pss_growth.append((r_name, growth))

  pss_growth.sort(key=lambda item: item[1], reverse=True)
  for name, growth in pss_growth[:10]:
    print(f'  - {name}: +{growth} kB PSS')

  print('\nTop 10 Memory Increases Over Time (RSS):')
  rss_growth = []
  if last_memory_data and first_memory_data:
    all_keys = set(first_memory_data.keys()) | set(last_memory_data.keys())
    for r_name in all_keys:
      initial_rss = first_memory_data.get(r_name, {}).get('rss', 0)
      final_rss = last_memory_data.get(r_name, {}).get('rss', 0)
      growth = final_rss - initial_rss
      if growth > 0:
        rss_growth.append((r_name, growth))

  rss_growth.sort(key=lambda item: item[1], reverse=True)
  for name, growth in rss_growth[:10]:
    print(f'  - {name}: +{growth} kB RSS')


def run_smaps_analysis_tool(argv=None):
  """Parses arguments and runs the smaps log analysis."""
  parser = argparse.ArgumentParser(
      description='Analyze processed smaps logs to identify '
      'memory consumers and growth.')
  parser.add_argument(
      'log_dir',
      type=str,
      help='Path to the directory containing processed smaps log files.')

  parser.add_argument(
      '--json_output',
      type=str,
      help='Optional: Path to a file where JSON output will be saved.')
  args = parser.parse_args(argv)
  analyze_logs(args.log_dir, args.json_output)


def main():
  """Main entry point."""
  run_smaps_analysis_tool()


if __name__ == '__main__':
  main()
