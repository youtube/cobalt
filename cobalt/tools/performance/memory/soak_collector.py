#!/usr/bin/env python3
# Copyright 2026 The Cobalt Authors. All Rights Reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Collects periodic smaps and memory-infra traces from running Cobalt."""

import argparse
import csv
import datetime
import json
import os
import subprocess
import sys
import time
import requests
import websocket

# Default parameters
DEFAULT_PORT = 9222
DEFAULT_INTERVAL_SECONDS = 300
DEFAULT_DURATION_SECONDS = 21600  # 6 hours
DEFAULT_OUTPUT_DIR = "soak_results"


def get_cobalt_pid(process_name="cobalt"):
  """Gets the main Cobalt process PID on Linux."""
  try:
    result = subprocess.run(["pidof", process_name],
                            capture_output=True,
                            text=True,
                            check=False)
    if result.returncode == 0:
      pids = result.stdout.strip().split()
      if not pids:
        return None
      if len(pids) == 1:
        return int(pids[0])
      # If multiple, find the parent-most one (oldest etime)
      ps_result = subprocess.run(
          ["ps", "-o", "pid,etimes", "-p", ",".join(pids)],
          capture_output=True,
          text=True,
          check=False)
      if ps_result.returncode == 0:
        lines = ps_result.stdout.strip().split("\n")[1:]
        pid_to_etime = {}
        for line in lines:
          parts = line.strip().split()
          if len(parts) == 2:
            pid, etime = parts
            if pid.isdigit() and etime.isdigit():
              pid_to_etime[int(pid)] = int(etime)
        if pid_to_etime:
          # Main PID is the one with the maximum elapsed time
          return max(pid_to_etime, key=pid_to_etime.get)
      return int(pids[0])
  except Exception as e:
    print(f"Error getting Cobalt PID: {e}", file=sys.stderr)
  return None


def parse_smaps_rollup(pid):
  """Parses /proc/<pid>/smaps_rollup and returns a dict of metrics in MB."""
  metrics = {}
  smaps_path = f"/proc/{pid}/smaps_rollup"
  if not os.path.exists(smaps_path):
    print(f"Error: {smaps_path} not found.", file=sys.stderr)
    return None

  try:
    with open(smaps_path, "r") as f:
      for line in f:
        parts = line.split()
        if len(parts) >= 2:
          key = parts[0].strip(":")
          val_kb = parts[1]
          if val_kb.isdigit():
            # Convert to MB (float)
            metrics[key] = float(val_kb) / 1024.0
  except Exception as e:
    print(f"Error parsing {smaps_path}: {e}", file=sys.stderr)
    return None
  return metrics


def get_devtools_ws_url(port):
  """Queries http://localhost:port/json/version to get browser websocket URL."""
  try:
    resp = requests.get(f"http://localhost:{port}/json/version", timeout=5)
    resp.raise_for_status()
    data = resp.json()
    return data.get("webSocketDebuggerUrl")
  except Exception as e:
    print(f"Error connecting to DevTools on port {port}: {e}", file=sys.stderr)
    return None


def get_page_ws_url(port):
  """Queries http://localhost:port/json to find target type='page' websocket URL."""
  try:
    resp = requests.get(f"http://localhost:{port}/json", timeout=5)
    resp.raise_for_status()
    targets = resp.json()
    for target in targets:
      if target.get("type") == "page":
        return target.get("webSocketDebuggerUrl")
  except Exception as e:
    print(f"Error getting page websocket URL on port {port}: {e}", file=sys.stderr)
  return None


def capture_uma_histograms(ws_url):
  """Triggers window.h5vcc.metrics.requestHistograms() and returns its result."""
  ws = None
  try:
    print(f"Connecting to page DevTools at {ws_url}...")
    ws = websocket.create_connection(ws_url, timeout=10)

    # Send Runtime.evaluate
    payload = {
        "id": 10,
        "method": "Runtime.evaluate",
        "params": {
            "expression": "window.h5vcc && window.h5vcc.metrics && window.h5vcc.metrics.requestHistograms()",
            "awaitPromise": True,
            "returnByValue": True
        }
    }
    ws.send(json.dumps(payload))

    # Read response
    resp = json.loads(ws.recv())
    if "result" in resp:
      result = resp["result"]
      if "exceptionDetails" in result:
        print(f"JS Exception during requestHistograms: {result['exceptionDetails']}", file=sys.stderr)
        return None

      val = result.get("result", {}).get("value")
      return val
    print(f"Unexpected response format from requestHistograms: {resp}", file=sys.stderr)
  except Exception as e:
    print(f"Exception during requestHistograms execution: {e}", file=sys.stderr)
  finally:
    if ws:
      try:
        ws.close()
      except Exception:
        pass
  return None


def capture_trace_snapshot(ws_url, output_path):
  """Triggers Tracing.requestMemoryDump and records the trace JSON."""
  ws = None
  try:
    print(f"Connecting to browser DevTools at {ws_url}...")
    ws = websocket.create_connection(ws_url, max_size=100 * 1024 * 1024)

    # 1. Start Tracing
    ws.send(json.dumps({
        "id": 1,
        "method": "Tracing.start",
        "params": {
            "traceConfig": {
                "includedCategories": ["disabled-by-default-memory-infra"],
                "memoryDumpConfig": {"triggers": []}
            }
        }
    }))
    # Read response for start
    start_resp = json.loads(ws.recv())
    print("Tracing.start response:", start_resp)

    # 2. Trigger Memory Dump
    print("Requesting detailed memory dump...")
    ws.send(json.dumps({
        "id": 2,
        "method": "Tracing.requestMemoryDump",
        "params": {"levelOfDetail": "detailed"}
    }))

    # We wait for the dump response (which contains success status)
    # We will read messages until we get the response for id=2
    dump_success = False
    dump_attempts = 0
    while dump_attempts < 20:
      msg = json.loads(ws.recv())
      if msg.get("id") == 2:
        result = msg.get("result", {})
        dump_success = result.get("success", False)
        print("Memory dump result success:", dump_success)
        break
      dump_attempts += 1

    # 3. End Tracing
    print("Ending tracing...")
    ws.send(json.dumps({
        "id": 3,
        "method": "Tracing.end"
    }))

    # 4. Read tracing data chunks until Tracing.tracingComplete
    trace_events = []
    tracing_complete = False
    while not tracing_complete:
      msg = json.loads(ws.recv())
      if "method" in msg:
        method = msg["method"]
        if method == "Tracing.dataCollected":
          value = msg["params"].get("value")
          if isinstance(value, list):
            trace_events.extend(value)
          else:
            trace_events.append(value)
        elif method == "Tracing.tracingComplete":
          tracing_complete = True
          print("Tracing data download complete.")
      elif msg.get("id") == 3:
        print("Tracing end acknowledged.")

    # 5. Write to JSON
    with open(output_path, "w", encoding="utf-8") as f:
      json.dump({"traceEvents": trace_events}, f)
    print(f"Successfully saved trace snapshot to {output_path}")
    return True

  except Exception as e:
    print(f"Exception during trace capture: {e}", file=sys.stderr)
    return False
  finally:
    if ws:
      try:
        ws.close()
      except Exception:
        pass


def run_soak_collection(args):
  """Orchestrates the periodic memory soak collection loop."""
  pid = args.pid
  if not pid:
    print(f"Searching for process '{args.process_name}'...")
    pid = get_cobalt_pid(args.process_name)
    if not pid:
      print(f"Error: Process '{args.process_name}' is not running!", file=sys.stderr)
      sys.exit(1)
  print(f"Targeting process PID: {pid}")

  os.makedirs(args.output_dir, exist_ok=True)
  summary_csv_path = os.path.join(args.output_dir, "summary.csv")

  # Setup CSV file with headers
  csv_headers = ["Timestamp", "Elapsed_Seconds", "RSS_MB", "PSS_MB", "Private_Dirty_MB", "Anonymous_MB"]
  write_headers = not os.path.exists(summary_csv_path)
  
  start_time = time.time()
  end_time = start_time + args.duration_seconds
  cycle_count = 0

  print(f"Starting soak collection for {args.duration_seconds} seconds (~{args.duration_seconds / 3600:.1f} hours).")
  print(f"Interval: {args.interval_seconds} seconds. Results saved to: {args.output_dir}")

  try:
    while time.time() < end_time:
      now = datetime.datetime.now()
      timestamp_str = now.strftime("%Y%m%d_%H%M%S")
      elapsed = int(time.time() - start_time)

      print("-" * 60)
      print(f"[{now.strftime('%Y-%m-%d %H:%M:%S')}] Cycle {cycle_count} (Elapsed: {elapsed}s)")

      # 1. Check if process is still running
      if not os.path.exists(f"/proc/{pid}"):
        print(f"Error: Process with PID {pid} has terminated!", file=sys.stderr)
        break

      # 2. Capture smaps_rollup
      smaps_metrics = parse_smaps_rollup(pid)
      if smaps_metrics:
        # Write to cycle-specific file
        smaps_file = os.path.join(args.output_dir, f"smaps_{timestamp_str}_{pid}.txt")
        subprocess.run(f"cat /proc/{pid}/smaps_rollup > {smaps_file}", shell=True, check=False)

        # Write status file as well
        status_file = os.path.join(args.output_dir, f"status_{timestamp_str}_{pid}.txt")
        subprocess.run(f"cat /proc/{pid}/status > {status_file}", shell=True, check=False)

        # Log summary to console
        rss = smaps_metrics.get("Rss", 0.0)
        pss = smaps_metrics.get("Pss", 0.0)
        private_dirty = smaps_metrics.get("Private_Dirty", 0.0)
        anonymous = smaps_metrics.get("Anonymous", 0.0)
        print(f"RSS: {rss:.2f} MB | PSS: {pss:.2f} MB | Private Dirty: {private_dirty:.2f} MB | Anonymous: {anonymous:.2f} MB")

        # Write to summary.csv
        with open(summary_csv_path, "a", newline="", encoding="utf-8") as f:
          writer = csv.writer(f)
          if write_headers:
            writer.writerow(csv_headers)
            write_headers = False
          writer.writerow([timestamp_str, elapsed, f"{rss:.2f}", f"{pss:.2f}", f"{private_dirty:.2f}", f"{anonymous:.2f}"])
      else:
        print("Warning: Failed to capture smaps_rollup.", file=sys.stderr)

      # 3. Capture CDP Trace
      ws_url = get_devtools_ws_url(args.port)
      if ws_url:
        trace_file = os.path.join(args.output_dir, f"trace_{timestamp_str}_{pid}.json")
        success = capture_trace_snapshot(ws_url, trace_file)
        if not success:
          print("Warning: Trace snapshot capture failed in this cycle.", file=sys.stderr)
      else:
        print("Warning: Could not get DevTools websocket URL. Is DevTools enabled?", file=sys.stderr)

      # 3.5. Capture UMA Histograms via JS API
      page_ws_url = get_page_ws_url(args.port)
      if page_ws_url:
        uma_data = capture_uma_histograms(page_ws_url)
        if uma_data is not None:
          if uma_data:
            uma_file = os.path.join(args.output_dir, f"uma_histograms_{timestamp_str}_{pid}.b64")
            with open(uma_file, "w", encoding="utf-8") as f:
              f.write(uma_data)
            print(f"Successfully saved UMA histograms to {uma_file}")
          else:
            print("No UMA histograms recorded since last query.")
        else:
          print("Warning: Failed to evaluate UMA histograms in this cycle.", file=sys.stderr)
      else:
        print("Warning: Could not get page DevTools websocket URL for UMA histograms.", file=sys.stderr)

      # 4. Wait for next cycle
      cycle_count += 1
      time.sleep(args.interval_seconds)

  except KeyboardInterrupt:
    print("\nCollection stopped by user.")
  print("Collection loop finished.")


if __name__ == "__main__":
  parser = argparse.ArgumentParser(description="Cobalt Memory Soak Collector")
  parser.add_argument("--pid", type=int, help="Target process PID (overrides --process_name)")
  parser.add_argument("--process_name", type=str, default="cobalt", help="Target process name (default: cobalt)")
  parser.add_argument("--port", type=int, default=DEFAULT_PORT, help=f"CDP remote debugging port (default: {DEFAULT_PORT})")
  parser.add_argument("-i", "--interval_seconds", type=int, default=DEFAULT_INTERVAL_SECONDS, help=f"Sampling interval in seconds (default: {DEFAULT_INTERVAL_SECONDS})")
  parser.add_argument("-d", "--duration_seconds", type=int, default=DEFAULT_DURATION_SECONDS, help=f"Total run duration in seconds (default: {DEFAULT_DURATION_SECONDS})")
  parser.add_argument("-o", "--output_dir", type=str, default=DEFAULT_OUTPUT_DIR, help=f"Output directory for logs (default: {DEFAULT_OUTPUT_DIR})")
  
  parsed_args = parser.parse_args()
  run_soak_collection(parsed_args)
