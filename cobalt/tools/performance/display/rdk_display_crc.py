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
"""Display ground truth for RDK (Amlogic) devices via DRM CRTC CRC.

Reads the per-VBlank output CRC exposed by the DRM debugfs interface
(`/sys/kernel/debug/dri/0/crtc-0/crc/data`). Each line is
`<vblank_seq> <crc>`; the CRC is computed by the VPP over the final
post-blend pixels sent to HDMI, so a CRC change == a new image reached
the panel. Consecutive identical CRCs == the same image was repeated.

This is independent of Cobalt / Chromium / the compositor: it measures what
the display actually shows and is therefore a ground truth for frame
cadence, dropped frames and video pulldown.

Typical use (from the workstation):

  # Live view: one line per displayed frame, plus a cadence summary on exit.
  rdk_display_crc.py --device-ip <ip>

  # Capture 30 s to a file and summarise.
  rdk_display_crc.py --device-ip <ip> --duration 30 --output /tmp/crc.txt

  # Print every VBlank (including repeats).
  rdk_display_crc.py --device-ip <ip> --raw

  # Analyse a previously captured file offline.
  rdk_display_crc.py --input /tmp/crc.txt

Notes:
  * Only ONE process may hold `crc/data` open at a time; a second reader
    gets EBUSY. The tool resets the CRC source on start and on exit.
  * Enabling the CRC source does not change what is displayed.
  * The panel cadence is assumed to be 59.94 Hz (16.683 ms per VBlank);
    override with --vsync-ms for other modes.
"""

import argparse
import collections
import shlex
import signal
import subprocess
import sys
import time

DEFAULT_CRC_DIR = '/sys/kernel/debug/dri/0/crtc-0/crc'
DEFAULT_CRC_SOURCE = 'vpp1'  # Post-blend VPP output on Amlogic.
DEFAULT_VSYNC_MS = 1000.0 / 59.94

SSH_OPTS = [
    '-o', 'StrictHostKeyChecking=no', '-o', 'UserKnownHostsFile=/dev/null',
    '-o', 'LogLevel=ERROR'
]


def ssh_cmd(args, remote):
  return ['ssh'] + SSH_OPTS + [f'{args.user}@{args.device_ip}', remote]


def run_remote(args, remote, check=True):
  return subprocess.run(
      ssh_cmd(args, remote), check=check, capture_output=True, text=True)


def set_crc_source(args, source):
  run_remote(
      args,
      f'echo {shlex.quote(source)} > {shlex.quote(args.crc_dir)}/control',
      check=False)


def parse_line(line):
  """Returns (seq, crc) or None. Tolerates NUL bytes emitted by the kernel."""
  parts = line.replace('\x00', '').split()
  if len(parts) < 2:
    return None
  try:
    return int(parts[0], 16), parts[1].lower()
  except ValueError:
    return None


class CadenceTracker:
  """Tracks CRC changes per VBlank and accumulates cadence statistics."""

  def __init__(self, vsync_ms, raw, out, *, idle_vsyncs=0, live=True):
    self.vsync_ms = vsync_ms
    self.raw = raw
    self.out = out
    self.idle_vsyncs = idle_vsyncs  # holds >= this are idle, not hitches
    self.live = live
    self.idle_periods = 0
    self.first_seq = None
    self.last_seq = None
    self.last_crc = None
    self.last_change_seq = None
    self.vblanks = 0
    self.changes = 0
    self.hold_hist = collections.Counter()  # vsyncs each new image was held
    self.hold_seq = []  # ordered hold lengths, for pulldown detection
    self.seq_gaps = 0  # missed vblank samples (reader too slow / wrap)

  def feed(self, seq, crc):
    if self.first_seq is None:
      self.first_seq = seq
    if self.last_seq is not None and seq != self.last_seq + 1:
      self.seq_gaps += 1
    self.vblanks += 1

    changed = self.last_crc is not None and crc != self.last_crc
    if self.raw:
      mark = '<-- new image' if changed else ''
      print(f'{seq:#010x} {seq:>12d}  {crc}  {mark}', file=self.out, flush=True)

    if changed:
      self.changes += 1
      stamp = time.strftime('%H:%M:%S') + '  ' if self.live else ''
      if self.last_change_seq is not None:
        held = seq - self.last_change_seq
        if held <= 0:
          # VBlank counter wrapped or reset: the hold length is meaningless,
          # so skip the cadence stats for this change (the non-consecutive
          # sequence is already counted in seq_gaps above).
          if not self.raw:
            print(
                f'{stamp}vblank={seq:#010x} ({seq})  crc={crc}  '
                '(seq reset, hold ignored)',
                file=self.out,
                flush=True)
          self.last_change_seq = seq
          self.last_seq = seq
          self.last_crc = crc
          return
        idle = self.idle_vsyncs and held >= self.idle_vsyncs
        if idle:
          self.idle_periods += 1
        else:
          self.hold_hist[held] += 1
          self.hold_seq.append(held)
        if not self.raw:
          note = ''
          if idle:
            note = f'   <- idle {held * self.vsync_ms / 1000.0:.1f} s'
          elif held == 2:
            note = '   <- held 2 vsyncs (1 vblank repeated)'
          elif held > 2:
            note = f'   <- held {held} vsyncs ({held - 1} vblanks repeated)'
          print(
              f'{stamp}vblank={seq:#010x} ({seq})  '
              f'crc={crc}  gap={held} ({held * self.vsync_ms:.1f} ms){note}',
              file=self.out,
              flush=True)
      elif not self.raw:
        print(
            f'{stamp}vblank={seq:#010x} ({seq})  crc={crc}  (first change)',
            file=self.out,
            flush=True)
      self.last_change_seq = seq

    self.last_seq = seq
    self.last_crc = crc

  def bucket(self, held):
    """VSync bucket with the same half-step boundaries as the YTS gate."""
    ms = held * self.vsync_ms
    if ms < 25.0:
      return '1'
    if ms < 41.7:
      return '2'
    if ms < 58.4:
      return '3'
    return '4+'

  def detect_pulldown(self):
    """Best-effort pulldown / fixed-cadence classification."""
    seq = self.hold_seq[-120:]
    if len(seq) < 8:
      return None
    s = ''.join(str(min(h, 9)) for h in seq)
    if set(seq) == {1}:
      return '60 fps (every vblank new)'
    if set(seq) == {2}:
      return '30 fps (each image held 2 vsyncs)'
    if set(seq) <= {2, 3} and ('32' in s or '23' in s):
      return '24 fps via 3:2 pulldown (2,3,2,3... hold pattern)'
    if set(seq) <= {2, 3}:
      return '24-30 fps region (2/3 vsync holds)'
    return None

  def summary(self):
    lines = []
    span = (self.last_seq - self.first_seq + 1) if self.vblanks else 0
    if span <= 0:  # Counter wrapped or reset; fall back to samples seen.
      span = self.vblanks
    secs = span * self.vsync_ms / 1000.0 if span else 0.0
    lines.append('')
    lines.append('==== display cadence summary ====')
    lines.append(
        f'vblanks sampled     : {self.vblanks}  '
        f'(seq span {span}, ~{secs:.1f} s at {1000.0 / self.vsync_ms:.2f} Hz)')
    if self.seq_gaps:
      lines.append(f'WARNING: {self.seq_gaps} non-consecutive vblank sequence '
                   'gaps (reader stalled or buffer overrun)')
    lines.append(
        f'new images on panel : {self.changes}  '
        f'(avg {self.changes / secs if secs else 0:.1f} fps on screen)')
    if self.idle_vsyncs:
      lines.append(f'idle periods excluded: {self.idle_periods}  '
                   f'(holds >= {self.idle_vsyncs} vsyncs)')
    if not self.hold_hist:
      return '\n'.join(lines)
    n = sum(self.hold_hist.values())
    buckets = collections.Counter()
    for held, cnt in self.hold_hist.items():
      buckets[self.bucket(held)] += cnt
    lines.append('hold length (vsyncs) : ' + ', '.join(
        f'{h}x{c}' for h, c in sorted(self.hold_hist.items())))
    lines.append('VSync buckets        : ' + '  '.join(
        f'{b}V={100.0 * buckets[b] / n:5.1f}%' for b in ('1', '2', '3', '4+')))
    hitch = 100.0 * (n - buckets['1']) / n
    lines.append(f'hitch rate (>1 VSync): {hitch:.1f}%')
    srt = sorted(self.hold_hist.elements())
    p50 = srt[len(srt) // 2] * self.vsync_ms
    p95 = srt[min(len(srt) - 1, int(len(srt) * 0.95))] * self.vsync_ms
    lines.append(f'present interval     : p50 {p50:.1f} ms  p95 {p95:.1f} ms  '
                 f'max {srt[-1] * self.vsync_ms:.1f} ms')
    pd = self.detect_pulldown()
    if pd:
      lines.append(f'cadence pattern      : {pd}')
    return '\n'.join(lines)


def stream_lines(proc):
  for raw in proc.stdout:
    yield raw.decode('ascii', errors='replace')


def main():
  ap = argparse.ArgumentParser(
      description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
  ap.add_argument('--device-ip', help='RDK device IP (SSH, root)')
  ap.add_argument('--user', default='root')
  ap.add_argument('--input', help='Analyse a saved crc/data capture instead')
  ap.add_argument('--output', help='Also save the raw crc/data stream here')
  ap.add_argument(
      '--duration',
      type=float,
      default=0,
      help='Seconds to capture (0 = until Ctrl-C)')
  ap.add_argument(
      '--raw',
      action='store_true',
      help='Print every vblank, not only CRC changes')
  ap.add_argument('--vsync-ms', type=float, default=DEFAULT_VSYNC_MS)
  ap.add_argument(
      '--idle-vsyncs',
      type=int,
      default=0,
      help='Treat holds >= N vsyncs as idle (excluded from '
      'cadence stats), e.g. 30 for UI navigation runs')
  ap.add_argument('--crc-dir', default=DEFAULT_CRC_DIR)
  ap.add_argument(
      '--crc-source',
      default=DEFAULT_CRC_SOURCE,
      help='CRC source written to crc/control (Amlogic: vpp1)')
  args = ap.parse_args()

  tracker = CadenceTracker(
      args.vsync_ms,
      args.raw,
      sys.stdout,
      idle_vsyncs=args.idle_vsyncs,
      live=not args.input)

  if args.input:
    with open(args.input, encoding='ascii', errors='replace') as f:
      for line in f:
        parsed = parse_line(line)
        if parsed:
          tracker.feed(*parsed)
    print(tracker.summary())
    return 0

  if not args.device_ip:
    ap.error('--device-ip or --input is required')

  # Reset the CRC source so a stale reader state does not leave us with EBUSY.
  set_crc_source(args, 'NULL')
  set_crc_source(args, args.crc_source)

  deadline = time.monotonic() + args.duration if args.duration else None
  stop = False

  def _on_sigint(unused_sig, unused_frame):
    nonlocal stop
    stop = True

  signal.signal(signal.SIGINT, _on_sigint)
  reader = None
  out_file = None
  try:
    # pylint: disable=consider-using-with  (lifetimes managed in finally below)
    reader = subprocess.Popen(
        ssh_cmd(args, f'cat {shlex.quote(args.crc_dir)}/data'),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE)
    if args.output:
      out_file = open(args.output, 'w', encoding='ascii')
    # pylint: enable=consider-using-with
    print(
        f'Reading {args.crc_dir}/data on {args.device_ip} '
        f'(source={args.crc_source}); Ctrl-C to stop.',
        flush=True)
    for line in stream_lines(reader):
      if out_file:
        out_file.write(line.replace('\x00', ''))
      parsed = parse_line(line)
      if parsed:
        tracker.feed(*parsed)
      if stop or (deadline and time.monotonic() >= deadline):
        break
  finally:
    if reader:
      reader.terminate()
      try:
        reader.wait(timeout=3)
      except subprocess.TimeoutExpired:
        reader.kill()
      err = reader.stderr.read().decode(errors='replace').strip()
      if 'busy' in err.lower():
        print(
            'ERROR: crc/data is busy - another reader holds it open on the '
            'device (pkill the stale `cat`/`awk` and retry).',
            file=sys.stderr)
    if out_file:
      out_file.close()
    set_crc_source(args, 'NULL')
    print(tracker.summary())
  return 0


if __name__ == '__main__':
  sys.exit(main())
