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
"""Summarizes the memory-related UMA histograms from a JSON histogram dump.

Reads a JSON file such as the one produced by CDP's ``Browser.getHistograms``
(a bare list of histograms, or ``{"histograms": [...]}``, or
``{"result": {"histograms": [...]}}``), keeps only the memory histograms, and
prints a table with the mean and median of each one. Memory sizes are
normalized to MB; percentages, counts and timings are shown as recorded with
their unit.

The unit of each histogram is derived from how it is emitted (see
cobalt/browser/metrics/cobalt_memory_metrics_emitter.cc and Chromium's
chrome/browser/metrics/process_memory_metrics_emitter.cc):

  Memory.Experimental.<Proc>2.<Metric>        kLarge  -> MiB
  Memory.Experimental.<Proc>2.Small.<Metric>  kSmall  -> KiB
  Memory.Experimental.<Proc>2.Tiny.<Metric>   kTiny   -> bytes / object counts
  Memory.<Proc>.{ResidentSet,...Rss,...Pss}   os dump -> MiB
  Memory.Total.*                                      -> MiB

Usage:
  python3 analyze_memory_umas.py histograms.json
  python3 analyze_memory_umas.py histograms.json --filter Browser2
  python3 analyze_memory_umas.py histograms.json --sizes-only
"""

import argparse
import json
import re
import sys

RESIDENT_SET = 'Memory.Browser.ResidentSet'

# Histogram name prefixes that are considered memory-related.
MEMORY_PREFIXES = (
    'Memory.',
    'MemoryAndroid.',
    'HeapProfiling.',
    'V8.MemoryHeapSample',
    'V8.MemoryExternalFragmentation',
    'V8.GC.Cycle.Memory.',
    'V8.GC.Cycle.Objects.',
    'V8.GC.Cycle.UserBlocking.Memory.',
    'V8.GC.Cycle.UserBlocking.Objects.',
)

# Unit names. Size units carry a factor converting the recorded value to MB.
MIB, KIB, KB, BYTES = 'MiB', 'KiB', 'KB', 'B'
PCT, COUNT, US, MS, MBPS, BOOL = '%', 'count', 'us', 'ms', 'MB/s', 'bool'
SIZE_FACTORS = {
    MIB: 1.0,
    KIB: 1.0 / 1024,
    KB: 1000.0 / (1024 * 1024),
    BYTES: 1.0 / (1024 * 1024),
}

# Exact-name units for histograms whose names do not follow the emitter
# scheme. Verified against the recording call sites.
EXACT_UNITS = {
    # components/discardable_memory
    'Memory.DiscardableAllocationSize': KIB,
    # blink parkable_image*.cc
    'Memory.ParkableImage.OnDiskSize.5min': KIB,
    'Memory.ParkableImage.TotalSize.5min': KIB,
    'Memory.ParkableImage.UnparkedSize.5min': KIB,
    'Memory.ParkableImage.Write.Size': KIB,
    'Memory.ParkableImage.Read.Latency': US,
    'Memory.ParkableImage.Write.Latency': US,
    'Memory.ParkableImage.Read.Throughput': MBPS,
    'Memory.ParkableImage.TotalReadTime.5min': MS,
    'Memory.ParkableImage.TotalWriteTime.5min': MS,
    # blink parkable_string*.cc
    'Memory.ParkableString.OnDiskSizeKb.5min': KB,
    'Memory.ParkableString.Compression.SizeKb': KB,
    'Memory.ParkableString.DiskReadTime.5min': MS,
    'Memory.ParkableString.DiskWriteTime.5min': MS,
    'Memory.ParkableString.TotalParkingThreadTime.5min': MS,
    'Memory.ParkableString.TotalUnparkingTime.5min': MS,
    'Memory.ParkableString.Read.Latency': US,
    'Memory.ParkableString.Compression.Latency': US,
    'Memory.ParkableString.Decompression.Latency': US,
    'Memory.ParkableString.Decompression.ThroughputMBps': MBPS,
    # base/allocator/partition_alloc_support.cc
    'Memory.PartitionAlloc.MemoryReclaim': US,
    'Memory.PartitionAlloc.PeriodicPurge': US,
    'Memory.PartitionAlloc.PartitionRoot.ExtrasSize': COUNT,
    # cobalt_memory_metrics_emitter.cc
    'Memory.Media.AllocatedEncodedBuffer': MIB,
    'Memory.Experimental.VirtualAddress.LargestFreeGapMb': MIB,
    'Memory.Experimental.VirtualAddress.TotalUnmappedVaMb': MIB,
    'Memory.Experimental.VirtualAddress.FragmentationRatio': PCT,
    'Memory.Experimental.VirtualAddress.VmaCount': COUNT,
    # Misc.
    'Memory.PressureLevel2': COUNT,
    'Memory.ProcessCount2': COUNT,
    'Memory.RendererProcessCount2': COUNT,
    'MemoryAndroid.LowRamDevice': BOOL,
    'HeapProfiling.InProcess.Enabled': BOOL,
}

# Regex rules, tried in order after EXACT_UNITS. The first match wins.
RULES = (
    # Memory.Experimental.<Proc>2.Small.<X> / .Tiny.<X> (kSmall / kTiny).
    (re.compile(r'^Memory\.Experimental\.\w+2\.Small\.'), KIB),
    (re.compile(r'^Memory\.Experimental\.\w+2\.Tiny\..*Count'), COUNT),
    (re.compile(r'^Memory\.Experimental\.\w+2\.Tiny\.NumberOf'), COUNT),
    (re.compile(r'^Memory\.Experimental\.\w+2\.Tiny\.'), BYTES),
    # Percentages and counts that live in the kLarge namespace.
    (re.compile(r'\.Fragmentation(\.\w+)?(\.After\d+H)?$'), PCT),
    (re.compile(r'FragmentationRatio$'), PCT),
    (re.compile(r'V8\.MemoryExternalFragmentation'), PCT),
    (re.compile(r'(\.MappingsCount|\.Count|\.ObjectCount|PerMinute)$'), COUNT),
    (re.compile(r'\.Tiny\.'), BYTES),
    (re.compile(r'\.Small\.'), KIB),
    # Memory.Experimental.<Proc>2.<X> (kLarge) and the per-process os-dump
    # metrics (ResidentSet, PrivateMemoryFootprint, *Rss, *Pss, ...).
    (re.compile(r'^Memory\.Experimental\.\w+2\.'), MIB),
    (re.compile(r'^Memory\.(Browser|Renderer|Gpu|GPU|Utility|NetworkService|'
                r'Extension|AudioService|CdmService|MediaFoundationService|'
                r'PaintPreviewCompositor)\.'), MIB),
    (re.compile(r'^Memory\.Total\.'), MIB),
    # Android background compaction / pre-freeze deltas
    # (base/android/pre_freeze_background_memory_trimmer.cc, MiB).
    (re.compile(r'^Memory\.(PreFreeze2|SelfCompact2|RunningCompact)\.'
                r'(\w+\.)?(Pss|PssAnon|PssFile|Rss|SwapPss|'
                r'PrivateMemoryFootprint|Vulkan)\.(After|Before|Diff)'), MIB),
    # Renderer peak footprint (content/browser/renderer_host, MiB).
    (re.compile(r'^Memory\.Experimental\.\w+\.(HighestPrivateMemoryFootprint|'
                r'PeakResidentSet)'), MIB),
    # Generic unit-bearing suffixes (Memory.System.MemAvailableMB, ...).
    (re.compile(r'(MB|MiB)\d?$'), MIB),
    (re.compile(r'(Percent|Percentage\w*)\d?$'), PCT),
    (re.compile(r'(Latency|Duration|Time)\d?$'), MS),
    # GPU peak-memory histograms (components/viz peak_gpu_memory_callback.cc).
    (re.compile(r'^Memory\.GPU\.(PeakMemoryUsage2|PeakMemoryAllocationSource2|'
                r'TileMemory|DecodedImages)'), MIB),
    # HeapProfiling (components/heap_profiling/in_process).
    (re.compile(r'^HeapProfiling\.InProcess\.TotalSampledMemory'), MIB),
    (re.compile(r'^HeapProfiling\.InProcess\.SampledAddressCacheHitRate'), PCT),
    (re.compile(r'^HeapProfiling\.InProcess\.'), COUNT),
    # V8 (v8/src/heap/heap.cc, blink v8_metrics.cc). Both record KB (1024).
    (re.compile(r'^V8\.MemoryHeapSample'), KIB),
    (re.compile(r'^V8\.GC\.Cycle\.(UserBlocking\.)?(Memory|Objects)\.'), KIB),
    (re.compile(r'^MemoryAndroid\.EvictedTreeSize'), KIB),
)


def unit_of(name):
  """Returns the unit a histogram records in, or None if unknown."""
  if name in EXACT_UNITS:
    return EXACT_UNITS[name]
  for pattern, unit in RULES:
    if pattern.search(name):
      return unit
  return None


def load_histograms(path):
  """Loads the histogram list from a JSON dump."""
  with open(path, 'r', encoding='utf-8') as f:
    data = json.load(f)
  if isinstance(data, dict):
    data = data.get('result', data).get('histograms', [])
  if not isinstance(data, list):
    raise ValueError('Input JSON does not contain a list of histograms.')
  return data


def median(histogram):
  """Returns the upper bound of the bucket containing the median sample."""
  total = histogram.get('count', 0)
  cumulative = 0
  for bucket in sorted(
      histogram.get('buckets', []), key=lambda b: b.get('low', 0)):
    cumulative += bucket.get('count', 0)
    if cumulative * 2 >= total:
      return bucket.get('high')
  return None


def peak(histogram):
  """Returns the upper bound of the highest non-empty bucket."""
  highs = [
      b.get('high')
      for b in histogram.get('buckets', [])
      if b.get('count', 0) > 0 and b.get('high') is not None
  ]
  return max(highs) if highs else None


def summarize(histograms, name_filter, sizes_only):
  """Returns rows (name, count, mean, median, peak, unit_label, is_size).

  Sizes are converted to MB; other units are left as recorded. Rows are
  sorted with sizes first (largest mean first), then everything else.
  """
  rows = []
  for h in histograms:
    name = h.get('name', '')
    count = h.get('count', 0)
    if not name.startswith(MEMORY_PREFIXES) or count <= 0:
      continue
    if name_filter and name_filter not in name:
      continue
    unit = unit_of(name)
    is_size = unit in SIZE_FACTORS
    if sizes_only and not is_size:
      continue
    factor = SIZE_FACTORS[unit] if is_size else 1.0
    mean = h.get('sum', 0) / count * factor
    med = median(h)
    med = med * factor if med is not None else None
    pk = peak(h)
    pk = pk * factor if pk is not None else None
    label = 'MB' if is_size else (unit or '?')
    rows.append((name, count, mean, med, pk, label, is_size))
  rows.sort(key=lambda r: (not r[6], -r[2]))
  return rows


def _fmt(value):
  return f'{value:.2f}' if value is not None else 'N/A'


def print_report(rows):
  """Prints the summary table."""
  name_w = max(len('Histogram'), max(len(r[0]) for r in rows))
  width = name_w + 69
  rs_mean = next((r[2] for r in rows if r[0] == RESIDENT_SET), None)

  title = 'Memory UMA Summary (sizes normalized to MB)'
  print(f'{title:^{width}}')
  print('=' * width)
  header = (f"{'Histogram':<{name_w}} | {'Count':>6} | {'Mean':>10} | "
            f"{'Median':>10} | {'Peak':>10} | {'Unit':>5} | {'% of RS':>8}")
  print(header)
  print('-' * width)
  for name, count, mean, med, pk, label, is_size in rows:
    pct = f'{mean / rs_mean * 100:.2f}%' if rs_mean and is_size else '-'
    print(f'{name:<{name_w}} | {count:>6} | {mean:>10.2f} | {_fmt(med):>10} | '
          f'{_fmt(pk):>10} | {label:>5} | {pct:>8}')
  print('-' * width)
  print('Median/Peak are bucket upper bounds; % of RS uses the mean.')
  if rs_mean is None:
    print(f'Note: {RESIDENT_SET} not present; "% of RS" unavailable.')
  if any(r[5] == '?' for r in rows):
    print('Unit "?" = unknown; value shown as recorded.')


def main():
  parser = argparse.ArgumentParser(
      description='Summarize memory-related UMA histograms from a JSON dump.')
  parser.add_argument(
      'input_file', help='JSON histogram dump (e.g. histograms.json).')
  parser.add_argument(
      '--filter',
      default='',
      help='Only include histograms whose name contains this substring.')
  parser.add_argument(
      '--sizes-only',
      action='store_true',
      help='Only show histograms that record memory sizes (hide counts, '
      'percentages and timings).')
  args = parser.parse_args()

  try:
    histograms = load_histograms(args.input_file)
  except (OSError, ValueError, json.JSONDecodeError) as e:
    print(f'Error reading {args.input_file}: {e}', file=sys.stderr)
    sys.exit(1)

  rows = summarize(histograms, args.filter, args.sizes_only)
  if not rows:
    print('No memory histograms with data found.')
    return
  print_report(rows)


if __name__ == '__main__':
  main()
