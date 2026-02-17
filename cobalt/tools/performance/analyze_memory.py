#!/usr/bin/env python3
# Copyright 2026 The Cobalt Authors. All Rights Reserved.
"""Utility to analyze Cobalt memory dumps."""

import argparse
import json
import sys


def analyze_memory(dump_path, threshold=0):
  """Analyzes a memory dump file and checks against an attribution threshold."""
  try:
    with open(dump_path, 'r', encoding='utf-8') as f:
      dump_data = json.load(f)
  except (IOError, json.JSONDecodeError) as e:
    print(f"Error reading dump file: {e}")
    return False

  # 1. Get Total Process RSS
  # Expected structure: dump_data['process_totals']['resident_set_bytes']
  rss = dump_data.get('process_totals', {}).get('resident_set_bytes', 0)
  if rss == 0:
    print('Error: Could not find Total Process RSS (resident_set_bytes)')
    return False

  # 2. Sum cobalt/* and chromium allocator dumps
  # Expected structure:
  # dump_data['allocators_dumps'] = {
  #   'name': { 'attrs': { 'size': { 'value': N } } }
  # }
  attributed_bytes = 0
  allocator_dumps = dump_data.get('allocators_dumps', {})

  for name, dump in allocator_dumps.items():
    if name.startswith('cobalt/') or name.startswith('chromium'):
      size = dump.get('attrs', {}).get('size', {}).get('value', 0)
      attributed_bytes += int(size)

  # 3. Calculate Attribution %
  attribution_pct = (attributed_bytes / rss) * 100

  print(f'Analysis Results for {dump_path}:')
  print(f'  Total RSS:         {rss / 1024 / 1024:.2f} MB')
  print(f'  Attributed Memory: {attributed_bytes / 1024 / 1024:.2f} MB')
  print(f'  Attribution %:     {attribution_pct:.2f}%')

  if threshold > 0 and attribution_pct < threshold:
    print(f'FAIL: Attribution {attribution_pct:.2f}% is below threshold '
          f'{threshold}%')
    return False

  return True


def main():
  """Main entry point for analyze_memory script."""
  parser = argparse.ArgumentParser(description='Analyze Cobalt Memory Dumps')
  parser.add_argument('dump_file', help='Path to the JSON memory dump file')
  parser.add_argument(
      '--threshold',
      type=float,
      default=0.0,
      help='Fail if attribution is below this percentage')
  args = parser.parse_args()

  if analyze_memory(args.dump_file, args.threshold):
    sys.exit(0)
  else:
    sys.exit(1)


if __name__ == '__main__':
  main()
