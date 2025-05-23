import subprocess
import re
import time
import threading
import csv
import os
import argparse

# Plotting library imports (conditionally imported)
try:
  import pandas as pd
  import matplotlib.pyplot as plt
  import matplotlib.dates as mdates
  import numpy as np  # Still used for colormap if needed
  MATPLOTLIB_AVAILABLE = True
except ImportError:
  MATPLOTLIB_AVAILABLE = False
  # Warning will be printed in main if plotting is attempted without these

# --- Configuration (defaults, can be overridden by command-line args) ---
DEFAULT_COBALT_PACKAGE_NAME = "dev.cobalt.coat"
DEFAULT_COBALT_ACTIVITY_NAME = "dev.cobalt.app.MainActivity"
DEFAULT_COBALT_URL = "https://youtube.com/tv/watch?v=1La4QzGeaaQ"
DEFAULT_POLL_INTERVAL_SECONDS = 10  # MODIFIED: Default polling interval now 10 seconds
DEFAULT_OUTPUT_DIRECTORY = "cobalt_monitoring_data"
# --- End Configuration ---

all_monitoring_data = []
script_start_time_for_filename = ""
stop_event = threading.Event()  # Event to signal threads to stop


# --- ADB and Data Fetching Functions ---
def run_adb_command(command_list_or_str, shell=False, timeout=30):
  """
    Helper function to run ADB commands.
    Returns a tuple (stdout, stderr_msg). stdout is string.
    stderr_msg is None on success, or error message string on failure/timeout.
    """
  try:
    if shell and isinstance(command_list_or_str, list):
      command_to_run = " ".join(command_list_or_str)
    else:  # Handles str for shell=True, and list for shell=False
      command_to_run = command_list_or_str

    process = subprocess.Popen(
        command_to_run,
        shell=shell,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        errors='replace',
        encoding='utf-8')
    stdout, stderr = process.communicate(timeout=timeout)

    if process.returncode != 0:
      cmd_str_repr = command_to_run if isinstance(
          command_to_run, str) else ' '.join(command_to_run)
      is_expected_pidof_fail = ("pidof" in cmd_str_repr and
                                process.returncode == 1)
      is_expected_meminfo_fail = ("dumpsys meminfo" in cmd_str_repr and
                                  ("No process found" in (stderr or "") or
                                   "No services match" in (stderr or "")))
      is_expected_sf_fail = ("dumpsys SurfaceFlinger" in cmd_str_repr and
                             stderr and
                             "service not found" in (stderr or "").lower())

      if not (is_expected_pidof_fail or is_expected_meminfo_fail or
              is_expected_sf_fail) and stderr:
        print(f"Stderr for '{cmd_str_repr}': {stderr.strip()}")
      return stdout.strip() if stdout else "", stderr.strip(
      ) if stderr else f"Command failed with return code {process.returncode}"
    return stdout.strip() if stdout else "", None
  except subprocess.TimeoutExpired:
    cmd_str_repr = command_list_or_str if isinstance(
        command_list_or_str, str) else ' '.join(command_list_or_str)
    print(f"Command timed out: {cmd_str_repr}")
    if 'process' in locals() and hasattr(process,
                                         'poll') and process.poll() is None:
      process.kill()
      process.communicate()
    return None, "Command timed out"
  except Exception as e:
    cmd_str_repr = command_list_or_str if isinstance(
        command_list_or_str, str) else ' '.join(command_list_or_str)
    print(f"Exception running command {cmd_str_repr}: {e}")
    return None, str(e)


def is_package_running(package_name):
  stdout, _ = run_adb_command(["adb", "shell", "pidof", package_name])
  return bool(stdout and stdout.strip().isdigit())


def stop_package(package_name):
  print(f"Attempting to stop package '{package_name}'...")
  _, stderr = run_adb_command(
      ["adb", "shell", "am", "force-stop", package_name])
  if stderr:
    print(f"Error stopping package '{package_name}': {stderr}")
    return False
  print(f"Package '{package_name}' stop command issued.")
  time.sleep(2)
  return True


def launch_cobalt(package_name, activity_name, url):
  print(f"Attempting to launch Cobalt ({package_name}) with URL: {url}...")
  command_str = (f"adb shell am start -n {package_name}/{activity_name} "
                 f"--esa commandLineArgs '--url=\"{url}\"'")
  stdout, stderr = run_adb_command(command_str, shell=True)
  if stderr:
    print(f"Error launching Cobalt: {stderr}")
  else:
    print("Cobalt launch command sent." +
          (f" Launch stdout: {stdout}" if stdout else ""))


def get_meminfo_data_internal(package_name):
  command = ["adb", "shell", "dumpsys", "meminfo", package_name]
  output, error_msg = run_adb_command(command)
  if error_msg or not output:
    return None

  mem_data = {}
  lines = output.splitlines()
  in_app_summary_section = False
  in_main_table = False
  column_headers = []
  for i, line in enumerate(lines):
    line_stripped = line.strip()
    if not in_app_summary_section and "** MEMINFO in pid" in line and package_name in line:
      in_app_summary_section = True
    if not in_app_summary_section:
      continue
    if not in_main_table and line_stripped.startswith(
        "------") and "------" in line_stripped[7:]:
      in_main_table = True
      if i >= 2:
        h1_parts = lines[i - 2].strip().split()
        h2_parts = lines[i - 1].strip().split()
        temp_headers = []
        try:
          if len(h1_parts) >= 8 and len(h2_parts) >= 8:
            for k_idx in range(8):
              temp_headers.append(f"{h1_parts[k_idx]} {h2_parts[k_idx]}")
            column_headers = temp_headers
          else:
            raise IndexError
        except IndexError:
          column_headers = [
              "Pss Total", "Private Dirty", "Private Clean", "SwapPss Dirty",
              "Rss Total", "Heap Size", "Heap Alloc", "Heap Free"
          ]
      else:
        column_headers = [
            "Pss Total", "Private Dirty", "Private Clean", "SwapPss Dirty",
            "Rss Total", "Heap Size", "Heap Alloc", "Heap Free"
        ]
      continue
    if not in_main_table or not column_headers:
      continue
    if not line_stripped or line_stripped.startswith(
        "Objects") or line_stripped.startswith(
            "SQL") or line_stripped.startswith("DATABASES"):
      in_main_table = False
      break
    parts = line_stripped.split()
    value_start_idx = -1
    if not parts:
      continue
    for k_idx, part in enumerate(parts):
      try:
        int(part)
        value_start_idx = k_idx
        break
      except ValueError:
        continue
    if value_start_idx > 0:
      cat_name = " ".join(parts[:value_start_idx]).strip(":")
      if not cat_name:
        continue
      val_str = parts[value_start_idx:]
      cat_vals = {}
      for k_h, h_name in enumerate(column_headers):
        if k_h < len(val_str):
          try:
            cat_vals[h_name] = int(val_str[k_h])
          except ValueError:
            cat_vals[h_name] = val_str[k_h]
        else:
          cat_vals[h_name] = "N/A"
      if cat_vals:
        mem_data[cat_name] = cat_vals
  return mem_data if mem_data else None


def get_surface_flinger_gba_kb():
  stats = {"GraphicBufferAllocator KB": None}
  stdout, stderr_msg = run_adb_command(
      ["adb", "shell", "dumpsys", "SurfaceFlinger"])
  if stdout is None:
    return stats
  if not stdout and stderr_msg:
    return stats

  gba_kb_m = re.search(
      r"Total allocated by GraphicBufferAllocator \(estimate\):\s*(\d+\.?\d*)\s*KB",
      stdout, re.I)
  if gba_kb_m:
    try:
      stats["GraphicBufferAllocator KB"] = float(gba_kb_m.group(1))
    except ValueError:
      print(f"Warning: Could not parse GBA KB value: {gba_kb_m.group(1)}")
  return stats


def data_polling_loop(package_name, poll_interval):
  print("Starting data polling thread (meminfo and GraphicBufferAllocator)...")
  iteration = 0
  global all_monitoring_data
  while not stop_event.is_set():
    iteration += 1
    current_time_str = time.strftime("%Y-%m-%d %H:%M:%S", time.localtime())
    header_prefix = f"[{current_time_str}][Poll Iter: {iteration}]"
    current_snapshot_data = {"timestamp": current_time_str}
    app_is_running = is_package_running(package_name)

    meminfo_total_pss_str = "N/A"
    if app_is_running:
      raw_mem_data = get_meminfo_data_internal(package_name)
      if raw_mem_data:
        total_pss_val = raw_mem_data.get("TOTAL", {}).get("Pss Total")
        if total_pss_val is not None:
          meminfo_total_pss_str = str(total_pss_val)
        for category, data_map in raw_mem_data.items():
          pss_total = data_map.get("Pss Total")
          if pss_total is not None:
            current_snapshot_data[f"{category} Pss Total"] = pss_total
      elif app_is_running:
        print(
            f"{header_prefix} Warning: No meminfo data parsed for {package_name} this cycle.",
            end=" | ")

    sf_stats = get_surface_flinger_gba_kb()
    current_snapshot_data.update(sf_stats)
    gba_kb_val = sf_stats.get('GraphicBufferAllocator KB')
    gba_kb_str = f"{gba_kb_val:.1f} KB" if gba_kb_val is not None else "N/A"

    if app_is_running or iteration == 1:  # Print if app running or first iteration (to show initial GBA)
      print(
          f"{header_prefix} Mem(TOTAL PSS): {meminfo_total_pss_str} KB | SF - GBA: {gba_kb_str}"
      )
    elif not app_is_running and (iteration % 5
                                 == 0):  # Print occasionally if app not running
      print(
          f"{header_prefix} App {package_name} not running. SF - GBA: {gba_kb_str}"
      )

    all_monitoring_data.append(current_snapshot_data)
    # Accurate sleep considering processing time (roughly)
    start_sleep_time = time.monotonic()
    while time.monotonic() - start_sleep_time < poll_interval:
      if stop_event.is_set():
        break
      time.sleep(0.1)  # Check stop event frequently during poll interval
    if stop_event.is_set():
      break

  print("Data polling thread stopped.")


def write_monitoring_data_csv(output_dir, filename_base):
  global all_monitoring_data
  if not all_monitoring_data:
    print("No data collected to write to CSV.")
    return False
  output_filename = os.path.join(output_dir, f"{filename_base}.csv")
  print(f"Writing monitoring data to {output_filename}...")
  fieldnames_set = set(["timestamp"])
  surfaceflinger_keys_ordered = ["GraphicBufferAllocator KB"]
  for key in surfaceflinger_keys_ordered:
    fieldnames_set.add(key)
  meminfo_keys_found = set()
  for row_data in all_monitoring_data:
    for key in row_data.keys():
      if key not in ["timestamp"] and key not in surfaceflinger_keys_ordered:
        meminfo_keys_found.add(key)
      fieldnames_set.add(key)
  sorted_meminfo_keys = sorted(list(meminfo_keys_found))
  final_fieldnames = ["timestamp"]
  for key in surfaceflinger_keys_ordered:
    if key in fieldnames_set:
      final_fieldnames.append(key)
  for mk in sorted_meminfo_keys:
    if mk not in final_fieldnames:
      final_fieldnames.append(mk)
  for key_in_set in sorted(list(fieldnames_set)):
    if key_in_set not in final_fieldnames:
      final_fieldnames.append(key_in_set)
  try:
    with open(output_filename, 'w', newline='', encoding='utf-8') as csvfile:
      writer = csv.DictWriter(csvfile, fieldnames=final_fieldnames, restval='')
      writer.writeheader()
      writer.writerows(all_monitoring_data)
    print(f"Monitoring data successfully written to {output_filename}")
    return True
  except Exception as e:
    print(f"Error writing CSV: {e}")
    return False


def plot_monitoring_data_from_df(df_input,
                                 output_filename_base_with_dir,
                                 output_filepath_override=None):
  if not MATPLOTLIB_AVAILABLE:
    print("Plotting libraries (pandas, matplotlib, numpy) not found.")
    return False
  df = df_input.copy()
  if 'timestamp' not in df.columns:
    print("Plotter: 'timestamp' column missing.")
    return False
  try:
    df['timestamp'] = pd.to_datetime(df['timestamp'])
  except Exception as e:
    print(f"Plotter: Error converting 'timestamp': {e}")
    return False

  cols_to_ffill = [
      col for col in df.columns
      if col.endswith(" Pss Total") or col == "GraphicBufferAllocator KB"
  ]
  for col_name in cols_to_ffill:
    if col_name in df.columns and col_name != 'timestamp':
      df[col_name] = pd.to_numeric(df[col_name], errors='coerce')
      df[col_name] = df[col_name].ffill()
  df = df.sort_values(by='timestamp').reset_index(drop=True)

  print("\n--- Plotter: Generating Plot ---")
  try:
    plt.style.use('seaborn-v0_8-whitegrid')
  except:
    print(
        "Note: 'seaborn-v0_8-whitegrid' style not available, using default Matplotlib style."
    )
    pass

  fig, ax1 = plt.subplots(figsize=(18, 10))
  pss_cols_for_stack = sorted([
      c for c in df.columns if c.endswith(" Pss Total") and
      c != "TOTAL Pss Total" and "mmap" not in c.lower()
  ])
  total_pss_col = "TOTAL Pss Total"
  gba_kb_col = "GraphicBufferAllocator KB"

  if pss_cols_for_stack:
    stack_labels = [c.replace(" Pss Total", "") for c in pss_cols_for_stack]
    y_stack_data_unfiltered = [
        df[c].fillna(0) for c in pss_cols_for_stack if c in df
    ]
    y_stack_data = []
    valid_labels = []
    for i, series in enumerate(y_stack_data_unfiltered):
      if i < len(stack_labels) and series.notnull().any() and not (series
                                                                   == 0).all():
        y_stack_data.append(series.values)
        valid_labels.append(stack_labels[i])
    if y_stack_data:
      try:
        num_colors = len(y_stack_data)
        cmap_stack = plt.get_cmap('Paired', num_colors if num_colors > 0 else 1)
        colors = [cmap_stack(j) for j in range(num_colors)]
      except Exception as e_cmap:
        print(
            f"Plotter: Colormap 'Paired' issue ({e_cmap}), using default colors."
        )
        colors = None
      ax1.stackplot(
          df['timestamp'],
          *y_stack_data,
          labels=valid_labels,
          colors=colors,
          alpha=0.75,
          zorder=1)

  if total_pss_col in df.columns and df[total_pss_col].notnull().any():
    ax1.plot(
        df['timestamp'],
        df[total_pss_col],
        label="TOTAL PSS (Stepped)",
        color='black',
        lw=2,
        ls='--',
        zorder=5)
  if gba_kb_col in df.columns and df[gba_kb_col].notnull().any():
    ax1.plot(
        df['timestamp'],
        df[gba_kb_col],
        label="GraphicBufferAllocator KB (Stepped)",
        color='darkcyan',
        lw=1.5,
        ls=':',
        zorder=6,
        marker='.',
        markersize=4,
        markevery=0.1)

  ax1.set_xlabel("Time", fontsize=12)
  ax1.set_ylabel("Memory Usage (KB)", fontsize=14, color='tab:blue')
  ax1.tick_params(axis='y', labelcolor='tab:blue', labelsize=10)
  ax1.grid(True, linestyle=':', alpha=0.6, axis='y')

  csv_basename = os.path.basename(output_filename_base_with_dir)
  fig.suptitle(
      f"Cobalt Memory Monitoring\n(Source: {csv_basename}.csv)",
      fontsize=18,
      y=0.98)
  ax1.xaxis.set_major_formatter(mdates.DateFormatter('%H:%M:%S'))
  fig.autofmt_xdate(rotation=30, ha='right')

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
        borderaxespad=0.,
        fontsize=legend_fontsize)
    fig.tight_layout(rect=[0.03, 0.05, legend_bbox_right_shift, 0.93])
  else:
    fig.tight_layout(rect=[0.03, 0.05, 0.97, 0.93])

  output_plot_path = output_filepath_override if output_filepath_override else output_filename_base_with_dir + "_plot.png"
  try:
    plot_dir = os.path.dirname(output_plot_path)
    if plot_dir and not os.path.exists(plot_dir):
      os.makedirs(plot_dir)
    plt.savefig(output_plot_path, dpi=150, bbox_inches='tight')
    print(f"Plot successfully saved to '{output_plot_path}'")
    return True
  except Exception as e:
    print(f"Error saving plot: {e}")
    return False


# --- Main Execution ---
def main():
  parser = argparse.ArgumentParser(
      description="Monitor Cobalt application (meminfo, GraphicBufferAllocator) and output data.",
      formatter_class=argparse.ArgumentDefaultsHelpFormatter)
  parser.add_argument(
      '--package',
      default=DEFAULT_COBALT_PACKAGE_NAME,
      help="Target package name.")
  parser.add_argument(
      '--activity',
      default=DEFAULT_COBALT_ACTIVITY_NAME,
      help="Target activity name.")
  parser.add_argument(
      '--url', default=DEFAULT_COBALT_URL, help="URL to launch on Cobalt.")
  parser.add_argument(
      '--output',
      choices=['csv', 'plot', 'both'],
      default='both',
      help='Output format(s).')
  parser.add_argument(
      '--interval',
      type=int,
      default=DEFAULT_POLL_INTERVAL_SECONDS,
      help='Polling interval (s).')
  parser.add_argument(
      '--outdir',
      type=str,
      default=DEFAULT_OUTPUT_DIRECTORY,
      help='Output directory.')
  args = parser.parse_args()

  global script_start_time_for_filename, OUTPUT_DIRECTORY, POLL_INTERVAL_SECONDS
  script_start_time_for_filename = time.strftime("%Y%m%d_%H%M%S",
                                                 time.localtime())
  OUTPUT_DIRECTORY = args.outdir
  POLL_INTERVAL_SECONDS = args.interval  # MODIFIED: Set global from args

  print(
      f"--- Configuration ---\nPackage: {args.package}\nActivity: {args.activity}\nURL: {args.url}\n"
      f"Interval: {POLL_INTERVAL_SECONDS}s\nOutDir: {OUTPUT_DIRECTORY}\nOutput: {args.output}\n"
      f"---------------------\n")

  if args.output in ['plot', 'both'] and not MATPLOTLIB_AVAILABLE:
    print(
        "CRITICAL ERROR: Plotting requested but pandas/matplotlib/numpy are not installed."
    )
    return
  if not os.path.exists(OUTPUT_DIRECTORY):
    try:
      os.makedirs(OUTPUT_DIRECTORY)
      print(f"Created output directory: {OUTPUT_DIRECTORY}")
    except OSError as e:
      print(f"Error creating output directory {OUTPUT_DIRECTORY}: {e}.")
      OUTPUT_DIRECTORY = "."

  print("Checking ADB connection...")
  stdout_dev, stderr_dev = run_adb_command(["adb", "devices"])
  if stderr_dev or not stdout_dev or "List of devices attached" not in stdout_dev:
    print(f"ADB not working. Exiting.")
    return
  if len(stdout_dev.strip().splitlines()) <= 1:
    print(f"No devices/emulators authorized. Exiting.")
    return

  if is_package_running(args.package):
    print(f"'{args.package}' running. Stopping it first.")
    stop_package(args.package)

  launch_cobalt(args.package, args.activity, args.url)

  # REMOVED: time.sleep(10) for Cobalt initialization
  print(f"\nStarting data polling immediately for {args.package}...")

  # MODIFIED: Check if app is running *before* starting thread, though loop also checks
  if not is_package_running(args.package):
    print(
        f"Warning: {args.package} did not seem to start immediately. Polling will still attempt to get data."
    )
  # else: # Not strictly needed as polling loop handles it
  #    print(f"{args.package} appears to be running. Starting data polling.")

  polling_thread = threading.Thread(
      target=data_polling_loop,
      args=(args.package, POLL_INTERVAL_SECONDS),
      daemon=True)
  polling_thread.start()

  print(
      f"\nPolling every {POLL_INTERVAL_SECONDS}s. Output in '{OUTPUT_DIRECTORY}'. Ctrl+C to stop & save."
  )

  try:
    while polling_thread.is_alive():
      time.sleep(1)
  except KeyboardInterrupt:
    print("\nCtrl+C received. Signaling polling thread to stop...")
  except Exception as e:
    print(f"Main loop encountered an error: {e}")
  finally:
    stop_event.set()

  print("Waiting for data polling thread to complete...")
  if polling_thread.is_alive():
    polling_thread.join(timeout=POLL_INTERVAL_SECONDS + 10)
  print("\nData polling stopped.")

  filename_base = f"monitoring_data_{script_start_time_for_filename}"
  output_filename_base_with_dir = os.path.join(OUTPUT_DIRECTORY, filename_base)

  if args.output == 'csv' or args.output == 'both':
    write_monitoring_data_csv(OUTPUT_DIRECTORY, filename_base)
  if args.output == 'plot' or args.output == 'both':
    if not MATPLOTLIB_AVAILABLE:
      print("Skipping plot: Plotting libraries not available.")
    elif not all_monitoring_data:
      print("No data collected, skipping plot generation.")
    else:
      df_for_plot = pd.DataFrame(all_monitoring_data)
      if not df_for_plot.empty:
        plot_monitoring_data_from_df(df_for_plot, output_filename_base_with_dir)
      else:
        print(
            "DataFrame for plotting is empty (no data rows), skipping plot generation."
        )

  print("Script finished.")


if __name__ == "__main__":
  main()
