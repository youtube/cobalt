# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
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
import re
import subprocess
import threading
import time
from typing import List, Dict, Tuple, Union, Optional

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

# --- Global Configuration Defaults (can be overridden by command-line args) ---
_DEFAULT_COBALT_PACKAGE_NAME = 'dev.cobalt.coat'
_DEFAULT_COBALT_ACTIVITY_NAME = 'dev.cobalt.app.MainActivity'
_DEFAULT_COBALT_URL = 'https://youtube.com/tv/watch?v=1La4QzGeaaQ'
# Default polling interval now 100 milliseconds.
_DEFAULT_POLL_INTERVAL_MILLISECONDS = 100
_DEFAULT_OUTPUT_DIRECTORY = 'cobalt_monitoring_data'
# --- End Global Configuration Defaults ---

# Global variables for shared state.
g_all_monitoring_data = []
g_script_start_time_for_filename = ''
# Event to signal threads to stop.
g_stop_event = threading.Event()


def _run_adb_command(command_list_or_str: Union[List[str], str],
                     shell: bool = False,
                     timeout: int = 30) -> Tuple[str, Optional[str]]:
  """Helper function to run ADB commands.

  Args:
    command_list_or_str: A list of command arguments or a single string
                         command.
    shell: If True, the command is executed through the shell.
    timeout: Maximum time in milliseconds to wait for the command to complete.

  Returns:
    A tuple (stdout, stderr_msg). stdout is a string.
    stderr_msg is None on success, or an error message string on
    failure/timeout.
  """
  try:
    if shell and isinstance(command_list_or_str, list):
      command_to_run = ' '.join(command_list_or_str)
    else:  # Handles str for shell=True, and list for shell=False
      command_to_run = command_list_or_str
    with subprocess.Popen(
        command_to_run,
        shell=shell,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        errors='replace',
        encoding='utf-8',
    ) as process:
      stdout, stderr = process.communicate(timeout=timeout)

      if process.returncode != 0:
        cmd_str_repr = (
            command_to_run if isinstance(command_to_run, str) else
            ' '.join(command_list_or_str))
        # Expected failures for specific commands
        # (e.g., pidof not finding process)
        is_expected_pidof_fail = ('pidof' in cmd_str_repr and
                                  process.returncode == 1)
        is_expected_meminfo_fail = ('dumpsys meminfo' in cmd_str_repr and
                                    ('No process found' in (stderr or '') or
                                     'No services match' in (stderr or '')))
        is_expected_sf_fail = ('dumpsys SurfaceFlinger' in cmd_str_repr and
                               stderr and
                               'service not found' in (stderr or '').lower())

        if not (is_expected_pidof_fail or is_expected_meminfo_fail or
                is_expected_sf_fail) and stderr:
          print(f'Stderr for \'{cmd_str_repr}\': {stderr.strip()}')
        return (
            stdout.strip() if stdout else '',
            stderr.strip() if stderr else
            f'Command failed with return code {process.returncode}',
        )
      return stdout.strip() if stdout else '', None
  except subprocess.TimeoutExpired:
    cmd_str_repr = (
        command_list_or_str if isinstance(command_list_or_str, str) else
        ' '.join(command_list_or_str))
    print(f'Command timed out: {cmd_str_repr}')
    if ('process' in locals() and hasattr(process, 'poll') and
        process.poll() is None):  # Line 65
      process.kill()
      process.communicate()
    return None, 'Command timed out'
  except Exception as e:  # pylint: disable=broad-except
    # Catching broad exception for robustness against unexpected adb issues.
    cmd_str_repr = (
        command_list_or_str if isinstance(command_list_or_str, str) else
        ' '.join(command_list_or_str))
    print(f'Exception running command {cmd_str_repr}: {e}')
    return None, str(e)


def _is_package_running(package_name: str) -> bool:
  """Checks if a given Android package is currently running.

  Args:
    package_name: The name of the Android package to check.

  Returns:
    True if the package is running (pid found), False otherwise.
  """
  stdout, _ = _run_adb_command(['adb', 'shell', 'pidof', package_name])
  return bool(stdout and stdout.strip().isdigit())


def _stop_package(package_name: str) -> bool:
  """Attempts to force-stop an Android package.

  Args:
    package_name: The name of the Android package to stop.

  Returns:
    True if the stop command was issued successfully, False otherwise.
  """
  print(f'Attempting to stop package \'{package_name}\'...')
  _, stderr = _run_adb_command(
      ['adb', 'shell', 'am', 'force-stop', package_name])
  if stderr:
    print(f'Error stopping package \'{package_name}\': {stderr}')
    return False
  print(f'Package \'{package_name}\' stop command issued.')
  time.sleep(2)  # Give time for the stop to propagate.
  return True


def _launch_cobalt(package_name: str, activity_name: str, url: str):
  """Launches the Cobalt application with a specified URL.

  Args:
    package_name: The package name of Cobalt.
    activity_name: The main activity name of Cobalt.
    url: The URL to pass to Cobalt as a command-line argument.
  """
  print(f'Attempting to launch Cobalt ({package_name}) with URL: {url}...')
  command_str = (f'adb shell am start -n {package_name}/{activity_name} '
                 f'--esa commandLineArgs \'--url="{url}"\'')  # Line 90
  stdout, stderr = _run_adb_command(command_str, shell=True)
  if stderr:
    print(f'Error launching Cobalt: {stderr}')
  else:
    print('Cobalt launch command sent.' +
          (f' Launch stdout: {stdout}' if stdout else ''))


def _get_meminfo_data_internal(
    package_name: str) -> Optional[Dict[str, Dict[str, Union[int, str]]]]:
  """Fetches and parses dumpsys meminfo for a given package.

  Args:
    package_name: The name of the Android package to query.

  Returns:
    A dictionary where keys are memory categories (e.g., 'TOTAL', 'Native Heap')
    and values are dictionaries of memory metrics (e.g., 'Pss Total',
    'Private Dirty'). Returns None if data cannot be parsed or command fails.
  """
  command = ['adb', 'shell', 'dumpsys', 'meminfo', package_name]
  output, error_msg = _run_adb_command(command)
  if error_msg or not output:
    return None

  mem_data = {}
  lines = output.splitlines()
  in_app_summary_section = False
  in_main_table = False
  column_headers = []

  for i, line in enumerate(lines):
    line_stripped = line.strip()

    # Identify the start of the app's meminfo summary.
    if (not in_app_summary_section and
        '** MEMINFO in pid' in line and  # Line 125
        package_name in line):
      in_app_summary_section = True
      continue
    if not in_app_summary_section:
      continue

    # Identify the start of the main memory table by the "------" separator.
    if (not in_main_table and line_stripped.startswith('------') and
        '------' in line_stripped[7:]):
      in_main_table = True
      # Attempt to parse headers from lines above the separator.
      if i >= 2:
        h1_parts = lines[i - 2].strip().split()
        h2_parts = lines[i - 1].strip().split()
        temp_headers = []
        try:
          if len(h1_parts) >= 8 and len(h2_parts) >= 8:
            # Combine multi-word headers if present.
            for k_idx in range(8):
              temp_headers.append(f'{h1_parts[k_idx]} {h2_parts[k_idx]}')
            column_headers = temp_headers
          else:
            raise IndexError  # Fallback to default if parsing fails.
        except IndexError:
          # Fallback to common headers if dynamic parsing fails.
          column_headers = [
              'Pss Total',
              'Private Dirty',
              'Private Clean',
              'SwapPss Dirty',
              'Rss Total',
              'Heap Size',
              'Heap Alloc',
              'Heap Free',
          ]
      else:
        # Default headers if there aren't enough preceding lines.
        column_headers = [
            'Pss Total',
            'Private Dirty',
            'Private Clean',
            'SwapPss Dirty',
            'Rss Total',
            'Heap Size',
            'Heap Alloc',
            'Heap Free',
        ]
      continue

    if not in_main_table or not column_headers:
      continue

    # Stop parsing when irrelevant sections are reached.
    if (line_stripped.startswith(
        ('Objects', 'SQL', 'DATABASES', 'AssetAllocations')) or
        not line_stripped):  # Line 163
      in_main_table = False
      break

    parts = line_stripped.split()
    if not parts:
      continue

    # Find the index where numeric values start (category name ends).
    value_start_idx = -1
    for k_idx, part in enumerate(parts):
      try:
        int(part)
        value_start_idx = k_idx
        break
      except ValueError:
        continue

    if value_start_idx > 0:
      category_name = ' '.join(parts[:value_start_idx]).strip(':')
      if not category_name:
        continue

      value_strings = parts[value_start_idx:]
      category_values = {}
      for k_h, header_name in enumerate(column_headers):
        if k_h < len(value_strings):
          try:
            # Convert values to int, if not possible, store as string.
            category_values[header_name] = int(value_strings[k_h])
          except ValueError:
            category_values[header_name] = value_strings[k_h]
        else:
          category_values[header_name] = 'N/A'  # Missing values.
      if category_values:
        mem_data[category_name] = category_values
  return mem_data if mem_data else None


def _get_surface_flinger_gba_kb() -> Dict[str, Optional[float]]:
  """Fetches GraphicBufferAllocator (GBA) usage from SurfaceFlinger dumpsys.

  Returns:
    A dictionary containing 'GraphicBufferAllocator KB' as a float or None.
  """
  stats = {'GraphicBufferAllocator KB': None}
  stdout, stderr_msg = _run_adb_command(
      ['adb', 'shell', 'dumpsys', 'SurfaceFlinger'])
  if stdout is None:
    return stats
  if not stdout and stderr_msg:
    return stats

  gba_kb_match = re.search(
      (r'Total allocated by GraphicBufferAllocator \(estimate\):\s*'
       r'(\d+\.?\d*)\s*KB'),
      stdout,
      re.I,
  )
  if gba_kb_match:
    try:
      stats['GraphicBufferAllocator KB'] = float(gba_kb_match.group(1))
    except ValueError:
      print(f'Warning: Could not parse GBA KB value: {gba_kb_match.group(1)}')
  return stats


def _data_polling_loop(package_name: str, poll_interval: int):
  """Continuously polls and collects memory and GBA data.

  This function runs in a separate thread and collects data at regular
  intervals until signaled to stop.

  Args:
    package_name: The Android package name to monitor.
    poll_interval: The polling interval in milliseconds.
  """
  print('Starting data polling thread (meminfo and GraphicBufferAllocator)...')
  iteration = 0
  while not g_stop_event.is_set():
    iteration += 1
    current_time_str = time.strftime('%Y-%m-%d %H:%M:%S', time.localtime())
    header_prefix = f'[{current_time_str}][Poll Iter: {iteration}]'
    current_snapshot_data = {'timestamp': current_time_str}
    app_is_running = _is_package_running(package_name)

    meminfo_total_pss_str = 'N/A'
    if app_is_running:
      raw_mem_data = _get_meminfo_data_internal(package_name)
      if raw_mem_data:
        total_pss_val = raw_mem_data.get('TOTAL', {}).get('Pss Total')
        if total_pss_val is not None:
          meminfo_total_pss_str = str(total_pss_val)
        # Add individual category Pss Total values to snapshot.
        for category, data_map in raw_mem_data.items():
          pss_total = data_map.get('Pss Total')
          if pss_total is not None:
            current_snapshot_data[f'{category} Pss Total'] = pss_total
      elif app_is_running:
        print(
            f'{header_prefix} Warning: No meminfo data parsed for '
            f'{package_name} this cycle.',
            end=' | ',
        )

    sf_stats = _get_surface_flinger_gba_kb()
    current_snapshot_data.update(sf_stats)
    gba_kb_val = sf_stats.get('GraphicBufferAllocator KB')
    gba_kb_str = f'{gba_kb_val:.1f} KB' if gba_kb_val is not None else 'N/A'

    # Print status messages based on app running state.
    if app_is_running or iteration == 1:
      # Print if app running or first iteration (to show initial GBA).
      print(f'{header_prefix} Mem(TOTAL PSS): {meminfo_total_pss_str} KB | SF -'
            f' GBA: {gba_kb_str}')
    elif not app_is_running and (iteration % 5 == 0):
      # Print occasionally if app not running to show it's still active.
      print(f'{header_prefix} App {package_name} not running. SF - GBA:'
            f' {gba_kb_str}')

    g_all_monitoring_data.append(current_snapshot_data)

    # Accurate sleep considering processing time.
    def _get_time_ms():
      return time.monotonic_ns() / 1000

    start_sleep_time = _get_time_ms()
    while _get_time_ms() - start_sleep_time < poll_interval:
      if g_stop_event.is_set():
        break
      # Check stop event frequently during poll interval.
      # This enables checks ~2 times before the interval should pass.
      time.sleep(1 / poll_interval / 2)
    if g_stop_event.is_set():
      break

  print('Data polling thread stopped.')


def _write_monitoring_data_csv(output_dir: str, filename_base: str) -> bool:
  """Writes collected monitoring data to a CSV file.

  Args:
    output_dir: The directory where the CSV file will be saved.
    filename_base: The base name for the CSV file (e.g., 'monitoring_data_').

  Returns:
    True if the data was successfully written, False otherwise.
  """
  if not g_all_monitoring_data:
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
  for row_data in g_all_monitoring_data:
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
      writer.writerows(g_all_monitoring_data)
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


def main():
  """Main function to parse arguments, run monitoring, and output results."""
  parser = argparse.ArgumentParser(
      description=(
          'Monitor Cobalt application (meminfo, GraphicBufferAllocator) and'
          ' output data.'),
      formatter_class=argparse.ArgumentDefaultsHelpFormatter,
  )
  parser.add_argument(
      '--package',
      default=_DEFAULT_COBALT_PACKAGE_NAME,
      help='Target package name.',
  )
  parser.add_argument(
      '--activity',
      default=_DEFAULT_COBALT_ACTIVITY_NAME,
      help='Target activity name.',
  )
  parser.add_argument(
      '--url', default=_DEFAULT_COBALT_URL, help='URL to launch on Cobalt.')
  parser.add_argument(
      '--output',
      choices=['csv', 'plot', 'both'],
      default='both',
      help='Output format(s).',
  )
  parser.add_argument(
      '--interval',
      type=int,
      default=_DEFAULT_POLL_INTERVAL_MILLISECONDS,
      help='Polling interval (ms).',
  )
  parser.add_argument(
      '--outdir',
      type=str,
      default=_DEFAULT_OUTPUT_DIRECTORY,
      help='Output directory.',
  )
  args = parser.parse_args()

  global g_script_start_time_for_filename
  g_script_start_time_for_filename = time.strftime('%Y%m%d_%H%M%S',
                                                   time.localtime())
  output_directory = args.outdir
  poll_interval_milliseconds = args.interval

  print('--- Configuration ---')
  print(f'Package: {args.package}')
  print(f'Activity: {args.activity}')
  print(f'URL: {args.url}')
  print(f'Interval: {poll_interval_milliseconds}ms')
  print(f'Output Dir: {output_directory}')
  print(f'Output Formats: {args.output}')
  print('---------------------\n')

  if args.output in ['plot', 'both'] and not _MATPLOTLIB_AVAILABLE:
    print('CRITICAL ERROR: Plotting requested but pandas/matplotlib are not'
          ' installed. Exiting.')
    return

  if not os.path.exists(output_directory):
    try:
      os.makedirs(output_directory)
      print(f'Created output directory: {output_directory}')
    except OSError as e:
      print(f'Error creating output directory {output_directory}: {e}.')
      output_directory = '.'  # Fallback to current directory.

  print('Checking ADB connection...')
  stdout_dev, stderr_dev = _run_adb_command(['adb', 'devices'])
  if stderr_dev or not stdout_dev \
      or 'List of devices attached' not in stdout_dev:  # pylint: disable=g-backslash-continuation
    print('ADB not working. Exiting.')
    return
  if len(stdout_dev.strip().splitlines()) <= 1:
    print('No devices/emulators authorized. Exiting.')
    return

  if _is_package_running(args.package):
    print(f'\'{args.package}\' running. Stopping it first.')
    _stop_package(args.package)

  _launch_cobalt(args.package, args.activity, args.url)

  print(f'\nStarting data polling immediately for {args.package}...')

  if not _is_package_running(args.package):
    print(f'Warning: {args.package} did not seem to start immediately. Polling'
          ' will still attempt to get data.')

  polling_thread = threading.Thread(
      target=_data_polling_loop,
      args=(args.package, poll_interval_milliseconds),
      daemon=True,
  )
  polling_thread.start()

  print(f'\nPolling every {poll_interval_milliseconds}ms. Output in'
        f' \'{output_directory}\'. Ctrl+C to stop & save.')

  try:
    while polling_thread.is_alive():
      time.sleep(1)
  except KeyboardInterrupt:
    print('\nCtrl+C received. Signaling polling thread to stop...')
  except Exception as e:  # pylint: disable=broad-except
    print(f'Main loop encountered an error: {e}')
  finally:
    g_stop_event.set()

  print('Waiting for data polling thread to complete...')
  if polling_thread.is_alive():
    polling_thread.join(timeout=poll_interval_milliseconds + 10 * 1000)
  print('\nData polling stopped.')

  filename_base = f'monitoring_data_{g_script_start_time_for_filename}'
  output_filename_base_with_dir = os.path.join(output_directory, filename_base)

  if args.output in ('csv', 'both'):
    _write_monitoring_data_csv(output_directory, filename_base)
  if args.output in ('plot', 'both'):
    if not _MATPLOTLIB_AVAILABLE:
      print('Skipping plot: Plotting libraries not available.')
    elif not g_all_monitoring_data:
      print('No data collected, skipping plot generation.')
    else:
      df_for_plot = pd.DataFrame(g_all_monitoring_data)
      if not df_for_plot.empty:
        _plot_monitoring_data_from_df(df_for_plot,
                                      output_filename_base_with_dir)
      else:
        print('DataFrame for plotting is empty (no data rows), skipping plot'
              ' generation.')

  print('Script finished.')


if __name__ == '__main__':
  main()
