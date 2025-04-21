# filename: run_monitor_plot_final_v2.py
import subprocess
import time
import signal
import sys
import re
import threading
import argparse
from datetime import datetime, timedelta  # Ensure timedelta is imported

# Matplotlib imports
try:
  import matplotlib.pyplot as plt
  import matplotlib.dates as mdates
  MATPLOTLIB_AVAILABLE = True
except ImportError:
  MATPLOTLIB_AVAILABLE = False

# --- Configuration ---
PROCESS_NAME = "dev.cobalt.coat"
PLOT_FILENAME = "memory_usage_plot.png"

POLL_INTERVAL_SECONDS = 1
ADB_PATH = "adb"
STARTUP_DELAY_SECONDS = 5

LOG_EVENTS_TO_MONITOR = {
    "starboard: prepare to suspend": "starboard: Prepare to suspend",
    "starboardbridge init.": "StarboardBridge init."
}
THOR_LOG_PATTERN = re.compile(r"YO THOR ALLOC:(\d+)\s+CURCAP:(\d+)",
                              re.IGNORECASE)
CPU_INFO_PATTERN = re.compile(
    r"(\d+(?:\.\d+)?)%?\s+\d+/" + re.escape(PROCESS_NAME) + r":", re.IGNORECASE)

# --- Global variables ---
keep_running = True
# Initialize lists globally (will be cleared properly in main)
memory_data = []
log_events = []
thor_data = []
cpu_data = []
log_lock = threading.Lock()
adb_logcat_proc = None
stop_monitoring_event = threading.Event()

# --- Functions ---


def run_adb_command(command_args, check_output=False, timeout=10):
  # (Remains the same)
  if not command_args:
    return None
  if command_args[0] != ADB_PATH:
    full_command = [ADB_PATH] + command_args
  else:
    full_command = command_args
  print(f"Executing: {' '.join(full_command)}")
  try:
    if check_output:
      result = subprocess.run(
          full_command,
          capture_output=True,
          text=True,
          check=True,
          timeout=timeout,
          encoding='utf-8',
          errors='replace')
      return result.stdout
    else:
      subprocess.run(full_command, check=True, timeout=timeout)
      return None  # Success
  except FileNotFoundError:
    print(f"Error: '{ADB_PATH}' command not found.")
    sys.exit(1)
  except subprocess.CalledProcessError as e:
    if keep_running:
      print(
          f"Error executing ADB command: {' '.join(full_command)} (Code: {e.returncode})"
      )
      stderr_lower = str(e.stderr).lower() if e.stderr else ""
      if "device unauthorized" in stderr_lower:
        print("Error: Device unauthorized.")
      elif "device" in stderr_lower and "not found" in stderr_lower:
        print("Error: Device not found.")
      else:
        print(f"stderr: {e.stderr or '(No stderr)'}")
    return False
  except subprocess.TimeoutExpired:
    print(f"Error: ADB command timed out: {' '.join(full_command)}")
    return False
  except Exception as e:
    print(f"An unexpected error occurred during ADB execution: {e}")
    sys.exit(1)


def get_memory_usage(process_name):
  # (Remains the same)
  command = ["shell", "dumpsys", "meminfo", process_name]
  output = run_adb_command(command, check_output=True, timeout=8)
  if output is None or output is False:
    return None
  match = re.search(r"^\s*TOTAL PSS:\s*(\d+)", output,
                    re.IGNORECASE | re.MULTILINE)
  if match:
    try:
      return int(match.group(1))
    except (ValueError, IndexError):
      return None
  else:
    return None


def get_cpu_usage(process_name):
  # (Remains the same)
  command = ["shell", "dumpsys", "cpuinfo"]
  output = run_adb_command(command, check_output=True, timeout=5)
  if output is None or output is False:
    return None
  match = CPU_INFO_PATTERN.search(output)
  if match:
    try:
      return float(match.group(1))
    except (ValueError, IndexError):
      print(
          f"Warning: Found CPU line but failed to parse percentage for {process_name}: {match.group(0)}"
      )
      return None
  else:
    return None


def monitor_logcat(stop_event):
  """Monitors `adb logcat` for specific messages in a separate thread."""
  global adb_logcat_proc, log_events, thor_data, log_lock  # Removed dropped_frame_data
  logcat_command = [ADB_PATH, 'logcat', '-v', 'time']
  adb_logcat_proc = None
  try:
    run_adb_command(['logcat', '-c'], timeout=5)
  except Exception as e:
    print(f"Logcat thread Warning: Error clearing logcat buffer: {e}")
  print(
      f"Logcat thread: Starting logcat monitoring: {' '.join(logcat_command)}")
  try:
    adb_logcat_proc = subprocess.Popen(
        logcat_command,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        encoding='utf-8',
        errors='replace')
    print("Logcat thread: Monitoring started.")
    for line in iter(adb_logcat_proc.stdout.readline, ''):
      if stop_event.is_set():
        break
      if not line:
        break
      timestamp_dt = datetime.now()
      processed_line = False

      thor_match = THOR_LOG_PATTERN.search(line)
      if thor_match:
        try:
          alloc_bytes = int(thor_match.group(1))
          curcap_bytes = int(thor_match.group(2))
          alloc_mb = alloc_bytes / (1024.0 * 1024.0)
          curcap_mb = curcap_bytes / (1024.0 * 1024.0)
          with log_lock:
            thor_data.append((timestamp_dt, alloc_mb, curcap_mb))
            # print(f"DEBUG: thor_data length in thread: {len(thor_data)}") # Keep debug? Remove for final.
          print(
              f"++++ THOR Data Recorded: {timestamp_dt.isoformat(sep=' ', timespec='milliseconds')} - ALLOC={alloc_mb:.2f}MB, CURCAP={curcap_mb:.2f}MB ++++"
          )
          processed_line = True
        except ValueError:
          print(
              f"Warning: Could not parse THOR numbers in line: {line.strip()}")

      if not processed_line:
        line_lower = line.lower()
        matched_event_key = None
        if "starboard" in line_lower and "prepare to suspend" in line_lower:
          matched_event_key = "starboard: prepare to suspend"
        elif "starboardbridge" in line_lower and "init." in line_lower:
          matched_event_key = "starboardbridge init."

        if matched_event_key:
          canonical_message = LOG_EVENTS_TO_MONITOR.get(matched_event_key,
                                                        "Unknown")
          with log_lock:
            log_events.append((timestamp_dt, canonical_message))
            # print(f"DEBUG: log_events length in thread: {len(log_events)}") # Keep debug? Remove for final.
          print(
              f"++++ Log Event Recorded: {timestamp_dt.isoformat(sep=' ', timespec='milliseconds')} - {canonical_message} ++++"
          )

  except Exception as e:
    print(f"Logcat thread Error: An unexpected error occurred: {e}")
    stop_event.set()
  finally:
    if adb_logcat_proc and adb_logcat_proc.poll() is None:
      try:
        adb_logcat_proc.terminate()
        adb_logcat_proc.wait(timeout=1)
      except Exception:
        adb_logcat_proc.kill()
    print("Logcat thread: Exiting.")


def signal_handler(sig, frame):
  # (Remains the same)
  global keep_running
  if not keep_running and stop_monitoring_event.is_set():
    return
  print("\nCtrl+C detected. Signaling monitoring threads to stop...")
  keep_running = False
  stop_monitoring_event.set()


# --- Plotting Function ---
# Reverted to plotting events as vertical lines (axvline)
def generate_plot(mem_data, evt_data, thor_data_list, cpu_data_list, target_url,
                  output_filename):
  """Generates the plot using Matplotlib, plotting events as vertical lines."""
  if not MATPLOTLIB_AVAILABLE:
    print("\nMatplotlib not found...")
    return

  print("\n--- Generating Plot ---")
  mem_times_dt = [item[0] for item in mem_data] if mem_data else []
  mem_values_mb = [(item[1] / 1024.0) for item in mem_data] if mem_data else []
  thor_times_dt = [item[0] for item in thor_data_list] if thor_data_list else []
  alloc_mb_values = [item[1] for item in thor_data_list
                    ] if thor_data_list else []
  curcap_mb_values = [item[2] for item in thor_data_list
                     ] if thor_data_list else []
  cpu_times_dt = [item[0] for item in cpu_data_list] if cpu_data_list else []
  cpu_values_percent = [item[1] for item in cpu_data_list
                       ] if cpu_data_list else []
  if evt_data:
    evt_data.sort(key=lambda x: x[0])
  evt_times_dt = [item[0] for item in evt_data] if evt_data else []

  all_times = mem_times_dt + thor_times_dt + evt_times_dt + cpu_times_dt
  if not all_times:
    print("No time-based data collected...")
    return

  min_dt = min(all_times)
  max_dt = max(all_times)
  duration_secs = (max_dt - min_dt).total_seconds()
  padding_secs = max(1.0, duration_secs * 0.01)
  padding_td = timedelta(seconds=padding_secs)
  plot_start_dt = min_dt - padding_td
  plot_end_dt = max_dt + padding_td
  print(f"Calculated Plot Time Range: {plot_start_dt} to {plot_end_dt}")

  try:
    fig, ax = plt.subplots(figsize=(13, 7))
    ax.set_xlim(plot_start_dt, plot_end_dt)  # Set explicit X limits first

    # --- Primary Y Axis (Left - PSS Memory & CPU) ---
    lines1 = []
    if mem_times_dt:
      line, = ax.plot(
          mem_times_dt,
          mem_values_mb,
          marker='.',
          linestyle='-',
          markersize=4,
          linewidth=1.5,
          label='Memory Usage (PSS MB)',
          color='tab:blue')
      lines1.append(line)
    if cpu_times_dt:
      line, = ax.plot(
          cpu_times_dt,
          cpu_values_percent,
          marker='+',
          linestyle='-.',
          markersize=5,
          linewidth=1.0,
          label='CPU Usage (%)',
          color='darkcyan')
      lines1.append(line)
    ax.set_ylabel('PSS (MB) / CPU (%)')
    ax.tick_params(axis='y', labelcolor='black')
    ax.ticklabel_format(axis='y', style='plain')
    ax.set_ylim(bottom=0)

    # --- Secondary Y Axis (Right - THOR Buffers) ---
    ax2 = None
    lines2 = []
    if thor_times_dt:
      ax2 = ax.twinx()
      line, = ax2.plot(
          thor_times_dt,
          alloc_mb_values,
          marker='^',
          linestyle=':',
          markersize=4,
          linewidth=1.0,
          label='DecoderBufferAllocation (MB)',
          color='tab:green')
      lines2.append(line)
      line, = ax2.plot(
          thor_times_dt,
          curcap_mb_values,
          marker='v',
          linestyle=':',
          markersize=4,
          linewidth=1.0,
          label='DecoderBufferCurrentCapacity (MB)',
          color='tab:purple')
      lines2.append(line)
      ax2.set_ylabel('DecoderBuffers(MB)', color='tab:gray')
      ax2.tick_params(axis='y', labelcolor='tab:gray')
      ax2.ticklabel_format(axis='y', style='plain')
      ax2.set_ylim(bottom=0)

    # --- Discrete Event Lines (on primary axis) --- REVERTED TO AXVLINE
    if evt_data:
      event_colors = {
          "StarboardBridge init.": "tab:cyan",
          "starboard: Prepare to suspend": "tab:orange"
      }
      default_event_color = 'red'
      labeled_events_legend = set()
      # No need to get ylim for axvline
      for dt, msg in evt_data:
        color = event_colors.get(msg, default_event_color)
        label_to_use = None
        if msg not in labeled_events_legend:
          label_to_use = msg
          labeled_events_legend.add(msg)
        # Use axvline - should now be visible due to explicit xlim set earlier
        ax.axvline(
            x=dt,
            color=color,
            linestyle='--',
            linewidth=1.5,
            label=label_to_use)

    # --- General Formatting ---
    plot_title = f'Chrobalt Playback Memory Usage - {target_url}'
    max_url_len = 60
    if len(target_url) > max_url_len:
      plot_title = f'Chrobalt Playback Memory Usage - ...{target_url[-max_url_len:]}'
    ax.set_title(plot_title)
    ax.set_xlabel('Time')
    ax.xaxis.set_major_formatter(mdates.DateFormatter('%H:%M:%S'))
    fig.autofmt_xdate(rotation=45, ha='right')
    ax.grid(True, linestyle=':', color='grey', alpha=0.7)

    # --- Combined Legend ---
    handles1, labels1 = ax.get_legend_handles_labels(
    )  # Gets handles from ax (lines + vlines)
    handles2, labels2 = [], []
    if ax2:
      handles2, labels2 = ax2.get_legend_handles_labels()
    if handles1 or handles2:
      ax.legend(handles1 + handles2, labels1 + labels2, loc='best')

    plt.tight_layout()
    plt.savefig(output_filename, dpi=150)
    print(f"Plot saved successfully to {output_filename}")

  except Exception as e:
    print(f"\nError generating plot: {e}")
    import traceback
    traceback.print_exc()


# --- Main Execution ---
def main():
  """Main script logic."""
  # Make sure lists are accessible and correctly initialized
  global memory_data, log_events, thor_data, cpu_data

  parser = argparse.ArgumentParser(
      description='Run Cobalt app, monitor memory/logs/cpu, and generate plot.')
  parser.add_argument(
      '--url',
      type=str,
      default='https://youtube.com/tv/watch?v=RACW52qnJMI',
      help='URL for Cobalt app')
  args = parser.parse_args()
  target_url = args.url
  print(f"Target URL: {target_url}")

  if not MATPLOTLIB_AVAILABLE:
    print("Error: Matplotlib needed...")
    sys.exit(1)

  # MODIFIED: Initialize lists *before* starting threads that modify them
  memory_data, log_events, thor_data, cpu_data = [], [], [], []

  signal.signal(signal.SIGINT, signal_handler)

  # Start App
  print("--- Starting Application ---")
  run_adb_command(["shell", "am", "force-stop", PROCESS_NAME])
  time.sleep(1)
  esa_argument_string = f'--remote-allow-origins=*,--url="{target_url}"'
  start_command = [
      "shell", "am", "start", "--esa", "commandLineArgs", esa_argument_string,
      f"{PROCESS_NAME}/dev.cobalt.app.MainActivity"
  ]
  start_result = run_adb_command(start_command)
  if start_result is False:
    sys.exit(1)
  print("Application start command sent.")

  # Start Logcat Thread
  print("\n--- Starting Logcat Monitoring Thread (before delay) ---")
  logcat_thread = threading.Thread(
      target=monitor_logcat, args=(stop_monitoring_event,), daemon=True)
  logcat_thread.start()

  # Optional Delay
  if STARTUP_DELAY_SECONDS > 0:
    print(
        f"\n--- Waiting {STARTUP_DELAY_SECONDS}s before starting memory polling ---"
    )
    interrupted = stop_monitoring_event.wait(timeout=STARTUP_DELAY_SECONDS)
    if interrupted or not keep_running:
      print("Stop requested during startup delay.")
      if not stop_monitoring_event.is_set():
        stop_monitoring_event.set()
      logcat_thread.join(timeout=2)
      print("Exiting early.")
      sys.exit(0)

  # Memory & CPU Monitoring Loop
  print(
      f"\n--- Starting Memory & CPU Monitoring (Interval: {POLL_INTERVAL_SECONDS}s) ---"
  )
  print(f"Monitoring process: {PROCESS_NAME}")
  print("Press Ctrl+C to stop.")
  # REMOVED re-initialization of lists here

  while keep_running:
    interrupted = stop_monitoring_event.wait(timeout=POLL_INTERVAL_SECONDS)
    if interrupted or not keep_running:
      break
    current_time_dt = datetime.now()
    pss_kb = get_memory_usage(PROCESS_NAME)
    cpu_percent = get_cpu_usage(PROCESS_NAME)
    print_str = f"{current_time_dt.isoformat(sep=' ', timespec='milliseconds')} - "
    if pss_kb is not None:
      print_str += f"Memory Usage (PSS): {pss_kb} KB"
      memory_data.append(
          (current_time_dt,
           pss_kb))  # Appends to list initialized before thread start
    else:
      print_str += "Could not get memory usage."
    if cpu_percent is not None:
      print_str += f" - CPU Usage: {cpu_percent:.1f}%"
      cpu_data.append(
          (current_time_dt,
           cpu_percent))  # Appends to list initialized before thread start
    else:
      print_str += " - Could not get CPU usage."
    print(print_str)

  # Stop Threads
  print("\n--- Stopping Monitoring ---")
  if not stop_monitoring_event.is_set():
    stop_monitoring_event.set()
  print("Waiting for logcat thread to finish...")
  logcat_thread.join(timeout=5)
  if logcat_thread.is_alive():
    print("Warning: Logcat thread did not exit cleanly.")

  # Generate Plot
  # REMOVED debug prints around lock/copy
  with log_lock:  # Get final copies of data collected by logcat thread
    log_events_final = list(log_events)
    thor_data_final = list(thor_data)

  # memory_data and cpu_data collected in main thread, no lock needed
  generate_plot(memory_data, log_events_final, thor_data_final, cpu_data,
                target_url, PLOT_FILENAME)

  print("--- Script Finished ---")


if __name__ == "__main__":
  from datetime import datetime, timedelta
  import argparse
  import re
  import sys
  import threading
  main()
