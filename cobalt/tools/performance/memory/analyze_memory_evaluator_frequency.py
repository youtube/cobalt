#!/usr/bin/env python3
# Copyright 2026 The Cobalt Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""analyze_memory_evaluator_frequency.py

Analyzes Cobalt System Memory Pressure Evaluator cadence and frequency.
Directly parses the C++ logging design in cobalt_system_memory_pressure_evaluator.cc:
1. Heartbeat Evaluation (VLOG(1)): Evaluated memory, budget, usage, and level.
2. Notification Dispatch (VLOG(1)): [DISPATCH] level and reason.
3. Startup Config (LOG(INFO)): Baseline budget, thresholds, poll interval, cooldown.

Diagnostic Goals:
- Evaluate Dwell Time: How long Cobalt stays in NONE, MODERATE, and CRITICAL.
- Evaluate Transition Dynamics: Frequency and breakdown of level transitions.
- Check Overstress Status: Boundary flapping, sustained emergency CRITICAL spam,
  and moderate cooldown damping compliance.

Usage:
  python3 analyze_memory_evaluator_frequency.py --log-file rdk_cobalt.log
  python3 analyze_memory_evaluator_frequency.py --log-file rdk_cobalt.log \
      --json output.json
"""

import argparse
from datetime import datetime, timedelta
import json
import os
import re
import sys
from typing import Any, Dict, List, Optional, Tuple


def parse_timestamp(line: str) -> Tuple[Optional[datetime], str]:
  """Extracts timestamp from syslog, Cobalt log, or dmesg format."""
  now = datetime.now()
  year = now.year

  # 1. Syslog / journalctl: "Sep 17 07:20:43"
  m_syslog = re.match(r"^([A-Z][a-z]{2}\s+\d+\s+\d{2}:\d{2}:\d{2}(?:\.\d+)?)",
                      line)
  if m_syslog:
    ts_str = m_syslog.group(1).split(".")[0]
    try:
      dt = datetime.strptime(f"{year} {ts_str}", "%Y %b %d %H:%M:%S")
      return dt, ts_str
    except (ValueError, TypeError):
      pass

  # 2. Cobalt format: "[32713:CrBrowserMain/32719:0917/032043.354663:INFO:..."
  m_cobalt = re.search(r"(\d{4})/(\d{2})(\d{2})(\d{2})\.(\d{6})", line)
  if m_cobalt:
    mmdd, hh, mm, ss, us = (
        m_cobalt.group(1),
        m_cobalt.group(2),
        m_cobalt.group(3),
        m_cobalt.group(4),
        m_cobalt.group(5),
    )
    ts_str = f"{mmdd} {hh}:{mm}:{ss}"
    try:
      month = int(mmdd[:2])
      day = int(mmdd[2:])
      dt = datetime(year, month, day, int(hh), int(mm), int(ss), int(us))
      return dt, ts_str
    except (ValueError, TypeError):
      pass

  # 3. Kernel uptime: "[  123.456789]"
  m_dmesg = re.match(r"^\[\s*(\d+\.\d+)\]", line)
  if m_dmesg:
    sec = float(m_dmesg.group(1))
    dt = datetime(year, 1, 1) + timedelta(seconds=sec)
    return dt, f"{sec:.2f}s"

  return None, ""


def parse_evaluator_logs(log_content: str) -> Dict[str, Any]:
  """Parses evaluator configuration, evaluations, and notification dispatches."""
  # Matches:
  # CobaltSystemMemoryPressureEvaluator initialized: baseline_budget=210 MB,
  # moderate_threshold=85%, critical_threshold=95%, poll_interval=5 s, cooldown=15 s
  init_pattern = re.compile(
      r"CobaltSystemMemoryPressureEvaluator initialized:\s*"
      r"baseline_budget=(\d+)\s*MB,\s*"
      r"moderate_threshold=([0-9.]+)%(?:\s*\([^)]*\))?,\s*"
      r"critical_threshold=([0-9.]+)%(?:\s*\([^)]*\))?,\s*"
      r"poll_interval=([0-9.]+)\s*s,\s*"
      r"cooldown=([0-9.]+)\s*s")
  timer_pattern = re.compile(
      r"CobaltSystemMemoryPressureEvaluator:\s*"
      r"Starting periodic evaluation timer\s*\((\d+)\s*s\)")
  # Matches:
  # CobaltSystemMemoryPressureEvaluator: Evaluated memory: private=199 MB,
  # effective_budget=231 MB (base=210 MB, media=21 MB), usage=86%, level=MODERATE
  eval_pattern = re.compile(r"CobaltSystemMemoryPressureEvaluator:\s*"
                            r"Evaluated memory:\s*private=(\d+)\s*MB,\s*"
                            r"effective_budget=(\d+)\s*MB\s*"
                            r"\(base=(\d+)\s*MB,\s*media=(\d+)\s*MB\),\s*"
                            r"usage=([0-9.]+)%,\s*level=(\w+)")
  # Matches:
  # CobaltSystemMemoryPressureEvaluator: [DISPATCH] MODERATE reason=transition
  # CobaltSystemMemoryPressureEvaluator: [DISPATCH] CRITICAL reason=cooldown_expired
  # as well as legacy:
  # CobaltSystemMemoryPressureEvaluator: [DISPATCH] Dispatching MODERATE notification
  # CobaltSystemMemoryPressureEvaluator: [DISPATCH SUSTAINED] Cooldown expired (30s). Re-dispatching CRITICAL
  dispatch_pattern = re.compile(
      r"CobaltSystemMemoryPressureEvaluator:\s*"
      r"(?:\[DISPATCH SUSTAINED\]\s*(.*?)\s*Re-dispatching\s+(\w+)|"
      r"\[DISPATCH\]\s*(?:Dispatching\s+)?(\w+)(?:\s+reason=(\w+))?(.*))")

  init_config: Optional[Dict[str, Any]] = None
  evaluations: List[Dict[str, Any]] = []
  dispatches: List[Dict[str, Any]] = []

  for raw_line in log_content.splitlines():
    line = raw_line.strip()
    dt, ts_str = parse_timestamp(line)

    # 1. Evaluator Configuration Init
    m_init = init_pattern.search(line)
    if m_init:
      init_config = {
          "timestamp": ts_str,
          "dt": dt,
          "baseline_budget_mb": int(m_init.group(1)),
          "moderate_threshold_pct": float(m_init.group(2)),
          "critical_threshold_pct": float(m_init.group(3)),
          "poll_interval_s": float(m_init.group(4)),
          "cooldown_s": float(m_init.group(5)),
      }
    elif not init_config:
      m_timer = timer_pattern.search(line)
      if m_timer:
        init_config = {
            "timestamp": ts_str,
            "dt": dt,
            "baseline_budget_mb": None,
            "moderate_threshold_pct": None,
            "critical_threshold_pct": None,
            "poll_interval_s": float(m_timer.group(1)),
            "cooldown_s": 15.0,  # Canonical default
        }

    # 2. Periodic Evaluation
    m_eval = eval_pattern.search(line)
    if m_eval:
      evaluations.append({
          "timestamp": ts_str,
          "dt": dt,
          "private_mb": int(m_eval.group(1)),
          "effective_budget_mb": int(m_eval.group(2)),
          "base_budget_mb": int(m_eval.group(3)),
          "media_allowance_mb": int(m_eval.group(4)),
          "usage_pct": float(m_eval.group(5)),
          "level": m_eval.group(6).upper(),
      })

    # 3. Notification Dispatch
    m_disp = dispatch_pattern.search(line)
    if m_disp:
      level = (m_disp.group(2) or m_disp.group(3) or "").upper()
      explicit_reason = m_disp.group(4)
      extra = (m_disp.group(1) or m_disp.group(5) or "").lower()
      if explicit_reason:
        reason = explicit_reason.lower()
      elif "cooldown expired" in extra or "sustained" in line.lower():
        reason = "cooldown_expired"
      else:
        reason = "transition"

      dispatches.append({
          "timestamp": ts_str,
          "dt": dt,
          "level": level,
          "reason": reason,
      })

  return {
      "init_config": init_config,
      "evaluations": evaluations,
      "dispatches": dispatches,
  }


def compute_frequency_metrics(events: List[Dict[str, Any]],) -> Dict[str, Any]:
  """Computes inter-event intervals (seconds) and cadence statistics."""
  intervals: List[float] = []
  for i in range(1, len(events)):
    dt_prev = events[i - 1].get("dt")
    dt_curr = events[i].get("dt")
    if dt_prev and dt_curr:
      diff = (dt_curr - dt_prev).total_seconds()
      if diff >= 0:
        intervals.append(diff)

  if not intervals:
    return {
        "count": len(events),
        "intervals": [],
        "min_s": None,
        "max_s": None,
        "avg_s": None,
    }

  return {
      "count": len(events),
      "intervals": intervals,
      "min_s": round(min(intervals), 2),
      "max_s": round(max(intervals), 2),
      "avg_s": round(sum(intervals) / len(intervals), 2),
  }


def analyze_frequency_and_cadence(data: Dict[str, Any]) -> Dict[str, Any]:
  """Derives dwell times, transitions, cadence, flapping, and stress metrics."""
  init = data.get("init_config") or {}
  cooldown_s = init.get("cooldown_s") or 15.0
  poll_s = init.get("poll_interval_s") or 5.0

  evaluations = data.get("evaluations", [])
  dispatches = data.get("dispatches", [])

  # 1. State Transitions: derived directly from consecutive evaluations
  transitions: List[Dict[str, Any]] = []
  transition_counts: Dict[str, int] = {}
  for prev_ev, curr_ev in zip(evaluations[:-1], evaluations[1:]):
    if prev_ev["level"] != curr_ev["level"]:
      trans_key = f"{prev_ev['level']} -> {curr_ev['level']}"
      transition_counts[trans_key] = transition_counts.get(trans_key, 0) + 1
      transitions.append({
          "timestamp": curr_ev["timestamp"],
          "dt": curr_ev["dt"],
          "from_level": prev_ev["level"],
          "to_level": curr_ev["level"],
          "memory": curr_ev,
      })

  # 2. Boundary Flapping: transitions alternating back within < 10 seconds
  flapping_count = 0
  for i in range(1, len(transitions)):
    prev_tr = transitions[i - 1]
    curr_tr = transitions[i]
    if prev_tr.get("dt") and curr_tr.get("dt"):
      delta = (curr_tr["dt"] - prev_tr["dt"]).total_seconds()
      if delta < 10.0 and prev_tr["from_level"] == curr_tr["to_level"]:
        flapping_count += 1

  # 3. Evaluation Polling Cadence
  poll_cadence = compute_frequency_metrics(evaluations)

  # 4. Level Dwell Times & Duty Cycle
  total_evals = len(evaluations)
  dwell_seconds: Dict[str, float] = {
      "NONE": 0.0,
      "MODERATE": 0.0,
      "CRITICAL": 0.0
  }
  dwell_ticks: Dict[str, int] = {"NONE": 0, "MODERATE": 0, "CRITICAL": 0}
  episode_counts: Dict[str, int] = {"NONE": 0, "MODERATE": 0, "CRITICAL": 0}

  current_episode_lvl = None
  for i, ev in enumerate(evaluations):
    lvl = ev["level"]
    if lvl in dwell_ticks:
      dwell_ticks[lvl] += 1

    # Episode counting
    if lvl != current_episode_lvl:
      if lvl in episode_counts:
        episode_counts[lvl] += 1
      current_episode_lvl = lvl

    # Approximate duration for this tick based on next tick dt or poll_s
    tick_dur = poll_s
    if i + 1 < total_evals and ev.get("dt") and evaluations[i + 1].get("dt"):
      step_s = (evaluations[i + 1]["dt"] - ev["dt"]).total_seconds()
      if 0 < step_s <= poll_s * 2.5:
        tick_dur = step_s

    if lvl in dwell_seconds:
      dwell_seconds[lvl] += tick_dur

  total_duration_s = sum(dwell_seconds.values())
  avg_dwell_per_episode: Dict[str, Optional[float]] = {}
  duty_cycle: Dict[str, float] = {}

  for lvl in ["NONE", "MODERATE", "CRITICAL"]:
    count = episode_counts[lvl]
    avg_dwell_per_episode[lvl] = (
        round(dwell_seconds[lvl] / count, 1) if count > 0 else None)
    duty_cycle[f"{lvl.lower()}_pct"] = (
        round(100.0 * dwell_seconds[lvl] /
              total_duration_s, 1) if total_duration_s > 0 else 0.0)

  transition_rate_per_min = (
      round(len(transitions) /
            (total_duration_s / 60.0), 2) if total_duration_s > 0 else 0.0)

  # 5. Dispatch Cadence & Cooldown Compliance
  critical_dispatches = [d for d in dispatches if d["level"] == "CRITICAL"]
  moderate_dispatches = [d for d in dispatches if d["level"] == "MODERATE"]

  crit_stats = compute_frequency_metrics(critical_dispatches)
  mod_stats = compute_frequency_metrics(moderate_dispatches)

  moderate_cooldown_violations = 0
  for iv in mod_stats.get("intervals", []):
    if iv < (cooldown_s - 1.0):  # Sub-second jitter margin
      moderate_cooldown_violations += 1

  # Non-NONE evaluation cycles where dispatch was suppressed by cooldown
  active_pressure_evals = dwell_ticks["MODERATE"] + dwell_ticks["CRITICAL"]
  throttled_ticks = max(0, active_pressure_evals - len(dispatches))

  # 6. Overstress Assessment Verdict
  is_emergency_overstressed = False
  if crit_stats["count"] >= 3 and crit_stats["avg_s"] and crit_stats[
      "avg_s"] <= (poll_s + 1.0):
    is_emergency_overstressed = True

  if is_emergency_overstressed:
    stress_verdict = "OVERSTRESSED_CRITICAL_SPAM"
  elif flapping_count >= 3:
    stress_verdict = "BORDER_FLAPPING_INSTABILITY"
  elif moderate_cooldown_violations > 0:
    stress_verdict = "COOLDOWN_VIOLATIONS"
  elif (duty_cycle["moderate_pct"] + duty_cycle["critical_pct"]) > 50.0:
    stress_verdict = "SUSTAINED_PRESSURE_DAMPENED"
  else:
    stress_verdict = "HEALTHY_STEADY_STATE"

  return {
      "transitions": transitions,
      "transition_counts": transition_counts,
      "transition_rate_per_min": transition_rate_per_min,
      "flapping_count": flapping_count,
      "poll_cadence": poll_cadence,
      "dwell_seconds": dwell_seconds,
      "avg_dwell_per_episode": avg_dwell_per_episode,
      "duty_cycle": duty_cycle,
      "total_duration_s": round(total_duration_s, 1),
      "critical_dispatches": crit_stats,
      "moderate_dispatches": mod_stats,
      "moderate_cooldown_violations": moderate_cooldown_violations,
      "throttled_ticks": throttled_ticks,
      "stress_verdict": stress_verdict,
  }


def print_report(data: Dict[str, Any], analysis: Dict[str, Any]) -> None:
  """Prints a structured ASCII report of evaluator behavior."""
  init = data.get("init_config") or {}
  print("\n" + "=" * 80)
  print("COBALT SYSTEM MEMORY PRESSURE EVALUATOR - CADENCE & FREQUENCY REPORT")
  print("=" * 80)

  # 1. Pipeline Config
  print("\n1. PIPELINE CONFIGURATION")
  print("-" * 40)
  if init:
    if init.get("baseline_budget_mb") is not None:
      print(f"  Baseline Budget:        {init.get('baseline_budget_mb')} MB")
      print(f"  Moderate Threshold:     {init.get('moderate_threshold_pct')}%")
      print(f"  Critical Threshold:     {init.get('critical_threshold_pct')}%")
    print(f"  Polling Interval:       {init.get('poll_interval_s')} s")
    print(f"  Moderate Cooldown:      {init.get('cooldown_s')} s")
  else:
    print("  (Evaluator configuration line not present in log segment)")

  # 2. Evaluations Cadence & Dwell Times
  evals = data.get("evaluations", [])
  poll = analysis["poll_cadence"]
  duty = analysis["duty_cycle"]
  dwell_s = analysis["dwell_seconds"]
  avg_dwell = analysis["avg_dwell_per_episode"]

  print(
      f"\n2. LEVEL DWELL TIMES & DUTY CYCLE (Observed: {analysis['total_duration_s']}s)"
  )
  print("-" * 40)
  if evals:
    poll_str = (
        f"Avg: {poll['avg_s']}s (Min: {poll['min_s']}s, Max: {poll['max_s']}s)"
        if poll["intervals"] else "N/A")
    print(f"  Heartbeat Polling:      {poll_str}")
    print("\n  Level Breakdown:")
    for lvl in ["NONE", "MODERATE", "CRITICAL"]:
      pct = duty[f"{lvl.lower()}_pct"]
      sec = round(dwell_s[lvl], 1)
      avg_s = f"{avg_dwell[lvl]}s" if avg_dwell[lvl] is not None else "N/A"
      print(f"    {lvl:<10}: {pct:>5.1f}% | Total Time: {sec:>6.1f}s |"
            f" Avg Episode Dwell: {avg_s}")
  else:
    print("  No memory evaluations found.")

  # 3. State Transitions & Frequency
  transitions = analysis["transitions"]
  print(
      f"\n3. STATE TRANSITIONS & DYNAMICS (Total Transitions: {len(transitions)})"
  )
  print("-" * 40)
  print(
      f"  Transition Frequency:   {analysis['transition_rate_per_min']} transitions / min"
  )
  if analysis["transition_counts"]:
    print("  Transition Types:")
    for trans_type, count in analysis["transition_counts"].items():
      print(f"    {trans_type:<24}: {count}")

  if transitions:
    print(f"\n  Transition Sequence:")
    print(f"    {'Timestamp':<16} {'Transition':<24} {'Private Memory':<16}"
          f" {'Usage %':<10}")
    print(f"    {'-'*16} {'-'*24} {'-'*16} {'-'*10}")
    for t in transitions:
      mem = t.get("memory") or {}
      priv_val = mem.get("private_mb", "-")
      eff_val = mem.get("effective_budget_mb", "-")
      priv_str = f"{priv_val} MB / {eff_val} MB"
      usage_str = f"{mem.get('usage_pct', '-')}%"
      trans_str = f"{t['from_level']} -> {t['to_level']}"
      print(f"    {t['timestamp']:<16} {trans_str:<24} {priv_str:<16}"
            f" {usage_str:<10}")
  else:
    print("  No state transitions (system remained at constant level).")

  # 4. Dispatch Frequencies
  print("\n4. NOTIFICATION DISPATCH FREQUENCIES (Evaluator Broadcasts)")
  print("-" * 40)
  crit_disp = analysis["critical_dispatches"]
  mod_disp = analysis["moderate_dispatches"]
  throttled_ticks = analysis["throttled_ticks"]

  print("  [CRITICAL DISPATCHES]")
  print(f"    Total Dispatches:     {crit_disp['count']}")
  if crit_disp["intervals"]:
    print(f"    Dispatch Intervals:   Min: {crit_disp['min_s']}s |"
          f" Avg: {crit_disp['avg_s']}s | Max: {crit_disp['max_s']}s")
  else:
    print("    Dispatch Intervals:   N/A (< 2 events)")

  print("\n  [MODERATE DISPATCHES]")
  print(f"    Total Dispatches:     {mod_disp['count']}")
  if mod_disp["intervals"]:
    print(f"    Dispatch Intervals:   Min: {mod_disp['min_s']}s |"
          f" Avg: {mod_disp['avg_s']}s | Max: {mod_disp['max_s']}s")
  else:
    print("    Dispatch Intervals:   N/A (< 2 events)")

  print(f"\n  [COOLDOWN THROTTLED TICKS]: {throttled_ticks}")

  # 5. Evaluator Overstress & Optimality Verdict
  print("\n5. EVALUATOR OVERSTRESS & HEALTH VERDICT")
  print("-" * 40)
  cooldown_s = init.get("cooldown_s", 15.0) if init else 15.0
  verdict = analysis["stress_verdict"]

  if verdict == "HEALTHY_STEADY_STATE":
    print(
        "  Overall Status:         [PASS] HEALTHY (Steady state, zero thrashing)"
    )
  elif verdict == "SUSTAINED_PRESSURE_DAMPENED":
    print(
        "  Overall Status:         [PASS] ELEVATED PRESSURE (Well-damped by cooldown)"
    )
  elif verdict == "OVERSTRESSED_CRITICAL_SPAM":
    print(
        "  Overall Status:         [CRITICAL OVERSTRESSED] Emergency CRITICAL spamming listeners!"
    )
  elif verdict == "BORDER_FLAPPING_INSTABILITY":
    print(
        "  Overall Status:         [WARNING] BOUNDARY FLAPPING (Rapid oscillation across threshold)"
    )
  else:
    print(f"  Overall Status:         [{verdict}]")

  # Detailed checks:
  flapping = analysis["flapping_count"]
  if flapping == 0:
    print(
        "  Boundary Stability:     [PASS] 0 rapid boundary oscillations detected (<10s)."
    )
  else:
    print(
        f"  Boundary Stability:     [WARN] {flapping} rapid oscillations detected (<10s)."
    )

  if analysis["moderate_cooldown_violations"] == 0:
    print(
        f"  Cooldown Damping:       [PASS] Respects {cooldown_s}s cooldown without listener spam."
    )
  else:
    print(
        f"  Cooldown Damping:       [WARN] Found {analysis['moderate_cooldown_violations']}"
        f" dispatches violating {cooldown_s}s cooldown!")

  print("\n" + "=" * 80 + "\n")


def main():
  parser = argparse.ArgumentParser(
      description=(
          "Cobalt Memory Pressure Evaluator: Cadence, Transition, and Frequency"
          " Analyzer"))
  parser.add_argument(
      "--log-file",
      type=str,
      required=True,
      help="Path to device log file (journalctl / Cobalt output).",
  )
  parser.add_argument(
      "--json",
      type=str,
      help="Optional path to output analysis metrics in JSON format.",
  )
  args = parser.parse_args()

  if not os.path.exists(args.log_file):
    print(f"Error: File {args.log_file} does not exist.", file=sys.stderr)
    sys.exit(1)
  with open(args.log_file, "r", encoding="utf-8", errors="ignore") as f:
    log_content = f.read()

  parsed_data = parse_evaluator_logs(log_content)
  analysis = analyze_frequency_and_cadence(parsed_data)

  print_report(parsed_data, analysis)

  if args.json:
    export_data = {
        "init_config": {
            k: (v.isoformat() if isinstance(v, datetime) else v)
            for k, v in (parsed_data.get("init_config") or {}).items()
        },
        "evaluations_count": len(parsed_data["evaluations"]),
        "transitions": [{
            "timestamp": t["timestamp"],
            "from": t["from_level"],
            "to": t["to_level"],
            "memory": t.get("memory"),
        } for t in analysis["transitions"]],
        "analysis": {
            "poll_cadence": analysis["poll_cadence"],
            "dwell_seconds": analysis["dwell_seconds"],
            "avg_dwell_per_episode": analysis["avg_dwell_per_episode"],
            "duty_cycle": analysis["duty_cycle"],
            "total_duration_s": analysis["total_duration_s"],
            "transition_counts": analysis["transition_counts"],
            "transition_rate_per_min": analysis["transition_rate_per_min"],
            "critical_dispatches": analysis["critical_dispatches"],
            "moderate_dispatches": analysis["moderate_dispatches"],
            "throttled_ticks": analysis["throttled_ticks"],
            "flapping_count": analysis["flapping_count"],
            "moderate_cooldown_violations":
                (analysis["moderate_cooldown_violations"]),
            "stress_verdict": analysis["stress_verdict"],
        },
    }
    with open(args.json, "w", encoding="utf-8") as jf:
      json.dump(export_data, jf, indent=2, default=str)
    print(f"Wrote JSON metrics to {args.json}")


if __name__ == "__main__":
  main()
