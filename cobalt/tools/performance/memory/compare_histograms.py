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
"""Tool for comparing and analyzing UMA memory histograms."""

import argparse
import collections
import datetime
import json
import os
import statistics
import time

# Normalize all units to MB (using 1024 for consistency with memory standards)
# Cobalt/Chromium histogram naming conventions and MetricSize types:
# kLarge: Already in MiB (recorded as value / 1024 / 1024)
# kSmall: In KiB (recorded as value / 1024)
# kTiny/kCustom: In Bytes (recorded as raw value)

UNIT_FACTORS = {
    # Large metrics (MiB)
    '.ResidentSet': 1.0,
    '.PrivateMemoryFootprint': 1.0,
    '.SharedMemoryFootprint': 1.0,
    '.BlinkGC': 1.0,
    '.AndroidOtherPss': 1.0,
    '.DawnSharedContext': 1.0,
    '.CommandBuffer': 1.0,
    '.SharedImages': 1.0,
    '.Vulkan': 1.0,
    '.JavaHeap': 1.0,
    '.Malloc': 1.0,
    '.PartitionAlloc': 1.0,
    '.Skia': 1.0,
    '.V8': 1.0,
    '.CodeOther': 1.0,
    '.Fonts': 1.0,
    '.AshmemJit': 1.0,
    '.AndroidRuntime': 1.0,
    '.Stacks': 1.0,
    'Memory.Total.TileMemory': 1.0,

    # Small metrics (KiB -> MB)
    '.FontCaches': 1.0 / 1024.0,
    '.LevelDatabase': 1.0 / 1024.0,
    '.SkGlyphCache': 1.0 / 1024.0,
    '.Sqlite': 1.0 / 1024.0,
    '.UI': 1.0 / 1024.0,
    '.Gpu.GraphiteShaderCache': 1.0 / 1024.0,
    '.Gpu.GrShaderCache': 1.0 / 1024.0,
    '.GpuMappedMemory': 1.0 / 1024.0,
    '.Small.': 1.0 / 1024.0,

    # Tiny/Custom metrics (Bytes -> MB)
    '.NumberOfDocuments': 1.0 / (1024 * 1024),
    '.NumberOfFrames': 1.0 / (1024 * 1024),
    '.NumberOfLayoutObjects': 1.0 / (1024 * 1024),
    '.ServiceDiscardableManager': 1.0 / (1024 * 1024),
    '.ServiceTransferCache': 1.0 / (1024 * 1024),
    '.Tiny.': 1.0 / (1024 * 1024),
}


def get_normalization_factor(name):
  """Returns the normalization factor for a given metric name."""
  for suffix, factor in sorted(
      UNIT_FACTORS.items(), key=lambda x: len(x[0]), reverse=True):
    if suffix in name:
      return factor
  return 1.0


def parse_uma_histos(filename):
  """Parses UMA histograms from a file and returns a history of metrics."""
  metrics_history = collections.defaultdict(list)

  if not os.path.exists(filename):
    return None

  with open(filename, 'r', encoding='utf-8') as f:
    for line in f:
      parts = line.strip().split(',', 2)
      if len(parts) < 3:
        continue

      try:
        payload = json.loads(parts[2])
        histograms = payload.get('result', {}).get('histograms', [])
        for h in histograms:
          name = h['name']
          if h['count'] > 0:
            avg_value = h['sum'] / h['count']
            factor = get_normalization_factor(name)
            metrics_history[name].append(avg_value * factor)
      except (json.JSONDecodeError, KeyError, ZeroDivisionError):
        continue

  return metrics_history


def generate_report(metrics_history, output_file=None):
  """Generates a memory histogram comparison report."""
  target_rs = 'Memory.Browser.ResidentSet'
  if target_rs not in metrics_history:
    msg = f'Error: {target_rs} not found in the input file.'
    if output_file:
      with open(output_file, 'a', encoding='utf-8') as f:
        f.write(msg + '\n')
    print(msg)
    return

  # Calculate medians
  medians = {
      name: statistics.median(values)
      for name, values in metrics_history.items()
  }
  rs_median = medians[target_rs]

  report_lines = []
  report_lines.append(f"{'Memory Histogram Comparison':^88}")
  report_lines.append(f"{'Normalized to MB (Median over time)':^88}")
  report_lines.append('=' * 88)
  report_lines.append(
      f"{'Histogram Name':<60} | {'Median (MB)':>12} | {'% of RS':>10}")
  report_lines.append('-' * 88)

  # Sort by median value descending
  sorted_metrics = sorted(medians.items(), key=lambda x: x[1], reverse=True)

  # Print Resident Set first for reference
  percent_100 = '100.00%'
  report_lines.append(
      f'{target_rs:<60} | {rs_median:>12.2f} | {percent_100:>10}')

  for name, median_val in sorted_metrics:
    if name == target_rs:
      continue

    percentage = (median_val / rs_median) * 100
    report_lines.append(
        f'{name:<60} | {median_val:>12.2f} | {percentage:>9.2f}%')
  report_lines.append('-' * 88)

  report_content = '\n'.join(report_lines)

  if output_file:
    with open(output_file, 'w', encoding='utf-8') as f:
      f.write(report_content + '\n')
    print(f'Report saved to: {output_file}')
  else:
    print(report_content)


def main():
  """Main entry point for the compare_histograms tool."""
  parser = argparse.ArgumentParser(
      description='Analyze UMA histograms periodically.')
  parser.add_argument(
      '--input',
      type=str,
      default='uma_histos.txt',
      help='Input file with UMA histograms')
  parser.add_argument(
      '--output_dir',
      type=str,
      default='histogram_reports',
      help='Directory to save reports')
  parser.add_argument(
      '--interval',
      type=int,
      default=0,
      help='Interval in seconds for periodic execution (0 for one-shot)')
  parser.add_argument(
      '--duration',
      type=int,
      default=3600,
      help='Total duration in seconds for periodic execution')

  args = parser.parse_args()

  if not os.path.exists(args.output_dir):
    os.makedirs(args.output_dir)

  start_time = time.time()

  try:
    while True:
      current_time = datetime.datetime.now().strftime('%Y%m%d_%H%M%S')
      print(f'[{current_time}] Analyzing {args.input}...')

      data = parse_uma_histos(args.input)
      if data:
        output_file = os.path.join(args.output_dir,
                                   f'report_{current_time}.txt')
        generate_report(data, output_file)
      else:
        print(f'Warning: Input file {args.input} not found or empty.')

      if args.interval <= 0:
        break

      elapsed = time.time() - start_time
      if elapsed >= args.duration:
        print(f'Completed duration of {args.duration} seconds.')
        break

      print(f'Waiting {args.interval} seconds for next analysis...')
      time.sleep(args.interval)

  except KeyboardInterrupt:
    print('\nAnalysis stopped by user.')


if __name__ == '__main__':
  main()
