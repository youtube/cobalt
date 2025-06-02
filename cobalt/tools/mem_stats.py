import subprocess
import re
import time
import threading
import csv  # For CSV writing
import os  # For path operations

# --- Configuration ---
COBALT_PACKAGE_NAME = "dev.cobalt.coat"
COBALT_ACTIVITY_NAME = "dev.cobalt.app.MainActivity"
COBALT_URL = "https://youtube.com/tv/watch?v=1La4QzGeaaQ"
MEMINFO_POLL_INTERVAL_SECONDS = 10
OUTPUT_DIRECTORY = "cobalt_monitoring_data"  # Directory to save CSV files

# Regex for logcat messages
LOGCAT_ALLOC_REGEX = re.compile(
    r"Allocated (\d+) bytes \(\d+%\) from a pool of capacity (\d+) bytes.")
# --- End Configuration ---

# Global list to store all data points and a lock for thread-safe appends
all_monitoring_data = []
data_lock = threading.Lock()
script_start_time_for_filename = time.strftime("%Y%m%d_%H%M%S",
                                               time.localtime())

# Event to signal threads to stop
stop_event = threading.Event()


def run_adb_command(command_list_or_str,
                    shell=False,
                    capture_output=True,
                    text=True,
                    timeout=30):
  try:
    if shell and not isinstance(command_list_or_str, str):
      command_to_run = " ".join(command_list_or_str)
    elif not shell and isinstance(command_list_or_str, str):
      command_to_run = command_list_or_str
    else:
      command_to_run = command_list_or_str
    process = subprocess.Popen(
        command_to_run,
        shell=shell,
        stdout=subprocess.PIPE if capture_output else None,
        stderr=subprocess.PIPE if capture_output else None,
        text=text,
        errors='replace')
    stdout, stderr = process.communicate(timeout=timeout)
    if process.returncode != 0:
      cmd_str_repr = command_to_run if isinstance(
          command_to_run, str) else ' '.join(command_to_run)
      is_expected_fail = ("pidof" in cmd_str_repr and process.returncode == 1) or \
                         ("dumpsys meminfo" in cmd_str_repr and ("No process found" in (stderr or "") or "No services match" in (stderr or "")))
      if not is_expected_fail and stderr:
        print(f"Stderr for '{cmd_str_repr}': {stderr.strip()}")
      return stdout.strip() if stdout else "", stderr.strip(
      ) if stderr else f"Command failed with return code {process.returncode}"
    return stdout.strip() if stdout else "", None
  except subprocess.TimeoutExpired:
    cmd_str_repr = command_list_or_str if isinstance(
        command_list_or_str, str) else ' '.join(command_list_or_str)
    print(f"Command timed out: {cmd_str_repr}")
    if 'process' in locals() and process.poll() is None:
      process.kill()
      process.communicate()
    return None, "Command timed out"
  except Exception as e:
    cmd_str_repr = command_list_or_str if isinstance(
        command_list_or_str, str) else ' '.join(command_list_or_str)
    print(f"Exception running command {cmd_str_repr}: {e}")
    return None, str(e)


def is_package_running(package_name):
  stdout, _ = run_adb_command(["adb", "shell", "pidof", package_name],
                              shell=False)
  return bool(stdout and stdout.strip().isdigit())


def stop_package(package_name):
  print(f"Attempting to stop package '{package_name}'...")
  _, stderr = run_adb_command(
      ["adb", "shell", "am", "force-stop", package_name], shell=False)
  if stderr:
    print(f"Error trying to stop package '{package_name}': {stderr}")
    return False
  print(f"Package '{package_name}' stop command issued.")
  time.sleep(2)
  return True


def launch_cobalt():
  print(
      f"Attempting to launch Cobalt ({COBALT_PACKAGE_NAME}) with URL: {COBALT_URL}..."
  )
  command_str = (
      f"adb shell am start -n {COBALT_PACKAGE_NAME}/{COBALT_ACTIVITY_NAME} "
      f"--esa commandLineArgs '--url=\"{COBALT_URL}\"'")
  stdout, stderr = run_adb_command(command_str, shell=True)
  if stderr:
    print(f"Error launching Cobalt: {stderr}")
  else:
    print("Cobalt launch command sent." +
          (f" Launch stdout: {stdout}" if stdout else ""))


def get_meminfo_data_internal():
  command = ["adb", "shell", "dumpsys", "meminfo", COBALT_PACKAGE_NAME]
  output, error = run_adb_command(command, shell=False)
  if error or not output:
    return None
  mem_data = {}
  lines = output.splitlines()
  in_app_summary_section = False
  in_main_table = False
  column_headers = []
  for i, line in enumerate(lines):
    line_stripped = line.strip()
    if not in_app_summary_section and "** MEMINFO in pid" in line and COBALT_PACKAGE_NAME in line:
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
          if len(h1_parts) >= 8 and len(
              h2_parts) >= 8:  # Common 8-column structure
            for k_idx in range(8):
              temp_headers.append(f"{h1_parts[k_idx]} {h2_parts[k_idx]}")
            column_headers = temp_headers
          else:
            column_headers = [
                "Pss Total", "Private Dirty", "Private Clean", "SwapPss Dirty",
                "Rss Total", "Heap Size", "Heap Alloc", "Heap Free"
            ]
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
    if not parts:
      continue
    value_start_idx = -1
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


def meminfo_monitor_loop():
  print("Starting meminfo monitoring thread...")
  iteration = 0
  global all_monitoring_data, data_lock

  while not stop_event.is_set():
    iteration += 1
    current_time_str = time.strftime("%Y-%m-%d %H:%M:%S", time.localtime())
    header_prefix = f"[{current_time_str}][Meminfo Iter: {iteration}]"

    if not is_package_running(COBALT_PACKAGE_NAME):
      print(
          f"{header_prefix} Cobalt ({COBALT_PACKAGE_NAME}) not running. Skipping meminfo."
      )
    else:
      raw_mem_data = get_meminfo_data_internal()
      if raw_mem_data:
        snapshot_for_csv = {"timestamp": current_time_str}
        print(f"{header_prefix} --- Meminfo (Pss Total in KB) ---")
        for category, data_map in raw_mem_data.items():
          pss_total = data_map.get("Pss Total")
          if pss_total is not None:
            snapshot_for_csv[f"{category} Pss Total"] = pss_total
            print(f"{header_prefix} {category}: {pss_total} KB")

        if len(snapshot_for_csv) > 1:
          with data_lock:
            all_monitoring_data.append(snapshot_for_csv)
      elif is_package_running(COBALT_PACKAGE_NAME):
        print(
            f"{header_prefix} Warning: No meminfo data parsed, though app is running."
        )

    for _ in range(MEMINFO_POLL_INTERVAL_SECONDS):
      if stop_event.is_set():
        break
      time.sleep(1)
  print("Meminfo monitoring thread stopped.")


def logcat_monitor_loop():
  print("Starting logcat monitoring thread...")
  run_adb_command(["adb", "logcat", "-c"], shell=False)
  global all_monitoring_data, data_lock
  logcat_process = None
  try:
    logcat_process = subprocess.Popen(["adb", "logcat", "-v", "brief"],
                                      stdout=subprocess.PIPE,
                                      stderr=subprocess.PIPE,
                                      text=True,
                                      bufsize=1,
                                      universal_newlines=True,
                                      errors='replace')
    print("Continuously monitoring logcat...")
    while not stop_event.is_set():
      if logcat_process.stdout:
        line = ""
        try:
          line = logcat_process.stdout.readline()
          if not line and logcat_process.poll() is not None:
            break
        except:
          if stop_event.is_set():
            break
          print("Error reading logcat line.")
          break
        line = line.strip()
        if line:
          match = LOGCAT_ALLOC_REGEX.search(line)
          if match:
            ts = time.strftime("%Y-%m-%d %H:%M:%S", time.localtime())
            try:
              alloc_kb = int(match.group(1)) / 1024.0
              cap_kb = int(match.group(2)) / 1024.0
              # Updated print statement for console
              print(
                  f"[{ts}] [Logcat Monitor] DecoderBufferAllocator Allocated: {alloc_kb:.2f} KB, Capacity: {cap_kb:.2f} KB"
              )
              log_entry_for_csv = {
                  "timestamp": ts,
                  "DecoderBufferAllocator Allocated KB":
                      alloc_kb,  # Renamed key
                  "DecoderBufferAllocator Capacity KB": cap_kb  # Renamed key
              }
              with data_lock:
                all_monitoring_data.append(log_entry_for_csv)
            except ValueError:
              print(
                  f"[{ts}] [Logcat Monitor] Error converting byte values: {match.group(1)}, {match.group(2)}"
              )
      else:
        break
      time.sleep(0.01)
  except Exception as e:
    if not stop_event.is_set():
      print(f"Logcat monitoring error: {e}")
  finally:
    if logcat_process and logcat_process.poll() is None:
      logcat_process.terminate()
      try:
        logcat_process.wait(timeout=2)
      except subprocess.TimeoutExpired:
        logcat_process.kill()
    print("Logcat monitoring thread stopped.")


def write_combined_csv_file():
  """Writes all collected data to a single CSV file."""
  global all_monitoring_data, script_start_time_for_filename, OUTPUT_DIRECTORY

  if not all_monitoring_data:
    print("No data collected to write to CSV.")
    return

  if not os.path.exists(OUTPUT_DIRECTORY):
    try:
      os.makedirs(OUTPUT_DIRECTORY)
      print(f"Created output directory: {OUTPUT_DIRECTORY}")
    except OSError as e:
      print(
          f"Error creating output directory {OUTPUT_DIRECTORY}: {e}. Saving to current directory."
      )
      OUTPUT_DIRECTORY = "."

  combined_filename = os.path.join(
      OUTPUT_DIRECTORY,
      f"combined_cobalt_data_{script_start_time_for_filename}.csv")
  print(f"Writing combined monitoring data to {combined_filename}...")

  fieldnames_set = set(["timestamp"])
  # Use the new, more descriptive logcat keys for ordering and checking
  logcat_specific_keys = [
      "DecoderBufferAllocator Allocated KB",
      "DecoderBufferAllocator Capacity KB"
  ]

  for key in logcat_specific_keys:
    fieldnames_set.add(key)  # Ensure they are part of the set if data exists

  meminfo_keys_found = set()
  for row_data in all_monitoring_data:
    for key in row_data.keys():
      # Identify meminfo keys (anything not timestamp and not our specific logcat keys)
      if key not in ["timestamp"] and key not in logcat_specific_keys:
        meminfo_keys_found.add(key)
      fieldnames_set.add(key)  # Add all keys found in data to the overall set

  sorted_meminfo_keys = sorted(list(meminfo_keys_found))

  final_fieldnames = ["timestamp"]
  for lk in logcat_specific_keys:  # Add logcat keys first (if they were in data)
    if lk in fieldnames_set:
      final_fieldnames.append(lk)

  for mk in sorted_meminfo_keys:  # Then add all unique meminfo keys
    if mk not in final_fieldnames:  # Avoid duplicates if a meminfo key somehow clashed
      final_fieldnames.append(mk)

  # Ensure all keys from fieldnames_set are included, just in case logic above missed one
  # This is a safety net, the above logic should be sufficient.
  for key_in_set in sorted(list(fieldnames_set)):
    if key_in_set not in final_fieldnames:
      final_fieldnames.append(key_in_set)

  try:
    with open(combined_filename, 'w', newline='', encoding='utf-8') as csvfile:
      writer = csv.DictWriter(csvfile, fieldnames=final_fieldnames, restval='')
      writer.writeheader()
      # Sort data by timestamp before writing for a chronological CSV
      sorted_data = sorted(all_monitoring_data, key=lambda x: x['timestamp'])
      writer.writerows(sorted_data)
    print(f"Combined data successfully written to {combined_filename}")
  except IOError as e:
    print(f"Error writing combined CSV: {e}")
  except Exception as e:
    print(f"An unexpected error occurred during CSV writing: {e}")


def main():
  global script_start_time_for_filename
  script_start_time_for_filename = time.strftime("%Y%m%d_%H%M%S",
                                                 time.localtime())

  print("Checking ADB connection and devices...")
  stdout_dev, stderr_dev = run_adb_command(["adb", "devices"], shell=False)
  if stderr_dev or not stdout_dev or "List of devices attached" not in stdout_dev:
    print(f"ADB is not working or no devices/emulators found. Exiting.")
    return
  if len(stdout_dev.strip().splitlines()) <= 1:
    print(f"No devices/emulators found attached and authorized. Exiting.")
    return

  if is_package_running(COBALT_PACKAGE_NAME):
    print(f"'{COBALT_PACKAGE_NAME}' is running. Attempting to stop it first.")
    stop_package(COBALT_PACKAGE_NAME)

  launch_cobalt()
  startup_wait_time = 10
  print(f"\nWaiting {startup_wait_time} seconds for Cobalt to initialize...")
  time.sleep(startup_wait_time)

  if not is_package_running(COBALT_PACKAGE_NAME):
    print(
        f"Cobalt ({COBALT_PACKAGE_NAME}) did not start successfully. Exiting monitoring."
    )
    return
  print(
      f"Cobalt ({COBALT_PACKAGE_NAME}) appears to be running. Starting monitors."
  )

  meminfo_thread = threading.Thread(target=meminfo_monitor_loop, daemon=True)
  logcat_thread = threading.Thread(target=logcat_monitor_loop, daemon=True)

  meminfo_thread.start()
  logcat_thread.start()

  print(
      f"\nMeminfo polling every {MEMINFO_POLL_INTERVAL_SECONDS}s. Logcat monitoring active."
  )
  print(f"CSV file will be saved in '{OUTPUT_DIRECTORY}' on exit.")
  print("Press Ctrl+C to stop all monitoring and save data.")

  try:
    while True:
      time.sleep(1)
      if not (meminfo_thread.is_alive() or logcat_thread.is_alive()):
        print("Both monitoring threads seem to have stopped.")
        break
  except KeyboardInterrupt:
    print("\nCtrl+C received. Signaling threads to stop...")
  except Exception as e:
    print(f"Main loop encountered an error: {e}")
  finally:
    stop_event.set()

    print("Waiting for monitoring threads to complete...")
    if meminfo_thread.is_alive():
      meminfo_thread.join(timeout=MEMINFO_POLL_INTERVAL_SECONDS + 5)
    if logcat_thread.is_alive():
      logcat_thread.join(timeout=5)

    print("\nAll monitoring stopped.")
    write_combined_csv_file()
    print("Script finished.")


if __name__ == "__main__":
  main()
