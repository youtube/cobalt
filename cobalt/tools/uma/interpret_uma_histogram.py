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
"""Calculates and displays percentiles from UMA histogram data."""

import argparse
import json
import sys
from collections import defaultdict
from datetime import datetime

# Import matplotlib for plotting, but handle the case where it's not installed.
try:
  import matplotlib.pyplot as plt
  import matplotlib.dates as mdates
  MATPLOTLIB_AVAILABLE = True
except ImportError:
  MATPLOTLIB_AVAILABLE = False


def calculate_percentiles(histogram, percentiles=(25, 50, 75, 95, 99)):
  """Calculates specified percentiles from histogram bucket data.

  Args:
    histogram: A dictionary representing the histogram data, containing 'count'
      and 'buckets'.
    percentiles: A tuple of percentiles to calculate (0-100).

  Returns:
    A dictionary mapping each percentile to its calculated value.
  """
  total_count = histogram.get('count', 0)
  buckets = histogram.get('buckets', [])
  if not total_count or not buckets:
    return {p: None for p in percentiles}

  buckets.sort(key=lambda b: b.get('low', 0))

  percentile_values = {}
  cumulative_count = 0
  bucket_index = 0

  for p in sorted(percentiles):
    percentile_rank = total_count * (p / 100.0)

    while bucket_index < len(buckets):
      bucket = buckets[bucket_index]
      bucket_count = bucket.get('count', 0)
      if cumulative_count + bucket_count >= percentile_rank:
        percentile_values[p] = bucket.get('high')
        break
      cumulative_count += bucket_count
      bucket_index += 1
    else:
      if buckets:
        percentile_values[p] = buckets[-1].get('high')
      else:
        percentile_values[p] = None

  return percentile_values


def plot_histogram_data(histogram_data):
  """Generates and saves plots for each histogram's percentiles over time.

  Args:
    histogram_data: A dictionary where keys are histogram names and values are
      lists of (timestamp, percentile_dict) tuples.
  """
  if not MATPLOTLIB_AVAILABLE:
    print(
        'Matplotlib is not installed. Skipping plot generation.',
        file=sys.stderr)
    print('Please install it with: pip install matplotlib', file=sys.stderr)
    return

  for name, data_points in histogram_data.items():
    if not data_points:
      continue

    # Sort data by timestamp
    data_points.sort(key=lambda x: x[0])
    timestamps = [dp[0] for dp in data_points]
    percentiles_data = {p: [] for p in data_points[0][1]}

    for _, p_values in data_points:
      for p, value in p_values.items():
        percentiles_data[p].append(value)

    fig, ax = plt.subplots(figsize=(12, 7))
    for p, values in percentiles_data.items():
      # Filter out None values for plotting
      plot_ts = [ts for ts, v in zip(timestamps, values) if v is not None]
      plot_vals = [v for v in values if v is not None]
      if plot_ts:
        ax.plot(plot_ts, plot_vals, marker='o', linestyle='-', label=f'P{p}')

    ax.set_title(f'Percentiles for "{name}"')
    ax.set_xlabel('Time')
    ax.set_ylabel('Value')
    ax.grid(True)
    ax.legend()

    # Format the x-axis to be readable
    fig.autofmt_xdate()
    ax.xaxis.set_major_formatter(mdates.DateFormatter('%Y-%m-%d %H:%M:%S'))

    # Sanitize filename
    safe_filename = name.replace('.', '_').replace('/', '_') + '.png'
    plt.savefig(safe_filename)
    plt.close(fig)
    print(f'Saved plot to {safe_filename}')


def main():
  parser = argparse.ArgumentParser(
      description='Interpret UMA histogram data and calculate percentiles.')
  parser.add_argument(
      'input_file',
      help='Path to the UMA histogram data file (e.g., test-uma-out.txt).')
  parser.add_argument(
      '--visualize',
      action='store_true',
      help='Generate and save plots of the percentile data over time.')
  args = parser.parse_args()

  # Data structure to hold info for plotting
  # { 'histogram_name': [(timestamp, {p25: val, p50: val, ...}), ...], ... }
  histogram_data_for_plotting = defaultdict(list)

  try:
    with open(args.input_file, 'r', encoding='utf-8') as f:
      for i, line in enumerate(f):
        try:
          parts = line.strip().split(',', 2)
          if len(parts) != 3:
            continue

          timestamp_str, _, json_str = parts
          data = json.loads(json_str)
          histograms = data.get('result', {}).get('histograms', [])
          if not histograms:
            continue

          for histogram in histograms:
            histogram_name = histogram.get('name', 'UnknownHistogram')

            # Criteria 1: Check if the histogram has data.
            if not histogram.get('count', 0) > 0:
              print(f'Skipping {histogram_name} (no data).')
              continue

            percentiles = calculate_percentiles(histogram)

            print(f'--- Entry {i+1}: {timestamp_str} | {histogram_name} ---')
            print(f'Total Count: {histogram.get("count")}')  # pylint: disable=inconsistent-quotes
            print(f'Sum: {histogram.get("sum")}')  # pylint: disable=inconsistent-quotes
            for p, value in percentiles.items():
              print(f'  P{p}: {value if value is not None else "N/A"}')  # pylint: disable=inconsistent-quotes
            print('')

            # Store data for plotting if the flag is set
            if args.visualize:
              dt_object = datetime.strptime(timestamp_str, '%Y-%m-%d %H:%M:%S')
              histogram_data_for_plotting[histogram_name].append(
                  (dt_object, percentiles))

        except (json.JSONDecodeError, IndexError, ValueError) as e:
          print(f'Skipping malformed line {i+1}: {e}', file=sys.stderr)
          continue

  except FileNotFoundError:
    print(f'Error: Input file not found at {args.input_file}', file=sys.stderr)
    sys.exit(1)

  # Generate plots if the flag was passed
  if args.visualize:
    plot_histogram_data(histogram_data_for_plotting)


if __name__ == '__main__':
  main()
