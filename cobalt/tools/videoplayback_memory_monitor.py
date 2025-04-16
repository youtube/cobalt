#!/usr/bin/env python3

import subprocess
import sys
import time
import argparse
import shlex
import re  # For parsing dumpsys/top output
import csv  # For saving data

# --- Configuration ---
ADB_EXECUTABLE = 'adb'
TARGET_DEVICE_SERIAL = None
COBALT_COMPONENT = 'dev.cobalt.coat/dev.cobalt.app.MainActivity'
COBALT_PACKAGE_NAME = COBALT_COMPONENT.split('/')[0]

# Sampling and display settings
SAMPLING_INTERVAL_SECONDS = 2  # How often to sample CPU/Memory
TIME_UPDATE_INTERVAL = 1  # How often to update the elapsed time display (can be more frequent than sampling)
PERFORMANCE_DATA_CSV_FILE = "cobalt_performance_data.csv"  # File to save raw data
# --- End Configuration ---


def run_adb_command(adb_path,
                    device_serial,
                    command_args,
                    timeout=30,
                    check=True,
                    verbose=True):
  """
    Helper function to build and run an ADB command using subprocess.
    Returns CompletedProcess object on success, None on failure.
    """
  full_adb_command = [adb_path]
  if device_serial:
    full_adb_command.extend(['-s', device_serial])
  full_adb_command.extend(command_args)

  if verbose:
    print("-" * 60)
    print("Executing ADB command:")
    try:
      print(f"  {' '.join(map(shlex.quote, full_adb_command))}")
    except AttributeError:
      print(f"  {' '.join(full_adb_command)}")
    print("-" * 60)

  try:
    result = subprocess.run(
        full_adb_command,
        check=check,
        capture_output=True,
        text=True,
        timeout=timeout)
    if verbose and result.stderr:
      print(f"--> stderr (potential warnings):\n{result.stderr.strip()}")
    return result
  except subprocess.CalledProcessError as e:
    if verbose:
      print(f"❌ Error: ADB command failed with return code {e.returncode}.")
      if e.stdout:
        print(f"--> stdout:\n{e.stdout.strip()}")
      if e.stderr:
        print(f"--> stderr:\n{e.stderr.strip()}")
    return None
  except FileNotFoundError:
    if verbose:
      print(f"❌ Error: The ADB executable ('{adb_path}') was not found.")
    return None
  except subprocess.TimeoutExpired:
    if verbose:
      print(f"❌ Error: ADB command timed out after {timeout} seconds.")
    return None
  except Exception as e:
    if verbose:
      print(f"❌ An unexpected error occurred during ADB execution: {e}")
    return None


# --- Performance Sampling Functions (with Enhanced Debugging) ---


def get_memory_usage(adb_path, device_serial, package_name):
  """
    Gets memory usage (PSS) for the package using 'dumpsys meminfo'.
    Includes verbose debugging output.
    Returns memory usage in MB, or None if an error occurs or data not found.
    """
  print("\nDEBUG(Mem): get_memory_usage called")  # Identify function call
  # Get FULL dumpsys output, parse in Python (more robust for debugging)
  meminfo_args = ['shell', 'dumpsys', 'meminfo', package_name]
  # Set verbose=False for run_adb_command to avoid double printing, handle output here
  result = run_adb_command(
      adb_path,
      device_serial,
      meminfo_args,
      timeout=15,
      check=False,
      verbose=False)

  if result and result.stdout:
    raw_output = result.stdout.strip()
    # --- PRINT RAW OUTPUT ---
    print(
        f"DEBUG(Mem): Received dumpsys meminfo output for {package_name}:\n---\n{raw_output}\n---"
    )  # Print full output

    if "No process found" in raw_output or not raw_output:
      print(
          "DEBUG(Mem): 'No process found' or empty output detected in dumpsys.")
      return None

    total_pss = None
    # Try regex for formats like "TOTAL PSS:  <value> kB" or "TOTAL PSS:      <value>"
    # Make matching more flexible regarding spaces and kB presence
    match_pss_direct = re.search(r"TOTAL PSS:\s+(\d+)(?:\s+kB)?", raw_output,
                                 re.IGNORECASE)
    if match_pss_direct:
      try:
        total_pss = int(match_pss_direct.group(1))
        print(f"DEBUG(Mem): Matched 'TOTAL PSS: VALUE', PSS={total_pss} kB")
      except ValueError:
        pass
    else:
      # Try regex for table format "TOTAL <pss_val> <uss_val> ..." (case-insensitive, multiline)
      match_pss_table = re.search(r"^\s*TOTAL\s+(\d+)", raw_output,
                                  re.MULTILINE | re.IGNORECASE)
      if match_pss_table:
        try:
          total_pss = int(match_pss_table.group(1))
          print(
              f"DEBUG(Mem): Matched table 'TOTAL VALUE ...', PSS={total_pss} kB"
          )
        except ValueError:
          pass

    if total_pss is not None:
      memory_mb = round(total_pss / 1024.0, 2)  # Convert kB to MB
      print(f"DEBUG(Mem): Successfully parsed PSS -> {memory_mb} MB"
           )  # Success message
      return memory_mb
    else:
      # --- Parsing Failed ---
      print("DEBUG(Mem): Failed to parse TOTAL PSS from the output above."
           )  # Explicit failure message
      return None  # Return None *after* printing raw output and failure message

  elif result is None:
    print(
        "DEBUG(Mem): run_adb_command failed for dumpsys meminfo (returned None). Check ADB connection/permissions."
    )
  else:  # result exists but stdout is empty
    print("DEBUG(Mem): dumpsys meminfo command ran but stdout was empty.")

  return None  # Return None if any issue


def get_cpu_usage(adb_path, device_serial, package_name):
  """
    Gets CPU usage percentage for the package using 'top -b -n 1'.
    Adjusted parsing based on observed output where State and CPU% are separate columns.
    Returns CPU %, or None if an error occurs or process not found.
    """
  print("\nDEBUG(CPU): get_cpu_usage called")  # Identify function call
  top_args = ['shell', 'top', '-b', '-n', '1']
  # Set verbose=False for run_adb_command, handle output here
  result = run_adb_command(
      adb_path, device_serial, top_args, timeout=10, check=False, verbose=False)

  if result and result.stdout:
    raw_output = result.stdout.strip()
    # --- PRINT RAW OUTPUT --- (Optional: Can comment out if no longer needed)
    # print(f"DEBUG(CPU): Received top -b -n 1 output:\n---\n{raw_output}\n---")

    lines = raw_output.split('\n')
    if not lines:
      print("DEBUG(CPU): 'top' output was empty after splitting lines.")
      return None

    header_line = None
    header_cpu_index = -1  # Index where '%CPU' was found in header
    # Find header line to locate %CPU column
    for line in lines:
      # Relaxed header check: look for PID and CPU (case-insensitive)
      if re.search(r"PID", line, re.IGNORECASE) and re.search(
          r"CPU", line, re.IGNORECASE):
        header_line = line.strip()
        # print(f"DEBUG(CPU): Found potential header line: '{header_line}'") # Optional debug
        try:
          headers = re.split(r'\s+', header_line)
          for i, h in enumerate(headers):
            if 'CPU'.casefold() in h.casefold():
              header_cpu_index = i
              # print(f"DEBUG(CPU): Found '%CPU' header text ('{h}') at index {header_cpu_index}.") # Optional debug
              break  # Found it
        except Exception as e:
          print(f"DEBUG(CPU): Error parsing header line: {e}")
        break  # Stop searching for header

    if header_cpu_index == -1:
      print(
          "DEBUG(CPU): Could not determine CPU% column index from header. Cannot parse CPU."
      )
      return None  # Cannot proceed reliably without the header index

    # --- MODIFICATION ---
    # Assume the actual CPU value is in the column AFTER the State/CPU header column index
    actual_cpu_value_index = header_cpu_index + 1
    # print(f"DEBUG(CPU): Header CPU index is {header_cpu_index}. Assuming actual value is at index {actual_cpu_value_index}.") # Optional debug

    # Find the process line and extract CPU% from the *actual* index
    process_line_found = False
    parsed_cpu = None
    for i, line in enumerate(lines):
      # Split line carefully
      fields = re.split(r'\s+', line.strip())
      # Check if package name is in the line or matches the last field (more robust)
      if package_name in line or (fields and fields[-1].endswith(package_name)):
        process_line_found = True
        # print(f"DEBUG(CPU): Found potential process line {i}: {line.strip()}") # Optional debug
        try:
          # Check bounds for the *actual* index we want to read
          if actual_cpu_value_index < len(fields):
            cpu_str = fields[actual_cpu_value_index].replace(
                '%', '')  # Get value from corrected index
            cpu_val = float(cpu_str)
            # print(f"DEBUG(CPU): Successfully parsed CPU from index {actual_cpu_value_index}: {cpu_val}%") # Optional debug
            parsed_cpu = cpu_val
            break  # Found and parsed, exit loop
          else:
            print(
                f"DEBUG(CPU): Actual CPU column index {actual_cpu_value_index} out of bounds for fields (len={len(fields)}) on process line."
            )
            # Break here, as this line format is unexpected
            break
        except (ValueError, IndexError) as e:
          print(
              f"DEBUG(CPU): Error parsing CPU value from index {actual_cpu_value_index}: {e}"
          )
          # Break here, error parsing this potential line
          break
        # If parsing failed for this specific line, stop searching for CPU on this line

    # After checking all lines
    if parsed_cpu is not None:
      return parsed_cpu  # Return the successfully parsed value

    # If we finish and haven't returned a value, report why
    if not process_line_found:
      print(
          f"DEBUG(CPU): Package name '{package_name}' not found in any line of top output."
      )
    # elif header_cpu_index == -1: # Already handled
    # pass
    else:  # Process line was found, but parsing failed (message printed in loop)
      print(
          f"DEBUG(CPU): Found process line(s) but failed to parse CPU% value from index {actual_cpu_value_index}."
      )

    return None  # Return None if parsing failed or process not found

  elif result is None:
    print(
        "DEBUG(CPU): run_adb_command failed for 'top' (returned None). Check ADB connection/permissions."
    )
  else:  # result exists but stdout is empty
    print("DEBUG(CPU): 'top' command ran but stdout was empty.")

  return None  # Return None if any issue


# --- Core Logic Functions (is_running, launch, kill - slightly adapted) ---


def is_cobalt_running(adb_path, device_serial, package_name):
  """Checks if process is running using 'pidof'."""
  print(f"\nChecking if process '{package_name}' is running...")
  pidof_args = ['shell', 'pidof', package_name]
  result = run_adb_command(
      adb_path,
      device_serial,
      pidof_args,
      timeout=10,
      check=False,
      verbose=False)
  if result is not None and result.returncode == 0 and result.stdout.strip():
    print(f"  Found running process (PID: {result.stdout.strip()}).")
    return True
  elif result is not None:
    print(f"  Process '{package_name}' not found.")
    return False
  else:
    print(f"  Could not determine process status (ADB error).")
    return False


def launch_cobalt(adb_path, device_serial, component, url):
  """Launches Cobalt. Returns True on success."""
  print(f"\nAttempting to launch Cobalt: {component}")
  args_value = f'--remote-allow-origins=*,--url="{url}"'
  print(f"Using commandLineArgs: {args_value}")
  am_start_args = [
      'shell', 'am', 'start', '--esa', 'commandLineArgs', args_value, component
  ]
  return run_adb_command(
      adb_path, device_serial, am_start_args, check=True) is not None


def kill_cobalt(adb_path, device_serial, package_name):
  """Force-stops Cobalt. Returns True if command sent ok."""
  print(f"\nAttempting to stop/kill process: {package_name}")
  am_kill_args = ['shell', 'am', 'force-stop', package_name]
  return run_adb_command(
      adb_path, device_serial, am_kill_args, check=False) is not None


# --- Data Handling ---


def save_data_to_csv(data, filename):
  """Saves the collected performance data to a CSV file."""
  if not data:
    print("No performance data to save.")
    return
  try:
    with open(filename, 'w', newline='') as csvfile:
      writer = csv.writer(csvfile)
      # Write header
      writer.writerow(['Timestamp (s)', 'CPU (%)', 'Memory (MB)'])
      # Write data rows
      for row in data:
        # Format None values as empty strings for CSV
        formatted_row = [
            f"{row[0]:.3f}",  # Timestamp
            f"{row[1]:.1f}" if row[1] is not None else '',  # CPU
            f"{row[2]:.2f}" if row[2] is not None else ''  # Memory
        ]
        writer.writerow(formatted_row)
    print(f"\nPerformance data saved to '{filename}'")
  except IOError as e:
    print(f"\n❌ Error saving data to CSV '{filename}': {e}")
  except Exception as e:
    print(f"\n❌ An unexpected error occurred during CSV saving: {e}")


def process_performance_data(data):
  """Calculates and prints summary statistics from performance data."""
  if not data:
    print("\nNo performance data collected to process.")
    return

  valid_cpu = [d[1] for d in data if d[1] is not None]
  valid_mem = [d[2] for d in data if d[2] is not None]

  print("\n--- Performance Summary ---")
  if valid_cpu:
    avg_cpu = sum(valid_cpu) / len(valid_cpu)
    max_cpu = max(valid_cpu)
    print(
        f"CPU Usage (%): Avg={avg_cpu:.1f}, Max={max_cpu:.1f} ({len(valid_cpu)} samples)"
    )
  else:
    print("CPU Usage (%): No valid samples collected.")

  if valid_mem:
    avg_mem = sum(valid_mem) / len(valid_mem)
    max_mem = max(valid_mem)
    print(
        f"Memory Usage (MB - PSS): Avg={avg_mem:.1f}, Max={max_mem:.1f} ({len(valid_mem)} samples)"
    )
  else:
    print("Memory Usage (MB - PSS): No valid samples collected.")
  print("---------------------------")


# --- Main script execution ---
if __name__ == "__main__":
  parser = argparse.ArgumentParser(
      description="Run Cobalt, sample performance, wait, and kill.",
      formatter_class=argparse.ArgumentDefaultsHelpFormatter)
  # ... (add arguments as before: --url, --duration, --adb-path, --device) ...
  parser.add_argument('--url', required=True, help='URL to load in Cobalt.')
  parser.add_argument(
      '--duration',
      required=True,
      type=int,
      help='Duration (seconds) to run Cobalt.')
  parser.add_argument(
      '--adb-path', default=ADB_EXECUTABLE, help='Path to ADB executable.')
  parser.add_argument(
      '--device',
      default=TARGET_DEVICE_SERIAL,
      help='Target device serial number.')
  parser.add_argument(
      '--sample-interval',
      type=int,
      default=SAMPLING_INTERVAL_SECONDS,
      help='Interval (seconds) for CPU/Memory sampling.')
  parser.add_argument(
      '--output-csv',
      default=PERFORMANCE_DATA_CSV_FILE,
      help='Filename for saving performance data.')

  args = parser.parse_args()

  print("--- Cobalt Performance Runner ---")
  # ... (print config details) ...
  print(f"Target URL: {args.url}")
  print(f"Run Duration: {args.duration} seconds")
  print(f"Sampling Interval: {args.sample_interval} seconds")
  print(f"Target Device: {'Default' if not args.device else args.device}")
  print(f"ADB Path: {args.adb_path}")
  print(f"Cobalt Package: {COBALT_PACKAGE_NAME}")
  print("-" * 31)

  # --- 0. Pre-launch Check & Kill ---
  if is_cobalt_running(args.adb_path, args.device, COBALT_PACKAGE_NAME):
    print("Attempting to stop existing process before launch...")
    if not kill_cobalt(args.adb_path, args.device, COBALT_PACKAGE_NAME):
      print("❌ Failed to stop existing process (ADB Error?). Exiting.")
      sys.exit(1)
    print("  Existing process stop command sent.")
    time.sleep(1)  # Allow time for process to die

  # --- 1. Launch Cobalt ---
  if not launch_cobalt(args.adb_path, args.device, COBALT_COMPONENT, args.url):
    print("\n❌ Failed to launch Cobalt. Exiting.")
    sys.exit(1)
  print("\n✅ Cobalt launch command sent successfully.")
  # Add a small delay to allow the app to initialize before first sample
  print("Waiting briefly for app to initialize...")
  time.sleep(3)

  # --- 2. Wait and Sample Performance ---
  print(
      f"\nStarting {args.duration}s run with sampling every {args.sample_interval}s..."
  )
  performance_data = []
  start_time = time.monotonic()
  end_time = start_time + args.duration
  run_completed_normally = True
  next_sample_time = start_time  # Schedule first sample immediately

  try:
    while True:
      current_time = time.monotonic()
      if current_time >= end_time:
        break  # Duration finished

      # --- Sampling ---
      # Check if it's time for the next sample
      if current_time >= next_sample_time:
        elapsed_time = current_time - start_time
        cpu = get_cpu_usage(args.adb_path, args.device, COBALT_PACKAGE_NAME)
        mem = get_memory_usage(args.adb_path, args.device, COBALT_PACKAGE_NAME)

        # Handle case where app might have died mid-run
        if cpu is None and mem is None:
          # Check if process still exists using pidof to be sure
          if not is_cobalt_running(args.adb_path, args.device,
                                   COBALT_PACKAGE_NAME):
            print(
                "\n⚠️ Process appears to have stopped unexpectedly during run. Stopping sampling."
            )
            run_completed_normally = False  # Mark as abnormal finish
            break  # Exit the sampling loop

        performance_data.append((elapsed_time, cpu, mem))

        # Schedule next sample
        next_sample_time += args.sample_interval

      # --- Time Update Display ---
      elapsed_display = time.monotonic() - start_time
      remaining_display = max(0, end_time - time.monotonic())
      # Get latest sample for display (handle empty list at start)
      latest_cpu_str = f"{performance_data[-1][1]:.1f}%" if performance_data and performance_data[
          -1][1] is not None else "N/A"
      latest_mem_str = f"{performance_data[-1][2]:.1f}MB" if performance_data and performance_data[
          -1][2] is not None else "N/A"

      print(
          f"\r  Elapsed: {elapsed_display:.1f}s / {args.duration}s | CPU: {latest_cpu_str} | Mem: {latest_mem_str} | Rem: {remaining_display:.1f}s  ",
          end='')

      # --- Sleep ---
      # Calculate sleep time: time until next sample or time until end, whichever is smaller
      # Also consider a minimum sleep to update the display regularly
      time_until_next_sample = max(0, next_sample_time - time.monotonic())
      time_until_end = max(0, end_time - time.monotonic())

      # Sleep for TIME_UPDATE_INTERVAL or until next sample/end, whichever is sooner
      sleep_duration = min(TIME_UPDATE_INTERVAL, time_until_next_sample,
                           time_until_end)

      if sleep_duration <= 0:
        # Prevent busy-waiting if sampling/update interval is very short or system is slow
        if time.monotonic(
        ) < end_time:  # Only sleep if not already past end time
          time.sleep(0.1)  # Short sleep to yield CPU
        continue  # Re-evaluate loop condition immediately

      time.sleep(sleep_duration)

    # Clear the updating line after the loop
    print("\r" + " " * 120 + "\r", end='')
    if run_completed_normally:
      print("  Run duration complete.")

  except KeyboardInterrupt:
    print("\n🛑 Sampling interrupted by user (Ctrl+C).")
    run_completed_normally = False

  # --- 3. Process Performance Data ---
  process_performance_data(performance_data)
  save_data_to_csv(performance_data, args.output_csv)

  # --- 4. Kill Cobalt (Post-wait) ---
  # Kill even if interrupted or run failed, to ensure cleanup
  print("\nAttempting final stop of Cobalt process...")
  if not kill_cobalt(args.adb_path, args.device, COBALT_PACKAGE_NAME):
    print("⚠️  Final stop command may have failed (ADB Error?).")
  else:
    print("✅ Final stop command sent successfully.")

  # --- Finish ---
  print("\n--- Script Finished ---")
  if not run_completed_normally:
    print("Result: Script finished, but run was interrupted or process died.")
    sys.exit(1)
  else:
    print("Result: Script completed normally.")
    sys.exit(0)
