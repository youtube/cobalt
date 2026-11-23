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
import re
import sys
from collections import defaultdict
from datetime import datetime

# CDP reports the upper bound of the overflow bucket as INT_MAX.
OVERFLOW_BUCKET_HIGH = 2**31 - 1
# Poll header line written by pull_uma_histogram_set_via_cdp.py.
POLL_HEADER = re.compile(r'^# poll (\d+) (\S+ \S+) (.*)$')


def _value_in_bucket(bucket, fraction):
  """Returns the value at `fraction` (0-1] of the way through a bucket."""
  low, high = bucket.get('low', 0), bucket.get('high', 0)
  if high >= OVERFLOW_BUCKET_HIGH:
    return f'>= {low}'
  if high - low <= 1:
    return low
  return low + fraction * (high - low)


def calculate_percentiles(histogram, percentiles=(25, 50, 75, 95, 99)):
  """Calculates specified percentiles from histogram bucket data.

  A percentile is interpolated linearly inside the [low, high) bucket that
  holds it. Width-1 buckets (enums, booleans, small counts) give their exact
  value `low`, and the overflow bucket gives the string '>= low'. With one
  sample, every percentile is that sample's exact value, the sum.

  Args:
    histogram: A dictionary representing the histogram data, containing 'count'
      and 'buckets'.
    percentiles: A tuple of percentiles to calculate (0-100).

  Returns:
    A dictionary mapping each percentile to its calculated value.
  """
  buckets = sorted((b for b in histogram.get('buckets', []) if b.get('count')),
                   key=lambda b: b.get('low', 0))
  total_count = sum(b['count'] for b in buckets)
  if not histogram.get('count') or not total_count:
    return {p: None for p in percentiles}
  if histogram['count'] == 1 and 'sum' in histogram:
    return {p: histogram['sum'] for p in percentiles}

  percentile_values = {}
  for p in percentiles:
    percentile_rank = total_count * (p / 100.0)
    cumulative_count = 0
    for bucket in buckets:
      if cumulative_count + bucket['count'] >= percentile_rank:
        percentile_values[p] = _value_in_bucket(
            bucket, (percentile_rank - cumulative_count) / bucket['count'])
        break
      cumulative_count += bucket['count']
  return percentile_values


def value_counts(histogram):
  """Returns {value: count} if every non-empty bucket has width 1."""
  buckets = [b for b in histogram.get('buckets', []) if b.get('count')]
  if not buckets or any(
      b.get('high', 0) - b.get('low', 0) != 1 for b in buckets):
    return None
  return {b['low']: b['count'] for b in sorted(buckets, key=lambda b: b['low'])}


def _format(value):
  if isinstance(value, float):
    return f'{value:.1f}'
  return 'N/A' if value is None else str(value)


def print_histogram(label, histogram):
  """Prints one histogram and returns its percentiles."""
  count = histogram.get('count', 0)
  total = histogram.get('sum', 0)
  percentiles = calculate_percentiles(histogram)
  print(f'--- {label} ---')
  print(f'Total Count: {count}')
  print(f'Sum: {total}')
  print(f'Mean: {_format(total / count)}')
  if count == 1:
    print(f'  Value: {total} (exact, n = 1)')
  else:
    for value, value_count in (value_counts(histogram) or {}).items():
      print(f'  Value {value}: {value_count}')
    for p, value in percentiles.items():
      print(f'  P{p}: {_format(value)}')
  print('')
  return percentiles


def plot_histogram_data(histogram_data):
  """Generates and saves plots for each histogram's percentiles over time.

  Args:
    histogram_data: A dictionary where keys are histogram names and values are
      lists of (timestamp, percentile_dict) tuples.
  """
  try:
    import matplotlib.pyplot as plt  # pylint: disable=import-outside-toplevel
    import matplotlib.dates as mdates  # pylint: disable=import-outside-toplevel
  except ImportError:
    print(
        'Error: --visualize needs matplotlib. Install it with: '
        'pip install matplotlib',
        file=sys.stderr)
    sys.exit(1)

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
      # Plots numbers only, not None or the '>= low' of the overflow bucket.
      points = [(ts, v)
                for ts, v in zip(timestamps, values)
                if isinstance(v, (int, float))]
      if points:
        ax.plot(*zip(*points), marker='o', linestyle='-', label=f'P{p}')

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
  poll_index = 0
  has_poll_headers = False
  poll_queries = set()
  poll_histograms = set()

  try:
    with open(args.input_file, 'r', encoding='utf-8') as f:
      for i, line in enumerate(f):
        try:
          header = POLL_HEADER.match(line.strip())
          if header:
            has_poll_headers = True
            poll_index = int(header.group(1))
            poll_queries.clear()
            poll_histograms.clear()
            print(f'=== Poll {poll_index} | {header.group(2)} | '
                  f'{header.group(3)} ===\n')
            continue

          parts = line.strip().split(',', 2)
          if len(parts) != 3:
            continue

          timestamp_str, query, json_str = parts
          data = json.loads(json_str)
          # Files without poll headers (from older versions of the puller):
          # a repeated query starts the next poll.
          if not has_poll_headers and (not poll_index or query in poll_queries):
            poll_index += 1
            poll_queries.clear()
            poll_histograms.clear()
            print(f'=== Poll {poll_index} | {timestamp_str} ===\n')
          poll_queries.add(query)
          label = f'Poll {poll_index} | {timestamp_str}'

          histograms = data.get('result', {}).get('histograms', [])
          if not histograms:
            print(f'--- {label} | {query}: no data ---\n')
            continue

          for histogram in histograms:
            histogram_name = histogram.get('name', 'UnknownHistogram')
            # Older files can report a histogram under several queries.
            if histogram_name in poll_histograms:
              continue
            poll_histograms.add(histogram_name)

            # Criteria 1: Check if the histogram has data.
            if not histogram.get('count', 0) > 0:
              print(f'Skipping {histogram_name} (no data).')
              continue

            percentiles = print_histogram(f'{label} | {histogram_name}',
                                          histogram)

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
