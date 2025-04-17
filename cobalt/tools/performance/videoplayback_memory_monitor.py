#!/usr/bin/env python3

# Example usage:
# python3 cobalt/tools/videoplayback_memory_monitor.py --url "https://youtube.com/tv/watch?absolute_experiments=9415422&v=1La4QzGeaaQ" --duration 30
# will output a file cobalt_performance_data.csv
# gnuplot -e "datafile='cobalt_performance_data.csv'; outfile='cobalt_performance_data_8k60.png'; cores=4; mem_total=3869" cobalt/tools/videoplayback_memory_graph.gp

import subprocess
import sys
import time
import argparse
import shlex
import re
import csv
import statistics

# --- Configuration ---
ADB_EXECUTABLE = 'adb'
TARGET_DEVICE_SERIAL = None
COBALT_COMPONENT = 'dev.cobalt.coat/dev.cobalt.app.MainActivity'
COBALT_PACKAGE_NAME = COBALT_COMPONENT.split('/')[0]
DEFAULT_SAMPLING_INTERVAL = 2
DEFAULT_CSV_FILENAME = "cobalt_performance_data.csv"
TIME_UPDATE_INTERVAL = 1
# --- End Configuration ---


def run_adb_command(adb_path,
                    device_serial,
                    command_args,
                    timeout=30,
                    check=True,
                    verbose=True):
  # ... (function remains the same as the previous 'cleaned' version) ...
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


# --- System Info Functions ---


def get_cpu_core_count(adb_path, device_serial):
  """Gets the number of logical CPU cores reported by the system."""
  # Counts cpu[0-9] entries in /sys/devices/system/cpu/
  # Using && echo to ensure grep result is captured even if ls has stderr noise
  core_cmd_args = [
      'shell', 'ls /sys/devices/system/cpu/ | grep -E \'^cpu[0-9]+$\' | wc -l'
  ]
  # Run quietly, check=False as wc -l might return 0 lines found (not error)
  result = run_adb_command(
      adb_path,
      device_serial,
      core_cmd_args,
      timeout=10,
      check=False,
      verbose=False)

  if result and result.stdout:
    try:
      count = int(result.stdout.strip())
      if count > 0:
        return count
      else:
        print(
            "\nWarning(CPU Count): Found 0 cores via sysfs, trying /proc/cpuinfo."
        )
        # Fallback or alternative method if needed (parsing /proc/cpuinfo)
        # Example fallback (simpler, might count hyperthreads differently):
        proc_cmd_args = ['shell', 'cat /proc/cpuinfo | grep -c ^processor']
        result = run_adb_command(
            adb_path,
            device_serial,
            proc_cmd_args,
            timeout=10,
            check=False,
            verbose=False)
        if result and result.stdout:
          try:
            count = int(result.stdout.strip())
            if count > 0:
              return count
          except ValueError:
            pass

    except ValueError:
      print(
          f"\nWarning(CPU Count): Could not parse core count integer from output: {result.stdout.strip()}"
      )
  # If command failed or parsing failed
  print("\nWarning(CPU Count): Could not determine CPU core count.")
  return None


def get_system_memory_info(adb_path, device_serial):
  """Gets total and available system memory from /proc/meminfo."""
  mem_cmd_args = ['shell', 'cat', '/proc/meminfo']
  # Run quietly
  result = run_adb_command(
      adb_path,
      device_serial,
      mem_cmd_args,
      timeout=10,
      check=False,
      verbose=False)
  mem_info = {}

  if result and result.stdout:
    try:
      for line in result.stdout.strip().split('\n'):
        if line.startswith("MemTotal:"):
          mem_info['total_kb'] = int(line.split()[1])
        elif line.startswith("MemAvailable:"):
          mem_info['available_kb'] = int(line.split()[1])
      # Check if we found the values
      if 'total_kb' in mem_info and 'available_kb' in mem_info:
        return {
            'total_mb': round(mem_info['total_kb'] / 1024.0, 1),
            'available_mb': round(mem_info['available_kb'] / 1024.0, 1)
        }
      else:
        print(
            "\nWarning(SysMem): Could not find MemTotal/MemAvailable in /proc/meminfo output."
        )
    except (ValueError, IndexError) as e:
      print(f"\nWarning(SysMem): Error parsing /proc/meminfo: {e}")
  else:
    print("\nWarning(SysMem): Failed to execute or read /proc/meminfo.")

  return None


# --- Performance Sampling Functions ---
# (get_memory_usage and get_cpu_usage remain the same as the previous 'cleaned' version)
def get_memory_usage(adb_path, device_serial, package_name):
  # ... (same as before) ...
  meminfo_args = ['shell', 'dumpsys', 'meminfo', package_name]
  result = run_adb_command(
      adb_path,
      device_serial,
      meminfo_args,
      timeout=15,
      check=False,
      verbose=False)
  if result and result.stdout:
    raw_output = result.stdout.strip()
    if "No process found" in raw_output or not raw_output:
      return None
    total_pss = None
    match_pss_direct = re.search(r"TOTAL PSS:\s+(\d+)(?:\s+kB)?", raw_output,
                                 re.IGNORECASE)
    if match_pss_direct:
      try:
        total_pss = int(match_pss_direct.group(1))
      except ValueError:
        pass
    else:
      match_pss_table = re.search(r"^\s*TOTAL\s+(\d+)", raw_output,
                                  re.MULTILINE | re.IGNORECASE)
      if match_pss_table:
        try:
          total_pss = int(match_pss_table.group(1))
        except ValueError:
          pass
    if total_pss is not None:
      return round(total_pss / 1024.0, 2)
    else:
      if "TOTAL" in raw_output.upper():
        print(
            f"\nWarning(Mem): Could not parse PSS from dumpsys output for {package_name}."
        )
      return None
  return None


def get_cpu_usage(adb_path, device_serial, package_name):
  # ... (same as before) ...
  top_args = ['shell', 'top', '-b', '-n', '1']
  result = run_adb_command(
      adb_path, device_serial, top_args, timeout=10, check=False, verbose=False)
  if result and result.stdout:
    raw_output = result.stdout.strip()
    lines = raw_output.split('\n')
    if not lines:
      return None
    header_cpu_index = -1
    for line in lines:
      if re.search(r"PID", line, re.IGNORECASE) and re.search(
          r"CPU", line, re.IGNORECASE):
        try:
          headers = re.split(r'\s+', line.strip())
          for i, h in enumerate(headers):
            if 'CPU'.casefold() in h.casefold():
              header_cpu_index = i
              break
        except Exception:
          pass
        break
    if header_cpu_index == -1:
      return None
    actual_cpu_value_index = header_cpu_index + 1
    for line in lines:
      fields = re.split(r'\s+', line.strip())
      if package_name in line or (fields and fields[-1].endswith(package_name)):
        try:
          if actual_cpu_value_index < len(fields):
            return float(fields[actual_cpu_value_index].replace('%', ''))
        except (ValueError, IndexError):
          pass
        return None  # Found line but failed parse/index
    return None  # Process name not found
  return None


# --- Core Logic Functions ---
# (is_cobalt_running, launch_cobalt, kill_cobalt remain the same)
def is_cobalt_running(adb_path, device_serial, package_name):
  pidof_args = ['shell', 'pidof', package_name]
  result = run_adb_command(
      adb_path,
      device_serial,
      pidof_args,
      timeout=10,
      check=False,
      verbose=False)
  return result is not None and result.returncode == 0 and result.stdout.strip()


def launch_cobalt(adb_path, device_serial, component, url):
  print(f"\nAttempting to launch Cobalt component: {component}")
  args_value = f'--remote-allow-origins=*,--url="{url}"'
  print(f"Using commandLineArgs: {args_value}")
  am_start_args = [
      'shell', 'am', 'start', '--esa', 'commandLineArgs', args_value, component
  ]
  return run_adb_command(
      adb_path, device_serial, am_start_args, check=True,
      verbose=True) is not None


def kill_cobalt(adb_path, device_serial, package_name):
  print(f"\nAttempting to stop/kill process: {package_name}")
  am_kill_args = ['shell', 'am', 'force-stop', package_name]
  return run_adb_command(
      adb_path, device_serial, am_kill_args, check=False,
      verbose=True) is not None


# --- Data Handling ---
def save_data_to_csv(data, filename):
  # ... (function remains the same) ...
  if not data:
    print("No performance data to save.")
    return
  try:
    with open(filename, 'w', newline='') as csvfile:
      writer = csv.writer(csvfile)
      writer.writerow(['Timestamp (s)', 'CPU (%)', 'Memory (MB)'])
      for row in data:
        writer.writerow([
            f"{row[0]:.3f}", f"{row[1]:.1f}" if row[1] is not None else '',
            f"{row[2]:.2f}" if row[2] is not None else ''
        ])
    print(f"\nPerformance data saved to '{filename}'")
  except IOError as e:
    print(f"\n❌ Error saving data to CSV '{filename}': {e}")
  except Exception as e:
    print(f"\n❌ An unexpected error occurred during CSV saving: {e}")


def process_performance_data(data, core_count, system_mem_info):  # Added args
  """Calculates and prints summary statistics, incorporating system info."""
  if not data:
    print("\nNo performance data collected to process.")
    return

  valid_cpu = [d[1] for d in data if d[1] is not None]
  valid_mem = [d[2] for d in data if d[2] is not None]

  print("\n--- Performance Summary ---")

  # CPU Summary
  if valid_cpu:
    avg_cpu = statistics.mean(valid_cpu)
    max_cpu = max(valid_cpu)
    print(
        f"CPU Usage (% of 1 Core): Avg={avg_cpu:.1f}, Max={max_cpu:.1f} ({len(valid_cpu)} samples)"
    )
    if core_count:
      # Calculate % of total system CPU capacity
      avg_cpu_total = (avg_cpu / core_count
                      )  # Already a percentage if avg_cpu is 100 = 1 core
      max_cpu_total = (max_cpu / core_count)
      print(
          f"CPU Usage (% of Total):  Avg={avg_cpu_total:.1f}, Max={max_cpu_total:.1f} (based on {core_count} cores)"
      )
    else:
      print("CPU Usage (% of Total):  N/A (core count unknown)")
  else:
    print("CPU Usage:               No valid samples collected.")

  print("-" * 27)  # Separator

  # Memory Summary
  if valid_mem:
    avg_mem = statistics.mean(valid_mem)
    max_mem = max(valid_mem)
    print(
        f"Memory Usage (MB PSS):   Avg={avg_mem:.1f}, Max={max_mem:.1f} ({len(valid_mem)} samples)"
    )
    if system_mem_info:
      # Show Max PSS as % of Total/Available system memory
      max_mem_perc_total = (max_mem / system_mem_info['total_mb']
                           ) * 100 if system_mem_info['total_mb'] else 0
      max_mem_perc_avail = (max_mem / system_mem_info['available_mb']
                           ) * 100 if system_mem_info['available_mb'] else 0
      print(
          f"Max Memory as % of Sys:  {max_mem_perc_total:.1f}% (Total RAM), {max_mem_perc_avail:.1f}% (Available RAM)"
      )
      print(
          f"  (System RAM: Total={system_mem_info['total_mb']:.0f}MB, Available={system_mem_info['available_mb']:.0f}MB)"
      )
    else:
      print("Max Memory as % of Sys:  N/A (system memory info unknown)")
  else:
    print("Memory Usage (MB PSS):   No valid samples collected.")
  print("---------------------------")


# --- Main script execution ---
if __name__ == "__main__":
  parser = argparse.ArgumentParser(
      description="Run Cobalt, sample performance, wait, print updates, and kill.",
      formatter_class=argparse.ArgumentDefaultsHelpFormatter)
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
      default=DEFAULT_SAMPLING_INTERVAL,
      help='Interval (seconds) for CPU/Memory sampling.')
  parser.add_argument(
      '--output-csv',
      default=DEFAULT_CSV_FILENAME,
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
  print(f"Output CSV: {args.output_csv}")
  print("-" * 31)

  # --- Get System Info ---
  print("\n--- Retrieving System Info ---")
  core_count = get_cpu_core_count(args.adb_path, args.device)
  system_mem_info = get_system_memory_info(args.adb_path, args.device)

  if core_count:
    print(f"Detected CPU Cores: {core_count}")
  else:
    print("Warning: Could not determine CPU core count.")

  if system_mem_info:
    print(
        f"System Memory: Total={system_mem_info['total_mb']:.0f}MB, Available={system_mem_info['available_mb']:.0f}MB (at script start)"
    )
  else:
    print("Warning: Could not determine system memory info.")
  print("-----------------------------")

  # --- 0. Pre-launch Check & Kill ---
  # ... (same as before) ...
  print("\nChecking if Cobalt process is already running...")
  if is_cobalt_running(args.adb_path, args.device, COBALT_PACKAGE_NAME):
    print(
        f"  Process found. Attempting to stop existing '{COBALT_PACKAGE_NAME}' process..."
    )
    if not kill_cobalt(args.adb_path, args.device, COBALT_PACKAGE_NAME):
      print(
          "\n❌ Failed to stop the existing Cobalt process (ADB Error?). Exiting."
      )
      sys.exit(1)
    print("  Existing process stop command sent.")
    time.sleep(1.5)
  else:
    print(f"  No existing '{COBALT_PACKAGE_NAME}' process found.")

  # --- 1. Launch Cobalt ---
  # ... (same as before) ...
  if not launch_cobalt(args.adb_path, args.device, COBALT_COMPONENT, args.url):
    print("\n❌ Failed to launch Cobalt. Exiting.")
    sys.exit(1)
  print("\n✅ Cobalt launch command sent successfully.")
  print("Waiting briefly for app to initialize...")
  time.sleep(4)

  # --- 2. Wait and Sample Performance ---
  # ... (loop logic remains the same as the previous 'cleaned' version) ...
  print(
      f"\nStarting {args.duration}s run, sampling every {args.sample_interval}s..."
  )
  performance_data = []
  start_time = time.monotonic()
  end_time = start_time + args.duration
  run_completed_normally = True
  next_sample_time = start_time
  try:
    while True:
      current_time = time.monotonic()
      elapsed_time = current_time - start_time
      if current_time >= end_time:
        break
      if current_time >= next_sample_time:
        cpu = get_cpu_usage(args.adb_path, args.device, COBALT_PACKAGE_NAME)
        mem = get_memory_usage(args.adb_path, args.device, COBALT_PACKAGE_NAME)
        performance_data.append((elapsed_time, cpu, mem))
        if cpu is None and mem is None and len(
            performance_data) > 1 and performance_data[-2][
                1] is None and performance_data[-2][2] is None:
          if not is_cobalt_running(args.adb_path, args.device,
                                   COBALT_PACKAGE_NAME):
            print(
                "\n⚠️ Process appears to have stopped unexpectedly. Stopping sampling."
            )
            run_completed_normally = False
            break
        next_sample_time += args.sample_interval
      remaining_display = max(0, end_time - time.monotonic())
      latest_cpu_str = f"{performance_data[-1][1]:.1f}%" if performance_data and performance_data[
          -1][1] is not None else "N/A"
      latest_mem_str = f"{performance_data[-1][2]:.1f}MB" if performance_data and performance_data[
          -1][2] is not None else "N/A"
      print(
          f"\r  Elapsed: {elapsed_time:.1f}s / {args.duration}s | CPU: {latest_cpu_str} | Mem: {latest_mem_str} | Rem: {remaining_display:.1f}s  ",
          end='')
      time_until_next_sample = max(0, next_sample_time - time.monotonic())
      time_until_end = max(0, end_time - time.monotonic())
      sleep_duration = min(TIME_UPDATE_INTERVAL, time_until_next_sample,
                           time_until_end)
      if sleep_duration <= 0.01 and time.monotonic() < end_time:
        time.sleep(0.01)
      elif sleep_duration > 0:
        time.sleep(sleep_duration)
    print("\r" + " " * 120 + "\r", end='')
    if run_completed_normally:
      print("  Run duration complete.")
  except KeyboardInterrupt:
    print("\n🛑 Sampling interrupted by user (Ctrl+C).")
    run_completed_normally = False

  # --- 3. Process Performance Data ---
  process_performance_data(performance_data, core_count,
                           system_mem_info)  # Pass system info
  save_data_to_csv(performance_data, args.output_csv)

  # --- 4. Kill Cobalt (Post-wait) ---
  # ... (same as before) ...
  print("\nAttempting final stop of Cobalt process...")
  if not kill_cobalt(args.adb_path, args.device, COBALT_PACKAGE_NAME):
    print(
        "⚠️  Final stop command may have failed (ADB Error or app already stopped)."
    )
  else:
    print("✅ Final stop command sent successfully.")

  # --- Finish ---
  # ... (same as before) ...
  print("\n--- Script Finished ---")
  if not run_completed_normally:
    print(
        "Result: Script finished, but run may have been interrupted or process died."
    )
    sys.exit(1)
  else:
    print("Result: Script completed normally.")
    sys.exit(0)
