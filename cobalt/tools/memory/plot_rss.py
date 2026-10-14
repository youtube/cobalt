#!/usr/bin/env python3
"""Plots and compares the average RSS memory usage from two CSV files."""
#
# plot_rss.py - Plots and compares the average RSS memory usage from two
#               multi-run CSV files (benchmark vs. experiment).
#
# Usage: python3 plot_rss.py <benchmark_file.csv> <experiment_file.csv>
# [output_file.png]
#

import sys
import pandas as pd
import matplotlib.pyplot as plt


def calculate_average(input_file):
  """Reads a multi-run CSV file and calculates the average RSS."""
  try:
    df = pd.read_csv(input_file)
  except FileNotFoundError:
    print(f"Error: Input file not found at '{input_file}'")
    return None

  if not all(c in df.columns for c in ['run', 'elapsed_seconds', 'rss_kb']):
    print(f"Error: CSV file '{input_file}' must contain 'run', "
          "'elapsed_seconds', and 'rss_kb' columns.")
    return None

  return df.groupby('elapsed_seconds')['rss_kb'].mean().reset_index()


def plot_comparison(benchmark_file, experiment_file, output_file):
  """Generates a plot comparing the average RSS of two data files."""
  benchmark_avg = calculate_average(benchmark_file)
  experiment_avg = calculate_average(experiment_file)

  if benchmark_avg is None or experiment_avg is None:
    sys.exit(1)  # Error messages are printed in calculate_average

  # --- Plotting ---
  plt.style.use('seaborn-v0_8-whitegrid')
  _, ax = plt.subplots(figsize=(12, 7))

  # Plot Benchmark Average
  ax.plot(
      benchmark_avg['elapsed_seconds'],
      benchmark_avg['rss_kb'],
      color='blue',
      linewidth=2,
      linestyle='-',
      marker='.',
      markersize=4,
      label='Benchmark Average')

  # Plot Experiment Average
  ax.plot(
      experiment_avg['elapsed_seconds'],
      experiment_avg['rss_kb'],
      color='red',
      linewidth=2,
      linestyle='--',
      marker='x',
      markersize=4,
      label='Experiment Average')

  # --- Formatting ---
  ax.set_title(
      'Benchmark vs. Experiment: Average RSS Memory Usage', fontsize=16)
  ax.set_xlabel('Elapsed Time (seconds)', fontsize=12)
  ax.set_ylabel('Average RSS (KB)', fontsize=12)
  ax.legend()
  ax.grid(True, which='both', linestyle='--', linewidth=0.5)

  plt.tight_layout()
  plt.savefig(output_file)
  print(f"Comparison plot saved to '{output_file}'")


if __name__ == '__main__':
  if len(sys.argv) < 3:
    print('Usage: python3 plot_rss.py <benchmark_file.csv> '
          '<experiment_file.csv> [output_file.png]')
    sys.exit(1)

  benchmark_csv = sys.argv[1]
  experiment_csv = sys.argv[2]
  output_png = sys.argv[3] if len(sys.argv) > 3 else 'rss_comparison_plot.png'
  plot_comparison(benchmark_csv, experiment_csv, output_png)
