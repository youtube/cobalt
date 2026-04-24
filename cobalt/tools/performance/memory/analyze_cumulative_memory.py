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
"""Tool for analyzing cumulative memory usage across reports."""

import os
import re
import statistics

metrics_to_track = [
    'Memory.Experimental.Browser2.Malloc', 'Memory.Browser.LibChrobaltRss',
    'Memory.GPU.PeakMemoryUsage2.PageLoad',
    'Memory.Experimental.Browser2.PartitionAlloc',
    'Memory.Experimental.Browser2.V8', 'Memory.Experimental.Browser2.JavaHeap',
    'Memory.Experimental.Browser2.BlinkGC', 'Memory.Experimental.Browser2.Skia'
]


def analyze_reports():
  """Analyzes memory reports in the histogram_reports directory."""
  report_dir = 'histogram_reports'
  if not os.path.exists(report_dir):
    print(f'Error: {report_dir} directory not found.')
    return

  files = sorted([f for f in os.listdir(report_dir) if f.startswith('report_')])

  # Dictionary to store percentage lists for each metric
  metric_percentages = {m: [] for m in metrics_to_track}
  total_accounted_percentages = []

  for filename in files:
    with open(os.path.join(report_dir, filename), 'r', encoding='utf-8') as f:
      content = f.read()

      # Find RSS
      rss_match = re.search(r'Memory.Browser.ResidentSet\s+\|\s+([\d.]+)',
                            content)
      if not rss_match:
        continue
      rss = float(rss_match.group(1))

      # Find specific metrics and calculate % of RSS
      current_snapshot_total_pct = 0
      for m in metrics_to_track:
        match = re.search(re.escape(m) + r'\s+\|\s+([\d.]+)', content)
        if match:
          val = float(match.group(1))
          pct = (val / rss) * 100
          metric_percentages[m].append(pct)
          current_snapshot_total_pct += pct
        else:
          metric_percentages[m].append(0.0)

      total_accounted_percentages.append(current_snapshot_total_pct)

  if not total_accounted_percentages:
    print('No reports found to analyze.')
    return

  print(f'Analysis of {len(files)} snapshots:')
  print('-' * 65)
  header_name = 'Histogram Name'
  header_avg = 'Avg % of RSS'
  print(f'{header_name:<45} | {header_avg:>15}')
  print('-' * 65)

  for m in metrics_to_track:
    avg_pct = statistics.mean(metric_percentages[m])
    print(f'{m:<45} | {avg_pct:>14.2f}%')

  print('-' * 65)
  avg_total = statistics.mean(total_accounted_percentages)
  footer_label = 'TOTAL ACCOUNTED (AVERAGE)'
  print(f'{footer_label:<45} | {avg_total:>14.2f}%')


if __name__ == '__main__':
  analyze_reports()
