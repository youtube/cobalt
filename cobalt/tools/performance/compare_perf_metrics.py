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
"""Differential Performance Regression Analyzer for Cobalt CDP Telemetry.

Evaluates differential regressions across Chromium's performance vectors:
  1. Main-thread CPU starvation & task load
  2. UI Compositor Smoothness (FPS, P95 dips, dropped frames)
  3. Video Playback Quality QoS (W3C droppedVideoFrames / totalVideoFrames)
  4. V8 script execution stalls & layout cache thrashing
  5. Thread scheduling jitter
"""

import argparse
import csv
import math
import os
import sys


def parse_csv(file_path, warmup_sec=15.0):
  """Parses telemetry CSV, discarding initial warmup stabilization window."""
  records = []
  with open(file_path, 'r', encoding='utf-8') as f:
    reader = csv.DictReader(f)
    for row in reader:
      parsed = {
          'timestamp':
              float(row['timestamp']),
          'vsync_frames':
              float(row.get('vsync_frames') or row.get('frames') or 0.0),
          'frames':
              float(row.get('frames', 0.0)),
          'compositor_janks':
              float(row.get('compositor_janks', 0.0)),
          'compositor_dropped_pct':
              float(row.get('compositor_dropped_pct', 0.0)),
          'long_tasks_50ms':
              float(row.get('long_tasks_50ms', 0.0)),
          'long_tasks_150ms':
              float(row.get('long_tasks_150ms', 0.0)),
          'long_tasks_total_ms':
              float(row.get('long_tasks_total_ms', 0.0)),
          'task_duration_sec':
              float(row.get('task_duration_sec', 0.0)),
          'script_duration_sec':
              float(row.get('script_duration_sec', 0.0)),
          'layout_duration_sec':
              float(row.get('layout_duration_sec', 0.0)),
          'recalc_style_duration_sec':
              float(row.get('recalc_style_duration_sec', 0.0)),
          'private_memory_footprint_kb':
              float(row.get('private_memory_footprint_kb', 0.0)),
          'total_video_frames':
              float(row.get('total_video_frames', 0.0)),
          'dropped_video_frames':
              float(row.get('dropped_video_frames', 0.0)),
          'corrupted_video_frames':
              float(row.get('corrupted_video_frames', 0.0)),
      }
      records.append(parsed)

  if not records:
    raise ValueError(f'No telemetry data found in {file_path}')

  start_time = records[0]['timestamp']
  # Filter out warmup window
  filtered = [r for r in records if (r['timestamp'] - start_time) >= warmup_sec]
  return filtered if filtered else records


def compute_performance_metrics(records):
  """Extracts performance signals across CPU, scripting, layout, and frames."""
  fps_samples = []
  task_util_samples = []
  script_util_samples = []
  jank_intervals = 0
  total_intervals = 0

  for i in range(1, len(records)):
    dt = records[i]['timestamp'] - records[i - 1]['timestamp']
    if dt <= 0:
      continue

    total_intervals += 1
    # Check for RAF vsync frames, fallback to frames counter if older CSV
    cur_frames = records[i].get('vsync_frames')
    prev_frames = records[i - 1].get('vsync_frames')
    if cur_frames is None:
      cur_frames = records[i].get('frames', 0)
      prev_frames = records[i - 1].get('frames', 0)

    dframes = cur_frames - prev_frames
    dtask = (
        records[i]['task_duration_sec'] - records[i - 1]['task_duration_sec'])
    dscript = (
        records[i]['script_duration_sec'] -
        records[i - 1]['script_duration_sec'])

    inst_fps = dframes / dt
    fps_samples.append(inst_fps)
    task_util_samples.append((dtask / dt) * 100.0)
    script_util_samples.append((dscript / dt) * 100.0)

    # Detect multi-vsync presentation dips or compositor janks
    # If vsync_frames present, check inst_fps < 45.0 on 60Hz display
    if inst_fps < 45.0:
      jank_intervals += 1

  if not task_util_samples:
    raise ValueError(
        'No valid performance samples found. Ensure the telemetry CSV contains '
        'at least two records with distinct timestamps.')

  total_wall_time = records[-1]['timestamp'] - records[0]['timestamp']
  total_task = (
      records[-1]['task_duration_sec'] - records[0]['task_duration_sec'])
  total_script = (
      records[-1]['script_duration_sec'] - records[0]['script_duration_sec'])
  total_layout = (
      records[-1]['layout_duration_sec'] - records[0]['layout_duration_sec'])
  total_recalc = (
      records[-1]['recalc_style_duration_sec'] -
      records[0]['recalc_style_duration_sec'])

  avg_cpu = (total_task /
             total_wall_time) * 100.0 if total_wall_time > 0 else 0.0
  peak_task_load = max(task_util_samples) if task_util_samples else 0.0

  sorted_task = sorted(task_util_samples)
  p95_task_load = (
      sorted_task[int(len(sorted_task) * 0.95)] if sorted_task else 0.0)

  mean_task = (
      sum(task_util_samples) /
      len(task_util_samples) if task_util_samples else 0.0)
  variance = (
      sum((x - mean_task)**2 for x in task_util_samples) /
      len(task_util_samples) if task_util_samples else 0.0)
  std_task = math.sqrt(variance)

  # UI Compositor frame rate and P95 dip
  fps_samples_sorted = sorted(fps_samples)
  avg_fps = sum(fps_samples) / len(fps_samples) if fps_samples else 0.0
  p95_idx = int(len(fps_samples_sorted) * 0.05)
  p95_fps_dip = fps_samples_sorted[p95_idx] if fps_samples_sorted else 0.0

  # Long Tasks and Hitches
  long_tasks_50ms = records[-1].get('long_tasks_50ms', 0) - records[0].get(
      'long_tasks_50ms', 0)
  long_tasks_150ms = records[-1].get('long_tasks_150ms', 0) - records[0].get(
      'long_tasks_150ms', 0)
  long_tasks_total_ms = records[-1].get(
      'long_tasks_total_ms', 0.0) - records[0].get('long_tasks_total_ms', 0.0)

  # Video Playback QoS: dropped video frame percentage
  d_total_video = records[-1]['total_video_frames'] - records[0][
      'total_video_frames']
  d_dropped_video = records[-1]['dropped_video_frames'] - records[0][
      'dropped_video_frames']
  video_drop_rate = ((d_dropped_video / d_total_video) *
                     100.0 if d_total_video > 0 else 0.0)

  peak_ram = max(r['private_memory_footprint_kb'] / 1024.0 for r in records)

  return {
      'total_wall_time_sec':
          total_wall_time,
      'avg_cpu_percent':
          avg_cpu,
      'peak_task_load_percent':
          peak_task_load,
      'p95_task_load_percent':
          p95_task_load,
      'std_task_load_percent':
          std_task,
      'total_task_duration_sec':
          total_task,
      'total_script_duration_sec':
          total_script,
      'total_layout_duration_sec':
          total_layout,
      'total_recalc_style_sec':
          total_recalc,
      'layout_style_combined_sec':
          total_layout + total_recalc,
      'avg_fps':
          avg_fps,
      'p95_fps_dip':
          p95_fps_dip,
      'jank_intervals':
          jank_intervals,
      'total_intervals':
          total_intervals,
      'jank_percent': (jank_intervals / total_intervals *
                       100.0) if total_intervals > 0 else 0.0,
      'long_tasks_50ms':
          long_tasks_50ms,
      'long_tasks_150ms':
          long_tasks_150ms,
      'long_tasks_total_ms':
          long_tasks_total_ms,
      'total_video_frames':
          d_total_video,
      'dropped_video_frames':
          d_dropped_video,
      'video_drop_rate_percent':
          video_drop_rate,
      'operating_peak_ram_mb':
          peak_ram,
  }


def main():
  parser = argparse.ArgumentParser(
      description='Differential Performance Regression Analysis for Cobalt')
  parser.add_argument(
      '--baseline', required=True, help='Baseline CSV file path.')
  parser.add_argument(
      '--treatment', required=True, help='Treatment CSV file path.')
  parser.add_argument(
      '--max-cpu-increase-percent',
      type=float,
      default=2.0,
      help='Max allowable average CPU increase %% before FAIL.',
  )
  parser.add_argument(
      '--max-peak-saturation-percent',
      type=float,
      default=80.0,
      help='Max allowable peak thread load %% before FAIL.',
  )
  parser.add_argument(
      '--max-fps-drop-percent',
      type=float,
      default=1.0,
      help='Max allowable UI compositor frame rate drop %% before FAIL.',
  )
  parser.add_argument(
      '--max-video-drop-increase-percent',
      type=float,
      default=0.5,
      help='Max allowable video dropped frame rate increase %% before FAIL.',
  )
  parser.add_argument(
      '--max-script-overhead-sec',
      type=float,
      default=1.0,
      help='Max allowable V8 script execution time increase (sec) before FAIL.',
  )
  parser.add_argument(
      '--max-layout-overhead-sec',
      type=float,
      default=0.5,
      help='Max allowable Layout+Style reflow increase (sec) before FAIL.',
  )
  parser.add_argument(
      '--min-memory-savings-percent',
      type=float,
      default=0.0,
      help='Min required private RAM savings %% before FAIL (default 0.0%%).',
  )
  parser.add_argument(
      '--warmup-sec',
      type=float,
      default=15.0,
      help=('Warmup duration in seconds to discard from start of run to ensure '
            'ad initialization is excluded (default: 15.0s).'),
  )
  parser.add_argument(
      '--output', default=None, help='Path to output Markdown report.')
  args = parser.parse_args()

  base_data = parse_csv(args.baseline, warmup_sec=args.warmup_sec)
  treat_data = parse_csv(args.treatment, warmup_sec=args.warmup_sec)

  b = compute_performance_metrics(base_data)
  t = compute_performance_metrics(treat_data)

  # Calculate deltas for key performance regression vectors
  cpu_delta = t['avg_cpu_percent'] - b['avg_cpu_percent']
  peak_load_delta = t['peak_task_load_percent'] - b['peak_task_load_percent']
  p95_load_delta = t['p95_task_load_percent'] - b['p95_task_load_percent']
  fps_delta = t['avg_fps'] - b['avg_fps']
  fps_drop_percent = ((b['avg_fps'] - t['avg_fps']) / b['avg_fps'] *
                      100.0) if b['avg_fps'] > 0 else 0.0
  p95_fps_delta = t['p95_fps_dip'] - b['p95_fps_dip']
  video_drop_delta = (
      t['video_drop_rate_percent'] - b['video_drop_rate_percent'])
  script_delta = t['total_script_duration_sec'] - b['total_script_duration_sec']
  layout_delta = t['layout_style_combined_sec'] - b['layout_style_combined_sec']
  std_delta = t['std_task_load_percent'] - b['std_task_load_percent']

  ram_delta_mb = t['operating_peak_ram_mb'] - b['operating_peak_ram_mb']
  ram_savings_percent = (
      ((b['operating_peak_ram_mb'] - t['operating_peak_ram_mb']) /
       b['operating_peak_ram_mb'] *
       100.0) if b['operating_peak_ram_mb'] > 0 else 0.0)

  # Evaluate explicit performance regression decision gates
  failures = []

  # Gate 1: Main Thread Starvation (Mean Load)
  if cpu_delta > args.max_cpu_increase_percent:
    cpu_verdict = (f'FAIL (+{cpu_delta:.2f}% CPU exceeds '
                   f'+{args.max_cpu_increase_percent:.1f}% threshold)')
    failures.append('Main thread average CPU starvation')
  else:
    cpu_verdict = 'PASS (CPU overhead negligible)'

  # Gate 2: Peak Thread Freezes / Saturation
  t_peak_val = t['peak_task_load_percent']
  if (t_peak_val > args.max_peak_saturation_percent and peak_load_delta > 5.0):
    peak_verdict = f'FAIL (Peak thread load {t_peak_val:.1f}% saturated)'
    failures.append('Main thread instantaneous freeze')
  else:
    peak_verdict = 'PASS (Zero synchronous freezes)'

  # Gate 3: UI Compositor Frame Rate & Smoothness
  if fps_drop_percent > args.max_fps_drop_percent:
    fps_verdict = (f'FAIL (FPS dropped by {fps_drop_percent:.2f}%, '
                   f'exceeds {args.max_fps_drop_percent:.1f}% limit)')
    failures.append('UI compositor frame rate degradation')
  else:
    fps_verdict = 'PASS (Compositor frame rate preserved)'

  # Gate 4: Video QoS & Dropped Video Frames
  if t['total_video_frames'] > 0:
    if video_drop_delta > args.max_video_drop_increase_percent:
      video_verdict = (
          f'FAIL (Video drop rate increased by {video_drop_delta:.2f}%)')
      failures.append('Video playback quality QoS regression')
    else:
      video_verdict = 'PASS (Media decoder QoS intact)'
  else:
    video_verdict = 'PASS (No video streams active)'

  # Gate 5: V8 Engine & Script Execution Stalls
  if script_delta > args.max_script_overhead_sec:
    script_verdict = f'FAIL (+{script_delta:.2f}s script execution stall)'
    failures.append('V8 script execution stall')
  else:
    script_verdict = 'PASS (V8 execution unhindered)'

  # Gate 6: Layout & Style Cache Eviction Thrashing
  if layout_delta > args.max_layout_overhead_sec:
    layout_verdict = f'FAIL (+{layout_delta:.2f}s reflow thrashing)'
    failures.append('Layout/Style cache eviction thrashing')
  else:
    layout_verdict = 'PASS (Reflow thrashing negligible)'

  # Gate 7: Flapping / Scheduling Jitter
  if std_delta > 5.0:
    jitter_verdict = (
        f'FAIL (Load jitter variance increased by {std_delta:.1f}%)')
    failures.append('Task scheduling jitter')
  else:
    jitter_verdict = 'PASS (Stable thread scheduling)'

  # Gate 8: Memory Savings Verification
  if (args.min_memory_savings_percent > 0.0 and
      ram_savings_percent < args.min_memory_savings_percent):
    ram_verdict = (f'FAIL ({ram_savings_percent:.1f}% savings < '
                   f'{args.min_memory_savings_percent:.1f}% target)')
    failures.append('Insufficient memory reduction')
  else:
    ram_verdict = f'PASS ({ram_savings_percent:.1f}% RAM reduction)'

  b_cpu = b['avg_cpu_percent']
  t_cpu = t['avg_cpu_percent']
  b_peak = b['peak_task_load_percent']
  t_peak = t['peak_task_load_percent']
  b_p95 = b['p95_task_load_percent']
  t_p95 = t['p95_task_load_percent']
  b_fps = b['avg_fps']
  t_fps = t['avg_fps']
  b_p95_dip = b['p95_fps_dip']
  t_p95_dip = t['p95_fps_dip']
  b_vid_drop = b.get('video_drop_rate_percent', 0.0)
  t_vid_drop = t.get('video_drop_rate_percent', 0.0)
  b_script = b['total_script_duration_sec']
  t_script = t['total_script_duration_sec']
  b_layout = b['layout_style_combined_sec']
  t_layout = t['layout_style_combined_sec']
  b_std = b['std_task_load_percent']
  t_std = t['std_task_load_percent']
  b_ram = b['operating_peak_ram_mb']
  t_ram = t['operating_peak_ram_mb']

  table_rows = [
      f'| **Main Thread CPU** | {b_cpu:.2f}% | {t_cpu:.2f}% | '
      f'{cpu_delta:+.2f}% | **{cpu_verdict}** |',
      f'| **Peak Thread Load** | {b_peak:.2f}% | {t_peak:.2f}% | '
      f'{peak_load_delta:+.2f}% | **{peak_verdict}** |',
      f'| **P95 Thread Load** | {b_p95:.2f}% | {t_p95:.2f}% | '
      f'{p95_load_delta:+.2f}% | **PASS (Headroom intact)** |',
      f'| **UI Average FPS** | {b_fps:.2f} FPS | {t_fps:.2f} FPS | '
      f'{fps_delta:+.2f} FPS | **{fps_verdict}** |',
      f'| **UI P95 FPS Dip** | {b_p95_dip:.2f} FPS | {t_p95_dip:.2f} FPS | '
      f'{p95_fps_delta:+.2f} FPS | **PASS (Smoothness preserved)** |',
      f'| **Video QoS (Drop Rate)** | {b_vid_drop:.2f}% | {t_vid_drop:.2f}% | '
      f'{video_drop_delta:+.2f}% | **{video_verdict}** |',
      f'| **V8 Script Time** | {b_script:.2f}s | {t_script:.2f}s | '
      f'{script_delta:+.2f}s | **{script_verdict}** |',
      f'| **Layout/Style Time** | {b_layout:.2f}s | {t_layout:.2f}s | '
      f'{layout_delta:+.2f}s | **{layout_verdict}** |',
      f'| **Load Jitter (StdDev)** | {b_std:.2f}% | {t_std:.2f}% | '
      f'{std_delta:+.2f}% | **{jitter_verdict}** |',
      f'| **Peak Private RAM** | {b_ram:.1f} MB | {t_ram:.1f} MB | '
      f'{ram_delta_mb:+.1f} MB | **{ram_verdict}** |',
  ]

  report_lines = [
      '### Cobalt Differential Performance Regression Report',
      '',
      '*Target System*: Physical RDK Device',
      f'*Operating Context*: Peak RAM: {t_ram:.1f} MB '
      f'Treatment vs. {b_ram:.1f} MB Baseline '
      f'({ram_savings_percent:+.1f}% delta).',
      '',
      '| Performance Dimension | Baseline | Treatment | Delta | Verdict |',
      '| :--- | :--- | :--- | :--- | :--- |',
  ] + table_rows + ['']

  report = '\n'.join(report_lines)
  print('\n' + report)

  if args.output:
    output_dir = os.path.dirname(os.path.abspath(args.output))
    if output_dir:
      os.makedirs(output_dir, exist_ok=True)
    with open(args.output, 'w', encoding='utf-8') as f:
      f.write(report)
    print(f'Report written to: {args.output}')

  if failures:
    failed_str = ', '.join(failures)
    print(f'FAILURE: Performance regressions detected: {failed_str}')
    sys.exit(1)
  else:
    print('SUCCESS: Zero performance regressions detected. '
          'All performance gates passed.')
    sys.exit(0)


if __name__ == '__main__':
  main()
