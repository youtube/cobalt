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
"""Generates a graphical analysis from a smaps JSON output file."""

import argparse
import json
import os
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.ticker as mticker


def create_visualization(json_filepath, output_image_path):
  """Loads the JSON data and generates a multi-plot visualization."""
  with open(json_filepath, 'r', encoding='utf-8') as f:
    data = json.load(f)

  if not data:
    print('Error: JSON file is empty.')
    return

  # --- Data Preparation ---
  timestamps = [
      pd.to_datetime(d['timestamp'], format='%Y%m%d_%H%M%S') for d in data
  ]
  total_pss = [d['total_memory'].get('pss', 0) for d in data]
  total_rss = [d['total_memory'].get('rss', 0) for d in data]

  # Create a DataFrame for all region data over time
  all_regions = {}
  for i, entry in enumerate(data):
    for region in entry['regions']:
      region_name = region['name']
      if region_name not in all_regions:
        all_regions[region_name] = [0] * len(data)
      all_regions[region_name][i] = region['pss']
  regions_df = pd.DataFrame(all_regions, index=timestamps)

  # --- Calculations ---
  # Top 10 Consumers: Based on the final measurement
  top_10_consumers = regions_df.iloc[-1].nlargest(10).index.tolist()

  # Top 10 Growers: Based on growth from start to finish
  growth = regions_df.iloc[-1] - regions_df.iloc[0]
  top_10_growers = growth.nlargest(10).index.tolist()

  # --- Plotting ---
  fig, (ax1, ax2, ax3) = plt.subplots(3, 1, figsize=(15, 18))
  fig.suptitle('Smaps Memory Analysis', fontsize=16)

  # Plot 1: Total Memory Usage
  ax1.plot(timestamps, total_pss, label='Total PSS', color='blue')
  ax1.plot(
      timestamps, total_rss, label='Total RSS', color='red', linestyle='--')
  ax1.set_title('Total Memory Usage Over Time')
  ax1.set_ylabel('Memory (kB)')
  ax1.legend()
  ax1.grid(True)
  ax1.yaxis.set_major_formatter(
      mticker.FuncFormatter(lambda x, _: f'{int(x):,}'))

  # Plot 2: Top 10 PSS Consumers (Stacked Area)
  colors = plt.cm.tab20.colors  # Use a colormap for distinct colors
  ax2.stackplot(
      timestamps,
      regions_df[top_10_consumers].T,
      labels=top_10_consumers,
      alpha=0.8,
      colors=colors[:len(top_10_consumers)])
  ax2.set_title('Top 10 PSS Consumers')
  ax2.set_ylabel('Memory (kB)')
  ax2.legend(loc='upper left')
  ax2.grid(True)
  ax2.yaxis.set_major_formatter(
      mticker.FuncFormatter(lambda x, _: f'{int(x):,}'))

  # Plot 3: Top 10 PSS Growers
  for i, region in enumerate(top_10_growers):
    ax3.plot(
        timestamps,
        regions_df[region],
        label=f'{region} (+{int(growth[region])} kB)',
        color=colors[i % len(colors)])
  ax3.set_title('Top 10 PSS Growers')
  ax3.set_ylabel('Memory (kB)')
  ax3.legend()
  ax3.grid(True)
  ax3.yaxis.set_major_formatter(
      mticker.FuncFormatter(lambda x, _: f'{int(x):,}'))

  # Final adjustments
  plt.xlabel('Time')
  plt.xticks(rotation=45)
  plt.tight_layout(rect=[0, 0, 1, 0.96])  # Adjust for suptitle
  plt.savefig(output_image_path)
  print(f'Graph saved to {output_image_path}')


def main():
  """Main entry point."""
  parser = argparse.ArgumentParser(
      description='Generate a visual graph from smaps analysis JSON data.')
  parser.add_argument(
      'json_filepath',
      type=str,
      help='Path to the input JSON file from analyze_smaps_logs.py.')
  parser.add_argument(
      '--output_image',
      type=str,
      default='smaps_analysis.png',
      help='Path to save the output PNG image file.')
  args = parser.parse_args()

  if not os.path.exists(args.json_filepath):
    print(f"Error: File not found at '{args.json_filepath}'")
    return

  create_visualization(args.json_filepath, args.output_image)


if __name__ == '__main__':
  main()
