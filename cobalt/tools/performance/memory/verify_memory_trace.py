#!/usr/bin/env python3
# Copyright 2026 The Cobalt Authors. All Rights Reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Verifies if a JSON trace is captured and contains memory dumps."""

import argparse
import json
import logging
import os
import sys
from typing import Any, Dict, List, Optional

# Configure logging
logging.basicConfig(level=logging.INFO, format="%(message)s")
logger = logging.getLogger(__name__)


def parse_trace_file(trace_path: str) -> Optional[List[Dict[str, Any]]]:
  """Parses trace file and extracts list of trace events.

  Args:
    trace_path: Path to the JSON trace file.

  Returns:
    List of event dictionaries, or None if validation fails.
  """
  # 1. Check file existence & size
  if not os.path.isfile(trace_path):
    logger.error("❌ Error: Trace path does not exist or is not a file!")
    return None

  try:
    file_size_mb = os.path.getsize(trace_path) / (1024 * 1024)
    logger.info("📊 File Size: %.2f MB", file_size_mb)
    if file_size_mb == 0:
      logger.error("❌ Error: Trace file is completely empty (0 bytes)!")
      return None
  except OSError as e:
    logger.error("❌ Error: Failed to read file size: %s", e)
    return None

  # 2. Check JSON validity
  logger.info("🔄 Parsing JSON structure...")
  try:
    with open(trace_path, "r", encoding="utf-8") as f:
      trace_data = json.load(f)
  except (json.JSONDecodeError, IOError) as e:
    logger.error("❌ Error: Invalid JSON trace file! Parsing failed: %s", e)
    return None

  events = []
  if isinstance(trace_data, list):
    events = trace_data
  elif isinstance(trace_data, dict):
    trace_events = trace_data.get("traceEvents")
    if isinstance(trace_events, list):
      events = trace_events
    else:
      logger.error(
          "❌ Error: 'traceEvents' key in JSON is missing or not a list!")
      return None
  else:
    logger.error(
        "❌ Error: Unexpected trace JSON structure (neither list nor dict)!")
    return None

  total_events = len(events)
  logger.info("✅ Total Trace Events: %d", total_events)
  if total_events == 0:
    logger.error("❌ Error: No trace events found inside the file!")
    return None

  return events


# Categories that are expected in a pure memory trace.
_ALLOWED_CATEGORIES = {
    "disabled-by-default-memory-infra",
    "__metadata",
}


def _parse_bytes_to_mb(val: Any) -> Optional[float]:
  """Safely parses hex string or numeric bytes to Megabytes (MB)."""
  if val is None or isinstance(val, bool):
    return None
  try:
    if isinstance(val, (int, float)):
      return float(val) / (1024 * 1024)
    if isinstance(val, str):
      # Chromium trace JSON formats byte values as hex strings.
      return float(int(val, 16)) / (1024 * 1024)
  except (ValueError, TypeError):
    pass
  return None


def analyze_memory_events(events: List[Dict[str, Any]]) -> Dict[str, Any]:
  """Analyzes trace events to gather memory diagnostic metrics.

  Args:
    events: List of event dictionaries.

  Returns:
    A dictionary containing collected metrics.
  """
  metrics = {
      "memory_dumps_count": 0,
      "has_heaps_v2": False,
      "has_process_mmaps": False,
      "has_process_totals": False,
      "latest_private_footprint_mb": None,
      "latest_peak_rss_mb": None,
      "has_redundant_data": False,
      "noise_categories": set(),
  }

  for event in events:
    if not isinstance(event, dict):
      continue

    # 1. Check for redundant CPU / timeline categories
    cat = event.get("cat")
    if cat and isinstance(cat, str):
      for c in cat.split(","):
        c_stripped = c.strip()
        if c_stripped and c_stripped not in _ALLOWED_CATEGORIES:
          metrics["has_redundant_data"] = True
          metrics["noise_categories"].add(c_stripped)

    # 2. Check for memory snapshot dumps
    if event.get("name") == "periodic_interval":
      metrics["memory_dumps_count"] += 1
      args = event.get("args")
      if not isinstance(args, dict):
        continue
      dumps = args.get("dumps")

      # The 'dumps' field can be a serialized JSON string or a dictionary
      dumps_dict = dumps
      if isinstance(dumps, str):
        try:
          dumps_dict = json.loads(dumps)
        except json.JSONDecodeError:
          pass

      if isinstance(dumps_dict, dict):
        if "heaps_v2" in dumps_dict:
          metrics["has_heaps_v2"] = True
        if "process_mmaps" in dumps_dict:
          metrics["has_process_mmaps"] = True
        if "process_totals" in dumps_dict:
          metrics["has_process_totals"] = True
          pt = dumps_dict["process_totals"]
          if isinstance(pt, dict):
            priv = _parse_bytes_to_mb(pt.get("private_footprint_bytes"))
            rss = _parse_bytes_to_mb(pt.get("peak_resident_set_size"))
            if priv is not None:
              metrics["latest_private_footprint_mb"] = priv
            if rss is not None:
              metrics["latest_peak_rss_mb"] = rss

  return metrics


def print_scorecard(metrics: Dict[str, Any]) -> None:
  """Prints the scorecard table summarizing the trace audit results."""
  logger.info("\n📋 TRACE VERIFICATION SCORECARD:")
  logger.info("-" * 40)

  dumps_count = metrics["memory_dumps_count"]
  if dumps_count > 0:
    logger.info("  • Memory Dumps Found:      🟢 YES (%d snapshots)",
                dumps_count)
  else:
    logger.info("  • Memory Dumps Found:      🔴 NO")

  if metrics["has_heaps_v2"]:
    logger.info(
        "  • Heap Profiler (heaps_v2):🟢 PRESENT (C++ allocations captured)")
  else:
    logger.info(
        "  • Heap Profiler (heaps_v2):🔴 MISSING (No C++ callstacks found)")

  if metrics["has_process_mmaps"]:
    logger.info(
        "  • VM Map (process_mmaps):  🟢 PRESENT (Library load addresses mapped)"
    )
  else:
    logger.info(
        "  • VM Map (process_mmaps):  🔴 MISSING (No memory mapping lists found)"
    )

  if metrics["has_process_totals"]:
    priv = metrics["latest_private_footprint_mb"]
    rss = metrics["latest_peak_rss_mb"]
    details = []
    if priv is not None:
      details.append(f"Private: {priv:.1f} MB")
    if rss is not None:
      details.append(f"Peak RSS: {rss:.1f} MB")
    detail_str = " (" + ", ".join(details) + ")" if details else ""
    logger.info("  • OS Memory Totals:        🟢 PRESENT%s", detail_str)
  else:
    logger.info("  • OS Memory Totals:        ⚪ NOT PRESENT")

  if not metrics["has_redundant_data"]:
    logger.info("  • Trace Redundancy:        🟢 CLEAN (No CPU timeline noise)")
  else:
    noise_str = ", ".join(sorted(metrics["noise_categories"]))
    logger.info("  • Trace Redundancy:        🔴 NOISY (%s)", noise_str)

  logger.info("-" * 40)


def print_verdict(metrics: Dict[str, Any]) -> bool:
  """Checks the metrics and prints the final verification verdict."""
  dumps_count = metrics["memory_dumps_count"]
  has_heaps_v2 = metrics["has_heaps_v2"]
  has_process_mmaps = metrics["has_process_mmaps"]
  has_redundant_data = metrics["has_redundant_data"]

  if dumps_count > 0 and has_heaps_v2 and has_process_mmaps:
    if not has_redundant_data:
      logger.info("\n🎉 [SIGNAL: PROCEED] Trace is valid and clean! Ready for "
                  "symbolization.")
    else:
      logger.info(
          "\n⚠️ [SIGNAL: PROCEED_WITH_WARNING] Trace is valid but contains "
          "redundant CPU events.")
      logger.info("   - Tip: Pass "
                  "--trace-startup=\"-*,disabled-by-default-memory-infra\" "
                  "to shrink trace size by 10x-30x.")
    logger.info("=" * 60)
    return True

  logger.info(
      "\n❌ [SIGNAL: ABORT] Trace is INCOMPLETE or INVALID for profiling!")
  reasons = []
  if dumps_count == 0:
    reasons.append(
        "Tracing did not capture any memory snapshots. Check if Cobalt "
        "crashed on start.")
  if not has_heaps_v2:
    reasons.append(
        "Trace does not contain 'heaps_v2'. Make sure --enable-heap-profiling "
        "and --memlog=all are passed.")
  if not has_process_mmaps:
    reasons.append(
        "Trace does not contain 'process_mmaps'. Memory addresses cannot "
        "be symbolized.")

  for reason in reasons:
    logger.info("   - Reason: %s", reason)
  logger.info("=" * 60)
  return False


def verify_trace(trace_path: str) -> bool:
  """Orchestrates the trace verification flow."""
  logger.info("=" * 60)
  logger.info("🔍 VERIFYING COBALT MEMORY TRACE: %s", trace_path)
  logger.info("=" * 60)

  events = parse_trace_file(trace_path)
  if events is None:
    return False

  metrics = analyze_memory_events(events)
  print_scorecard(metrics)
  return print_verdict(metrics)


if __name__ == "__main__":
  # Reconfigure stdout/stderr to use UTF-8 to prevent UnicodeEncodeError
  # in non-UTF-8 console environments.
  sys.stdout.reconfigure(encoding="utf-8")
  sys.stderr.reconfigure(encoding="utf-8")

  parser = argparse.ArgumentParser(description="Verifies Cobalt Memory Trace")
  parser.add_argument("trace_path", help="Path to JSON trace file")
  parsed_args = parser.parse_args()

  success = verify_trace(parsed_args.trace_path)
  sys.exit(0 if success else 1)
