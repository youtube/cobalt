import subprocess
import re
import time
import argparse
from datetime import datetime
from collections import deque, defaultdict

# example:
# python cobalt/tools/pss_plot.py dev.cobalt.coat -i 30 -mp 0

# Use a non-interactive backend for matplotlib, suitable for remote/headless use.
import matplotlib

matplotlib.use('Agg')
import matplotlib.pyplot as plt
import matplotlib.dates as mdates

# Define the categories we want to parse and the regex patterns to find them.
CATEGORIES_TO_PARSE = {
    # Order here can influence parsing preference if a line matches multiple, but unlikely with these.
    # The plotting order will be defined separately for stacking.
    "Java Heap": [r"^\s*Dalvik Heap", r"^\s*ART Heap"],
    "Native Heap": [r"^\s*Native Heap"],
    "Graphics": [
        r"^\s*Graphics",  # Check for an explicit "Graphics" summary line first
        r"^\s*Gfx dev",
        r"^\s*EGL mtrack",
        r"^\s*GL mtrack"
    ],
    "Stack": [r"^\s*Stack"],
    "Code": [
        r"^\s*\.so mmap", r"^\s*\.jar mmap", r"^\s*\.apk mmap",
        r"^\s*\.dex mmap", r"^\s*\.oat mmap", r"^\s*\.art mmap"
    ],
    "Others":
        [  # Order matters: "Unknown" should be less specific than others if overlap
            r"^\s*Dalvik Other", r"^\s*\.ttf mmap", r"^\s*Other mmap",
            r"^\s*Ashmem", r"^\s*Other dev", r"^\s*Unknown"
        ],
    # TOTAL PSS is handled specially due to its unique line structure
}

# Specific handling for TOTAL PSS line format
TOTAL_PSS_LINE_START = r"^\s*TOTAL"  # Line starts with TOTAL
TOTAL_PSS_VALUE_REGEX = r"^\s*TOTAL\s+([\d,]+)"  # PSS is the first number after TOTAL
TOTAL_PSS_EXPLICIT_REGEX = r"TOTAL PSS:\s*([\d,]+)\s*kB"  # Explicit "TOTAL PSS: ... kB" line


def parse_pss_from_line(line_content, category_patterns):
  """Helper to parse PSS from a line using a list of regex patterns for that category."""
  for pattern_str in category_patterns:
    # Regex: ^\s*PATTERN_TEXT\s+PSS_VALUE(\s+.*)?$
    # PSS_VALUE is the first number after the pattern text.
    # It should handle cases where PSS is the only number or followed by others.
    match = re.search(pattern_str + r"\s+([\d,]+)(?:\s+|$)", line_content,
                      re.IGNORECASE)
    if match:
      try:
        return int(match.group(1).replace(',', ''))
      except ValueError:
        return 0  # Should not happen if regex is correct
  return 0


def get_memory_details_kb(package_name):
  try:
    command = ["adb", "shell", "dumpsys", "meminfo", package_name,
               "-S"]  # -S for summary view, often more stable
    process = subprocess.Popen(
        command,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        encoding='utf-8',
        errors='replace')
    stdout, stderr = process.communicate(timeout=20)

    if process.returncode != 0:
      print(f"Error executing ADB command: {stderr.strip()}")
      if "No process found" in stderr:
        print(f"Package '{package_name}' not found or not running.")
      return None

    results = defaultdict(int)
    lines = stdout.splitlines()
    parsed_categories_on_current_run = set()

    # First, try to get TOTAL PSS as it's crucial
    total_pss_found_on_this_run = False
    for line in lines:
      line_stripped = line.strip()
      if line_stripped.startswith("TOTAL"):  # Broad check first
        match_explicit = re.search(TOTAL_PSS_EXPLICIT_REGEX, line_stripped)
        if match_explicit:
          results["TOTAL PSS"] = int(match_explicit.group(1).replace(',', ''))
          total_pss_found_on_this_run = True
          break

        match_general_total = re.search(TOTAL_PSS_VALUE_REGEX, line_stripped)
        if match_general_total:
          results["TOTAL PSS"] = int(
              match_general_total.group(1).replace(',', ''))
          total_pss_found_on_this_run = True
          break

    if not total_pss_found_on_this_run:
      # Try again without -S if total PSS wasn't found with it
      command = ["adb", "shell", "dumpsys", "meminfo", package_name]
      process = subprocess.Popen(
          command,
          stdout=subprocess.PIPE,
          stderr=subprocess.PIPE,
          text=True,
          encoding='utf-8',
          errors='replace')
      stdout, stderr = process.communicate(timeout=20)
      if process.returncode == 0:
        lines = stdout.splitlines()  # Re-assign lines
        for line in lines:
          line_stripped = line.strip()
          if line_stripped.startswith("TOTAL"):
            match_explicit = re.search(TOTAL_PSS_EXPLICIT_REGEX, line_stripped)
            if match_explicit:
              results["TOTAL PSS"] = int(
                  match_explicit.group(1).replace(',', ''))
              total_pss_found_on_this_run = True
              break
            match_general_total = re.search(TOTAL_PSS_VALUE_REGEX,
                                            line_stripped)
            if match_general_total:
              results["TOTAL PSS"] = int(
                  match_general_total.group(1).replace(',', ''))
              total_pss_found_on_this_run = True
              break
      else:
        print(f"Error executing ADB command (fallback): {stderr.strip()}")
        return None

    if not total_pss_found_on_this_run:
      print(
          f"Could not find TOTAL PSS value in dumpsys output for {package_name}."
      )
      return None

    # Process other defined categories
    for line in lines:
      line_stripped = line.strip()
      # Skip already processed TOTAL PSS line or empty lines
      if line_stripped.startswith("TOTAL") or not line_stripped:
        continue

      for category_name, patterns in CATEGORIES_TO_PARSE.items():
        pss_value = parse_pss_from_line(line_stripped, patterns)
        if pss_value > 0:
          # If a line matches multiple patterns for *different* sum-up categories,
          # this simple loop might double count. More robust would be to ensure a line
          # contributes to only one primary category group if patterns are very generic.
          # For now, assuming patterns are distinct enough or summed categories are okay.
          results[category_name] += pss_value
          parsed_categories_on_current_run.add(category_name)
          break  # Line matched one category definition, move to next line

    # Ensure all defined categories have a default value of 0 if not found
    for category_name in CATEGORIES_TO_PARSE.keys():
      if category_name not in results:
        results[category_name] = 0

    return results

  except subprocess.TimeoutExpired:
    print("Error: ADB command timed out.")
    return None
  except FileNotFoundError:
    print("Error: 'adb' command not found. Make sure it's in your PATH.")
    return None
  except Exception as e:
    print(f"An unexpected error occurred in get_memory_details_kb: {e}")
    return None


def generate_pss_graph(all_memory_data, package_name, output_filename,
                       stack_order_pref):
  timestamps_deque = all_memory_data.get("Timestamps")
  if not timestamps_deque or len(
      timestamps_deque) < 2:  # Need at least 2 points for a meaningful plot
    print("Not enough data collected to generate a graph.")
    return

  try:
    timestamps = list(timestamps_deque)
    fig, ax = plt.subplots(figsize=(18, 9))

    # Prepare data for stackplot
    y_data_for_stack = []
    stack_labels = []

    # Use a copy of all_memory_data keys, excluding Timestamps and TOTAL PSS for stacking
    component_keys = [
        k for k in CATEGORIES_TO_PARSE.keys() if k in all_memory_data
    ]

    # Order the components for stacking (bottom to top)
    # You can customize this order if needed
    # Ensure all keys in stack_order_pref exist in component_keys
    ordered_stack_components = [
        cat for cat in stack_order_pref if cat in component_keys
    ]
    # Add any remaining keys not in stack_order_pref (though they should be)
    for cat in component_keys:
      if cat not in ordered_stack_components:
        ordered_stack_components.append(cat)

    for category_name in ordered_stack_components:
      # Ensure all component data lists are the same length as timestamps
      # The deque should handle this if data was consistently added
      component_pss_mb_list = list(all_memory_data[category_name])
      if len(component_pss_mb_list) == len(timestamps):
        y_data_for_stack.append(component_pss_mb_list)
        stack_labels.append(category_name)
      else:
        print(
            f"Warning: Data length mismatch for '{category_name}'. Expected {len(timestamps)}, got {len(component_pss_mb_list)}. Skipping this component in stack."
        )

    if not y_data_for_stack:
      print(
          "No component data prepared for stacking. Aborting graph generation.")
      return

    # Define colors for the stack plot
    # Using tab20 which has more distinct colors for many categories
    num_colors = len(y_data_for_stack)
    colors = plt.cm.get_cmap('tab20', num_colors if num_colors > 0 else 1)

    ax.stackplot(
        timestamps,
        *y_data_for_stack,
        labels=stack_labels,
        colors=[colors(i) for i in range(num_colors)],
        alpha=0.8)

    # Plot TOTAL PSS (Reported) as a separate line for verification
    total_pss_values_mb = list(all_memory_data.get("TOTAL PSS", deque()))
    if total_pss_values_mb and len(total_pss_values_mb) == len(timestamps):
      ax.plot(
          timestamps,
          total_pss_values_mb,
          color='black',
          linestyle='--',
          linewidth=2,
          label='TOTAL PSS (Reported)',
          marker='.',
          markersize=3)

    ax.set_xlabel("Time")
    ax.set_ylabel("PSS (MB)")
    current_datetime_str = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
    ax.set_title(
        f"PSS Memory Stack for {package_name}\n(Collected up to {current_datetime_str})",
        fontsize=16)

    ax.xaxis.set_major_formatter(mdates.DateFormatter('%H:%M:%S'))
    fig.autofmt_xdate(rotation=30)

    ax.grid(True, linestyle=':', alpha=0.5)
    # Place legend outside the plot area
    ax.legend(
        loc='upper left',
        bbox_to_anchor=(1.02, 1),
        borderaxespad=0.,
        fontsize='small')

    plt.subplots_adjust(
        right=0.78)  # Adjust right margin to make space for the legend
    plt.savefig(output_filename, dpi=150)
    print(f"PSS graph saved to {output_filename}")
    plt.close(fig)

  except Exception as e:
    print(f"An error occurred while generating the graph: {e}")
    import traceback
    traceback.print_exc()


def main():
  parser = argparse.ArgumentParser(
      description="Monitor PSS memory components for an Android app and save a stacked graph on exit."
  )
  parser.add_argument("package_name", help="The package name of the app.")
  parser.add_argument(
      "-i",
      "--interval",
      type=float,
      default=5.0,
      help="Polling interval in seconds (default: 5.0).")
  parser.add_argument(
      "-s",
      "--samples",
      type=int,
      default=0,
      help="Number of samples to collect (default: 0 for indefinite).")
  parser.add_argument(
      "-mp",
      "--max_data_points",
      type=int,
      default=720,
      help="Maximum number of recent data points to store (default: 720). 0 for all (can consume memory)."
  )

  args = parser.parse_args()

  print(f"Monitoring PSS components for package: {args.package_name}")
  print(f"Polling every {args.interval} seconds.")
  if args.samples > 0:
    print(f"Will collect {args.samples} samples.")
  else:
    print("Running indefinitely until Ctrl+C.")
  if args.max_data_points > 0:
    print(
        f"Will store and plot up to the last {args.max_data_points} data points."
    )
  else:
    print("Will store and plot all collected data points.")
  print("Press Ctrl+C to stop and generate the graph.")

  maxlen = args.max_data_points if args.max_data_points > 0 else None
  all_memory_data = defaultdict(lambda: deque(maxlen=maxlen))
  all_memory_data["Timestamps"] = deque(maxlen=maxlen)

  # Preferred order for stacking (bottom to top) - ensures they are created in defaultdict if not present
  stack_order_preference = [
      "Code", "Stack", "Native Heap", "Java Heap", "Graphics", "Others"
  ]
  for cat in stack_order_preference:
    _ = all_memory_data[cat]  # Initialize deque for these categories
  _ = all_memory_data["TOTAL PSS"]

  sample_count = 0
  try:
    while True:
      if args.samples > 0 and sample_count >= args.samples:
        print(f"Finished collecting {args.samples} samples.")
        break

      current_time = datetime.now()
      memory_details_kb = get_memory_details_kb(args.package_name)

      if memory_details_kb:
        all_memory_data["Timestamps"].append(current_time)
        log_line_parts = [f"[{current_time.strftime('%Y-%m-%d %H:%M:%S')}]"]

        processed_categories_for_log = set()

        # Log TOTAL PSS first
        total_pss_kb = memory_details_kb.get("TOTAL PSS", 0)
        total_pss_mb = total_pss_kb / 1024.0
        all_memory_data["TOTAL PSS"].append(total_pss_mb)
        log_line_parts.append(f"TOTAL PSS: {total_pss_mb:.2f} MB")
        processed_categories_for_log.add("TOTAL PSS")

        # Log other components
        for category_name in CATEGORIES_TO_PARSE.keys(
        ):  # Iterate in defined order
          if category_name == "TOTAL PSS":
            continue  # Already handled

          pss_kb = memory_details_kb.get(category_name, 0)
          pss_mb = pss_kb / 1024.0
          all_memory_data[category_name].append(
              pss_mb)  # Store all, even if not logged below

          if pss_mb > 0.1 and category_name not in processed_categories_for_log:  # Only log if somewhat significant
            log_line_parts.append(f"{category_name}: {pss_mb:.2f} MB")
            processed_categories_for_log.add(category_name)

        print(" | ".join(log_line_parts))

      else:
        print(
            f"[{current_time.strftime('%Y-%m-%d %H:%M:%S')}] Failed to get memory details. Retrying in {args.interval}s..."
        )

      if args.samples > 0:
        sample_count += 1

      time.sleep(args.interval)

  except KeyboardInterrupt:
    print("\nMonitoring stopped by user. Generating PSS graph...")
  finally:
    if all_memory_data["Timestamps"]:
      filename_timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
      sanitized_package_name = args.package_name.replace('.', '_')
      output_filename = f"pss_stacked_graph_{sanitized_package_name}_{filename_timestamp}.png"
      generate_pss_graph(all_memory_data, args.package_name, output_filename,
                         stack_order_preference)
    else:
      print("No data was collected, so no graph will be generated.")
    print("Exiting.")


if __name__ == "__main__":
  main()
