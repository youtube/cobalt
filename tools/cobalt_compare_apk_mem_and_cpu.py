#!/usr/bin/env python3

import subprocess
import time
import re
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.dates as mdates
from datetime import datetime, timedelta
import os
import sys
import argparse

# --- Configurable Defaults ---
DEFAULT_APK1_PATH = "/usr/local/google/home/sideboard/Code/cobalt/src/out/android-arm_devel/apks/Cobalt.apk"
DEFAULT_APK2_PATH = "/usr/local/google/home/sideboard/Code/cobalt-25/out/android-arm_devel/cobalt.apk"
DEFAULT_DURATION = 60  # Seconds
DEFAULT_OUTPUT_DIR = "/tmp/cobalt_monitoring"
DEFAULT_YOUTUBE_URL = "https://youtube.com/tv/watch?v=UdqZ3xoaekk&absolute_experiments=9415422"
# --- End Configurable Defaults ---

PACKAGE_NAME = "dev.cobalt.coat"
POLL_INTERVAL = 5  # Seconds
LAUNCH_ACTIVITY = "dev.cobalt.app.MainActivity"

def run_adb_command(command, check=True, suppress_output=False):
    full_command = ['adb'] + command
    if not suppress_output:
        print(f"Running ADB: {' '.join(full_command)}")
    try:
        result = subprocess.run(full_command, capture_output=True, text=True, check=check)
        return result.stdout.strip()
    except subprocess.CalledProcessError as e:
        if not suppress_output:
            print(f"ADB command failed: {' '.join(command)}")
            print(f"Error: {e}")
            print(f"Stderr: {e.stderr}")
        if check:
            raise
        return None

def get_cobalt_pid():
    return run_adb_command(['shell', 'pidof', PACKAGE_NAME], check=False, suppress_output=True)

def monitor_memory(pid):
    try:
        meminfo = run_adb_command(['shell', 'dumpsys', 'meminfo', pid], suppress_output=True)
        if not meminfo: return None

        def get_pss(category_name):
            match = re.search(r"^\s*" + re.escape(category_name) + r"\s+(\d+)", meminfo, re.MULTILINE)
            return int(match.group(1)) if match else 0

        def get_app_summary_pss(category_name):
            match = re.search(r"\s+" + re.escape(category_name) + r":\s+(\d+)", meminfo)
            return int(match.group(1)) if match else 0

        return {
            'Timestamp': datetime.now(),
            'PSS': get_app_summary_pss("TOTAL PSS"), 'JavaHeap': get_app_summary_pss("Java Heap"),
            'NativeHeap': get_app_summary_pss("Native Heap"), 'Code': get_app_summary_pss("Code"),
            'Stack': get_app_summary_pss("Stack"), 'Graphics': get_app_summary_pss("Graphics"),
            'Other': get_app_summary_pss("Private Other"), 'System': get_app_summary_pss("System"),
            'so_mmap': get_pss(".so mmap"), 'jar_mmap': get_pss(".jar mmap"), 'apk_mmap': get_pss(".apk mmap"),
            'ttf_mmap': get_pss(".ttf mmap"), 'dex_mmap': get_pss(".dex mmap"), 'oat_mmap': get_pss(".oat mmap"),
            'art_mmap': get_pss(".art mmap"), 'EGL_mtrack': get_pss("EGL mtrack"), 'GL_mtrack': get_pss("GL mtrack"),
            'Ashmem': get_pss("Ashmem"), 'Other_dev': get_pss("Other dev"), 'Other_mmap': get_pss("Other mmap"),
            'Unknown': get_pss("Unknown"),
        }
    except Exception as e:
        print(f"Error parsing meminfo: {e}")
        return None

def monitor_cpu(pid):
    try:
        top_output = run_adb_command(['shell', 'top', '-p', pid, '-n', '1'], suppress_output=True)
        if not top_output: return None
        lines = top_output.splitlines()
        header_line = None
        header_index = -1
        cpu_index = -1
        cpu_header = "S[%CPU]"

        for i, line in enumerate(lines):
            if 'PID' in line and 'USER' in line and cpu_header in line:
                header_line = line
                header_index = i
                break

        if not header_line:
            print("Error: Could not find header row in top output.")
            return None

        headers = header_line.split()
        try:
            cpu_index = headers.index(cpu_header)
        except ValueError:
            print(f"Error: Index of '{cpu_header}' not found in header: {header_line}")
            return None

        for i in range(header_index + 1, len(lines)):
            line = lines[i]
            if pid in line:
                parts = line.split()
                if len(parts) > cpu_index:
                    try:
                        return {'Timestamp': datetime.now(), 'CPU': float(parts[cpu_index])}
                    except ValueError:
                        print(f"Error converting CPU: {parts[cpu_index]} in line: {line}")
                        return None
        return None
    except Exception as e:
        print(f"Error parsing top: {e}")
        return None

def uninstall_apk(package_name):
    print(f"Uninstalling {package_name}...")
    run_adb_command(['uninstall', package_name], check=False)
    time.sleep(2) # Wait for uninstall to complete

def install_apk(apk_path):
    print(f"Installing {apk_path}...")
    if not os.path.exists(apk_path):
        print(f"Error: APK not found at {apk_path}")
        return False
    # Use -r to replace existing, -d to allow downgrade
    run_adb_command(['install', '-r', '-d', apk_path])
    print("Install complete.")
    time.sleep(2)
    return True

def launch_app(package_name, youtube_url):
    print(f"Launching {package_name} with URL: {youtube_url}")
    url_arg = f'--url="{youtube_url}"'
    launch_command = [
        'shell', 'am', 'start',
        '--esa', 'commandLineArgs', url_arg,
        '--esa', 'args', url_arg,
        f'{package_name}/{LAUNCH_ACTIVITY}'
    ]
    run_adb_command(launch_command)
    print("Waiting for app to start...")
    time.sleep(10)

def run_test_on_apk(apk_path, package_name, duration_seconds, youtube_url):
    mem_data, cpu_data = [], []
    uninstall_apk(package_name)
    if not install_apk(apk_path):
        return [], [], []

    launch_app(package_name, youtube_url)

    cobalt_pid = None
    for _ in range(5):
        cobalt_pid = get_cobalt_pid()
        if cobalt_pid: break
        time.sleep(2)

    if not cobalt_pid:
        print(f"Failed to get PID for {package_name}.")
        uninstall_apk(package_name)
        return [], [], []
    print(f"Cobalt PID: {cobalt_pid}")

    print(f"Starting monitoring for {os.path.basename(apk_path)} for {duration_seconds}s...")
    start_time = time.time()
    while time.time() - start_time < duration_seconds:
        mem = monitor_memory(cobalt_pid)
        if mem: mem_data.append(mem)
        cpu = monitor_cpu(cobalt_pid)
        if cpu: cpu_data.append(cpu)

        remaining = duration_seconds - (time.time() - start_time)
        sleep_time = min(POLL_INTERVAL, remaining)
        if sleep_time > 0:
            time.sleep(sleep_time)

    print(f"Monitoring finished for {os.path.basename(apk_path)}.")
    run_adb_command(['shell', 'am', 'force-stop', package_name], check=False)
    uninstall_apk(package_name)
    return mem_data, cpu_data

def plot_combined_data(data1, data2, label1, label2, output_dir):
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)

    mem_data1, cpu_data1 = data1 if data1 else ([], [])
    mem_data2, cpu_data2 = data2 if data2 else ([], [])

    # Normalize timestamps to be relative to the start of each run
    def normalize_time(data):
        if not data:
            return [] # Return empty list if no data
        start_time = data[0]['Timestamp']
        for item in data:
            item['RelTime'] = (item['Timestamp'] - start_time).total_seconds()
        return data

    mem_data1 = normalize_time(mem_data1)
    cpu_data1 = normalize_time(cpu_data1)
    mem_data2 = normalize_time(mem_data2)
    cpu_data2 = normalize_time(cpu_data2)

    fig = plt.figure(figsize=(24, 24)) # Increased figure size for the new plot
    fig.suptitle(f'Cobalt Performance Comparison: {label1} vs {label2}', fontsize=16)

    # Determine shared Y-axis limit for memory plots
    max_pss = 0
    if mem_data1:
        df_mem1_temp = pd.DataFrame(mem_data1)
        if not df_mem1_temp.empty and 'PSS' in df_mem1_temp.columns:
            max_pss = max(max_pss, df_mem1_temp['PSS'].max())
    if mem_data2:
        df_mem2_temp = pd.DataFrame(mem_data2)
        if not df_mem2_temp.empty and 'PSS' in df_mem2_temp.columns:
            max_pss = max(max_pss, df_mem2_temp['PSS'].max())
    mem_ylim = (0, max_pss * 1.1 + 1) # Add 10% padding and ensure non-zero limit

    labels = ['JavaHeap', 'NativeHeap', 'Stack', 'System', 'so_mmap', 'jar_mmap', 'apk_mmap',
              'ttf_mmap', 'dex_mmap', 'oat_mmap', 'art_mmap', 'EGL_mtrack', 'GL_mtrack',
              'Ashmem', 'Other_dev', 'Other_mmap', 'Unknown']

    # Memory Plot for APK1 (Top Left)
    ax1 = fig.add_subplot(3, 2, 1)
    if mem_data1:
        df_mem1 = pd.DataFrame(mem_data1)
        if not df_mem1.empty and 'RelTime' in df_mem1.columns:
            columns1 = [df_mem1.get(label, 0) for label in labels]
            ax1.stackplot(df_mem1['RelTime'], *columns1, labels=labels, alpha=0.8)
            ax1.plot(df_mem1['RelTime'], df_mem1['PSS'], label='Total PSS', color='black', linestyle='--')
            df_mem1.to_csv(os.path.join(output_dir, f'memory_usage_{label1}.csv'), index=False)
    ax1.set_xlabel('Time (seconds)')
    ax1.set_ylabel('Memory (KB)')
    ax1.set_title(f'Memory Usage: {label1}')
    ax1.set_ylim(mem_ylim)
    ax1.grid(True)

    # Memory Plot for APK2 (Top Right)
    ax2 = fig.add_subplot(3, 2, 2)
    if mem_data2:
        df_mem2 = pd.DataFrame(mem_data2)
        if not df_mem2.empty and 'RelTime' in df_mem2.columns:
            columns2 = [df_mem2.get(label, 0) for label in labels]
            ax2.stackplot(df_mem2['RelTime'], *columns2, labels=labels, alpha=0.8)
            ax2.plot(df_mem2['RelTime'], df_mem2['PSS'], label='Total PSS', color='black', linestyle='--')
            df_mem2.to_csv(os.path.join(output_dir, f'memory_usage_{label2}.csv'), index=False)
    ax2.set_xlabel('Time (seconds)')
    ax2.set_ylabel('Memory (KB)')
    ax2.set_title(f'Memory Usage: {label2}')
    ax2.set_ylim(mem_ylim)
    ax2.grid(True)

    # Memory legend
    handles, labels_leg = ax2.get_legend_handles_labels()
    if handles:
        stack_handles = handles[:len(labels)]
        stack_labels = labels[:len(labels)]
        other_handles = handles[len(labels):]
        other_labels = labels_leg[len(labels):]
        fig.legend(stack_handles[::-1] + other_handles, stack_labels[::-1] + other_labels, loc='upper right', bbox_to_anchor=(0.99, 0.95))

    # CPU Plot (Combined - Middle Row)
    ax_cpu = fig.add_subplot(3, 1, 2)
    if cpu_data1:
        df_cpu1 = pd.DataFrame(cpu_data1)
        if not df_cpu1.empty and 'RelTime' in df_cpu1.columns:
            ax_cpu.plot(df_cpu1['RelTime'], df_cpu1['CPU'], label=f'{label1} CPU %', linestyle='-')
            df_cpu1.to_csv(os.path.join(output_dir, f'cpu_usage_{label1}.csv'), index=False)
    if cpu_data2:
        df_cpu2 = pd.DataFrame(cpu_data2)
        if not df_cpu2.empty and 'RelTime' in df_cpu2.columns:
            ax_cpu.plot(df_cpu2['RelTime'], df_cpu2['CPU'], label=f'{label2} CPU %', linestyle='--')
            df_cpu2.to_csv(os.path.join(output_dir, f'cpu_usage_{label2}.csv'), index=False)
    ax_cpu.set_xlabel('Time (seconds)')
    ax_cpu.set_ylabel('CPU Usage (%)')
    ax_cpu.set_title('CPU Usage Comparison')
    if (cpu_data1 and not pd.DataFrame(cpu_data1).empty) or (cpu_data2 and not pd.DataFrame(cpu_data2).empty):
         ax_cpu.legend()
    ax_cpu.grid(True)

    plt.tight_layout(rect=[0, 0, 1, 0.96])
    combined_filename = os.path.join(output_dir, f'comparison_{label1}_vs_{label2}.png')
    plt.savefig(combined_filename)
    print(f"Combined comparison plot saved to {combined_filename}")

def main(apk1_path, apk2_path, duration, output_dir, youtube_url):
    print(f"--- Running Test for APK 1: {apk1_path} ---")
    data1 = run_test_on_apk(apk1_path, PACKAGE_NAME, duration, youtube_url)

    print(f"\n--- Running Test for APK 2: {apk2_path} ---")
    data2 = run_test_on_apk(apk2_path, PACKAGE_NAME, duration, youtube_url)

    if (data1 and data1[0]) and (data2 and data2[0]):
        print("\n--- Generating Comparison Plots ---")
        plot_combined_data(data1, data2, "APK1", "APK2", output_dir)
    else:
        print("Data collection failed for one or both APKs, skipping plotting.")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Monitor and Compare Cobalt APKs.")
    parser.add_argument("--apk1", default=DEFAULT_APK1_PATH, help="Path to the first Cobalt APK")
    parser.add_argument("--apk2", default=DEFAULT_APK2_PATH, help="Path to the second Cobalt APK")
    parser.add_argument("--duration", type=int, default=DEFAULT_DURATION, help="Duration to monitor each APK in seconds")
    parser.add_argument("--outdir", default=DEFAULT_OUTPUT_DIR, help="Directory to save plots and CSV files")
    parser.add_argument("--url", default=DEFAULT_YOUTUBE_URL, help="YouTube URL to launch")
    args = parser.parse_args()

    try:
        run_adb_command(['--version'])
    except Exception as e:
        print(f"Error: ADB check failed: {e}")
        sys.exit(1)
    main(args.apk1, args.apk2, args.duration, args.outdir, args.url)
