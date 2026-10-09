#!/usr/bin/env python3
#
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
"""Captures /proc/<pid>/smaps (and friends) for a process on a remote device.

Connects over SSH (key-based by default, or with a password via ``sshpass``),
finds the target process with the equivalent of ``ps -ef | grep <pattern>``
(picking the candidate with the largest RSS when several match, e.g. a
launcher wrapper and its child), and saves a snapshot directory containing:

  smaps.txt          /proc/<pid>/smaps
  smaps_rollup.txt   /proc/<pid>/smaps_rollup (if available)
  status.txt         /proc/<pid>/status
  cmdline.txt        /proc/<pid>/cmdline
  meminfo.txt        /proc/meminfo
  ps.txt             the ps listing used to find the process

Everything is fetched in a single SSH round trip so the files are as close to
an atomic snapshot as possible.

Usage:
  python3 capture_smaps.py --host 10.0.0.5
  python3 capture_smaps.py --host 10.0.0.5 --pattern WPEProcess
  python3 capture_smaps.py --host 10.0.0.5 --password secret  # needs sshpass
  python3 capture_smaps.py --host 10.0.0.5 --ask-password     # prompt for pass
  SMAPS_SSH_PASSWORD=secret python3 capture_smaps.py --host 10.0.0.5
  python3 capture_smaps.py --host 10.0.0.5 --pid 8661 --analyze
  python3 capture_smaps.py --host 10.0.0.5 --interval 5 --count 12
"""

import argparse
import datetime
import getpass
import os
import shutil
import subprocess
import sys
import time

DEFAULT_USER = 'root'
DEFAULT_PATTERN = 'cobalt'
DEFAULT_OUT_DIR = 'smaps_snapshots'

SSH_OPTS = [
    '-o',
    'StrictHostKeyChecking=no',
    '-o',
    'UserKnownHostsFile=/dev/null',
    '-o',
    'LogLevel=ERROR',
    '-o',
    'ConnectTimeout=10',
]

SECTION_MARK = '@@@SECTION '

# Set by main() from --password / $SMAPS_SSH_PASSWORD; None = key-based auth.
_PASSWORD = None


def ssh(host, user, remote_cmd, timeout=60):
  """Runs a command on the device and returns stdout (raises on failure)."""
  env = os.environ.copy()
  if _PASSWORD is None:
    cmd = ['ssh', '-o', 'BatchMode=yes'
          ] + SSH_OPTS + [f'{user}@{host}', remote_cmd]
  else:
    if shutil.which('sshpass') is None:
      raise RuntimeError('--password requires the "sshpass" tool '
                         '(e.g. sudo apt install sshpass), or set up SSH keys.')
    # `sshpass -e` reads the password from $SSHPASS so it is not visible in
    # the process list.
    env['SSHPASS'] = _PASSWORD
    cmd = ([
        'sshpass', '-e', 'ssh', '-o',
        'PreferredAuthentications=password,keyboard-interactive', '-o',
        'PubkeyAuthentication=no'
    ] + SSH_OPTS + [f'{user}@{host}', remote_cmd])
  res = subprocess.run(
      cmd, capture_output=True, timeout=timeout, check=False, env=env)
  if res.returncode != 0:
    err = res.stderr.decode('utf-8', 'replace').strip()
    if _PASSWORD is not None and res.returncode == 5:
      err = 'invalid password'
    raise RuntimeError(f'ssh to {user}@{host} failed ({res.returncode}): {err}')
  return res.stdout.decode('utf-8', 'replace')


def find_pid(host, user, pattern):
  """Finds the PID of the process matching ``pattern``.

  Mirrors ``ps -ef | grep <pattern> | grep -v grep``. If several processes
  match, the one with the largest VmRSS is chosen (that is the real engine
  process rather than a launcher/wrapper).
  """
  listing = ssh(host, user, 'ps -ef 2>/dev/null || ps 2>/dev/null')
  is_ps_ef = any(l.strip().startswith('UID') for l in listing.splitlines()[:5])
  candidates = []
  for line in listing.splitlines():
    low = line.lower()
    if pattern.lower() not in low or 'grep' in low or 'ps -ef' in low:
      continue
    parts = line.split()
    if len(parts) < 2:
      continue
    # ps -ef: UID PID PPID ... ; busybox ps: PID USER ...
    if is_ps_ef:
      pid = parts[1] if parts[1].isdigit() else None
    else:
      pid = parts[0] if parts[0].isdigit() else (
          parts[1] if parts[1].isdigit() else None)
    if pid and int(pid) > 0:
      candidates.append((int(pid), line.strip()))
  if not candidates:
    raise RuntimeError(f'No process matching "{pattern}" found on {host}. '
                       'Is the app running? Try --pattern or --pid.')

  if len(candidates) == 1:
    return candidates[0][0], candidates[0][1], listing

  # Several matches: pick the one with the largest resident set.
  pids = ' '.join(str(p) for p, _ in candidates)
  rss_out = ssh(
      host, user, f'for p in {pids}; do printf "%s " $p; '
      'grep VmRSS /proc/$p/status 2>/dev/null | awk \'{print $2}\' || echo 0; '
      'done')
  rss = {}
  for line in rss_out.splitlines():
    parts = line.split()
    if len(parts) >= 2 and parts[0].isdigit() and parts[1].isdigit():
      rss[int(parts[0])] = int(parts[1])
  best_pid, best_line = max(candidates, key=lambda c: rss.get(c[0], 0))
  print(f'[!] {len(candidates)} processes match "{pattern}":')
  for pid, line in candidates:
    mark = '*' if pid == best_pid else ' '
    rss_mb = rss.get(pid, 0) // 1024
    print(f'    {mark} pid {pid:>6}  VmRSS {rss_mb:>5} MB  {line[:60]}')
  return best_pid, best_line, listing


def capture(host, user, pid, out_root):
  """Fetches smaps/status/meminfo for ``pid`` into a new snapshot directory."""
  remote = (f'P={pid}; '
            f'echo "{SECTION_MARK}smaps.txt"; cat /proc/$P/smaps; '
            f'echo "{SECTION_MARK}smaps_rollup.txt"; '
            f'cat /proc/$P/smaps_rollup 2>/dev/null; '
            f'echo "{SECTION_MARK}status.txt"; cat /proc/$P/status; '
            f'echo "{SECTION_MARK}cmdline.txt"; '
            f'tr "\\0" " " < /proc/$P/cmdline; echo; '
            f'echo "{SECTION_MARK}meminfo.txt"; cat /proc/meminfo; '
            f'echo "{SECTION_MARK}uname.txt"; uname -a; '
            f'echo "{SECTION_MARK}END"')
  raw = ssh(host, user, remote, timeout=120)

  sections = {}
  name = None
  for line in raw.splitlines(keepends=True):
    if line.startswith(SECTION_MARK):
      name = line[len(SECTION_MARK):].strip()
      sections[name] = []
    elif name:
      sections[name].append(line)

  if not ''.join(sections.get('smaps.txt', [])).strip():
    raise RuntimeError(f'/proc/{pid}/smaps is empty or unreadable on {host} '
                       '(process gone, or insufficient permissions).')

  timestamp = datetime.datetime.now().strftime('%Y%m%d_%H%M%S')
  snap_dir = os.path.join(out_root, f'{timestamp}_pid{pid}')
  os.makedirs(snap_dir, exist_ok=True)
  for fname, lines in sections.items():
    if fname == 'END':
      continue
    content = ''.join(lines)
    if content.strip():
      with open(os.path.join(snap_dir, fname), 'w', encoding='utf-8') as f:
        f.write(content)
  return snap_dir


def summarize_status(snap_dir):
  """Prints the key VmRSS numbers from the captured status file."""
  path = os.path.join(snap_dir, 'status.txt')
  if not os.path.exists(path):
    return
  wanted = ('Name', 'Threads', 'VmSize', 'VmRSS', 'VmHWM', 'RssAnon', 'RssFile',
            'RssShmem', 'VmSwap')
  vals = {}
  with open(path, encoding='utf-8') as f:
    for line in f:
      key, _, val = line.partition(':')
      if key in wanted:
        vals[key] = val.strip()
  parts = []
  for key in wanted:
    if key in vals:
      v = vals[key]
      if v.endswith('kB'):
        v = f'{int(v.split()[0]) / 1024:.1f} MB'
      parts.append(f'{key}={v}')
  print('    ' + '  '.join(parts))


def main():
  parser = argparse.ArgumentParser(
      description='Capture /proc/<pid>/smaps from a remote device over SSH.')
  parser.add_argument(
      '--host', required=True, help='Device address (IP or hostname).')
  parser.add_argument(
      '--user',
      default=DEFAULT_USER,
      help=f'SSH user (default: {DEFAULT_USER}).')
  parser.add_argument(
      '--password',
      metavar='PASS',
      help='SSH password (needs "sshpass"). Use "-" to be prompted. Defaults '
      'to $SMAPS_SSH_PASSWORD if set; otherwise key-based auth is used.')
  parser.add_argument(
      '--ask-password',
      action='store_true',
      help='Prompt for the SSH password.')
  parser.add_argument(
      '--pattern',
      default=DEFAULT_PATTERN,
      help=f'Substring to look for in "ps -ef" (default: {DEFAULT_PATTERN}).')
  parser.add_argument(
      '--pid', type=int, help='Use this PID instead of searching.')
  parser.add_argument(
      '--out-dir',
      default=DEFAULT_OUT_DIR,
      help=f'Where to put snapshot directories (default: ./{DEFAULT_OUT_DIR}).')
  parser.add_argument(
      '--interval',
      type=float,
      default=0,
      help='Seconds between repeated snapshots (default: single snapshot).')
  parser.add_argument(
      '--count',
      type=int,
      default=1,
      help='Number of snapshots to take when --interval is set (default: 1).')
  parser.add_argument(
      '--analyze',
      action='store_true',
      help='Run analyze_smaps.py on each snapshot after capturing it.')
  args = parser.parse_args()

  global _PASSWORD  # pylint: disable=global-statement
  password = args.password if args.password is not None else os.environ.get(
      'SMAPS_SSH_PASSWORD')
  if args.ask_password or password == '-':
    password = getpass.getpass(f'Password for {args.user}@{args.host}: ')
  _PASSWORD = password or None

  try:
    if args.pid:
      pid, desc = args.pid, f'pid {args.pid}'
    else:
      print(f'[*] Looking for "{args.pattern}" on {args.user}@{args.host} ...')
      pid, desc, _ = find_pid(args.host, args.user, args.pattern)
    print(f'[+] Target: pid {pid}: {desc[:100]}')
  except (RuntimeError, subprocess.TimeoutExpired) as e:
    print(f'[!] {e}', file=sys.stderr)
    sys.exit(1)

  analyzer = os.path.join(
      os.path.dirname(os.path.abspath(__file__)), 'analyze_smaps.py')
  count = max(1, args.count) if args.interval > 0 else 1
  for i in range(count):
    try:
      snap_dir = capture(args.host, args.user, pid, args.out_dir)
    except (RuntimeError, subprocess.TimeoutExpired) as e:
      print(f'[!] {e}', file=sys.stderr)
      sys.exit(1)
    print(f'[+] Snapshot {i + 1}/{count} saved to {snap_dir}/')
    summarize_status(snap_dir)
    if args.analyze:
      if os.path.exists(analyzer):
        subprocess.run([sys.executable, analyzer, snap_dir], check=False)
      else:
        print(f'[-] {analyzer} not found; skipping analysis.')
    if i + 1 < count:
      time.sleep(args.interval)


if __name__ == '__main__':
  main()
