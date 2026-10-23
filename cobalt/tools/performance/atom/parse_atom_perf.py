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
"""ATOM_PERF (PoC): Parses run_once.sh output into metrics.

Usage: parse_atom_perf.py <run_dir> [<run_dir> ...]

With one run dir, prints a timeline. With several, prints one row per run and
median / min / max / CV per metric. All ATOM_PERF timestamps are
CLOCK_MONOTONIC microseconds.
"""
import os
import re
import statistics
import sys

LINE_RE = re.compile(r'ATOM_PERF (\S+)(.*)$')
KV_RE = re.compile(r'(\w+)=(\S+)')
PSS_RE = re.compile(r'TOTAL PSS:\s+(\d+)')

# A quiet period this long after an image paint means the screen has settled.
SETTLE_GAP_US = 3_000_000
FAR_FUTURE = 1 << 62
# Number of D-pad presses run_once.sh sends before OK.
N_DPAD = 10

METRICS = [
    # (key, label)
    ('activity_create', 'process start -> activity create'),
    ('nav_start', 'process start -> YT nav start'),
    ('fcp', 'process start -> YT first paint'),
    ('cold_launch', 'COLD LAUNCH: process start -> visually complete'),
    ('home_ui_load', 'HOME UI LOAD: nav start -> visually complete'),
    ('input_median', 'INPUT: D-pad keydown -> presented (median)'),
    ('input_max', 'INPUT: D-pad keydown -> presented (max)'),
    ('browse_to_watch', 'BROWSE-TO-WATCH: OK keydown -> presenting'),
    ('warm_launch', 'WARM LAUNCH: activity start -> first frame'),
    ('mem_home_mb', 'MEMORY: PSS on home (MB)'),
    ('mem_playback_mb', 'MEMORY: PSS in playback (MB)'),
    ('launched_to_picker', 'Launched to account picker (1=yes)'),
]


def read_events(run_dir):
  """Returns [(name, kv, phase)]. Phase comes from run_once.sh markers."""
  events = []
  phase = None
  with open(
      os.path.join(run_dir, 'atom_perf.log'),
      encoding='utf-8',
      errors='replace') as f:
    for line in f:
      m = LINE_RE.search(line)
      if not m:
        continue
      kv = dict(KV_RE.findall(m.group(2)))
      if m.group(1) == 'phase':
        phase = kv.get('name')
        continue
      events.append((m.group(1), kv, phase))
  return events


def read_pss_mb(run_dir, name):
  try:
    with open(os.path.join(run_dir, name), encoding='utf-8') as f:
      m = PSS_RE.search(f.read())
      return int(m.group(1)) / 1024 if m else None
  except FileNotFoundError:
    return None


def settle_time(times, start_us, end_us):
  """Last time in [start, end) before a SETTLE_GAP_US quiet period."""
  ts = sorted(t for t in times if start_us <= t < end_us)
  if not ts:
    return None, 0
  last, count = ts[0], 1
  for t in ts[1:]:
    if t - last > SETTLE_GAP_US:
      break
    last, count = t, count + 1
  return last, count


def analyze(run_dir):
  """Returns (metrics in ms, notes)."""
  events = read_events(run_dir)

  def t(kv, k='t_us'):
    return int(kv[k])

  def of(name, phase=None):
    return [
        kv for n, kv, p in events if n == name and (phase is None or p == phase)
    ]

  def first(name):
    return next(iter(of(name)), None)

  def keydowns_in(phase):
    return [kv for kv in of('event', phase) if kv.get('name') == 'keydown']

  r, notes = {}, []
  # Exclude blob: images: the splash screen logo and the autoplay preview
  # frame of the focused tile (which appears seconds later, right before the
  # preview video starts) are not part of the home UI loading.
  image_paints = [
      kv for kv in of('image_paint')
      if not kv.get('url', '').startswith('blob:')
  ]
  paints = [t(kv) for kv in image_paints]
  keydowns = [kv for kv in of('event') if kv.get('name') == 'keydown']
  stops = [t(kv) for kv in of('activity_stop')]
  first_stop = min(stops, default=FAR_FUTURE)
  first_key = min((int(k['created_us']) for k in keydowns), default=FAR_FUTURE)

  # The app occasionally starts on the account picker instead of HOME. HOME
  # paints video thumbnails (i.ytimg.com/vi/) before the first key press.
  launched_to_home = any(
      t(kv) < first_key and 'i.ytimg.com/vi/' in kv.get('url', '')
      for kv in image_paints)
  r['launched_to_picker'] = 0 if launched_to_home else 1
  if not launched_to_home:
    notes.append('launched to account picker; cold launch metrics skipped')

  # Cold launch. The splash screen is a separate document with its own FCP;
  # the YouTube app's FCP is the last one before the first key press.
  ps = first('process_start')
  fcps = [kv for kv in of('fcp') if t(kv) < first_key]
  fcp = fcps[-1] if fcps else None
  if ps:
    start = t(ps)
    if first('activity_create'):
      r['activity_create'] = (t(first('activity_create')) - start) / 1000
    if fcp:
      r['nav_start'] = (int(fcp['nav_start_us']) - start) / 1000
    if fcp and launched_to_home:
      r['fcp'] = (t(fcp) - start) / 1000
    vc, n = settle_time(paints, start, first_key)
    if vc and launched_to_home:
      r['cold_launch'] = (vc - start) / 1000
      notes.append(f'{n} image paints before visually complete')
      if fcp:
        r['home_ui_load'] = (vc - int(fcp['nav_start_us'])) / 1000

  has_phases = any(p for _, _, p in events)
  if has_phases:
    dpad_keys = keydowns_in('dpad')
    ok_keys = keydowns_in('browse_to_watch')
  else:
    # Older runs without phase markers: the first N_DPAD keydowns are the
    # D-pad presses, the next one is OK.
    session_keys = [k for k in keydowns if int(k['created_us']) < first_stop]
    dpad_keys = session_keys[:N_DPAD]
    ok_keys = session_keys[N_DPAD:N_DPAD + 1]

  # Input latency: D-pad keydown -> next frame presented.
  lat = [
      (int(k['presented_us']) - int(k['created_us'])) / 1000 for k in dpad_keys
  ]
  if lat:
    r['input_median'] = statistics.median(lat)
    r['input_max'] = max(lat)

  # Browse-to-watch: OK keydown -> first Presenting after it.
  if ok_keys:
    ok = int(ok_keys[0]['created_us'])
    presenting = [
        t(kv) for kv in of('player_state') if
        kv.get('name') == 'kSbPlayerStatePresenting' and ok < t(kv) < first_stop
    ]
    if presenting:
      r['browse_to_watch'] = (presenting[0] - ok) / 1000

  # Warm launch: activity_start after the stop -> first frame after visible.
  warm_starts = [t(kv) for kv in of('activity_start') if t(kv) > first_stop]
  if warm_starts:
    ws = warm_starts[0]
    frames = [t(kv) for kv in of('first_frame_after_visible') if t(kv) > ws]
    if frames:
      r['warm_launch'] = (frames[0] - ws) / 1000
    else:
      notes.append('no first_frame_after_visible after warm start')

  r['mem_home_mb'] = (
      read_pss_mb(run_dir, 'meminfo_home.txt') if launched_to_home else None)
  r['mem_playback_mb'] = read_pss_mb(run_dir, 'meminfo_playback.txt')
  return {k: v for k, v in r.items() if v is not None}, notes


def fmt(v):
  return '-' if v is None else f'{v:.0f}'


def main(run_dirs):
  results = [(d, *analyze(d)) for d in run_dirs]
  if len(results) == 1:
    d, r, notes = results[0]
    print(f'== {d}')
    for key, label in METRICS:
      print(f'  {label:50s} {fmt(r.get(key)):>7s}')
    for n in notes:
      print(f'  note: {n}')
    return

  names = [os.path.basename(d.rstrip('/')) for d, _, _ in results]
  header = ['median', 'min', 'max']
  print('metric (ms unless noted)'.ljust(50) + ' ' +
        ' '.join(n.rjust(7) for n in names) + ' ' +
        ' '.join(h.rjust(7) for h in header) + ' ' + 'CV%'.rjust(5))
  for key, label in METRICS:
    vals = [r.get(key) for _, r, _ in results]
    present = [v for v in vals if v is not None]
    row = f'{label:50s} ' + ' '.join(f'{fmt(v):>7s}' for v in vals)
    if present:
      med = statistics.median(present)
      cv = (
          statistics.stdev(present) / statistics.mean(present) *
          100 if len(present) > 1 and statistics.mean(present) else 0)
      row += (f' {med:7.0f} {min(present):7.0f} {max(present):7.0f}'
              f' {cv:5.1f}')
    print(row)
  for d, _, notes in results:
    for n in notes:
      print(f'note [{os.path.basename(d)}]: {n}')


if __name__ == '__main__':
  main(sys.argv[1:])
