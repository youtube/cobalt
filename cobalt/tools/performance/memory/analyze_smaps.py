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
"""Analyzes a Linux /proc/<pid>/smaps snapshot of a Cobalt/Chromium process.

Device independent: it only relies on generic Linux, glibc, Chromium and Cobalt
conventions (mapping names such as [anon:partition_alloc], [anon:v8], [heap],
[stack], /dev/shm, memfd, .so files, the anonymously loaded Cobalt ELF image).

The report contains:
  1. Totals (VSS/RSS/PSS/USS/swap) and a reconciliation against
     /proc/<pid>/status (VmRSS, RssAnon, RssFile, RssShmem) when available.
  2. A category breakdown ranked by resident memory with % of total and a
     running sum, so you can see which few categories make up the footprint.
  3. For each category, the largest constituent items ("... and N minor").
  4. Shared libraries, thread stacks and the top individual mappings.

Usage:
  python3 analyze_smaps.py smaps_snapshots/20261008_120000_pid8661/
  python3 analyze_smaps.py smaps.txt --status status.txt --meminfo meminfo.txt
  python3 analyze_smaps.py snapshot_dir --metric pss --top 15 --md report.md
  python3 analyze_smaps.py snapshot_dir --json > data.json
"""

import argparse
import json
import os
import re
import sys
# pylint: disable=inconsistent-quotes

from collections import defaultdict

KIB = 1024
MIB = 1024 * 1024
HEADER_RE = re.compile(
    r'^([0-9a-f]+)-([0-9a-f]+)\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s*(.*)$')

# Named categories; order here is only used as a tie-breaker.
CAT_COBALT_TEXT = 'Cobalt engine (anon ELF) .text'
CAT_COBALT_RODATA = 'Cobalt engine (anon ELF) .rodata'
CAT_COBALT_DATA = 'Cobalt engine (anon ELF) .data/.bss'
CAT_COBALT_FILES = 'Cobalt binaries & assets (file)'
CAT_PA = 'PartitionAlloc'
CAT_V8 = 'V8 heap'
CAT_NAMED_ANON = 'Other named anon [anon:*]'
CAT_STACKS = 'Thread stacks'
CAT_BRK_HEAP = 'C runtime heap [heap]'
CAT_ARENAS = 'glibc malloc arenas'
CAT_SHM = 'Shared memory / IPC'
CAT_DEVICE = 'Device mappings (/dev/*)'
CAT_LIBS_CODE = 'Shared libraries (code/rodata)'
CAT_LIBS_DATA = 'Shared libraries (CoW data)'
CAT_FONTS = 'Fonts'
CAT_FILES = 'Other file mappings'
CAT_ANON_RW = 'Anonymous rw-p (unnamed mmap)'
CAT_ANON_X = 'Anonymous executable (JIT)'
CAT_ANON_RO = 'Anonymous read-only'
CAT_ANON_NONE = 'Anonymous reserved ---p'
CAT_SPECIAL = 'Special kernel mappings'


class Mapping:
  """One VMA from smaps.

  This class represents a single Virtual Memory Area (VMA) parsed from a Linux
  smaps file, holding its address range, permissions, offset, device, inode,
  path, and various memory fields (RSS, PSS, Swap, etc.).

  Lifetime and Ownership:
    Instances are created during smaps parsing in parse_smaps and are owned
    by the returned list. They are short-lived, typically surviving only for
    the duration of the analysis and report generation.

  Threading Model:
    This class is not thread-safe and is Thread-affine (intended to be used
    from a single thread).
  """

  def __init__(self, match):
    self.start = int(match.group(1), 16)
    self.end = int(match.group(2), 16)
    self.perms = match.group(3)
    self.offset = match.group(4)
    self.dev = match.group(5)
    self.inode = match.group(6)
    self.path = match.group(7).strip()
    self.size_kb = (self.end - self.start) // KIB
    self.fields = {}
    self.vmflags = ()
    self.category = None

  @property
  def addr(self):
    return f'{self.start:x}-{self.end:x}'

  def kb(self, key):
    return self.fields.get(key, 0)

  @property
  def rss(self):
    return self.kb('Rss')

  @property
  def pss(self):
    return self.kb('Pss')

  @property
  def swap(self):
    return self.kb('Swap')

  @property
  def swap_pss(self):
    return self.fields.get('SwapPss', self.swap)

  @property
  def private(self):
    return self.kb('Private_Clean') + self.kb('Private_Dirty')

  @property
  def anonymous(self):
    return self.kb('Anonymous')

  @property
  def is_anon(self):
    return not self.path or self.path.startswith('[anon')

  @property
  def is_file(self):
    return bool(self.path) and not self.path.startswith(
        '[') and not self.path.startswith('/dev/')

  @property
  def basename(self):
    return self.path.split('/')[-1] if self.path else ''


def parse_smaps(path):
  mappings = []
  cur = None
  with open(path, encoding='utf-8', errors='replace') as f:
    for line in f:
      m = HEADER_RE.match(line)
      if m:
        cur = Mapping(m)
        mappings.append(cur)
      elif cur is not None:
        key, _, val = line.partition(':')
        val = val.split()
        if key == 'VmFlags':
          cur.vmflags = tuple(val)
        elif val and val[0].lstrip('-').isdigit():
          cur.fields[key.strip()] = int(val[0])
  return mappings


def parse_kv_kb(path):
  """Parses /proc/<pid>/status or /proc/meminfo into {key: value}."""
  out = {}
  if not path or not os.path.exists(path):
    return out
  with open(path, encoding='utf-8', errors='replace') as f:
    for line in f:
      key, _, val = line.partition(':')
      val = val.strip()
      if not key or not val:
        continue
      parts = val.split()
      out[key.strip()] = int(parts[0]) if parts[0].isdigit() else val
  return out


# ---------------------------------------------------------------------------
# Classification
# ---------------------------------------------------------------------------


def find_anon_elf_image(mappings):
  """Locates a large ELF image loaded into anonymous memory.

  Cobalt Evergreen's loader maps libcobalt.so with MAP_ANONYMOUS, so it shows
  up as a big unnamed r-xp VMA with adjacent r--p/rw-p segments rather than as
  a file mapping. Returns (text_mapping, start, end) or (None, 0, 0).
  """
  text = None
  for m in mappings:
    if m.is_anon and not m.path and 'x' in m.perms and m.size_kb >= 16 * KIB:
      if text is None or m.size_kb > text.size_kb:
        text = m
  if text is None:
    return None, 0, 0

  idx = mappings.index(text)
  start, end = text.start, text.end
  # Walk backwards/forwards over adjacent unnamed anon segments (rodata before,
  # data/bss after), stopping at the first gap or non-anon mapping.
  i = idx - 1
  while i >= 0 and mappings[i].is_anon and not mappings[i].path and mappings[
      i].end == start:
    start = mappings[i].start
    i -= 1
  i = idx + 1
  while i < len(mappings) and mappings[
      i].is_anon and not mappings[i].path and mappings[i].start == end:
    end = mappings[i].end
    i += 1
  return text, start, end


def mark_glibc_arenas(mappings):
  """Flags anonymous rw-p mappings that look like glibc malloc arenas.

  A non-main glibc arena is a HEAP_MAX_SIZE-aligned region (1 MiB on 32-bit,
  64 MiB on 64-bit) whose used part is rw-p and whose tail is PROT_NONE.
  """
  arena_sizes = (1 * MIB, 64 * MIB)
  for i, m in enumerate(mappings):
    if m.category or not (m.is_anon and not m.path and
                          m.perms.startswith('rw')):
      continue
    nxt = mappings[i + 1] if i + 1 < len(mappings) else None
    for hsz in arena_sizes:
      if m.start % hsz:
        continue
      if m.end - m.start == hsz:
        m.category = CAT_ARENAS
      elif (nxt is not None and nxt.is_anon and not nxt.path and
            nxt.perms.startswith('---') and nxt.start == m.end and
            nxt.end - m.start == hsz):
        m.category = CAT_ARENAS
        nxt.category = CAT_ARENAS


def mark_pthread_stacks(mappings):
  """Flags unnamed anon rw-p VMAs that are laid out like pthread stacks.

  glibc allocates a thread stack as one mmap whose lowest page(s) are
  mprotect'ed PROT_NONE as a guard, so the stack shows up as a small anon
  ``---p`` VMA immediately followed by an anon ``rw-p`` VMA. Kernels with
  anon-VMA naming show these as ``[anon:stack...]`` instead and are handled by
  name in classify().
  """
  # pylint: disable=chained-comparison
  for i in range(1, len(mappings)):
    m, prev = mappings[i], mappings[i - 1]
    if m.category or prev.category:
      continue
    if (m.is_anon and not m.path and m.perms.startswith('rw') and
        prev.is_anon and not prev.path and prev.perms.startswith('---') and
        prev.end == m.start and prev.size_kb <= 64 and m.size_kb >= 64 and
        m.size_kb <= 64 * KIB):
      m.category = CAT_STACKS
      prev.category = CAT_STACKS


def classify(m, elf_text, elf_start, elf_end):
  if m.category:
    return m.category
  p = m.path
  pl = p.lower()

  if elf_text is not None and elf_start <= m.start < elf_end and not p:
    if m is elf_text or 'x' in m.perms:
      return CAT_COBALT_TEXT
    if 'w' in m.perms:
      return CAT_COBALT_DATA
    return CAT_COBALT_RODATA

  if p.startswith('[anon:'):
    name = p[6:].rstrip(']')
    if 'partition_alloc' in name:
      return CAT_PA
    if name.startswith('v8') or '/v8' in name:
      return CAT_V8
    if 'stack' in name:
      return CAT_STACKS
    if name.startswith('libc_malloc') or name.startswith('scudo'):
      return CAT_ARENAS
    return CAT_NAMED_ANON
  if p.startswith('[stack') or p.startswith('[tstack'):
    return CAT_STACKS
  if p == '[heap]':
    return CAT_BRK_HEAP
  if p.startswith('['):
    return CAT_SPECIAL

  if 'cobalt' in pl or 'starboard' in pl or 'loader_app' in pl:
    return CAT_COBALT_FILES
  if '.so' in pl:
    return CAT_LIBS_DATA if 'w' in m.perms else CAT_LIBS_CODE
  if pl.endswith(('.ttf', '.otf', '.ttc', '.woff', '.woff2')) or 'font' in pl:
    return CAT_FONTS

  if (p.startswith('/dev/shm') or 'memfd:' in p or p.startswith('/SYSV') or
      '/dev/ashmem' in p or '(deleted)' in p or '.org.chromium.' in p):
    return CAT_SHM
  if p.startswith('/dev/'):
    return CAT_DEVICE
  if p:
    return CAT_FILES

  if 'x' in m.perms:
    return CAT_ANON_X
  if 'w' in m.perms:
    return CAT_ANON_RW
  if 'r' in m.perms:
    return CAT_ANON_RO
  return CAT_ANON_NONE


def item_key(m):
  """Groups mappings within a category into human-meaningful items."""
  if m.path.startswith('[anon:') or m.path.startswith('['):
    return f'{m.path} ({m.perms})'
  if m.path:
    return f'{m.basename} ({m.perms})'
  if m.category == CAT_ARENAS:
    return f'arena chunk {m.size_kb} kB ({m.perms})'
  if m.category == CAT_STACKS:
    return f'stack {m.size_kb} kB ({m.perms})'
  return f'anon {m.size_kb} kB {m.perms} @ {m.addr}'


# ---------------------------------------------------------------------------
# Aggregation
# ---------------------------------------------------------------------------


def new_bucket():
  return {
      'count': 0,
      'size': 0,
      'rss': 0,
      'pss': 0,
      'private': 0,
      'private_dirty': 0,
      'shared_clean': 0,
      'swap': 0,
      'swap_pss': 0,
      'anon': 0
  }


def add(bucket, m):
  bucket['count'] += 1
  bucket['size'] += m.size_kb
  bucket['rss'] += m.rss
  bucket['pss'] += m.pss
  bucket['private'] += m.private
  bucket['private_dirty'] += m.kb('Private_Dirty')
  bucket['shared_clean'] += m.kb('Shared_Clean')
  bucket['swap'] += m.swap
  bucket['swap_pss'] += m.swap_pss
  bucket['anon'] += m.anonymous


def analyze(mappings, metric):
  elf_text, elf_start, elf_end = find_anon_elf_image(mappings)
  # Protect the engine image from the heuristics below.
  for m in mappings:
    if elf_text is not None and not m.path and elf_start <= m.start < elf_end:
      m.category = classify(m, elf_text, elf_start, elf_end)
  mark_glibc_arenas(mappings)
  mark_pthread_stacks(mappings)
  for m in mappings:
    m.category = classify(m, elf_text, elf_start, elf_end)

  totals = new_bucket()
  cats = defaultdict(new_bucket)
  items = defaultdict(lambda: defaultdict(new_bucket))
  libs = defaultdict(new_bucket)
  for m in mappings:
    add(totals, m)
    add(cats[m.category], m)
    add(items[m.category][item_key(m)], m)
    if m.is_file and '.so' in m.path.lower():
      add(libs[m.basename], m)

  def key(b):
    return (b[metric], b['rss'])

  sorted_cats = sorted(cats.items(), key=lambda kv: key(kv[1]), reverse=True)
  sorted_items = {
      c: sorted(items[c].items(), key=lambda kv: key(kv[1]), reverse=True)
      for c in cats
  }
  sorted_libs = sorted(libs.items(), key=lambda kv: key(kv[1]), reverse=True)
  stacks = sorted((m for m in mappings if m.category == CAT_STACKS),
                  key=lambda m: (getattr(m, metric), m.rss),
                  reverse=True)
  top = sorted(
      mappings, key=lambda m: (getattr(m, metric), m.rss), reverse=True)

  return {
      'totals': totals,
      'categories': sorted_cats,
      'items': sorted_items,
      'libs': sorted_libs,
      'stacks': stacks,
      'top': top,
      'elf': (elf_text, elf_start, elf_end),
      'mappings': mappings,
  }


# ---------------------------------------------------------------------------
# Reporting
# ---------------------------------------------------------------------------


def mb(kb):
  return kb / KIB


# pylint: disable=too-many-positional-arguments,too-many-arguments
def render(result, status, meminfo, source, metric, top_n, md=False):
  """Renders the report; returns a list of lines (console or markdown)."""
  t = result['totals']
  total_metric = t[metric] or 1
  lines = []
  metric_label = metric.upper()

  def h(text):
    lines.append(f'\n## {text}' if md else f'\n{text}\n' + '-' * len(text))

  def table(headers, rows, aligns=None):
    if md:
      lines.append('| ' + ' | '.join(headers) + ' |')
      lines.append('|' + '|'.join(' ---: ' if a == '>' else ' :--- '
                                  for a in (aligns or ['<'] * len(headers))) +
                   '|')
      for r in rows:
        lines.append('| ' + ' | '.join(str(c) for c in r) + ' |')
      return
    widths = [max(len(str(x)) for x in col) for col in zip(headers, *rows)
             ] if rows else [len(x) for x in headers]
    aligns = aligns or ['<'] * len(headers)
    fmt = ' | '.join(f'{{:{a}{w}}}' for a, w in zip(aligns, widths))
    lines.append(fmt.format(*headers))
    lines.append('-+-'.join('-' * w for w in widths))
    for r in rows:
      lines.append(fmt.format(*[str(c) for c in r]))

  name = status.get('Name', '?')
  pid = status.get('Pid', '?')
  title = f'smaps analysis: {name} (pid {pid})'
  lines.append(f'# {title}' if md else '=' * 100 + f'\n{title}\n' + '=' * 100)
  lines.append(f'Source: {source}   Mappings: {len(result["mappings"])}   '
               f'Ranked by: {metric_label}')

  # 1. Totals and reconciliation.
  h('1. Totals')
  rows = [
      ('VSS (virtual)', f'{mb(t["size"]):.2f}', ''),
      ('RSS', f'{mb(t["rss"]):.2f}',
       'physical pages mapped (shared pages counted fully)'),
      ('PSS', f'{mb(t["pss"]):.2f}',
       'RSS with shared pages divided among sharers'),
      ('USS (private)', f'{mb(t["private"]):.2f}',
       'pages only this process maps'),
      ('Private dirty', f'{mb(t["private_dirty"]):.2f}',
       'process-written pages (heap, stacks, CoW)'),
      ('Shared clean', f'{mb(t["shared_clean"]):.2f}',
       'shared file pages (library code, etc.)'),
      ('Anonymous', f'{mb(t["anon"]):.2f}', 'non file-backed resident pages'),
      ('Swap', f'{mb(t["swap"]):.2f}', 'swapped-out pages (zram/swapfile)'),
      ('RSS + Swap', f'{mb(t["rss"] + t["swap"]):.2f}',
       'total populated memory'),
  ]
  table(['Metric', 'MB', 'Meaning'], rows, ['<', '>', '<'])

  if status:
    vmrss = status.get('VmRSS', 0)
    rows = []
    for key, label in (('VmRSS', 'VmRSS'), ('RssAnon', 'RssAnon'),
                       ('RssFile', 'RssFile'), ('RssShmem', 'RssShmem'),
                       ('VmHWM', 'VmHWM (peak RSS)'), ('VmSwap', 'VmSwap'),
                       ('VmSize', 'VmSize'), ('Threads', 'Threads')):
      if key in status:
        v = status[key]
        rows.append((label, f'{mb(v):.2f}' if key != 'Threads' else str(v)))
    lines.append('')
    table(['/proc/<pid>/status', 'MB'], rows, ['<', '>'])
    if vmrss:
      diff = vmrss - t['rss']
      lines.append('')
      lines.append(
          f'Reconciliation: VmRSS {mb(vmrss):.2f} MB vs smaps RSS '
          f'{mb(t["rss"]):.2f} MB -> {mb(diff):+.2f} MB not visible in smaps.')
      if abs(diff) > 1024:
        lines.append(
            '  (Typically driver memory in VM_PFNMAP/VM_IO mappings, e.g. GPU '
            'buffers, which the kernel counts in RssFile but reports as 0 kB in'
            ' smaps.)')

  if meminfo:
    tot, avail = meminfo.get('MemTotal', 0), meminfo.get('MemAvailable', 0)
    st, sf = meminfo.get('SwapTotal', 0), meminfo.get('SwapFree', 0)
    lines.append('')
    lines.append(
        f'System: MemTotal {mb(tot):.0f} MB, MemAvailable {mb(avail):.0f} MB' +
        (f', swap used {mb(st - sf):.0f}/{mb(st):.0f} MB' if st else '') +
        (f'; this process = {t["rss"] * 100 / tot:.1f}% of RAM' if tot else ''))

  # 2. Category breakdown.
  h(f'2. Breakdown by category (ranked by {metric_label})')
  rows = []
  running = 0
  for i, (cat, b) in enumerate(result['categories'], 1):
    running += b[metric]
    rows.append((i, cat, f'{mb(b["rss"]):.2f}', f'{mb(b["pss"]):.2f}',
                 f'{mb(b["private_dirty"]):.2f}', f'{mb(b["swap"]):.2f}',
                 f'{b[metric] * 100 / total_metric:.1f}%', f'{mb(running):.2f}',
                 b['count']))
  rows.append(('', 'TOTAL', f'{mb(t["rss"]):.2f}', f'{mb(t["pss"]):.2f}',
               f'{mb(t["private_dirty"]):.2f}', f'{mb(t["swap"]):.2f}',
               '100.0%', f'{mb(t[metric]):.2f}', t['count']))
  table([
      '#', 'Category', 'RSS MB', 'PSS MB', 'PrivDirty', 'Swap',
      f'% {metric_label}', 'Running', 'VMAs'
  ], rows, ['>', '<', '>', '>', '>', '>', '>', '>', '>'])

  # 3. Per-category items.
  h(f'3. Largest items per category (top {top_n})')
  for cat, b in result['categories']:
    if b[metric] <= 0:
      continue
    md_prefix = '### ' if md else ''
    cat_pct = b[metric] * 100 / total_metric
    lines.append(
        f'\n{md_prefix}{cat}  —  {mb(b[metric]):.2f} MB {metric_label} '
        f'({cat_pct:.1f}%), {b["count"]} VMAs')
    its = result['items'][cat]
    rows = [(k, f'{mb(v["rss"]):.2f}', f'{mb(v["pss"]):.2f}',
             f'{mb(v["swap"]):.2f}', v['count']) for k, v in its[:top_n]]
    rest = its[top_n:]
    if rest:
      rows.append((f'... and {len(rest)} other minor entries',
                   f'{mb(sum(v["rss"] for _, v in rest)):.2f}',
                   f'{mb(sum(v["pss"] for _, v in rest)):.2f}',
                   f'{mb(sum(v["swap"] for _, v in rest)):.2f}',
                   sum(v['count'] for _, v in rest)))
    table(['Item', 'RSS MB', 'PSS MB', 'Swap', 'VMAs'], rows,
          ['<', '>', '>', '>', '>'])

  # 4. Shared libraries.
  libs = result['libs']
  h(f'4. Shared libraries ({len(libs)} distinct, top {top_n})')
  rows = [
      (n, f'{mb(v["size"]):.2f}', f'{mb(v["rss"]):.2f}', f'{mb(v["pss"]):.2f}',
       f'{mb(v["private_dirty"]):.2f}', f'{mb(v["shared_clean"]):.2f}')
      for n, v in libs[:top_n]
  ]
  rest = libs[top_n:]
  if rest:
    rows.append((f'... and {len(rest)} more',
                 f'{mb(sum(v["size"] for _, v in rest)):.2f}',
                 f'{mb(sum(v["rss"] for _, v in rest)):.2f}',
                 f'{mb(sum(v["pss"] for _, v in rest)):.2f}',
                 f'{mb(sum(v["private_dirty"] for _, v in rest)):.2f}',
                 f'{mb(sum(v["shared_clean"] for _, v in rest)):.2f}'))
  table(['Library', 'Size MB', 'RSS MB', 'PSS MB', 'PrivDirty', 'SharedClean'],
        rows, ['<', '>', '>', '>', '>', '>'])

  # 5. Thread stacks.
  stacks = result['stacks']
  if stacks:
    stack_rss = mb(sum(s.rss for s in stacks))
    h(f'5. Thread stacks ({len(stacks)} VMAs, RSS {stack_rss:.2f} MB)')
    rows = [(s.path or f'anon @ {s.addr}', s.size_kb, s.rss, s.pss, s.swap)
            for s in stacks[:top_n]]
    if len(stacks) > top_n:
      rows.append((f'... and {len(stacks) - top_n} more', '',
                   sum(s.rss for s in stacks[top_n:]), '', ''))
    table(['Stack', 'Size kB', 'RSS kB', 'PSS kB', 'Swap kB'], rows,
          ['<', '>', '>', '>', '>'])

  # 6. Top mappings.
  h(f'6. Top {top_n} individual mappings')
  rows = [(m.addr, m.perms, f'{mb(m.size_kb):.2f}', f'{mb(m.rss):.2f}',
           f'{mb(m.pss):.2f}', f'{mb(m.swap):.2f}', m.category, m.path[:60])
          for m in result['top'][:top_n]]
  table(['Address', 'Perm', 'Size', 'RSS', 'PSS', 'Swap', 'Category', 'Path'],
        rows, ['<', '<', '>', '>', '>', '>', '<', '<'])

  elf_text, elf_start, elf_end = result['elf']
  if elf_text is not None:
    elf_mb = (elf_end - elf_start) // MIB
    lines.append('')
    lines.append(
        f'Note: anonymous ELF image detected at {elf_start:x}-{elf_end:x} '
        f'({elf_mb} MB virtual, text VMA {elf_text.addr}); '
        'treated as the Cobalt engine.')
  return lines


def to_json(result, status, meminfo, metric):

  def b2mb(b):
    return {k: (round(v / KIB, 3) if k != 'count' else v) for k, v in b.items()}

  return {
      'metric': metric,
      'status': status,
      'meminfo': {
          k: v
          for k, v in meminfo.items()
          if k in ('MemTotal', 'MemFree', 'MemAvailable', 'SwapTotal',
                   'SwapFree')
      },
      'totals_mb': b2mb(result['totals']),
      'categories_mb': {
          c: b2mb(b) for c, b in result['categories']
      },
      'items_mb': {
          c: {
              k: b2mb(v) for k, v in its
          } for c, its in result['items'].items()
      },
      'libraries_mb': {
          n: b2mb(v) for n, v in result['libs']
      },
  }


# ---------------------------------------------------------------------------


def resolve_inputs(target, status_arg, meminfo_arg):
  """Accepts a snapshot directory or a smaps file; finds sibling files."""
  if os.path.isdir(target):
    base = target
    smaps = None
    for cand in ('smaps.txt', 'smaps'):
      if os.path.exists(os.path.join(base, cand)):
        smaps = os.path.join(base, cand)
        break
    if smaps is None:
      found = sorted(
          f for f in os.listdir(base)
          if f.startswith('smaps') and 'rollup' not in f)
      if not found:
        sys.exit(f'No smaps file found in {base}')
      smaps = os.path.join(base, found[-1])
  else:
    smaps = target
    base = os.path.dirname(os.path.abspath(target))

  def sibling(explicit, prefixes):
    if explicit:
      return explicit
    for f in sorted(os.listdir(base)):
      if f.startswith(prefixes) and f.endswith('.txt'):
        return os.path.join(base, f)
    return None

  return smaps, sibling(status_arg,
                        ('status',)), sibling(meminfo_arg, ('meminfo',))


def main():
  parser = argparse.ArgumentParser(
      description='Analyze a /proc/<pid>/smaps snapshot.')
  parser.add_argument(
      'target',
      help='Snapshot directory (from capture_smaps.py) or an smaps file.')
  parser.add_argument(
      '--status', help='/proc/<pid>/status file (auto-detected next to smaps).')
  parser.add_argument(
      '--meminfo', help='/proc/meminfo file (auto-detected next to smaps).')
  parser.add_argument(
      '--metric',
      choices=('rss', 'pss'),
      default='rss',
      help='Metric used for ranking and percentages (default: rss).')
  parser.add_argument(
      '--top', type=int, default=10, help='Rows per table (default: 10).')
  parser.add_argument(
      '--md', metavar='FILE', help='Also write a Markdown report to FILE.')
  parser.add_argument(
      '--json',
      action='store_true',
      help='Print JSON instead of the text report.')
  args = parser.parse_args()

  smaps, status_file, meminfo_file = resolve_inputs(args.target, args.status,
                                                    args.meminfo)
  if not os.path.exists(smaps):
    sys.exit(f'smaps file not found: {smaps}')

  mappings = parse_smaps(smaps)
  if not mappings:
    sys.exit(f'No mappings parsed from {smaps}')
  status = parse_kv_kb(status_file)
  meminfo = parse_kv_kb(meminfo_file)
  result = analyze(mappings, args.metric)

  if args.json:
    print(json.dumps(to_json(result, status, meminfo, args.metric), indent=2))
  else:
    print('\n'.join(
        render(result, status, meminfo, smaps, args.metric, args.top)))

  if args.md:
    with open(args.md, 'w', encoding='utf-8') as f:
      f.write('\n'.join(
          render(
              result, status, meminfo, smaps, args.metric, args.top, md=True)) +
              '\n')
    print(f'\nMarkdown report written to {args.md}')


if __name__ == '__main__':
  main()
