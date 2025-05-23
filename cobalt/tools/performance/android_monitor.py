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
"""Monitors application memory and graphic buffer allocator usage on Android.

This script connects to an Android device via ADB to collect memory
information (dumpsys meminfo) and GraphicBufferAllocator data (dumpsys
SurfaceFlinger) for a specified Cobalt package. It can optionally launch Cobalt
with a given URL.

Collected data is written to a CSV file, and can also be plotted using
matplotlib and pandas if those libraries are available.
"""

import argparse
import csv
import os
import threading
import time
from typing import Optional

from adb_command_runner import run_adb_command
from configuration import AppConfig, OutputConfig, PackageConfig
from device_monitor import DeviceMonitor
from data_collector import DataCollectorBuilder
from data_store import MonitoringDataStore
from time_provider import TimeProvider

# Plotting library imports (conditionally imported)
# _MATPLOTLIB_AVAILABLE is a global indicating if plotting dependencies are met.
_MATPLOTLIB_AVAILABLE = False
try:
  import pandas as pd  # pylint: disable=g-import-not-at-top
  import matplotlib.pyplot as plt  # pylint: disable=g-import-not-at-top
  import matplotlib.dates as mdates  # pylint: disable=g-import-not-at-top
  _MATPLOTLIB_AVAILABLE = True
except ImportError as e:
  # Warning will be printed in main if plotting is attempted without these.
  print(e)


def _write_monitoring_data_csv(output_dir: str, filename_base: str,
                               monitoring_data) -> bool:
  """Writes collected monitoring data to a CSV file.

  Args:
    output_dir: The directory where the CSV file will be saved.
    filename_base: The base name for the CSV file (e.g., 'monitoring_data_').

  Returns:
    True if the data was successfully written, False otherwise.
  """
  if not monitoring_data:
    print('No data collected to write to CSV.')
    return False

  output_filename = os.path.join(output_dir, f'{filename_base}.csv')
  print(f'Writing monitoring data to {output_filename}...')

  # Dynamically build fieldnames to ensure all keys are included and ordered.
  fieldnames_set = set(['timestamp'])
  surfaceflinger_keys_ordered = ['GraphicBufferAllocator KB']
  for key in surfaceflinger_keys_ordered:
    fieldnames_set.add(key)

  meminfo_keys_found = set()
  for row_data in monitoring_data:
    for key in row_data.keys():
      if key not in ['timestamp'] and key not in surfaceflinger_keys_ordered:
        meminfo_keys_found.add(key)
      fieldnames_set.add(key)

  sorted_meminfo_keys = sorted(list(meminfo_keys_found))
  final_fieldnames = ['timestamp']
  # Ensure SurfaceFlinger keys are at the beginning after timestamp.
  for key in surfaceflinger_keys_ordered:
    if key in fieldnames_set:
      final_fieldnames.append(key)
  # Add sorted meminfo keys.
  for mem_key in sorted_meminfo_keys:
    if mem_key not in final_fieldnames:
      final_fieldnames.append(mem_key)
  # Add any remaining keys from the set (should be none if logic is perfect).
  for key_in_set in sorted(list(fieldnames_set)):
    if key_in_set not in final_fieldnames:
      final_fieldnames.append(key_in_set)

  try:
    with open(output_filename, 'w', newline='', encoding='utf-8') as csvfile:
      writer = csv.DictWriter(csvfile, fieldnames=final_fieldnames, restval='')
      writer.writeheader()
      writer.writerows(monitoring_data)
    print(f'Monitoring data successfully written to {output_filename}')
    return True
  except Exception as e:  # pylint: disable=broad-except
    print(f'Error writing CSV: {e}')
    return False


def _plot_monitoring_data_from_df(
    df_input: pd.DataFrame,
    output_filename_base_with_dir: str,
    output_filepath_override: Optional[str] = None,
) -> bool:
  """Generates and saves a plot of monitoring data from a Pandas DataFrame.

  Args:
    df_input: Pandas DataFrame containing the monitoring data.
    output_filename_base_with_dir: Base path for the output plot file.
    output_filepath_override: Optional full path to save the plot, overrides
      the base path.

  Returns:
    True if the plot was successfully saved, False otherwise.
  """
  if not _MATPLOTLIB_AVAILABLE:
    print('Plotting libraries (pandas, matplotlib) not found.')
    return False
  if df_input.empty:
    print('Plotter: Input DataFrame is empty, no plot generated.')
    return False

  df = df_input.copy()
  if 'timestamp' not in df.columns:
    print('Plotter: \'timestamp\' column missing from DataFrame.')
    return False
  try:
    df['timestamp'] = pd.to_datetime(df['timestamp'])
  except Exception as e:  # pylint: disable=broad-except
    print(f'Plotter: Error converting \'timestamp\': {e}')
    return False

  # Forward-fill numeric columns to handle missing values in a stepped plot.
  cols_to_ffill = [
      col for col in df.columns
      if col.endswith(' Pss Total') or col == 'GraphicBufferAllocator KB'
  ]
  for col_name in cols_to_ffill:
    if col_name in df.columns and col_name != 'timestamp':
      df[col_name] = pd.to_numeric(df[col_name], errors='coerce')
      df[col_name] = df[col_name].ffill()
  df = df.sort_values(by='timestamp').reset_index(drop=True)

  print('\n--- Plotter: Generating Plot ---')
  try:
    plt.style.use('seaborn-v0_8-whitegrid')
  except:  # pylint: disable=bare-except
    # Fallback if the specific seaborn style is not available.
    print('Note: \'seaborn-v0_8-whitegrid\' style not available, using default'
          ' Matplotlib style.')

  fig, ax1 = plt.subplots(figsize=(18, 10))

  # Filter and prepare PSS columns for stacking, excluding 'TOTAL' and 'mmap'.
  pss_cols_for_stack = sorted([
      c for c in df.columns if c.endswith(' Pss Total') and
      c != 'TOTAL Pss Total' and 'mmap' not in c.lower()
  ])
  total_pss_col = 'TOTAL Pss Total'
  gba_kb_col = 'GraphicBufferAllocator KB'

  if pss_cols_for_stack:
    stack_labels = [c.replace(' Pss Total', '') for c in pss_cols_for_stack]
    y_stack_data_unfiltered = [
        df[c].fillna(0) for c in pss_cols_for_stack if c in df
    ]
    y_stack_data = []
    valid_labels = []
    for i, series in enumerate(y_stack_data_unfiltered):
      # Only include series that have at least one non-null and non-zero value.
      if (i < len(stack_labels) and series.notnull().any() and
          not (series == 0).all()):
        y_stack_data.append(series.values)
        valid_labels.append(stack_labels[i])

    if y_stack_data:
      try:
        num_colors = len(y_stack_data)
        # Use 'Paired' colormap for distinct colors.
        cmap_stack = plt.get_cmap('Paired', num_colors if num_colors > 0 else 1)
        colors = [cmap_stack(j) for j in range(num_colors)]
      except Exception as e_cmap:  # pylint: disable=broad-except
        print(f'Plotter: Colormap \'Paired\' issue ({e_cmap}), using default'
              ' colors.')
        colors = None
      ax1.stackplot(
          df['timestamp'],
          *y_stack_data,
          labels=valid_labels,
          colors=colors,
          alpha=0.75,
          zorder=1,
      )

  # Plot TOTAL PSS separately.
  if total_pss_col in df.columns and df[total_pss_col].notnull().any():
    ax1.plot(
        df['timestamp'],
        df[total_pss_col],
        label='TOTAL PSS (Stepped)',
        color='black',
        lw=2,
        ls='--',
        zorder=5,
    )
  # Plot GraphicBufferAllocator KB separately.
  if gba_kb_col in df.columns and df[gba_kb_col].notnull().any():
    ax1.plot(
        df['timestamp'],
        df[gba_kb_col],
        label='GraphicBufferAllocator KB (Stepped)',
        color='darkcyan',
        lw=1.5,
        ls=':',
        zorder=6,
        marker='.',
        markersize=4,
        markevery=0.1,
    )

  ax1.set_xlabel('Time', fontsize=12)
  ax1.set_ylabel('Memory Usage (KB)', fontsize=14, color='tab:blue')
  ax1.tick_params(axis='y', labelcolor='tab:blue', labelsize=10)
  ax1.grid(True, linestyle=':', alpha=0.6, axis='y')

  # Set plot title.
  csv_basename = os.path.basename(output_filename_base_with_dir)
  fig.suptitle(
      f'Cobalt Memory Monitoring\n(Source: {csv_basename}.csv)',
      fontsize=18,
      y=0.98,
  )
  ax1.xaxis.set_major_formatter(mdates.DateFormatter('%H:%M:%S'))
  fig.autofmt_xdate(rotation=30, ha='right')

  # Adjust legend position and font size based on number of entries.
  lines, labels = ax1.get_legend_handles_labels()
  legend_fontsize = 8
  legend_bbox_right_shift = 0.82
  if len(labels) > 10:
    legend_fontsize = 7
    legend_bbox_right_shift = 0.78
  if lines:
    ax1.legend(
        lines,
        labels,
        loc='upper left',
        bbox_to_anchor=(1.02, 1),
        borderaxespad=0.0,
        fontsize=legend_fontsize,
    )
    # Adjust layout to make room for the legend.
    fig.tight_layout(rect=[0.03, 0.05, legend_bbox_right_shift, 0.93])
  else:
    fig.tight_layout(rect=[0.03, 0.05, 0.97, 0.93])

  # Determine output path and save plot.
  output_plot_path = (
      output_filepath_override if output_filepath_override else
      output_filename_base_with_dir + '_plot.png')
  try:
    plot_dir = os.path.dirname(output_plot_path)
    if plot_dir and not os.path.exists(plot_dir):
      os.makedirs(plot_dir)
    plt.savefig(output_plot_path, dpi=150, bbox_inches='tight')
    print(f'Plot successfully saved to \'{output_plot_path}\'')
    return True
  except Exception as e:  # pylint: disable=broad-except
    print(f'Error saving plot: {e}')
    return False


def _parse_cli_args() -> AppConfig:
  """Parses command-line arguments and returns an AppConfig object."""
  parser = argparse.ArgumentParser(
      description=(
          'Monitor Cobalt application (meminfo, GraphicBufferAllocator) and'
          ' output data.'),
      formatter_class=argparse.ArgumentDefaultsHelpFormatter,
  )
  parser.add_argument(
      '--package',
      default=PackageConfig.DEFAULT_COBALT_PACKAGE_NAME,
      help='Target package name.',
  )
  parser.add_argument(
      '--activity',
      default=PackageConfig.DEFAULT_COBALT_ACTIVITY_NAME,
      help='Target activity name.',
  )
  parser.add_argument(
      '--url',
      default=AppConfig.DEFAULT_COBALT_URL,
      help='URL to launch on Cobalt.')
  parser.add_argument(
      '--output',
      choices=['csv', 'plot', 'both'],
      default='both',
      help='Output format(s).',
  )
  parser.add_argument(
      '--interval',
      type=int,
      default=OutputConfig.DEFAULT_POLL_INTERVAL_MILLISECONDS,
      help='Polling interval (ms).',
  )
  parser.add_argument(
      '--outdir',
      type=str,
      default=OutputConfig.DEFAULT_OUTPUT_DIRECTORY,
      help='Output directory.',
  )
  parser.add_argument(
      '--flags',
      type=str,
      default='',
      help=f'Cobalt CLI & Experiment flags. {AppConfig.EXPECTED_FLAGS_FORMAT}',
  )
  args = parser.parse_args()

  package_config = PackageConfig(
      package_name=args.package, activity_name=args.activity)
  output_config = OutputConfig(
      output_format=args.output,
      poll_interval_ms=args.interval,
      output_directory=args.outdir)
  config = AppConfig(
      package_config=package_config,
      url=args.url,
      output_config=output_config,
      cobalt_flags=args.flags)

  print('--- Configuration ---')
  print(f'Package: {config.package_name}')
  print(f'Activity: {config.activity_name}')
  print(f'URL: {config.url}')
  print(f'Interval: {config.poll_interval_ms}ms')
  print(f'Output Dir: {config.output_directory}')
  print(f'Output Formats: {config.output_format}')
  print(f'Cobalt Flags: {config.cobalt_flags}')
  print('---------------------\n')

  return config


def main():
  """Main function to parse arguments, run monitoring, and output results."""
  config = _parse_cli_args()
  output_directory = config.output_directory
  poll_interval_milliseconds = config.poll_interval_ms

  data_store = MonitoringDataStore()
  device_monitor =\
    DeviceMonitor(adb_runner=run_adb_command, time_provider=time.sleep)
  time_provider = TimeProvider()
  stop_event = threading.Event()

  if config.output_format in ['plot', 'both'] and not _MATPLOTLIB_AVAILABLE:
    print('CRITICAL ERROR: Plotting requested but pandas/matplotlib '
          'are not installed. Exiting.')
    return

  if not os.path.exists(output_directory):
    try:
      os.makedirs(output_directory)
      print(f'Created output directory: {output_directory}')
    except OSError as e:
      print(f'Error creating output directory {output_directory}: {e}.'
            'Falling back to current directory.')
      output_directory = '.'  # Fallback for directory creation failure

  if not device_monitor.check_adb_connection():
    return

  if device_monitor.is_package_running(config.package_name):
    print(f'\'{config.package_name}\' running. Stopping it first.')
    device_monitor.stop_package(config.package_name)

  parsed_flags = config.parse_cobalt_cli_flags()
  print(f'Attempting to launch Cobalt ({config.package_name}) ' +
        f'with URL: {config.url}...')
  device_monitor.launch_cobalt(config.package_name, config.activity_name,
                               parsed_flags)

  print(f'\nStarting data polling immediately for {config.package_name}...')

  if not device_monitor.is_package_running(config.package_name):
    print(f'Warning: {config.package_name} did not seem to start immediately. '
          'Polling will still attempt to get data.')
  data_collector = DataCollectorBuilder()\
    .with_package_name(config.package_name)\
    .with_poll_interval_ms(config.poll_interval_ms)\
    .with_data_store(data_store)\
    .with_device_monitor(device_monitor)\
    .with_time_provider(time_provider)\
    .with_stop_event(stop_event)\
    .build()

  polling_thread = threading.Thread(
      target=data_collector.run_polling_loop,
      daemon=True,
  )
  polling_thread.start()

  print(f'\nPolling every {poll_interval_milliseconds}ms. Output in'
        f' \'{output_directory}\'. Ctrl+C to stop & save.')

  try:
    while polling_thread.is_alive():
      time_provider.sleep(1)  # Use the time_provider for main loop sleep too
  except KeyboardInterrupt:
    print('\nCtrl+C received. Signaling polling thread to stop...')
  except Exception as e:  # pylint: disable=broad-exception-caught
    print(f'Main loop encountered an error: {e}')
  finally:
    stop_event.set()

  print('Waiting for data polling thread to complete...')
  if polling_thread.is_alive():
    # Give it a bit more time than the polling interval to stop
    polling_thread.join(timeout=(config.poll_interval_ms / 1000) + 10)
  print('\nData polling stopped.')

  time_for_file_name = time.strftime('%Y%m%d_%H%M%S', time.localtime())
  filename_base = f'monitoring_data_{time_for_file_name}'
  output_filename_base_with_dir = os.path.join(output_directory, filename_base)

  all_data = data_store.get_all_data()
  if config.output_format in ('csv', 'both'):
    _write_monitoring_data_csv(output_directory, filename_base, all_data)
  if config.output_format in ('plot', 'both'):
    if not _MATPLOTLIB_AVAILABLE:
      print('Skipping plot: Plotting libraries not available.')
    elif not all_data:
      print('No data collected, skipping plot generation.')
    else:
      df_for_plot = pd.DataFrame(all_data)
      if not df_for_plot.empty:
        _plot_monitoring_data_from_df(df_for_plot,
                                      output_filename_base_with_dir)
      else:
        print('DataFrame for plotting is empty (no data rows), skipping plot'
              ' generation.')

  print('Script finished.')


if __name__ == '__main__':
  main()
