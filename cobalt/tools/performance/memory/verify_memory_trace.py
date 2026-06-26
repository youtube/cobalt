#!/usr/bin/env python3
# Copyright 2026 The Cobalt Authors. All Rights Reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Verifies if a JSON trace is captured and contains memory dumps."""

import argparse
import json
import os
import sys


def verify_trace(trace_path):
  print("=" * 60)
  print(f"🔍 VERIFYING COBALT MEMORY TRACE: {trace_path}")
  print("=" * 60)

  # 1. Check file existence & size
  if not os.path.exists(trace_path):
    print("❌ Error: Trace file does not exist!")
    return False

  file_size_mb = os.path.getsize(trace_path) / (1024 * 1024)
  print(f"📊 File Size: {file_size_mb:.2f} MB")
  if file_size_mb == 0:
    print("❌ Error: Trace file is completely empty (0 bytes)!")
    return False

  # 2. Check JSON validity
  print("🔄 Parsing JSON structure...")
  try:
    with open(trace_path, "r", encoding="utf-8") as f:
      trace_data = json.load(f)
  except (json.JSONDecodeError, IOError) as e:
    print(f"❌ Error: Invalid JSON trace file! Parsing failed: {e}")
    return False

  events = []
  if isinstance(trace_data, list):
    events = trace_data
  elif isinstance(trace_data, dict):
    events = trace_data.get("traceEvents", [])
  else:
    print("❌ Error: Unexpected trace JSON structure (neither list nor dict)!")
    return False

  total_events = len(events)
  print(f"✅ Total Trace Events: {total_events}")
  if total_events == 0:
    print("❌ Error: No trace events found inside the file!")
    return False

  # 3. Analyze memory dump events
  memory_dumps_count = 0
  has_heaps_v2 = False
  has_process_mmaps = False
  has_smaps = False

  for event in events:
    if event.get("name") == "periodic_interval":
      memory_dumps_count += 1
      args = event.get("args", {})
      dumps = args.get("dumps", {})

      # The 'dumps' field can be a serialized JSON string or a dictionary
      dumps_dict = dumps
      if isinstance(dumps, str):
        try:
          dumps_dict = json.loads(dumps)
        except json.JSONDecodeError:
          pass

      if isinstance(dumps_dict, dict):
        if "heaps_v2" in dumps_dict:
          has_heaps_v2 = True
        if "process_mmaps" in dumps_dict:
          has_process_mmaps = True
        if "smaps" in dumps_dict:
          has_smaps = True

  # 4. Print scorecard
  print("\n📋 TRACE VERIFICATION SCORECARD:")
  print("-" * 40)

  if memory_dumps_count > 0:
    print(
        f"  • Memory Dumps Found:      🟢 YES ({memory_dumps_count} snapshots)")
  else:
    print("  • Memory Dumps Found:      🔴 NO")

  if has_heaps_v2:
    print("  • Heap Profiler (heaps_v2):🟢 PRESENT (C++ allocations captured)")
  else:
    print("  • Heap Profiler (heaps_v2):🔴 MISSING (No C++ callstacks found)")

  if has_process_mmaps:
    print(
        "  • VM Map (process_mmaps):  🟢 PRESENT (Library load addresses mapped)"
    )
  else:
    print(
        "  • VM Map (process_mmaps):  🔴 MISSING (No memory mapping lists found)"
    )

  if has_smaps:
    print("  • PSS Categories (smaps):  🟢 PRESENT (Memory categories mapped)")
  else:
    print(
        "  • PSS Categories (smaps):  ⚪ NOT PRESENT (Categorized memory absent)"
    )
  print("-" * 40)

  # 5. Final Verdict
  if memory_dumps_count > 0 and has_heaps_v2 and has_process_mmaps:
    print("\n🎉 SUCCESS: The trace is valid and fully profileable!")
    print("=" * 60)
    return True
  else:
    print("\n❌ VERDICT: The trace is INCOMPLETE or INVALID for profiling!")
    if memory_dumps_count == 0:
      print("   - Reason: Tracing did not capture any memory snapshots. "
            "Check if Cobalt crashed on start.")
    elif not has_heaps_v2:
      print("   - Reason: Trace does not contain 'heaps_v2'. "
            "Make sure --enable-heap-profiling and --memlog=all are passed.")
    elif not has_process_mmaps:
      print("   - Reason: Trace does not contain 'process_mmaps'. "
            "Memory addresses cannot be symbolized.")
    print("=" * 60)
    return False


if __name__ == "__main__":
  parser = argparse.ArgumentParser(description="Verifies Cobalt Memory Trace")
  parser.add_argument("trace_path", help="Path to JSON trace file")
  parsed_args = parser.parse_args()

  success = verify_trace(parsed_args.trace_path)
  sys.exit(0 if success else 1)
