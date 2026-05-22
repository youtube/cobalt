#!/usr/bin/env python3
#
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
"""Tool for comparing UMA memory histograms with Smaps ground truth."""

import argparse
import json
import os
import statistics
import subprocess
import sys
import tempfile
from datetime import datetime
from typing import Any, Dict, List, Optional, Tuple

# Mapping of UMA histograms to Smaps categories
# Key: UMA Histogram Name
# Value: (Smaps Category Key, Is Experimental Override)
UMA_MAP: Dict[str, Tuple[str, bool]] = {
    'Memory.Browser.ResidentSet': ('total', False),
    'Memory.Experimental.Browser2.PartitionAlloc': ('partition_alloc', True),
    'Memory.Experimental.Browser2.Malloc': ('malloc', True),
    'Memory.Experimental.Browser2.V8': ('v8', True),
    'Memory.Experimental.Browser2.CodeOther': ('code_other', True),
    'Memory.Experimental.Browser2.Fonts': ('fonts', True),
    'Memory.Experimental.Browser2.AshmemJit': ('ashmem_jit', True),
    'Memory.Experimental.Browser2.AndroidRuntime': ('android_runtime', True),
    'Memory.Experimental.Browser2.Stacks': ('stacks', True),
    'Memory.Browser.PartitionAllocRss': ('partition_alloc', False),
    'Memory.Browser.MallocRss': ('malloc', False),
    'Memory.Browser.LibChrobaltRss': ('libchrobalt', False),
}


def parse_uma_timestamp(ts_str: str) -> datetime:
  """Parses UMA timestamp format: 2026-03-25 02:55:45"""
  return datetime.strptime(ts_str, '%Y-%m-%d %H:%M:%S')


def parse_smaps_timestamp(ts_str: str) -> datetime:
  """Parses Smaps timestamp format: 20260325_025545"""
  return datetime.strptime(ts_str, '%Y%m%d_%H%M%S')


def get_uma_data(uma_log: str) -> List[Dict[str, Any]]:
  """Parses UMA log and returns a list of snapshots with delta values."""
  history: List[Dict[str, Any]] = []
  if not os.path.exists(uma_log):
    return history

  # Tracks (last_sum, last_count) for each histogram name to calculate deltas
  prev_metrics: Dict[str, Tuple[float, float]] = {}

  with open(uma_log, 'r', encoding='utf-8') as f:
    for line in f:
      parts = line.strip().split(',', 2)
      if len(parts) < 3:
        continue

      try:
        ts = parse_uma_timestamp(parts[0])
        payload = json.loads(parts[2])
        histograms = payload.get('result', {}).get('histograms', [])

        snapshot: Dict[str, Any] = {'timestamp': ts, 'metrics': {}}
        for h in histograms:
          name = h['name']
          if name in UMA_MAP:
            curr_sum = h['sum']
            curr_count = h['count']

            if name in prev_metrics:
              last_sum, last_count = prev_metrics[name]
              delta_sum = curr_sum - last_sum
              delta_count = curr_count - last_count
              if delta_count > 0:
                val = delta_sum / delta_count
                snapshot['metrics'][name] = val

            prev_metrics[name] = (curr_sum, curr_count)

        if snapshot['metrics']:
          history.append(snapshot)
      except (ValueError, json.JSONDecodeError):
        continue
  return history


def aggregate_smaps(snapshot: Dict[str, Any]) -> Dict[str, float]:
  """Aggregates Smaps regions into Cobalt categories (RSS in MiB).

  NOTE: The sub-metric categorization heuristics are currently Android-specific.
  """
  res = {
      'total': snapshot['total_memory'].get('rss', 0) / 1024.0,
      'partition_alloc': 0.0,
      'v8': 0.0,
      'malloc': 0.0,
      'libchrobalt': 0.0,
      'code_other': 0.0,
      'fonts': 0.0,
      'ashmem_jit': 0.0,
      'android_runtime': 0.0,
      'stacks': 0.0
  }

  for region in snapshot['regions']:
    name = region['name']
    rss = region['rss']
    if 'libchrobalt.so' in name:
      res['libchrobalt'] += rss
    elif 'partition_alloc' in name:
      res['partition_alloc'] += rss
    elif 'v8' in name:
      res['v8'] += rss
    elif 'scudo' in name or '[heap]' in name:
      res['malloc'] += rss
    elif '.ttf' in name or '.ttc' in name or 'fonts/' in name:
      res['fonts'] += rss
    elif '/dev/ashmem/' in name or 'memfd:jit' in name:
      res['ashmem_jit'] += rss
    elif any(x in name for x in ['art', 'oat', 'vdex', 'odex', 'jar', 'hyb']
            ) or 'dalvik-' in name:
      res['android_runtime'] += rss
    elif 'stack_and_tls' in name or '[stack]' in name:
      res['stacks'] += rss
    elif any(x in name for x in ['.so', '.apk', '.dex']):
      res['code_other'] += rss

  # Convert categories to MiB
  for k in [
      'partition_alloc', 'v8', 'malloc', 'libchrobalt', 'code_other', 'fonts',
      'ashmem_jit', 'android_runtime', 'stacks'
  ]:
    res[k] /= 1024.0

  return res


def run_comparison(uma_history: List[Dict[str, Any]], smaps_json: str) -> None:
  """Aligns UMA and Smaps data and calculates accuracy."""
  with open(smaps_json, 'r', encoding='utf-8') as f:
    smaps_data = json.load(f)

  smaps_history = []
  for s in smaps_data:
    smaps_history.append({
        'timestamp': parse_smaps_timestamp(s['timestamp']),
        'metrics': aggregate_smaps(s)
    })

  matches: List[Tuple[Dict[str, Any], Dict[str, Any]]] = []
  jitter_warnings = 0
  for u in uma_history:
    # Find closest smaps snapshot within 30s
    closest: Optional[Dict[str, Any]] = None
    min_delta = 30.0
    for s in smaps_history:
      delta = abs((u['timestamp'] - s['timestamp']).total_seconds())
      if delta < min_delta:
        min_delta = delta
        closest = s

    if closest:
      matches.append((u, closest))
      if min_delta > 5.0:
        jitter_warnings += 1

  if not matches:
    print('No overlapping data found between UMA and Smaps.')
    return

  print(f'Found {len(matches)} synchronized data points.')
  if jitter_warnings > 0:
    print(f'Warning: {jitter_warnings} matches have a time delta > 5s. '
          'Expect increased Max Error due to jitter.\n')

  header = (f"{'Metric':<60} | {'UMA (MB)':>10} | {'Smaps (MB)':>10} | "
            f"{'Delta':>8} | {'Error %':>8}")
  print(header)
  print('-' * len(header))

  all_errors: Dict[str, List[float]] = {}

  for uma_name, (smaps_key, is_experimental) in UMA_MAP.items():
    uma_vals = []
    smaps_vals = []
    errors = []

    for u, s in matches:
      if uma_name in u['metrics']:
        u_val = u['metrics'][uma_name]
        s_val = s['metrics'][smaps_key]

        uma_vals.append(u_val)
        smaps_vals.append(s_val)
        delta = u_val - s_val
        error_pct = (delta / s_val *
                     100) if s_val > 0 else (100.0 if u_val > 0 else 0.0)
        errors.append(error_pct)

    if uma_vals:
      u_med = statistics.median(uma_vals)
      s_med = statistics.median(smaps_vals)
      d_med = u_med - s_med
      e_med = statistics.median(errors)
      label = uma_name
      if is_experimental:
        label += ' (Accurate Override)'
      print(f'{label[:60]:<60} | {u_med:>10.2f} | {s_med:>10.2f} | '
            f'{d_med:>8.2f} | {e_med:>7.2f}%')
      all_errors[uma_name] = errors

  print('\nSummary of Error Rates (Median):')
  for name, errors in all_errors.items():
    median_err = statistics.median(errors)
    max_err = max(errors, key=abs)
    print(f'  - {name:<60}: {median_err:>6.2f}% (Max: {max_err:>6.2f}%)')


def main() -> None:
  """Main entry point."""
  parser = argparse.ArgumentParser(
      description='Compare UMA accuracy vs Smaps ground truth.')
  parser.add_argument('--uma_log', type=str, default='uma_histos.txt')
  parser.add_argument('--smaps_dir', type=str, default='sample_logs')
  parser.add_argument('--platform', type=str, default='android')
  args = parser.parse_args()

  # Step 1: Run smaps pipeline to get JSON
  print('Step 1: Processing Smaps logs...')

  try:
    with tempfile.TemporaryDirectory() as tmpdir:
      json_path = os.path.join(tmpdir, 'analysis.json')
      processed_dir = os.path.join(tmpdir, 'processed')
      os.makedirs(processed_dir)

      # Step 1.1: read_smaps_batch.py
      cmd_batch = [
          'python3', 'cobalt/tools/performance/smaps/read_smaps_batch.py',
          args.smaps_dir, '-o', processed_dir, '--platform', args.platform
      ]
      if args.platform == 'android':
        cmd_batch.append('-d')
      subprocess.run(cmd_batch, check=True, capture_output=True, text=True)

      # Step 1.2: analyze_smaps_logs.py
      cmd_analyze = [
          'python3', 'cobalt/tools/performance/smaps/analyze_smaps_logs.py',
          processed_dir, '--json_output', json_path
      ]
      subprocess.run(cmd_analyze, check=True, capture_output=True, text=True)

      # Step 2: Parse UMA
      print('Step 2: Parsing UMA logs...')
      uma_history = get_uma_data(args.uma_log)

      # Step 3: Compare
      print('Step 3: Comparing results...\n')
      run_comparison(uma_history, json_path)

  except subprocess.CalledProcessError as e:
    print(
        f'Error: Smaps processing pipeline failed with exit code {e.returncode}'
    )
    print(f'Stdout: {e.stdout}')
    print(f'Stderr: {e.stderr}', file=sys.stderr)
    sys.exit(1)
  except (IOError, ValueError, json.JSONDecodeError) as e:
    print(f'Error parsing data: {e}', file=sys.stderr)
    sys.exit(1)


if __name__ == '__main__':
  main()
