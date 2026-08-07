#!/usr/bin/env python3
# Copyright 2026 The Cobalt Authors. All Rights Reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Collects soak test memory telemetry and writes it to a WAL-enabled SQLite DB."""

import argparse
import datetime
import json
import os
import sqlite3
import subprocess
import sys
import time
import requests
import websocket

# Default parameters
DEFAULT_PORT = 9222
DEFAULT_INTERVAL_SECONDS = 300
DEFAULT_DURATION_SECONDS = 36000  # 10 hours default
DEFAULT_DB_PATH = "soak_telemetry.db"


def get_cobalt_pid(process_name="cobalt"):
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
  metrics = {}
  smaps_path = f"/proc/{pid}/smaps_rollup"
  if not os.path.exists(smaps_path):
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
  try:
    resp = requests.get(f"http://localhost:{port}/json/version", timeout=5)
    resp.raise_for_status()
    return resp.json().get("webSocketDebuggerUrl")
  except Exception as e:
    print(f"Error connecting to DevTools: {e}", file=sys.stderr)
    return None


def get_page_ws_url(port):
  try:
    resp = requests.get(f"http://localhost:{port}/json", timeout=5)
    resp.raise_for_status()
    for target in resp.json():
      if target.get("type") == "page":
        return target.get("webSocketDebuggerUrl")
  except Exception as e:
    print(f"Error getting page WS URL: {e}", file=sys.stderr)
  return None


def capture_uma_histograms(ws_url):
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
    print("DEBUG: Runtime.evaluate response for UMA:", json.dumps(resp))
    if "result" in resp:
      result = resp["result"]
      if "exceptionDetails" in result:
        print("DEBUG: UMA javascript exception:", json.dumps(result["exceptionDetails"]))
        return None
      val = result.get("result", {}).get("value")
      if val:
        return val
  except Exception as e:
    print(f"DEBUG: capture_uma_histograms exception: {e}")
  finally:
    if ws:
      try:
        ws.close()
      except Exception:
        pass
  return None


def capture_js_heap_metrics(ws_url):
  ws = None
  try:
    ws = websocket.create_connection(ws_url, timeout=10)
    payload = {
        "id": 11,
        "method": "Runtime.evaluate",
        "params": {
            "expression": "(window.performance && window.performance.memory) ? JSON.stringify({used: window.performance.memory.usedJSHeapSize, total: window.performance.memory.totalJSHeapSize}) : 'null'",
            "returnByValue": True
        }
    }
    ws.send(json.dumps(payload))
    resp = json.loads(ws.recv())
    if "result" in resp:
      result = resp["result"]
      if "exceptionDetails" not in result:
        val = result.get("result", {}).get("value")
        if val and val != "null":
          return json.loads(val)
  except Exception as e:
    print(f"DEBUG: capture_js_heap_metrics exception: {e}")
  finally:
    if ws:
      try:
        ws.close()
      except Exception:
        pass
  return None


# Database helper functions
def init_db(db_path):
  conn = sqlite3.connect(db_path)
  # Enable WAL mode
  conn.execute("PRAGMA journal_mode=WAL;")
  
  # Create tables
  conn.execute("""
  CREATE TABLE IF NOT EXISTS runs (
      run_id INTEGER PRIMARY KEY AUTOINCREMENT,
      start_time TEXT,
      duration_seconds INTEGER,
      interval_seconds INTEGER,
      cobalt_pid INTEGER
  );
  """)
  
  conn.execute("""
  CREATE TABLE IF NOT EXISTS snapshots (
      snapshot_id INTEGER PRIMARY KEY AUTOINCREMENT,
      run_id INTEGER,
      timestamp TEXT,
      elapsed_seconds INTEGER,
      rss_mb REAL,
      pss_mb REAL,
      private_dirty_mb REAL,
      anonymous_mb REAL,
      used_js_heap_bytes INTEGER,
      total_js_heap_bytes INTEGER,
      FOREIGN KEY(run_id) REFERENCES runs(run_id)
  );
  """)
  
  conn.execute("""
  CREATE TABLE IF NOT EXISTS uma_payloads (
      snapshot_id INTEGER PRIMARY KEY,
      payload_json TEXT,
      FOREIGN KEY(snapshot_id) REFERENCES snapshots(snapshot_id)
  );
  """)
  
  conn.execute("""
  CREATE TABLE IF NOT EXISTS heap_allocations (
      allocation_id INTEGER PRIMARY KEY AUTOINCREMENT,
      snapshot_id INTEGER,
      allocator TEXT,
      size_bytes INTEGER,
      node_id INTEGER,
      parent_node_id INTEGER,
      symbol TEXT,
      FOREIGN KEY(snapshot_id) REFERENCES snapshots(snapshot_id)
  );
  """)
  conn.commit()
  return conn


# LLVM Symbolizer integration helper
class Symbolizer:
  def __init__(self, binary_path):
    self.binary_path = binary_path
    self.symbolizer_path = self._find_symbolizer()
    self.proc = None
    self.base_address = 0
    if self.symbolizer_path:
      self.proc = subprocess.Popen(
          [self.symbolizer_path, "--obj=" + self.binary_path, "--functions=linkage", "--inlines"],
          stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True
      )

  def _find_symbolizer(self):
    paths = [
        "third_party/llvm-build/Release+Asserts/bin/llvm-symbolizer",
        "/usr/bin/llvm-symbolizer"
    ]
    for p in paths:
      if os.path.exists(p):
        return p
    return None

  def set_base_address(self, addr):
    self.base_address = addr

  def symbolize(self, pc):
    if not self.proc:
      return f"pc:0x{pc:x}"
    offset = pc - self.base_address
    try:
      self.proc.stdin.write(f"0x{offset:x}\n")
      self.proc.stdin.flush()
      func = self.proc.stdout.readline().strip()
      file_line = self.proc.stdout.readline().strip()
      if func == "??" or not func:
        return f"pc:0x{pc:x}"
      return f"{func} ({file_line})"
    except Exception:
      return f"pc:0x{pc:x}"

  def close(self):
    if self.proc:
      self.proc.terminate()


def main():
  parser = argparse.ArgumentParser(description="WAL SQLite Telemetry Soak Collector.")
  parser.add_argument("--pid", type=int, help="PID of Cobalt")
  parser.add_argument("--process_name", type=str, default="cobalt", help="Process name")
  parser.add_argument("--port", type=int, default=DEFAULT_PORT, help="DevTools Port")
  parser.add_argument("--interval_seconds", type=int, default=DEFAULT_INTERVAL_SECONDS, help="Interval")
  parser.add_argument("--duration_seconds", type=int, default=DEFAULT_DURATION_SECONDS, help="Duration")
  parser.add_argument("--db_path", type=str, default=DEFAULT_DB_PATH, help="Output DB path")
  parser.add_argument("--binary", type=str, help="Path to unstripped cobalt binary (for post-run symbolization)")
  args = parser.parse_args()

  pid = args.pid
  if not pid:
    pid = get_cobalt_pid(args.process_name)
    if not pid:
      print(f"Error: Process '{args.process_name}' not running!", file=sys.stderr)
      sys.exit(1)

  print(f"Initializing SQLite Database at: {args.db_path}...")
  conn = init_db(args.db_path)
  
  # Register Run
  cursor = conn.cursor()
  cursor.execute(
      "INSERT INTO runs (start_time, duration_seconds, interval_seconds, cobalt_pid) VALUES (?, ?, ?, ?)",
      (datetime.datetime.now().isoformat(), args.duration_seconds, args.interval_seconds, pid)
  )
  run_id = cursor.lastrowid
  conn.commit()
  print(f"Registered Run ID: {run_id}")

  # Connect DevTools
  ws_url = get_devtools_ws_url(args.port)
  if not ws_url:
    print("Error: Could not retrieve browser DevTools URL!", file=sys.stderr)
    sys.exit(1)

  print("Connecting to DevTools and starting tracing...")
  ws = websocket.create_connection(ws_url, max_size=100 * 1024 * 1024)
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
  ws.recv() # Acknowledge start

  start_time = time.time()
  end_time = start_time + args.duration_seconds
  cycle_count = 0
  snapshot_ids = []

  try:
    while time.time() < end_time:
      elapsed = int(time.time() - start_time)
      timestamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")

      # 1. Capture Smaps Rollup
      smaps = parse_smaps_rollup(pid)
      
      # 1b. Capture JS Heap Memory
      used_js = None
      total_js = None
      page_ws = None
      try:
        page_ws = get_page_ws_url(args.port)
        if page_ws:
          js_metrics = capture_js_heap_metrics(page_ws)
          if js_metrics:
            used_js = js_metrics.get("used")
            total_js = js_metrics.get("total")
      except Exception as e:
        print(f"[{timestamp}] Warning: Failed to resolve page WS or query JS heap: {e}")

      if smaps:
        rss = smaps.get("Rss", 0)
        pss = smaps.get("Pss", 0)
        pd = smaps.get("Private_Dirty", 0)
        anon = smaps.get("Anonymous", 0)
        
        try:
          cursor.execute(
              "INSERT INTO snapshots (run_id, timestamp, elapsed_seconds, rss_mb, pss_mb, private_dirty_mb, anonymous_mb, used_js_heap_bytes, total_js_heap_bytes) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)",
              (run_id, timestamp, elapsed, rss, pss, pd, anon, used_js, total_js)
          )
          snapshot_id = cursor.lastrowid
          snapshot_ids.append((cycle_count, snapshot_id))
          conn.commit()
          js_mb = f", JS = {used_js/(1024*1024):.2f} MB" if used_js else ""
          print(f"[{timestamp}] Cycle {cycle_count}: PSS = {pss:.2f} MB{js_mb} (Saved Snapshot ID: {snapshot_id})")
        except Exception as e:
          print(f"[{timestamp}] Warning: Failed to write snapshot to DB: {e}")

        # 2. Capture UMA Histograms
        if page_ws:
          try:
            uma_payload = capture_uma_histograms(page_ws)
            if uma_payload:
              cursor.execute(
                  "INSERT INTO uma_payloads (snapshot_id, payload_json) VALUES (?, ?)",
                  (snapshot_id, json.dumps(uma_payload))
              )
              conn.commit()
              print("  ✅ Saved UMA Histograms Snapshot.")
          except Exception as e:
            print(f"[{timestamp}] Warning: Failed to capture UMA histograms: {e}")

      # 3. Request Trace memory dump
      try:
        ws.send(json.dumps({
            "id": 100 + cycle_count,
            "method": "Tracing.requestMemoryDump",
            "params": {"levelOfDetail": "detailed"}
        }))
        
        # Consume response
        attempts = 0
        while attempts < 20:
          msg = json.loads(ws.recv())
          if msg.get("id") == 100 + cycle_count:
            break
          attempts += 1
      except Exception as e:
        print(f"[{timestamp}] Warning: Failed to request memory dump from browser DevTools: {e}")

      cycle_count += 1
      time.sleep(args.interval_seconds)

    # End Tracing & Download Trace
    print("==========================================================")
    print("Ending tracing and downloading memory-infra dumps...")
    ws.send(json.dumps({"id": 2, "method": "Tracing.end"}))
    
    trace_events = []
    complete = False
    while not complete:
      msg = json.loads(ws.recv())
      if "method" in msg:
        if msg["method"] == "Tracing.dataCollected":
          val = msg["params"].get("value")
          if isinstance(val, list):
            trace_events.extend(val)
          else:
            trace_events.append(val)
        elif msg["method"] == "Tracing.tracingComplete":
          complete = True

    print(f"Trace download complete. Parsing {len(trace_events)} trace events...")

    # Locate periodic_interval events
    periodic_events = [ev for ev in trace_events if ev.get("name") == "periodic_interval"]
    
    # Filter only browser events containing heaps_v2
    valid_heap_dumps = []
    for ev in periodic_events:
      dumps = ev.get("args", {}).get("dumps", {})
      if isinstance(dumps, str):
        try:
          dumps = json.loads(dumps)
        except Exception:
          continue
      if isinstance(dumps, dict) and "heaps_v2" in dumps:
        valid_heap_dumps.append((ev["ts"], dumps["heaps_v2"]))

    print(f"Found {len(valid_heap_dumps)} valid heap snapshots inside trace.")

    # Perform Post-run symbolization and write to DB
    if valid_heap_dumps and args.binary:
      print("Starting symbolizer mapping...")
      # Determine ELF base address from mappings in the first snapshot
      # We extract mapping using process_mmaps from the first dump
      elf_base = None
      for ev in trace_events:
        if ev.get("name") == "periodic_interval":
          dumps = ev.get("args", {}).get("dumps", {})
          if isinstance(dumps, str):
            try:
              dumps = json.loads(dumps)
            except Exception:
              continue
          if isinstance(dumps, dict) and "process_mmaps" in dumps:
            # Parse mmaps
            vm_regions = dumps["process_mmaps"].get("vm_regions", [])
            binary_mappings = []
            for region in vm_regions:
              mapped_file = region.get("mf", "")
              if "cobalt" in mapped_file or "loader_app" in mapped_file:
                start_addr = region.get("sa", "")
                if start_addr:
                  binary_mappings.append(int(start_addr, 16))
            if binary_mappings:
              elf_base = min(binary_mappings)
              print(f"Found ELF base load address: 0x{elf_base:x}")
              break

      symbolizer = Symbolizer(args.binary)
      if elf_base:
        symbolizer.set_base_address(elf_base)
      else:
        print("Warning: Could not determine base ELF address, symbolization offsets might be incorrect.")

      # Match trace timestamp to snapshot database records
      valid_heap_dumps.sort(key=lambda x: x[0])
      
      for idx, (ts, heaps) in enumerate(valid_heap_dumps):
        if idx >= len(snapshot_ids):
          break
        _, snapshot_db_id = snapshot_ids[idx]
        
        print(f"Writing heap profile for Snapshot DB ID {snapshot_db_id}...")
        strings = heaps.get("maps", heaps).get("strings", [])
        nodes_list = heaps.get("maps", heaps).get("nodes", [])
        types_list = heaps.get("maps", heaps).get("types", [])
        
        id_to_string = {s["id"]: s["string"] for s in strings if "id" in s and "string" in s}
        id_to_type = {}
        for t in types_list:
          if "id" in t and "name_sid" in t:
            id_to_type[t["id"]] = id_to_string.get(t["name_sid"], f"type_{t['id']}")

        node_to_info = {}
        for n in nodes_list:
          if "id" not in n:
            continue
          node_id = n["id"]
          name_id = n.get("name_sid", n.get("name_id", n.get("sid")))
          parent_id = n.get("parent", n.get("parent_id"))
          
          raw_symbol = id_to_string.get(name_id, f"node_{node_id}")
          symbol = raw_symbol
          
          if raw_symbol.startswith("pc:0x") or raw_symbol.startswith("0x"):
            try:
              pc_val = int(raw_symbol.split(":")[-1], 16)
              symbol = symbolizer.symbolize(pc_val)
            except Exception:
              pass
              
          node_to_info[node_id] = {
              "symbol": symbol,
              "parent_id": parent_id
          }

        allocators = heaps.get("allocators", {})
        db_records = []
        
        for alloc_name, alloc_data in allocators.items():
          col_nodes = alloc_data.get("nodes")
          col_sizes = alloc_data.get("sizes", alloc_data.get("bytes"))
          col_types = alloc_data.get("types")
          
          if isinstance(col_nodes, list) and col_sizes:
            for a_idx, node_id in enumerate(col_nodes):
              size = col_sizes[a_idx] if a_idx < len(col_sizes) else 0
              if isinstance(size, str):
                size = int(size, 16) if size.startswith("0x") else int(size)
              size = int(size or 0)
              
              type_id = col_types[a_idx] if (col_types and a_idx < len(col_types)) else None
              eff_alloc_name = alloc_name
              if type_id is not None and type_id in id_to_type:
                type_name = id_to_type[type_id]
                if type_name and not type_name.startswith("type_"):
                  eff_alloc_name = f"{alloc_name}:{type_name}"

              symbol = "unknown"
              parent_id = None
              if node_id and node_id in node_to_info:
                symbol = node_to_info[node_id]["symbol"]
                parent_id = node_to_info[node_id]["parent_id"]
                
              db_records.append((snapshot_db_id, eff_alloc_name, size, node_id, parent_id, symbol))

        if db_records:
          cursor.executemany(
              "INSERT INTO heap_allocations (snapshot_id, allocator, size_bytes, node_id, parent_node_id, symbol) VALUES (?, ?, ?, ?, ?, ?)",
              db_records
          )
          conn.commit()

      symbolizer.close()
      print("🎉 Finished populating database with symbolized heap profiles!")

  except Exception as e:
    print(f"Exception in collection loop: {e}", file=sys.stderr)
  finally:
    ws.close()
    conn.close()
    print("Telemetry collection closed.")


if __name__ == "__main__":
  main()
