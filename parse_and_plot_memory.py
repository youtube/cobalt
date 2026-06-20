#!/usr/bin/env python3
"""
Cobalt Memory Log Parser & Plotter (Standalone Edition)

This script parses Android logcat files containing Cobalt memory logs (PA_STATS, PSS_STATS, V8_HEAP_STATS).
It extracts the unpolluted JVM native overhead (dalvik_other) and other core memory pillars,
exports the data to a clean CSV spreadsheet, and generates a beautiful memory timeline plot.

Usage:
  python3 parse_and_plot_memory.py --log-file log.cobalt.latest.txt

Requirements:
  pip install matplotlib numpy
"""

import os
import re
import sys
import csv
import argparse
from datetime import datetime

# We import matplotlib inside a try/except block so the script fails gracefully
# if the user runs it in an environment without GUI/plotting libraries.
try:
    import matplotlib.pyplot as plt
except ImportError:
    print("Error: 'matplotlib' is not installed. Please run: pip install matplotlib")
    sys.exit(1)

# Regex patterns to parse logcat lines
log_line_pattern = re.compile(
    r"(\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{3})\s+(\d+)\s+(\d+)\s+I\s+([\w\.]+)\s*:\s+(.*)"
)
pa_pattern = re.compile(
    r"PA_STATS:\s+mmapped=([\d\']+),\s+committed=([\d\']+),\s+allocated=([\d\']+),\s+frag_ratio=([\d\.]+),\s+decoder_buffer\(mib\)=(\d+),\s+skia_cache\(mib\)=(\d+)"
)
pss_pattern = re.compile(
    r"PSS_STATS \(KiB\):\s+total=([\d\']+),\s+native=([\d\']+),\s+dalvik=([\d\']+),\s+graphics=([\d\']+),\s+code=([\d\']+),\s+other=([\d\']+),\s+dalvik_other=([\d\']+)"
)
v8_pattern = re.compile(
    r"V8_HEAP_STATS:\s+used\(KiB\)=([\d\']+),\s+total\(KiB\)=([\d\']+)(?:,\s+physical\(KiB\)=([\d\']+),\s+executable\(KiB\)=([\d\']+),\s+external\(KiB\)=([\d\']+))?,\s+limit\(KiB\)=([\d\']+)"
)

def clean_int(val_str):
    """Removes single quotes used as digit separators and converts to int."""
    return int(val_str.replace("'", ""))

def parse_logcat(log_path):
    """Parses the logcat file and groups matching memory stats by PID."""
    raw_data = []
    print(f"Reading log file: {log_path} ...")
    
    if not os.path.exists(log_path):
        print(f"Error: Log file '{log_path}' not found!")
        sys.exit(1)

    with open(log_path, 'r', encoding='utf-8', errors='ignore') as f:
        current_entry = {}
        for line in f:
            if len(line) < 19:
                continue
            
            # Extract timestamp from the start of the line (e.g. "06-12 21:20:14.000")
            ts_str = line[:18]
            try:
                ts = datetime.strptime(ts_str, "%m-%d %H:%M:%S.%f")
            except ValueError:
                continue
            
            # Extract the true system-level PID from the logcat tag (e.g. I/starboard(12345):)
            pid_match = re.search(r"[VDIWE]/[a-zA-Z0-9_\-\s]+\(\s*(\d+)\):", line)
            if not pid_match:
                continue
            pid = pid_match.group(1)
            
            # 1. Match PartitionAlloc C++ Heap Stats
            pa_match = pa_pattern.search(line)
            if pa_match:
                if current_entry and "pa" in current_entry:
                    raw_data.append(current_entry)
                    current_entry = {}
                current_entry = {
                    "ts": ts,
                    "ts_str": ts_str,
                    "pid": pid,
                    "pa": pa_match.groups()
                }
                continue
                
            # 2. Match OS PSS Stats (including our new dalvik_other!)
            pss_match = pss_pattern.search(line)
            if pss_match and current_entry and current_entry.get("pid") == pid:
                current_entry["pss"] = pss_match.groups()
                continue

            # 3. Match V8 JS Heap Stats
            v8_match = v8_pattern.search(line)
            if v8_match and current_entry and current_entry.get("pid") == pid:
                current_entry["v8"] = v8_match.groups()
                if "pss" in current_entry:
                    raw_data.append(current_entry)
                    current_entry = {}
                    
    return raw_data

def main():
    # Generate dynamic default filenames based on current date and time (MMDD_HHMM)
    now = datetime.now().strftime("%m%d_%H%M")
    default_csv = f"cobalt_memory_{now}.csv"
    default_plot = f"cobalt_memory_{now}.png"

    parser = argparse.ArgumentParser(description="Parse and plot Cobalt memory metrics from logcat.")
    parser.add_argument(
        "-l", "--log-file",
        type=str,
        default="log.cobalt.latest.txt",
        help="Path to the input logcat file (default: log.cobalt.latest.txt)"
    )
    parser.add_argument(
        "-c", "--csv-file",
        type=str,
        default=default_csv,
        help=f"Path to save the output CSV spreadsheet (default: {default_csv})"
    )
    parser.add_argument(
        "-p", "--plot-file",
        type=str,
        default=default_plot,
        help=f"Path to save the output PNG plot (default: {default_plot})"
    )
    args = parser.parse_args()

    raw_data = parse_logcat(args.log_file)

    # Find all unique PIDs that printed memory stats
    pids = list(set([d["pid"] for d in raw_data]))
    if not pids:
        print("Error: No valid Cobalt memory log entries (PA_STATS/PSS_STATS) found in the log file!")
        sys.exit(1)
    
    # We target the latest PID (most recent run)
    active_pid = sorted(pids)[-1]
    print(f"Detected active Cobalt process PID: {active_pid}")

    # Filter and clean session data
    session_data = [d for d in raw_data if d["pid"] == active_pid and "pss" in d]
    if not session_data:
        print(f"Error: No complete memory snapshots found for PID {active_pid}!")
        sys.exit(1)

    start_time = session_data[0]["ts"]
    print(f"Extracting {len(session_data)} data points starting from {session_data[0]['ts_str']} ...")

    # Arrays for data processing
    times = []
    total_pss = []
    process_native_pss = []
    cpp_heap = []
    jvm_native_overhead = [] # dalvik_other
    jvm_java_heap = []       # dalvik
    decoder_buffer = []
    skia_cache = []
    v8_used = []
    v8_physical = []
    v8_executable = []
    v8_external = []
    code_pss = []
    other_pss = []

    for entry in session_data:
        t_elapsed = (entry["ts"] - start_time).total_seconds()
        times.append(t_elapsed)
        
        # PSS Stats (KiB -> MiB)
        total = clean_int(entry["pss"][0]) / 1024
        native = clean_int(entry["pss"][1]) / 1024
        dalvik = clean_int(entry["pss"][2]) / 1024
        graphics = clean_int(entry["pss"][3]) / 1024
        code = clean_int(entry["pss"][4]) / 1024
        other = clean_int(entry["pss"][5]) / 1024
        dalvik_other = clean_int(entry["pss"][6]) / 1024
        
        # PA Stats (Bytes -> MiB)
        pa_committed = clean_int(entry["pa"][1]) / (1024 * 1024)
        dec_buf = int(entry["pa"][4])
        skia = int(entry["pa"][5])
        
        # V8 Stats (KiB -> MiB)
        v8_usd = 0
        v8_phys = 0
        v8_exec = 0
        v8_ext = 0
        if "v8" in entry:
            v8_usd = clean_int(entry["v8"][0]) / 1024
            # If we matched the new format, we will have 6 groups (limit is group 5)
            # and groups 2, 3, 4 will contain physical, executable, external
            if len(entry["v8"]) >= 6 and entry["v8"][2] is not None:
                v8_phys = clean_int(entry["v8"][2]) / 1024
                v8_exec = clean_int(entry["v8"][3]) / 1024
                v8_ext = clean_int(entry["v8"][4]) / 1024
        else:
            v8_usd = v8_used[-1] if v8_used else 0
            v8_phys = v8_physical[-1] if v8_physical else 0
            v8_exec = v8_executable[-1] if v8_executable else 0
            v8_ext = v8_external[-1] if v8_external else 0
            
        total_pss.append(total)
        process_native_pss.append(native)
        cpp_heap.append(pa_committed)
        jvm_native_overhead.append(dalvik_other)
        jvm_java_heap.append(dalvik)
        decoder_buffer.append(dec_buf)
        skia_cache.append(skia)
        v8_used.append(v8_usd)
        v8_physical.append(v8_phys)
        v8_executable.append(v8_exec)
        v8_external.append(v8_ext)
        code_pss.append(code)
        other_pss.append(other)

    # 1. Export Data to CSV
    print(f"Exporting metrics to CSV: {args.csv_file} ...")
    with open(args.csv_file, 'w', newline='', encoding='utf-8') as csvfile:
        writer = csv.writer(csvfile)
        writer.writerow([
            "Timestamp", "Elapsed_Seconds", "Total_PSS_MiB", "Total_Native_PSS_MiB",
            "CPP_Heap_PA_Committed_MiB", "JVM_Native_Overhead_dalvik_other_MiB",
            "JVM_Java_Heap_dalvik_MiB", "JS_Heap_Live_V8_Used_MiB",
            "JS_Heap_Physical_V8_MiB", "JS_Heap_Executable_V8_MiB", "JS_Heap_External_V8_MiB",
            "Media_Decoder_Buffer_Pool_MiB", "Skia_GPU_Cache_MiB",
            "Code_PSS_MiB", "Other_PSS_MiB"
        ])
        for i, entry in enumerate(session_data):
            writer.writerow([
                entry["ts_str"],
                f"{times[i]:.1f}",
                f"{total_pss[i]:.2f}",
                f"{process_native_pss[i]:.2f}",
                f"{cpp_heap[i]:.2f}",
                f"{jvm_native_overhead[i]:.2f}",
                f"{jvm_java_heap[i]:.2f}",
                f"{v8_used[i]:.2f}",
                f"{v8_physical[i]:.2f}",
                f"{v8_executable[i]:.2f}",
                f"{v8_external[i]:.2f}",
                f"{decoder_buffer[i]:.2f}",
                f"{skia_cache[i]:.2f}",
                f"{code_pss[i]:.2f}",
                f"{other_pss[i]:.2f}"
            ])
    print("CSV Export Successful!")

    # 2. Generate and Save the Plot
    print(f"Generating memory timeline plot: {args.plot_file} ...")
    plt.figure(figsize=(16, 9))

    plt.plot(times, total_pss, label='Total Process PSS (OS)', color='black', linewidth=2.5, linestyle='-')
    plt.plot(times, process_native_pss, label='Total Process Native PSS (OS)', color='gray', linewidth=1.5, linestyle=':')
    plt.plot(times, cpp_heap, label='C++ Heap (PartitionAlloc Committed)', color='royalblue', linewidth=2.0, linestyle='-')
    plt.plot(times, jvm_native_overhead, label='JVM Native Overhead (dalvik_other JNI)', color='crimson', linewidth=2.2, linestyle='--')
    plt.plot(times, jvm_java_heap, label='JVM Java Heap (dalvik PSS)', color='gold', linewidth=1.8, linestyle='-.')
    plt.plot(times, v8_used, label='JS Heap Live (V8 Used)', color='darkorange', linewidth=2.0, linestyle='-')
    plt.plot(times, decoder_buffer, label='Media Decoder Buffer Pool', color='purple', linewidth=1.8, linestyle='-.')
    plt.plot(times, skia_cache, label='Skia GPU Resource Cache', color='limegreen', linewidth=1.8, linestyle='-')

    plt.title(
        f'Cobalt Memory Component Timeline (PID: {active_pid})\nFeaturing Pure JVM Native Overhead (dalvik_other)',
        fontsize=14, weight='bold', pad=15
    )
    plt.xlabel('Elapsed Time (seconds) from Session Start', fontsize=11)
    plt.ylabel('Memory Footprint (MiB)', fontsize=11)
    plt.grid(True, linestyle=':', alpha=0.5)
    plt.legend(fontsize=10, loc='upper left')

    plt.tight_layout()
    plt.savefig(args.plot_file, dpi=150)
    print(f"Plot successfully saved to {args.plot_file}")
    print("\n🎉 Process Complete! You have successfully parsed and plotted your Cobalt run!")

if __name__ == "__main__":
    main()
