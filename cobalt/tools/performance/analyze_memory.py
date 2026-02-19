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

  process_dumps = dump_data.get('process_dumps', [])
  if not process_dumps:
    print('Error: No "process_dumps" found in the JSON.')
    return False

  total_rss_kb = 0
  total_gpu_kb = 0
  attributed_bytes = 0
  breakdown = {}

  for proc in process_dumps:
    os_dump = proc.get('os_dump', {})
    total_rss_kb += os_dump.get('resident_set_kb', 0)
    total_gpu_kb += os_dump.get('gpu_memory_kb', 0)

    allocators = proc.get('chrome_allocator_dumps', {})
    for name, metrics in allocators.items():
      if name.startswith('cobalt/') or name.startswith('chromium'):
        # Cobalt providers use "size" as the key for byte counts
        size = metrics.get('size', 0)
        attributed_bytes += int(size)
        breakdown[name] = breakdown.get(name, 0) + int(size)

  # Total Footprint = RSS + GPU (since DMA-BUFs are often extra-process)
  total_footprint_bytes = (total_rss_kb + total_gpu_kb) * 1024
  if total_footprint_bytes == 0:
    print('Error: Total footprint is 0.')
    return False

  attribution_pct = (attributed_bytes / total_footprint_bytes) * 100

  print(f'Analysis Results for {dump_path}:')
  print(f'  Total RSS:         {total_rss_kb / 1024:.2f} MB')
  print(f'  Extra GPU Memory:  {total_gpu_kb / 1024:.2f} MB')
  print(f'  Total Footprint:   {total_footprint_bytes / 1024 / 1024:.2f} MB')
  print(f'  Attributed Memory: {attributed_bytes / 1024 / 1024:.2f} MB')
  print(f'  Attribution %:     {attribution_pct:.2f}%')
  print('\n  Breakdown:')
  for name, size in sorted(breakdown.items(), key=lambda x: x[1], reverse=True):
    print(f'    {name:40} {size / 1024 / 1024:8.2f} MB')

  if threshold > 0 and attribution_pct < threshold:
    print(f'\nFAIL: Attribution {attribution_pct:.2f}% is below threshold '
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
