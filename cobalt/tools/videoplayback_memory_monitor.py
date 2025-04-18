#!/usr/bin/env python3

import subprocess
import sys
import time
import argparse
import shlex
import re # For parsing logs
import csv # For saving data
import statistics # For calculating averages
import threading # For logcat monitoring
import queue     # For thread-safe communication
from collections import defaultdict # For easier log processing

# --- Configuration ---
ADB_EXECUTABLE = 'adb'
TARGET_DEVICE_SERIAL = None # Default: Let adb decide
COBALT_COMPONENT = 'dev.cobalt.coat/dev.cobalt.app.MainActivity'
COBALT_PACKAGE_NAME = COBALT_COMPONENT.split('/')[0]

# Sampling and display settings
DEFAULT_SAMPLING_INTERVAL = 2 # Default seconds between CPU/Memory samples
DEFAULT_CSV_FILENAME = "cobalt_performance_data.csv"
TIME_UPDATE_INTERVAL = 1 # How often to update the elapsed time display (seconds)

# --- !!! Define Log Patterns to Extract Here !!! ---
# List of tuples: ('YourMetricName', compiled_regex_with_one_capture_group)
# The regex MUST have exactly one capturing group `()` around the number you want.
DEFAULT_LOG_PATTERNS = [
    # Example 1: Track frame timings
    ('MemoryAllocZ', re.compile(r"YO THOR: allocated:(\d+)")),
    #('FrameTime_ms', re.compile(r"PerfLog: Frame rendered in (\d+\.?\d*)\s*ms")),
    # Example 2: Track cache misses
    #('CacheMisses', re.compile(r"CacheStats: L2 Misses = (\d+)")),
    # Example 3: Track some GPU metric
    # ('GpuUtil_%', re.compile(r"GpuMonitor: Usage=(\d+\.?\d*)%")),
    # Add more patterns here as needed
]
# --- End Configuration ---

# --- Helper Functions ---

def run_adb_command(adb_path, device_serial, command_args, timeout=30, check=True, verbose=True):
    """
    Helper function to build and run an ADB command using subprocess.
    Returns CompletedProcess object on success, None on failure.
    """
    full_adb_command = [adb_path]
    if device_serial:
        full_adb_command.extend(['-s', device_serial])
    full_adb_command.extend(command_args)

    # Conditionally print the command being executed
    if verbose:
        print("-" * 60)
        print("Executing ADB command:")
        try: print(f"  {' '.join(map(shlex.quote, full_adb_command))}")
        except AttributeError: print(f"  {' '.join(full_adb_command)}")
        print("-" * 60)

    try:
        result = subprocess.run(full_adb_command, check=check, capture_output=True, text=True, timeout=timeout, errors='ignore')
        # Print stderr only if verbose and potentially interesting
        if verbose and result.stderr and result.returncode == 0:
             print(f"--> stderr (potential warnings):\n{result.stderr.strip()}")
        return result

    except subprocess.CalledProcessError as e:
        if verbose:
            print(f"❌ Error: ADB command failed with return code {e.returncode}.")
            if e.stderr: print(f"--> stderr:\n{e.stderr.strip()}")
            # Optionally print stdout on error too if helpful
            # if e.stdout: print(f"--> stdout:\n{e.stdout.strip()}")
        return None
    except FileNotFoundError:
        if verbose: print(f"❌ Error: The ADB executable ('{adb_path}') was not found.")
        return None
    except subprocess.TimeoutExpired:
        if verbose: print(f"❌ Error: ADB command timed out after {timeout} seconds.")
        return None
    except Exception as e:
        if verbose: print(f"❌ An unexpected error occurred during ADB execution: {e}")
        return None

# --- System Info Functions ---

def get_cpu_core_count(adb_path, device_serial):
    """Gets the number of logical CPU cores reported by the system."""
    core_cmd_args = ['shell', 'ls /sys/devices/system/cpu/ | grep -E \'^cpu[0-9]+$\' | wc -l']
    result = run_adb_command(adb_path, device_serial, core_cmd_args, timeout=10, check=False, verbose=False)
    count = None
    if result and result.stdout:
        try: count = int(result.stdout.strip())
        except ValueError: pass

    if count is not None and count > 0: return count

    # Fallback attempt (might count threads differently)
    proc_cmd_args = ['shell', 'cat /proc/cpuinfo | grep -c ^processor']
    result_fb = run_adb_command(adb_path, device_serial, proc_cmd_args, timeout=10, check=False, verbose=False)
    if result_fb and result_fb.stdout:
        try: count = int(result_fb.stdout.strip()); return count
        except ValueError: pass

    print("\nWarning(CPU Count): Could not determine CPU core count.")
    return None


def get_system_memory_info(adb_path, device_serial):
    """Gets total and available system memory from /proc/meminfo."""
    mem_cmd_args = ['shell', 'cat', '/proc/meminfo']
    result = run_adb_command(adb_path, device_serial, mem_cmd_args, timeout=10, check=False, verbose=False)
    mem_info = {}
    if result and result.stdout:
        try:
            for line in result.stdout.strip().split('\n'):
                if line.startswith("MemTotal:"): mem_info['total_kb'] = int(line.split()[1])
                elif line.startswith("MemAvailable:"): mem_info['available_kb'] = int(line.split()[1])
            if 'total_kb' in mem_info and 'available_kb' in mem_info:
                 return {'total_mb': round(mem_info['total_kb'] / 1024.0, 1),
                         'available_mb': round(mem_info['available_kb'] / 1024.0, 1)}
            else: print("\nWarning(SysMem): Could not find MemTotal/MemAvailable in /proc/meminfo.")
        except (ValueError, IndexError) as e: print(f"\nWarning(SysMem): Error parsing /proc/meminfo: {e}")
    else: print("\nWarning(SysMem): Failed to execute or read /proc/meminfo.")
    return None

# --- Performance Sampling Functions ---

def get_memory_usage(adb_path, device_serial, package_name):
    """Gets memory usage (PSS) for the package. Returns MB or None."""
    meminfo_args = ['shell', 'dumpsys', 'meminfo', package_name]
    result = run_adb_command(adb_path, device_serial, meminfo_args, timeout=15, check=False, verbose=False)
    if result and result.stdout:
        raw_output = result.stdout.strip()
        if "No process found" in raw_output or not raw_output: return None
        total_pss = None
        match_pss_direct = re.search(r"TOTAL PSS:\s+(\d+)(?:\s+kB)?", raw_output, re.IGNORECASE)
        if match_pss_direct:
            try: total_pss = int(match_pss_direct.group(1))
            except ValueError: pass
        else:
            match_pss_table = re.search(r"^\s*TOTAL\s+(\d+)", raw_output, re.MULTILINE | re.IGNORECASE)
            if match_pss_table:
                try: total_pss = int(match_pss_table.group(1))
                except ValueError: pass
        if total_pss is not None: return round(total_pss / 1024.0, 2)
        # else: Optionally print warning if TOTAL was seen but not parsed
    return None


def get_cpu_usage(adb_path, device_serial, package_name):
    """Gets CPU usage percentage for the package. Returns % or None."""
    top_args = ['shell', 'top', '-b', '-n', '1']
    result = run_adb_command(adb_path, device_serial, top_args, timeout=10, check=False, verbose=False)
    if result and result.stdout:
        lines = result.stdout.strip().split('\n')
        if not lines: return None
        header_cpu_index = -1
        for line in lines:
            if re.search(r"PID", line, re.IGNORECASE) and re.search(r"CPU", line, re.IGNORECASE):
                try:
                    headers = re.split(r'\s+', line.strip())
                    for i, h in enumerate(headers):
                        if 'CPU'.casefold() in h.casefold(): header_cpu_index = i; break
                except Exception: pass
                break
        if header_cpu_index == -1: return None
        actual_cpu_value_index = header_cpu_index + 1
        for line in lines:
            fields = re.split(r'\s+', line.strip())
            if package_name in line or (fields and fields[-1].endswith(package_name)):
                try:
                    if actual_cpu_value_index < len(fields): return float(fields[actual_cpu_value_index].replace('%', ''))
                except (ValueError, IndexError): pass
                return None # Found line but parse/index failed
        return None # Package name not found in any line
    return None

# --- Core Logic Functions ---

def is_cobalt_running(adb_path, device_serial, package_name):
    """Checks if process is running using 'pidof'."""
    pidof_args = ['shell', 'pidof', package_name]
    result = run_adb_command(adb_path, device_serial, pidof_args, timeout=10, check=False, verbose=False)
    return result is not None and result.returncode == 0 and result.stdout.strip()

def launch_cobalt(adb_path, device_serial, component, url):
    """Launches Cobalt. Returns True on success."""
    print(f"\nAttempting to launch Cobalt component: {component}")
    args_value = f'--remote-allow-origins=*,--url="{url}"'
    print(f"Using commandLineArgs: {args_value}")
    am_start_args = ['shell','am','start','--esa','commandLineArgs',args_value,component]
    return run_adb_command(adb_path, device_serial, am_start_args, check=True, verbose=True) is not None

def kill_cobalt(adb_path, device_serial, package_name):
    """Force-stops Cobalt. Returns True if command sent ok."""
    print(f"\nAttempting to stop/kill process: {package_name}")
    am_kill_args = ['shell', 'am', 'force-stop', package_name]
    return run_adb_command(adb_path, device_serial, am_kill_args, check=False, verbose=True) is not None

# --- Log Reading Thread Function ---

def log_reader_thread(adb_path, device_serial, patterns_to_extract, output_queue, stop_event):
    """
    Runs 'adb logcat -v time', parses lines matching patterns, and puts results on queue.
    """
    logcat_clear_args = ['logcat', '-c']
    logcat_monitor_args = ['logcat', '-v', 'time'] # '-v time' helps correlate but we use monotonic()

    # Build full commands
    clear_cmd = [adb_path]
    if device_serial: clear_cmd.extend(['-s', device_serial])
    clear_cmd.extend(logcat_clear_args)

    monitor_cmd = [adb_path]
    if device_serial: monitor_cmd.extend(['-s', device_serial])
    monitor_cmd.extend(logcat_monitor_args)

    # Attempt to clear buffer first
    run_adb_command(adb_path, device_serial, logcat_clear_args, verbose=False)
    time.sleep(0.1)

    print(f"\nStarting logcat monitoring thread...")
    proc = None
    try:
        proc = subprocess.Popen(monitor_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, errors='ignore')
        print("Logcat thread started.")
        # Directly iterate over stdout lines
        for line in iter(proc.stdout.readline, ''):
            if stop_event.is_set():
                print("Logcat thread: Stop event received.")
                break
            if not line: continue

            # Use monotonic clock for timestamping when Python processed the line
            py_timestamp = time.monotonic()

            for key, pattern in patterns_to_extract:
                match = pattern.search(line)
                if match:
                    try:
                        value = float(match.group(1)) # Extract group 1, convert to float
                        output_queue.put((py_timestamp, key, value))
                        break # Assume only one pattern matches per line
                    except (IndexError, ValueError) as e:
                        # Warning only if needed, can be noisy
                        # print(f"\nWarning(Log): Failed extraction for '{key}': {e} in line: {line.strip()}")
                        pass # Silently ignore lines that match but fail extraction
                    except Exception as e:
                        print(f"\nWarning(Log): Unexpected error parsing line for key '{key}': {e}")
                        pass # Ignore other unexpected errors for this line
        print("Logcat thread: Exited read loop.")
    except FileNotFoundError:
        print(f"❌ Error(Log): ADB executable not found at '{adb_path}' in log thread.")
    except Exception as e:
        print(f"❌ Error(Log): Unexpected error starting or reading logcat thread: {e}")
    finally:
        # Ensure the subprocess is cleaned up
        if proc:
            if proc.poll() is None: # If still running
                print("Logcat thread: Terminating logcat process...")
                proc.terminate()
                try: proc.wait(timeout=2)
                except subprocess.TimeoutExpired:
                    print("Logcat thread: Logcat did not terminate, killing.")
                    proc.kill()
            # Consume any remaining output? Might be large...
            try: proc.stdout.close()
            except Exception: pass
            try: proc.stderr.close()
            except Exception: pass
        print("Logcat thread finished.")


# --- Data Handling ---

def save_data_to_csv(data, filename, headers):
    """Saves the collected performance data (list of dicts) to a CSV file."""
    if not data:
        print("No performance data to save.")
        return
    try:
        with open(filename, 'w', newline='') as csvfile:
            writer = csv.DictWriter(csvfile, fieldnames=headers, extrasaction='ignore') # Ignore extra keys not in header
            writer.writeheader()
            for row_dict in data:
                # Format numeric values for better readability in CSV
                formatted_row = {}
                for key, value in row_dict.items():
                    if key not in headers: continue # Skip keys not in header
                    if isinstance(value, float):
                        if "MB" in key or "ms" in key: formatted_row[key] = f"{value:.2f}"
                        elif "%" in key: formatted_row[key] = f"{value:.1f}"
                        else: formatted_row[key] = f"{value:.3f}" # Default float format
                    else: # Handles timestamp string and potential None values
                        formatted_row[key] = value if value is not None else ''
                writer.writerow(formatted_row)
        print(f"\nPerformance data saved to '{filename}'")
    except IOError as e:
        print(f"\n❌ Error saving data to CSV '{filename}': {e}")
    except Exception as e:
        print(f"\n❌ An unexpected error occurred during CSV saving: {e}")


def process_performance_data(data, core_count, system_mem_info):
    """Calculates and prints summary statistics from performance data (list of dicts)."""
    if not data:
        print("\nNo performance data collected to process.")
        return

    # Dynamically find numeric columns (excluding timestamp)
    if not isinstance(data[0], dict): # Basic check
        print("Error: process_performance_data expects a list of dictionaries.")
        return
    numeric_keys = sorted([k for k, v in data[0].items() if k != 'Timestamp (s)' and isinstance(v, (int, float, type(None)))]) # Include None as potential numeric

    print("\n--- Performance Summary ---")
    if core_count: print(f"System Info: {core_count} CPU Cores")
    if system_mem_info: print(f"             Total={system_mem_info['total_mb']:.0f}MB, Available={system_mem_info['available_mb']:.0f}MB RAM")
    print("-" * 27)

    for key in numeric_keys:
        valid_data = [d.get(key) for d in data if d.get(key) is not None] # Collect non-None values
        if valid_data:
            avg_val = statistics.mean(valid_data)
            max_val = max(valid_data)
            unit = "%" if "%" in key else ("MB" if "MB" in key else ("ms" if "ms" in key else "")) # Basic unit guess
            print(f"{key+':':<25} Avg={avg_val:.1f}{unit}, Max={max_val:.1f}{unit} ({len(valid_data)} samples)")

            # Add specific context if possible
            if 'CPU' in key and core_count:
                avg_cpu_total = (avg_val / core_count)
                max_cpu_total = (max_val / core_count)
                print(f"{'(% of Total CPU):':<25} Avg={avg_cpu_total:.1f}%, Max={max_cpu_total:.1f}%")
            elif 'Memory' in key and system_mem_info:
                 max_mem_perc_total = (max_val / system_mem_info['total_mb']) * 100 if system_mem_info['total_mb'] else 0
                 print(f"{'(% of Total RAM):':<25} Max={max_mem_perc_total:.1f}%")
        else:
            print(f"{key+':':<25} No valid samples collected.")
    print("---------------------------")


# --- Main script execution ---
if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Run Cobalt, sample performance & logs, wait, print updates, kill.",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )
    parser.add_argument('--url', required=True, help='URL to load in Cobalt.')
    parser.add_argument('--duration', required=True, type=int, help='Duration (seconds) to run Cobalt.')
    parser.add_argument('--adb-path', default=ADB_EXECUTABLE, help='Path to ADB executable.')
    parser.add_argument('--device', default=TARGET_DEVICE_SERIAL, help='Target device serial number.')
    parser.add_argument('--sample-interval', type=int, default=DEFAULT_SAMPLING_INTERVAL, help='Interval (seconds) for CPU/Memory sampling.')
    parser.add_argument('--output-csv', default=DEFAULT_CSV_FILENAME, help='Filename for saving performance data.')
    args = parser.parse_args()

    # --- Initial Setup ---
    log_patterns_to_extract = DEFAULT_LOG_PATTERNS # Use default patterns defined at top
    log_metric_keys = [key for key, _ in log_patterns_to_extract]

    print("--- Cobalt Performance Runner (with Log Monitoring) ---")
    print(f"Target URL: {args.url}")
    print(f"Run Duration: {args.duration} seconds")
    print(f"Sampling Interval: {args.sample_interval} seconds")
    print(f"Target Device: {'Default' if not args.device else args.device}")
    print(f"ADB Path: {args.adb_path}")
    print(f"Cobalt Package: {COBALT_PACKAGE_NAME}")
    print(f"Output CSV: {args.output_csv}")
    print(f"Log Metrics Tracked: {log_metric_keys if log_metric_keys else 'None'}")
    print("-" * 31)

    # --- Get System Info ---
    print("\n--- Retrieving System Info ---")
    core_count = get_cpu_core_count(args.adb_path, args.device)
    system_mem_info = get_system_memory_info(args.adb_path, args.device)
    if core_count: print(f"Detected CPU Cores: {core_count}")
    if system_mem_info: print(f"System Memory: Total={system_mem_info['total_mb']:.0f}MB, Available={system_mem_info['available_mb']:.0f}MB")
    print("-----------------------------")

    # --- Pre-launch Check & Kill ---
    print("\nChecking if Cobalt process is already running...")
    if is_cobalt_running(args.adb_path, args.device, COBALT_PACKAGE_NAME):
        print(f"  Process found. Stopping existing process...")
        if not kill_cobalt(args.adb_path, args.device, COBALT_PACKAGE_NAME): print("❌ Failed to stop existing process. Exiting."); sys.exit(1)
        print("  Existing process stop command sent."); time.sleep(1.5)
    else: print(f"  No existing process found.")

    # --- Start Logcat Thread ---
    log_queue = queue.Queue()
    stop_log_event = threading.Event()
    # Compile regex patterns before passing to thread
    compiled_log_patterns = [(key, re.compile(pattern.pattern)) for key, pattern in log_patterns_to_extract]

    log_thread = threading.Thread(
        target=log_reader_thread,
        args=(args.adb_path, args.device, compiled_log_patterns, log_queue, stop_log_event),
        daemon=True # Exits if main thread exits unexpectedly
    )
    log_thread.start()
    time.sleep(1) # Give thread time to start logcat

    # --- 1. Launch Cobalt ---
    launch_successful = False
    try:
        launch_successful = launch_cobalt(args.adb_path, args.device, COBALT_COMPONENT, args.url)
    except Exception as e:
        print(f"\n❌ Unexpected error during launch attempt: {e}")
        # Ensure thread cleanup even if launch fails badly
        stop_log_event.set(); log_thread.join(timeout=5)
        sys.exit(1)

    if not launch_successful:
        print("\n❌ Failed to launch Cobalt. Stopping log thread and exiting.")
        stop_log_event.set(); log_thread.join(timeout=5); sys.exit(1)

    print("\n✅ Cobalt launch command sent successfully.")
    print("Waiting briefly for app to initialize...")
    time.sleep(4)


    # --- 2. Wait and Sample Performance ---
    print(f"\nStarting {args.duration}s run, sampling every {args.sample_interval}s...")
    performance_data = [] # Stores (ts, cpu, mem) tuples
    start_time = time.monotonic(); end_time = start_time + args.duration
    run_completed_normally = True; next_sample_time = start_time

    try:
        while True:
            current_time = time.monotonic(); elapsed_time = current_time - start_time
            if current_time >= end_time: break # Duration finished

            # --- Sampling CPU/Mem ---
            if current_time >= next_sample_time:
                cpu = get_cpu_usage(args.adb_path, args.device, COBALT_PACKAGE_NAME)
                mem = get_memory_usage(args.adb_path, args.device, COBALT_PACKAGE_NAME)
                performance_data.append((elapsed_time, cpu, mem))

                # Check if process died (more robust check)
                if cpu is None and mem is None and len(performance_data) > 2 and \
                   performance_data[-2][1] is None and performance_data[-2][2] is None:
                     # Double check with pidof if two consecutive samples failed
                     if not is_cobalt_running(args.adb_path, args.device, COBALT_PACKAGE_NAME):
                         print("\n⚠️ Process appears to have stopped unexpectedly. Stopping sampling.")
                         run_completed_normally = False; break

                # Schedule next sample based on interval, not actual time taken
                next_sample_time += args.sample_interval

            # --- Time Update Display ---
            remaining_display = max(0, end_time - time.monotonic())
            latest_cpu_str = f"{performance_data[-1][1]:.1f}%" if performance_data and performance_data[-1][1] is not None else "N/A"
            latest_mem_str = f"{performance_data[-1][2]:.1f}MB" if performance_data and performance_data[-1][2] is not None else "N/A"
            print(f"\r  Elapsed: {elapsed_time:.1f}s / {args.duration}s | CPU: {latest_cpu_str} | Mem: {latest_mem_str} | Rem: {remaining_display:.1f}s  ", end='')

            # --- Sleep ---
            time_until_next_sample = max(0, next_sample_time - time.monotonic())
            time_until_end = max(0, end_time - time.monotonic())
            sleep_duration = min(TIME_UPDATE_INTERVAL, time_until_next_sample, time_until_end)
            if sleep_duration <= 0.01 and time.monotonic() < end_time: time.sleep(0.01) # Prevent busy loop
            elif sleep_duration > 0: time.sleep(sleep_duration)

        # Clear the updating line after the loop
        print("\r" + " " * 120 + "\r", end='')
        if run_completed_normally: print("  Run duration complete.")

    except KeyboardInterrupt:
        print("\n🛑 Sampling interrupted by user (Ctrl+C).")
        run_completed_normally = False
    finally:
        # --- Stop Logcat Thread (Critical) ---
        print("\nStopping logcat monitoring thread...")
        stop_log_event.set()
        log_thread.join(timeout=5) # Wait for thread to finish
        if log_thread.is_alive():
            print("Warning: Logcat thread did not exit cleanly.")


    # --- 3. Process Log Queue ---
    print("Processing collected log data...")
    raw_log_data = defaultdict(list) # {key: [(ts, val), ...]}
    items_processed = 0
    while not log_queue.empty():
        try:
            log_py_ts, log_key, log_value = log_queue.get_nowait()
            log_elapsed = log_py_ts - start_time # Convert to elapsed time relative to main loop start
            if log_elapsed >= -1: # Allow logs slightly before official start
                 raw_log_data[log_key].append((log_elapsed, log_value))
                 items_processed += 1
        except queue.Empty: break
        except Exception as e: print(f"Error processing item from log queue: {e}")

    # Sort logged data by timestamp for each key
    for key in raw_log_data: raw_log_data[key].sort()
    print(f"Processed {items_processed} log events for keys: {list(raw_log_data.keys())}")


    # --- 4. Merge Performance and Log Data ---
    print("Merging performance samples with log data...")
    final_merged_data = [] # List of dictionaries for CSV
    last_log_indices = {key: 0 for key in raw_log_data} # Optimize lookup

    for perf_sample in performance_data:
        sample_ts, cpu_val, mem_val = perf_sample
        # Start with base performance data
        merged_row = {'Timestamp (s)': f"{sample_ts:.3f}", 'CPU (%)': cpu_val, 'Memory (MB)': mem_val}
        # Add placeholder for all potential log keys
        for key in log_metric_keys: merged_row[key] = None

        # Find the latest log value for each key that occurred *before or at* this sample's time
        for key, logged_events in raw_log_data.items():
            latest_val_for_key = None
            start_idx = last_log_indices[key]
            for i in range(start_idx, len(logged_events)):
                log_ts, log_val = logged_events[i]
                if log_ts <= sample_ts:
                    latest_val_for_key = log_val # Update latest value found so far
                    last_log_indices[key] = i # Remember index for next sample's search
                else:
                    break # Logs are sorted, no need to check further for this key/sample_ts
            merged_row[key] = latest_val_for_key # Assign the last found value

        final_merged_data.append(merged_row)
    print("Data merging complete.")


    # --- 5. Process Final Data (Summary & CSV) ---
    csv_headers = ['Timestamp (s)', 'CPU (%)', 'Memory (MB)'] + sorted(log_metric_keys)
    process_performance_data(final_merged_data, core_count, system_mem_info)
    save_data_to_csv(final_merged_data, args.output_csv, csv_headers)


    # --- 6. Kill Cobalt (Post-wait) ---
    print("\nAttempting final stop of Cobalt process...")
    if not kill_cobalt(args.adb_path, args.device, COBALT_PACKAGE_NAME): print("⚠️  Final stop command failed.")
    else: print("✅ Final stop command sent successfully.")


    # --- Finish ---
    print("\n--- Script Finished ---")
    if not run_completed_normally: print("Result: Script finished, but run may have been interrupted or process died."); sys.exit(1)
    else: print("Result: Script completed normally."); sys.exit(0)
