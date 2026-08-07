#!/usr/bin/env python3
# Copyright 2026 The Cobalt Authors. All Rights Reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Collects persistent memory-infra tracing over a single long-lived session."""

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
DEFAULT_DURATION_SECONDS = 3600  # 1 hour
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
      # Find parent-most PID
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
    ws = websocket.create_connection(ws_url, timeout=10)
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
    resp = json.loads(ws.recv())
    if "result" in resp:
      result = resp["result"]
      if "exceptionDetails" in result:
        return None
      return result.get("result", {}).get("value")
  except Exception:
    pass
  finally:
    if ws:
      try:
        ws.close()
      except Exception:
        pass
  return None


def run_persistent_collection(args):
  """Executes the persistent tracing collection loop."""
  pid = args.pid
  if not pid:
    pid = get_cobalt_pid(args.process_name)
    if not pid:
      print(f"Error: Process '{args.process_name}' is not running!", file=sys.stderr)
      sys.exit(1)
  print(f"Targeting process PID: {pid}")

  os.makedirs(args.output_dir, exist_ok=True)
  summary_csv_path = os.path.join(args.output_dir, "summary.csv")

  # Setup CSV file with headers
  csv_headers = ["Timestamp", "Elapsed_Seconds", "RSS_MB", "PSS_MB", "Private_Dirty_MB", "Anonymous_MB"]
  with open(summary_csv_path, "w", newline="") as f:
    writer = csv.writer(f)
    writer.writerow(csv_headers)

  # 1. Connect to browser DevTools
  ws_url = get_devtools_ws_url(args.port)
  if not ws_url:
    print("Error: Could not retrieve browser DevTools URL!", file=sys.stderr)
    sys.exit(1)

  print(f"Connecting to browser DevTools at {ws_url}...")
  ws = websocket.create_connection(ws_url, max_size=100 * 1024 * 1024)

  # 2. Start Tracing (with memory-infra category only, excluding everything else)
  print("Starting persistent trace recording...")
  ws.send(json.dumps({
      "id": 1,
      "method": "Tracing.start",
      "params": {
          "traceConfig": {
              "includedCategories": ["disabled-by-default-memory-infra"],
              "excludedCategories": ["*"],
              "memoryDumpConfig": {"triggers": []}
          }
      }
  }))
  start_resp = json.loads(ws.recv())
  print("Tracing.start response:", start_resp)

  start_time = time.time()
  end_time = start_time + args.duration_seconds
  cycle_count = 0

  try:
    while time.time() < end_time:
      timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
      elapsed = int(time.time() - start_time)

      # A. Save smaps rollup
      smaps_metrics = parse_smaps_rollup(pid)
      if smaps_metrics:
        rss = smaps_metrics.get("Rss", 0)
        pss = smaps_metrics.get("Pss", 0)
        private_dirty = smaps_metrics.get("Private_Dirty", 0)
        anon = smaps_metrics.get("Anonymous", 0)

        # Print standard line
        print("-" * 60)
        print(f"[{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] Cycle {cycle_count} (Elapsed: {elapsed}s)")
        print(f"RSS: {rss:.2f} MB | PSS: {pss:.2f} MB | Private Dirty: {private_dirty:.2f} MB | Anonymous: {anon:.2f} MB")

        # Save to summary CSV
        with open(summary_csv_path, "a", newline="") as f:
          writer = csv.writer(f)
          writer.writerow([timestamp, elapsed, f"{rss:.2f}", f"{pss:.2f}", f"{private_dirty:.2f}", f"{anon:.2f}"])

        # Backup raw smaps contents for rollup analysis
        smaps_out = os.path.join(args.output_dir, f"smaps_{timestamp}_{pid}.txt")
        try:
          with open(f"/proc/{pid}/smaps_rollup", "r") as src, open(smaps_out, "w") as dest:
            dest.write(src.read())
        except Exception as e:
          print(f"Failed to copy smaps_rollup: {e}", file=sys.stderr)

      # B. Trigger Memory Dump
      print("Requesting detailed memory dump...")
      ws.send(json.dumps({
          "id": 100 + cycle_count,
          "method": "Tracing.requestMemoryDump",
          "params": {"levelOfDetail": "detailed"}
      }))

      # Wait for dump confirmation
      dump_success = False
      dump_attempts = 0
      while dump_attempts < 20:
        msg = json.loads(ws.recv())
        if msg.get("id") == 100 + cycle_count:
          result = msg.get("result", {})
          dump_success = result.get("success", False)
          print("Memory dump result success:", dump_success)
          break
        dump_attempts += 1

      # C. Query UMA histograms on the page connection
      page_ws_url = get_page_ws_url(args.port)
      if page_ws_url:
        capture_uma_histograms(page_ws_url)

      # D. Sleep until next cycle
      cycle_count += 1
      time.sleep(args.interval_seconds)

    # 3. End Tracing and Download trace data
    print("==========================================================")
    print("Ending tracing session and fetching trace data...")
    ws.send(json.dumps({
        "id": 2,
        "method": "Tracing.end"
    }))

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
      elif msg.get("id") == 2:
        print("Tracing end acknowledged.")

    # 4. Save the single unified trace file
    trace_output_path = os.path.join(args.output_dir, f"persistent_soak_trace_{pid}.json")
    with open(trace_output_path, "w", encoding="utf-8") as f:
      json.dump({"traceEvents": trace_events}, f)
    print(f"Successfully saved persistent trace to {trace_output_path}")

  except Exception as e:
    print(f"Exception in collection loop: {e}", file=sys.stderr)
  finally:
    ws.close()
    print("Telemetry collection closed.")


def main():
  parser = argparse.ArgumentParser(description="Persistent memory soak collector.")
  parser.add_argument("--pid", type=int, help="PID of the Cobalt process")
  parser.add_argument("--process_name", type=str, default="cobalt", help="Process name to search for")
  parser.add_argument("--port", type=int, default=DEFAULT_PORT, help="DevTools port")
  parser.add_argument("--interval_seconds", type=int, default=DEFAULT_INTERVAL_SECONDS, help="Sample interval")
  parser.add_argument("--duration_seconds", type=int, default=DEFAULT_DURATION_SECONDS, help="Soak duration")
  parser.add_argument("--output_dir", type=str, default=DEFAULT_OUTPUT_DIR, help="Output directory")

  args = parser.parse_args()
  run_persistent_collection(args)


if __name__ == "__main__":
  main()
