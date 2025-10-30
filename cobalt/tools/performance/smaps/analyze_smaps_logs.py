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

import argparse
import os
import re
from collections import defaultdict, OrderedDict

class ParsingError(Exception):
    """Custom exception for parsing errors."""
    pass

def parse_smaps_file(filepath):
    """Parses a single processed smaps file and returns memory data."""
    memory_data = OrderedDict()
    total_data = {}
    with open(filepath, 'r') as f:
        lines = f.readlines()

    # Find the header line to get field order
    header_line_index = -1
    for i, line in enumerate(lines):
        # A header line should contain 'name', 'pss', and 'rss'
        if 'name' in line and 'pss' in line and 'rss' in line:
            header_line_index = i
            break
    
    if header_line_index == -1:
        raise ParsingError(f"Could not find header in {filepath}")

    header_parts = [p.strip() for p in lines[header_line_index].split('|')]
    # The first part is 'name', subsequent parts are memory fields
    fields = [h for h in header_parts if h and h != 'name']

    # Parse data rows
    for line in lines[header_line_index + 1:]:
        stripped_line = line.strip()
        
        if stripped_line.startswith('Output saved to'):
            break # Stop if we hit the footer

        if '|' in stripped_line:
            parts = [p.strip() for p in stripped_line.split('|')]
            name = parts[0]

            if name == 'total':
                for i, field in enumerate(fields):
                    total_data[field] = int(parts[i + 1])
                continue # Continue to next line after parsing total

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
    match = re.search(r'_(\d{8})_(\d{6})_\d{4}_processed\.txt$', filename)
    if match:
        return f"{match.group(1)}_{match.group(2)}"
    # TODO: Consider logging a warning or raising an exception if the
    #       timestamp cannot be extracted.
    return "00000000_000000" # Default for files without a clear timestamp

def get_top_consumers(memory_data, metric='pss', top_n=5):
    """Returns the top N memory consumers by a given metric."""
    if not memory_data:
        return []
    sorted_consumers = sorted(
        memory_data.items(), key=lambda item: item[1].get(metric, 0), reverse=True)
    return sorted_consumers[:top_n]

def analyze_logs(log_dir):
    """Analyzes a directory of processed smaps logs."""
    all_files = [os.path.join(log_dir, f) for f in os.listdir(log_dir) if f.endswith('_processed.txt')]
    all_files.sort(key=extract_timestamp)

    if not all_files:
        print(f"No processed smaps files found in {log_dir}")
        return

    print(f"Analyzing {len(all_files)} processed smaps files...")

    # Store data over time for each memory region
    region_history = defaultdict(lambda: defaultdict(list))
    total_history = defaultdict(list)

    first_timestamp = None
    last_timestamp = None
    last_memory_data = None

    for filepath in all_files:
        filename = os.path.basename(filepath)
        timestamp = extract_timestamp(filename)
        if not first_timestamp:
            first_timestamp = timestamp
        last_timestamp = timestamp

        try:
            memory_data, total_data = parse_smaps_file(filepath)
        except ParsingError as e:
            print(f"Warning: {e}")
            continue
        
        last_memory_data = memory_data # Keep track of the last data for end log analysis

        for region_name, data in memory_data.items():
            for metric, value in data.items():
                region_history[region_name][metric].append(value)
        
        for metric, value in total_data.items():
            total_history[metric].append(value)

    print("\n" + "=" * 50)
    print(f"Analysis from {first_timestamp} to {last_timestamp}")
    print("=" * 50)

    # 1. Largest Consumers by the end log
    print("\nTop 5 Largest Consumers by the End Log (PSS):")
    top_pss = get_top_consumers(last_memory_data, metric='pss', top_n=5)
    for name, data in top_pss:
        print(f"  - {name}: {data.get('pss', 0)} kB PSS, {data.get('rss', 0)} kB RSS")

    print("\nTop 5 Largest Consumers by the End Log (RSS):")
    top_rss = get_top_consumers(last_memory_data, metric='rss', top_n=5)
    for name, data in top_rss:
        print(f"  - {name}: {data.get('rss', 0)} kB RSS, {data.get('pss', 0)} kB PSS")

    # 2. Top 5 Increases in Memory Over Time
    print("\nTop 5 Memory Increases Over Time (PSS):")
    pss_growth = []
    for region_name, history in region_history.items():
        if 'pss' in history and len(history['pss']) > 1:
            growth = history['pss'][-1] - history['pss'][0]
            if growth > 0:
                pss_growth.append((region_name, growth))
    
    pss_growth.sort(key=lambda item: item[1], reverse=True)
    for name, growth in pss_growth[:5]:
        print(f"  - {name}: +{growth} kB PSS")

    print("\nTop 5 Memory Increases Over Time (RSS):")
    rss_growth = []
    for region_name, history in region_history.items():
        if 'rss' in history and len(history['rss']) > 1:
            growth = history['rss'][-1] - history['rss'][0]
            if growth > 0:
                rss_growth.append((region_name, growth))
    
    rss_growth.sort(key=lambda item: item[1], reverse=True)
    for name, growth in rss_growth[:5]:
        print(f"  - {name}: +{growth} kB RSS")

    # Overall Total Memory Change
    print("\nOverall Total Memory Change:")
    if 'pss' in total_history and len(total_history['pss']) > 1:
        total_pss_change = total_history['pss'][-1] - total_history['pss'][0]
        print(f"  Total PSS Change: {total_pss_change} kB")
    if 'rss' in total_history and len(total_history['rss']) > 1:
        total_rss_change = total_history['rss'][-1] - total_history['rss'][0]
        print(f"  Total RSS Change: {total_rss_change} kB")

def main():
    parser = argparse.ArgumentParser(
        description="Analyze processed smaps logs to identify memory consumers and growth.")
    parser.add_argument(
        'log_dir',
        type=str,
        help="Path to the directory containing processed smaps log files.")
    
    args = parser.parse_args()
    analyze_logs(args.log_dir)

if __name__ == '__main__':
    main()
