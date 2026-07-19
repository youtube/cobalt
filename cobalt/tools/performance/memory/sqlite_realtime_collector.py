#!/usr/bin/env python3
# Copyright 2026 The Cobalt Authors. All Rights Reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Collects memory telemetry and writes it to SQLite in real-time on every cycle."""

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
import select
import traceback
import signal

# Default parameters
DEFAULT_PORT = 9222
DEFAULT_INTERVAL_SECONDS = 300
DEFAULT_DURATION_SECONDS = 36000
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


def capture_uma_histograms(ws):
  try:
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
      if "exceptionDetails" not in result:
        val = result.get("result", {}).get("value")
        if val:
          return val
  except Exception:
    pass
  return None


def capture_js_heap_metrics(ws):
  try:
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
  except Exception:
    pass
  return None


def init_db(db_path):
  conn = sqlite3.connect(db_path)
  conn.execute("PRAGMA journal_mode=WAL;")
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


class Symbolizer:
  def __init__(self, binary_path):
    self.binary_path = binary_path
    self.symbolizer_path = self._find_symbolizer()
    self.proc = None
    self.base_address = 0
    self._start_process()

  def _start_process(self):
    if not self.symbolizer_path:
      return
    self.proc = subprocess.Popen(
        [self.symbolizer_path, "--obj=" + self.binary_path, "--functions=linkage", "--no-inlines"],
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True
    )
    try:
      self.proc.stdin.write("INVALID\n")
      self.proc.stdin.flush()
      r, _, _ = select.select([self.proc.stdout], [], [], 60.0)
      if r:
        self.proc.stdout.readline() # ??
        self.proc.stdout.readline() # ??:0:0
        self.proc.stdout.readline() # Consume trailing empty line separator
      else:
        print("  [Symbolizer] Warning: llvm-symbolizer pre-initialization timed out.")
    except Exception as e:
      print(f"  [Symbolizer] Warning: Pre-initialization failed: {e}")

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
      self.proc.stdout.readline() # Consume trailing empty line separator
      
      if func == "??" or not func:
        return f"pc:0x{pc:x}"
      
      if file_line:
        return f"{func} ({file_line})"
      return func
    except Exception as e:
      print(f"  [Symbolizer] Error during symbolization: {e}")
      self._restart()
      return f"pc:0x{pc:x}"

  def _restart(self):
    print("  [Symbolizer] Restarting llvm-symbolizer subprocess...")
    try:
      self.proc.terminate()
      self.proc.wait(timeout=5)
    except Exception:
      try:
        self.proc.kill()
      except Exception:
        pass
    self._start_process()

  def close(self):
    if self.proc:
      try:
        self.proc.terminate()
      except Exception:
        pass


def _consume_response(ws, msg_id, timeout=10):
  start = time.time()
  try:
    ws.settimeout(timeout)
  except Exception:
    pass
  while time.time() - start < timeout:
    try:
      msg = json.loads(ws.recv())
      if msg.get("id") == msg_id:
        return msg
    except websocket.WebSocketTimeoutException:
      break
    except Exception as e:
      raise e
  return None


def write_heaps_to_db(conn, snapshot_id, heaps, symbolizer):
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
    
    if raw_symbol.startswith("pc:") or raw_symbol.startswith("0x"):
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
          
        db_records.append((snapshot_id, eff_alloc_name, size, node_id, parent_id, symbol))

  if db_records:
    cursor = conn.cursor()
    cursor.executemany(
        "INSERT INTO heap_allocations (snapshot_id, allocator, size_bytes, node_id, parent_node_id, symbol) VALUES (?, ?, ?, ?, ?, ?)",
        db_records
    )
    conn.commit()


def capture_and_write_heap_dump(ws, snapshot_id, symbolizer, conn):
  # 1. Start Tracing
  ws.send(json.dumps({
      "id": 200,
      "method": "Tracing.start",
      "params": {
          "traceConfig": {
              "includedCategories": ["disabled-by-default-memory-infra"],
              "excludedCategories": ["*"],
              "memoryDumpConfig": {"triggers": []}
          }
      }
  }))
  _consume_response(ws, 200)
  time.sleep(1) # Settle

  # 2. Request Dump
  ws.send(json.dumps({
      "id": 201,
      "method": "Tracing.requestMemoryDump",
      "params": {"levelOfDetail": "detailed"}
  }))
  _consume_response(ws, 201)

  # 3. End Tracing
  ws.send(json.dumps({"id": 202, "method": "Tracing.end"}))
  
  # Download Trace
  trace_events = []
  complete = False
  try:
    ws.settimeout(30)
  except Exception:
    pass
  while not complete:
    try:
      raw = ws.recv()
      if not raw:
        break
      msg = json.loads(raw)
      if "method" in msg:
        if msg["method"] == "Tracing.dataCollected":
          val = msg["params"].get("value")
          if isinstance(val, list):
            trace_events.extend(val)
          else:
            trace_events.append(val)
        elif msg["method"] == "Tracing.tracingComplete":
          complete = True
    except websocket.WebSocketTimeoutException:
      break
    except Exception as e:
      raise e

  # Parse heaps_v2
  periodic_events = [ev for ev in trace_events if ev.get("name") == "periodic_interval"]
  valid_heaps = None
  for ev in periodic_events:
    dumps = ev.get("args", {}).get("dumps", {})
    if isinstance(dumps, str):
      try:
        dumps = json.loads(dumps)
      except Exception:
        continue
    if isinstance(dumps, dict) and "heaps_v2" in dumps:
      valid_heaps = dumps["heaps_v2"]
      break

  if not valid_heaps:
    return

  # Resolve base address if not yet set
  if symbolizer and symbolizer.base_address == 0:
    for ev in trace_events:
      if ev.get("name") == "periodic_interval":
        dumps = ev.get("args", {}).get("dumps", {})
        if isinstance(dumps, str):
          try:
            dumps = json.loads(dumps)
          except Exception:
            continue
        if isinstance(dumps, dict) and "process_mmaps" in dumps:
          vm_regions = dumps["process_mmaps"].get("vm_regions", [])
          binary_mappings = []
          for region in vm_regions:
            mapped_file = region.get("mf", "")
            if "cobalt" in mapped_file or "loader_app" in mapped_file:
              start_addr = region.get("sa", "")
              if start_addr:
                binary_mappings.append(int(start_addr, 16))
          if binary_mappings:
            base_addr = min(binary_mappings)
            symbolizer.set_base_address(base_addr)
            print(f"  🔍 Resolved ELF load base address: 0x{base_addr:x}")
            break

  # Bulk Insert allocations
  write_heaps_to_db(conn, snapshot_id, valid_heaps, symbolizer)
  print("  ✅ Logged and symbolized memory-infra heap allocations to SQLite.")


def dump_stack(sig, frame):
  print("=" * 60)
  print(f"Received signal {sig}. Dumping stack trace:")
  traceback.print_stack(frame)
  print("=" * 60)
  sys.stdout.flush()


def main():
  signal.signal(signal.SIGUSR1, dump_stack)
  parser = argparse.ArgumentParser(description="Real-Time SQLite Telemetry Soak Collector.")
  parser.add_argument("--pid", type=int, help="PID of Cobalt")
  parser.add_argument("--process_name", type=str, default="cobalt", help="Process name")
  parser.add_argument("--port", type=int, default=DEFAULT_PORT, help="DevTools Port")
  parser.add_argument("--interval_seconds", type=int, default=DEFAULT_INTERVAL_SECONDS, help="Interval")
  parser.add_argument("--duration_seconds", type=int, default=DEFAULT_DURATION_SECONDS, help="Duration")
  parser.add_argument("--db_path", type=str, default=DEFAULT_DB_PATH, help="Output DB path")
  parser.add_argument("--binary", type=str, required=True, help="Path to unstripped cobalt binary")
  args = parser.parse_args()

  pid = args.pid
  if not pid:
    pid = get_cobalt_pid(args.process_name)
    if not pid:
      print(f"Error: Process '{args.process_name}' not running!", file=sys.stderr)
      sys.exit(1)

  print(f"Initializing SQLite Database at: {args.db_path}...")
  conn = init_db(args.db_path)
  
  cursor = conn.cursor()
  cursor.execute(
      "INSERT INTO runs (start_time, duration_seconds, interval_seconds, cobalt_pid) VALUES (?, ?, ?, ?)",
      (datetime.datetime.now().isoformat(), args.duration_seconds, args.interval_seconds, pid)
  )
  run_id = cursor.lastrowid
  conn.commit()
  print(f"Registered Run ID: {run_id}")

  symbolizer = Symbolizer(args.binary)
  start_time = time.time()
  end_time = start_time + args.duration_seconds
  cycle_count = 0

  # Connection helper for DevTools Browser
  def connect_browser_ws():
    ws_url = get_devtools_ws_url(args.port)
    if not ws_url:
      return None
    try:
      return websocket.create_connection(ws_url, max_size=100 * 1024 * 1024, timeout=15)
    except Exception:
      return None

  ws = connect_browser_ws()

  try:
    while time.time() < end_time:
      elapsed = int(time.time() - start_time)
      timestamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")

      # 1. Smaps Rollup
      smaps = parse_smaps_rollup(pid)

      # 2. JS Heap Memory & UMA
      used_js = None
      total_js = None
      uma_payload = None
      page_ws_url = get_page_ws_url(args.port)
      if page_ws_url:
        page_ws = None
        try:
          page_ws = websocket.create_connection(page_ws_url, timeout=10)
          
          # Query JS Heap
          js_metrics = capture_js_heap_metrics(page_ws)
          if js_metrics:
            used_js = js_metrics.get("used")
            total_js = js_metrics.get("total")
            
          # Query UMA
          uma_payload = capture_uma_histograms(page_ws)
        except Exception as e:
          print(f"[{timestamp}] Warning: Failed to query page metrics: {e}")
        finally:
          if page_ws:
            try:
              page_ws.close()
            except Exception:
              pass

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
          conn.commit()
          js_mb = f", JS = {used_js/(1024*1024):.2f} MB" if used_js else ""
          print(f"[{timestamp}] Cycle {cycle_count}: PSS = {pss:.2f} MB{js_mb} (Saved Snapshot ID: {snapshot_id})")
        except Exception as e:
          print(f"[{timestamp}] Warning: Failed to write snapshot to DB: {e}")
          cycle_count += 1
          time.sleep(args.interval_seconds)
          continue

        # 3. UMA Histograms
        if uma_payload:
          try:
            cursor.execute(
                "INSERT INTO uma_payloads (snapshot_id, payload_json) VALUES (?, ?)",
                (snapshot_id, json.dumps(uma_payload))
            )
            conn.commit()
          except Exception as e:
            print(f"[{timestamp}] Warning: Failed to write UMA payload to DB: {e}")

        # 4. Tracing Memory Heap allocations
        # Ensure browser websocket is active
        if ws is None:
          ws = connect_browser_ws()
        
        if ws:
          try:
            capture_and_write_heap_dump(ws, snapshot_id, symbolizer, conn)
          except Exception as e:
            print(f"[{timestamp}] Warning: Failed to capture trace dump: {e}")
            try:
              ws.close()
            except Exception:
              pass
            ws = None # Trigger reconnect on next cycle
        else:
          print(f"[{timestamp}] Warning: DevTools Browser websocket unavailable, skipping heap profile.")

      cycle_count += 1
      time.sleep(args.interval_seconds)

  except Exception as e:
    print(f"Exception in main loop: {e}", file=sys.stderr)
  finally:
    if ws:
      try:
        ws.close()
      except Exception:
        pass
    symbolizer.close()
    conn.close()
    print("Telemetry collection closed.")


if __name__ == "__main__":
  main()
