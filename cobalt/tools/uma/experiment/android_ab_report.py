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
"""Builds the [IMPROVED]/[REGRESSED]/[NOISE] table from android_ab_harness.py.

Everything is recomputed from the raw dumps listed in manifest.jsonl:
  * Basic YouTube TV Home Cold-Start + Exit CUJ (homefeed / wpr): in-session
    cumulative snapshot S1, plus lifecycle exit flush (S2 - S1) when .s2.txt is
    present.

For every histogram statistic (count, sum and mean; the value of a
one-sample histogram; total count and active per-bucket counts of an enum
histogram; only the mean of a periodic gauge such as Memory.* or
Browser.MainThreadsCongestion*) the report computes the paired difference
B - A (Treatment - Control) for every ABBA pair, the median difference, a 95%
percentile-bootstrap CI of that median, and the sign counts with an exact
two-sided sign-test p-value. With No-Op (A/A) pairs from the same session, it
also bootstraps median(A/B diffs) - median(No-Op diffs).

Pure Python 3 (no third-party packages):
  python3 cobalt/tools/uma/experiment/android_ab_report.py \\
      --results-dir out/ab/ls --experiment ls_home65 \\
      --out out/ab/ls/ls_home65_report.md
"""

import argparse
import json
import math
import os
import random
import statistics
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
UMA_DIR = os.path.dirname(SCRIPT_DIR)
if SCRIPT_DIR not in sys.path:
  sys.path.insert(0, SCRIPT_DIR)

import android_uma_dump as uma_dump  # pylint: disable=wrong-import-position

DEFAULT_HISTOGRAM_FILE = os.path.join(UMA_DIR, 'storage_histograms.txt')

# Enum histograms: sample count (plus active named buckets) is reported.
ENUM_HISTOGRAMS = (
    'HttpCache.Pattern',
    'V8.CompileScript.CacheBehaviour',
    'LevelDBEnv.SyncResult',
    'SessionStorageContext.CommitResult',
    'LocalStorage.DatabaseOpen',
    'Cobalt.Startup.MilestoneReached',
    'Cobalt.StorageMigration.Outcome',
    'Cobalt.StorageMigration.ReadResult',
    'Cobalt.StorageMigration.CookieInjectionResult',
    'Cobalt.StorageMigration.LocalStorageInjectionResult',
    'Cobalt.StorageMigration.SentinelWriteResult',
    'Cookie.LoadProblem',
    'Cookie.CommitProblem',
    'Cobalt.LocalStorage.DatabaseOpenError',
    'Cobalt.LocalStorage.DatabaseReadError',
    'Cobalt.LocalStorage.DatabaseCommitError',
    'Cobalt.LocalStorage.DatabaseVersionMismatch',
    'Cobalt.LocalStorage.DatabaseResetReason',
    'Cobalt.Storage.Cookie.FlushAction',
)
# Histograms where each sample is a commit, sync or error: fewer is better.
LOWER_COUNT_IS_BETTER = (
    'Cobalt.Storage.Cookie.CommitDuration',
    'Cobalt.Storage.Cookie.PendingOperationsAtCommit',
    'Cobalt.Storage.LocalStorage.CommitDuration',
    'DOMStorage.CommitSizeBytesAggregated',
    'LevelDBEnv.SyncResult',
    'SessionStorageContext.CommitResult',
    'Cookie.LoadProblem',
    'Cookie.CommitProblem',
    'Cobalt.LocalStorage.DatabaseOpenError',
    'Cobalt.LocalStorage.DatabaseReadError',
    'Cobalt.LocalStorage.DatabaseCommitError',
    'Cobalt.LocalStorage.DatabaseVersionMismatch',
    'Cobalt.LocalStorage.DatabaseResetReason',
)
# Histograms where a higher value/mean/sum is an intentional improvement
# (raising the Code Cache cap to prevent cold-start eviction, or deferring V8
# bytecode writes further off the startup critical path).
HIGHER_VALUE_IS_BETTER = (
    'Cobalt.Storage.CodeCache.ConfiguredMaxSizeKB',
    'SimpleCache.Code.CacheSizeOnInit',
    'Cobalt.V8CodeCache.DeferredWriteDelayMs',
)
# Sums/means without a better/worse direction: batch sizes, commit delays
# (commit batching raises delays on purpose), and cache sizes.
NEUTRAL_VALUE = (
    'Cobalt.Storage.Cookie.PendingOperationsAtCommit',
    'DOMStorage.CommitMeasuredDelay',
    'LevelDBWrapper.CommitDelay',
    'SimpleCache.Http.CacheSizeOnInit',
    'Cobalt.Storage.DirCache.StartupTotalSizeKB',
    'Cobalt.Storage.GpuShaderCache.ConfiguredMaxSizeKB',
    'Cobalt.StorageMigration.',
)
# Periodic gauges: one sample per timer tick, so only the mean is meaningful.
SAMPLED_PREFIXES = ('Memory.', 'Browser.MainThreadsCongestion',
                    'Cobalt.Storage.ProcessIo.WriteRateKBPerMin')
# Below MIN_PAIRS a difference is never significant; below SEPARATION_PAIRS
# the two arms' value ranges must also not overlap.
MIN_PAIRS = 3
SEPARATION_PAIRS = 5

PHASE_TITLES = {
    'homefeed': 'In-Session Cold Start to Home (S1)',
    'exit_flush': 'Lifecycle Exit Flush (S2 - S1)',
}

# Enum histograms whose per-bucket sample counts are reported alongside total
# count whenever a bucket has non-zero samples in any run.
ENUM_BUCKET_NAMES = {
    'V8.CompileScript.CacheBehaviour': {
        0: 'ProduceCodeCache',
        1: 'HitIsolateCacheWhenNoCache',
        2: 'ConsumeCodeCache',
        3: 'ConsumeCodeCacheFailed',
        4: 'NoCacheBecauseInlineScript',
        5: 'NoCacheBecauseScriptTooSmall',
        6: 'NoCacheBecauseCacheTooCold',
        7: 'NoCacheNoReason',
        8: 'NoCacheBecauseNoResource',
        9: 'NoCacheBecauseInspector',
        10: 'NoCacheBecauseCachingDisabled',
        11: 'NoCacheBecauseModule',
        12: 'NoCacheBecauseStreamingSource',
        13: 'NoCacheBecauseV8Extension',
        14: 'HitIsolateCacheWhenProduceCodeCache',
        15: 'HitIsolateCacheWhenConsumeCodeCache',
        16: 'NoCacheBecauseExtensionModule',
        17: 'NoCacheBecausePacScript',
        18: 'NoCacheBecauseInDocumentWrite',
        19: 'NoCacheBecauseResourceWithNoCacheHandler',
        20: 'HitIsolateCacheWhenStreamingSource',
        21: 'NoCacheBecauseStaticCodeCache',
    },
    'HttpCache.Pattern': {
        0: 'ENTRY_UNDEFINED',
        1: 'ENTRY_OTHER',
        2: 'ENTRY_NOT_IN_CACHE',
        3: 'ENTRY_USED',
        4: 'ENTRY_VALIDATED',
        5: 'ENTRY_UPDATED',
        6: 'ENTRY_CANT_CONDITIONALIZE',
    },
    'Cobalt.LocalStorage.DatabaseResetReason': {
        0: 'OpenFailure',
        1: 'CommitFailure',
        2: 'VersionMismatch',
        3: 'ExplicitPurge',
        4: 'MissingCurrentFile',
    },
    'Cobalt.Storage.Cookie.FlushAction': {
        0: 'Executed',
        1: 'CoalescedInFlight',
        2: 'SkippedRecentFlush',
    },
}
ENUM_BUCKET_POLARITY = {
    ('V8.CompileScript.CacheBehaviour', 'ConsumeCodeCache'):
        1,
    ('V8.CompileScript.CacheBehaviour', 'HitIsolateCacheWhenNoCache'):
        1,
    ('V8.CompileScript.CacheBehaviour', 'HitIsolateCacheWhenProduceCodeCache'):
        1,
    ('V8.CompileScript.CacheBehaviour', 'HitIsolateCacheWhenConsumeCodeCache'):
        1,
    ('V8.CompileScript.CacheBehaviour', 'HitIsolateCacheWhenStreamingSource'):
        1,
    ('V8.CompileScript.CacheBehaviour', 'ProduceCodeCache'):
        -1,
    ('V8.CompileScript.CacheBehaviour', 'ConsumeCodeCacheFailed'):
        -1,
    ('Cobalt.Storage.Cookie.FlushAction', 'Executed'):
        -1,
    ('Cobalt.Storage.Cookie.FlushAction', 'CoalescedInFlight'):
        1,
    ('Cobalt.Storage.Cookie.FlushAction', 'SkippedRecentFlush'):
        1,
}
_BUCKET_STAT_TO_LOW = {
    label: low for mapping in ENUM_BUCKET_NAMES.values()
    for low, label in mapping.items()
}


def _matches(name: str, patterns) -> bool:
  for p in patterns:
    if p.endswith('.'):
      if name.startswith(p):
        return True
    elif name == p or name.startswith(p + '.'):
      return True
  return False


def is_enum(name: str) -> bool:
  return _matches(name, ENUM_HISTOGRAMS)


def is_sampled(name: str) -> bool:
  return name.startswith(SAMPLED_PREFIXES)


def sign_test_p(diffs: list) -> float:
  """Exact two-sided sign-test p-value (ties dropped)."""
  negative = sum(d < 0 for d in diffs)
  positive = sum(d > 0 for d in diffs)
  n = negative + positive
  if not n:
    return 1.0
  tail = sum(math.comb(n, i) for i in range(min(negative, positive) + 1))
  return min(1.0, 2 * tail / 2**n)


def bucket_low_for_stat(stat: str, name: str = ''):
  """Returns the integer bucket low value if stat is an enum bucket, or None."""
  if name in ENUM_BUCKET_NAMES:
    for low, label in ENUM_BUCKET_NAMES[name].items():
      if label == stat:
        return low
  if stat in _BUCKET_STAT_TO_LOW:
    return _BUCKET_STAT_TO_LOW[stat]
  if stat.startswith('bucket_'):
    suffix = stat.split('_', 1)[1]
    if suffix.isdigit():
      return int(suffix)
  return None


def polarity(name: str, stat: str) -> int:
  """-1 if lower is better, +1 if higher is better, 0 if neither."""
  if stat == 'count':
    return -1 if _matches(name, LOWER_COUNT_IS_BETTER) else 0
  if is_enum(name):
    if (name, stat) in ENUM_BUCKET_POLARITY:
      return ENUM_BUCKET_POLARITY[(name, stat)]
    if name == 'Cobalt.LocalStorage.DatabaseResetReason':
      return -1
    return 0
  if name == 'DOMStorage.CommitSizeBytesAggregated':
    return -1 if stat == 'sum' else 0
  if _matches(name, HIGHER_VALUE_IS_BETTER):
    return 1
  if _matches(name, NEUTRAL_VALUE):
    return 0
  return -1


def stat_value(histogram, stat: str):
  """Returns one statistic of a (possibly missing) histogram, or None."""
  count = histogram.get('count', 0) if histogram else 0
  total = histogram.get('sum', 0) if histogram else 0
  if stat == 'count':
    return count
  if stat == 'sum':
    return total
  if stat not in ('mean', 'value'):
    low = bucket_low_for_stat(stat,
                              histogram.get('name', '') if histogram else '')
    if low is not None:
      if not histogram:
        return 0
      for b in histogram.get('buckets', []):
        b_low = b.get('low', 0)
        b_high = b.get('high', b_low + 1)
        if b_low <= low < b_high:
          return b.get('count', 0)
      return 0
  if count <= 0:
    return None
  return total / count


class Bootstrap:
  """Percentile bootstrap of the median with fixed, reusable resamples."""

  def __init__(self, iterations: int, seed: int):
    self.iterations = iterations
    self.seed = seed
    self._indices = {}
    self._cache = {}

  def _rows(self, n: int):
    if n not in self._indices:
      rng = random.Random(self.seed * 1009 + n)
      self._indices[n] = [
          [rng.randrange(n) for _ in range(n)] for _ in range(self.iterations)
      ]
    return self._indices[n]

  def medians(self, values: list) -> list:
    key = tuple(values)
    if key not in self._cache:
      self._cache[key] = [
          statistics.median([values[i]
                             for i in row])
          for row in self._rows(len(values))
      ]
    return self._cache[key]

  def median_ci(self, values: list, alpha: float = 0.05):
    return percentile_interval(self.medians(values), alpha)

  def difference_ci(self, values: list, baseline: list, alpha: float = 0.05):
    """CI of median(values) - median(baseline), resampled independently."""
    a = self.medians(values)
    b = list(self.medians(baseline))
    random.Random(self.seed + 7).shuffle(b)
    return percentile_interval([x - y for x, y in zip(a, b)], alpha)


def percentile_interval(samples: list, alpha: float = 0.05):
  ordered = sorted(samples)
  last = len(ordered) - 1
  return (ordered[int(math.floor(alpha / 2 * last))],
          ordered[int(math.ceil((1 - alpha / 2) * last))])


def load_experiment(results_dir: str, experiment: str):
  """Returns (runs {(group, pair, arm): record}, pairs {(group, pair): rec})."""
  runs, pairs, prewarm, attempts = {}, {}, [], []
  with open(os.path.join(results_dir, 'manifest.jsonl'), encoding='utf-8') as f:
    for line in f:
      record = json.loads(line)
      if record.get('experiment') != experiment:
        continue
      kind = record.get('type')
      if kind == 'run':
        attempts.append(record)
        if record.get('valid'):
          runs[(record['group'], record['pair'], record['arm'])] = record
      elif kind == 'pair':
        pairs[(record['group'], record['pair'])] = record
      elif kind == 'prewarm':
        prewarm.append(record)
  return runs, pairs, attempts, prewarm


def run_phases(results_dir: str, record: dict) -> dict:
  """Returns {phase: {histogram name: histogram}} for one run."""
  dump = os.path.join(results_dir, record['dump'])
  root, ext = os.path.splitext(dump)
  s1 = uma_dump.parse_dump(dump)[0]
  phases = {'homefeed': s1}
  s1_path = f'{root}.s1{ext}'
  s2_path = f'{root}.s2{ext}'
  if os.path.exists(s2_path):
    base_s1 = uma_dump.parse_dump(s1_path)[0] if os.path.exists(s1_path) else s1
    s2 = uma_dump.parse_dump(s2_path)[0]
    phases['exit_flush'] = uma_dump.snapshot_delta(base_s1, s2)
  return phases


def build_pairs(results_dir: str, runs: dict, pair_records: dict,
                group: str) -> list:
  pairs = []
  numbers = sorted({p for (g, p, _) in runs if g == group})
  for number in numbers:
    a, b = runs.get((group, number, 'A')), runs.get((group, number, 'B'))
    if not a or not b:
      continue
    meta = pair_records.get((group, number), {})
    pairs.append({
        'pair': number,
        'order': meta.get('order', ''),
        'A': a,
        'B': b,
        'A_phases': run_phases(results_dir, a),
        'B_phases': run_phases(results_dir, b),
    })
  return pairs


def histogram_order(histogram_file: str):
  queries = []
  if histogram_file and os.path.exists(histogram_file):
    with open(histogram_file, encoding='utf-8') as f:
      queries = [
          line.strip()
          for line in f
          if line.strip() and not line.startswith('#')
      ]

  def key(name):
    for i, query in enumerate(queries):
      if query in name:
        return (i, name)
    return (len(queries), name)

  return key


def paired_values(pairs: list, phase: str, name: str, stat: str):
  a_values, b_values, diffs = [], [], []
  for pair in pairs:
    va = stat_value(pair['A_phases'].get(phase, {}).get(name), stat)
    vb = stat_value(pair['B_phases'].get(phase, {}).get(name), stat)
    if va is None or vb is None:
      continue
    a_values.append(va)
    b_values.append(vb)
    diffs.append(vb - va)
  return a_values, b_values, diffs


def _ranges_separated(row: dict) -> bool:
  """Whether every B value lies beyond every A value, in the Δ direction."""
  if row['median'] < 0:
    return max(row['b_values']) < min(row['a_values'])
  return min(row['b_values']) > max(row['a_values'])


def tag_row(row: dict, min_rel_effect: float):
  diffs = row['diffs']
  if not diffs:
    return 'N/A', 'no paired data'
  if all(d == 0 for d in diffs):
    return '[UNCHANGED]', ''
  if len(diffs) < MIN_PAIRS:
    return '[NOISE]', f'n < {MIN_PAIRS}'
  low, high = row['ci']
  if low <= 0 <= high:
    return '[NOISE]', 'CI includes 0'
  if row['adj_ci'] is not None and row['adj_ci'][0] <= 0 <= row['adj_ci'][1]:
    return '[NOISE]', 'within No-Op spread'
  if len(diffs) < SEPARATION_PAIRS and not _ranges_separated(row):
    return '[NOISE]', f'arm ranges overlap (n < {SEPARATION_PAIRS})'
  if row['rel'] is not None and abs(row['rel']) < min_rel_effect:
    return '[NOISE]', f'effect < {min_rel_effect:.0%}'
  if row['polarity'] == 0:
    return '[CHANGED]', 'no better/worse direction'
  improved = (row['median'] < 0) == (row['polarity'] < 0)
  return ('[IMPROVED]' if improved else '[REGRESSED]'), ''


SIGNIFICANT_TAGS = ('[IMPROVED]', '[REGRESSED]', '[CHANGED]')
FDR_Q = 0.05


def parse_primary(spec: str) -> list:
  """'Hist' or 'Hist:stat' items, comma-separated -> [(name, stat or None)]."""
  out = []
  for item in (spec or '').split(','):
    name, _, stat = item.strip().partition(':')
    if name.strip():
      out.append((name.strip(), stat.strip() or None))
  return out


def is_primary(row: dict, primary: list) -> bool:
  return any(row['name'] == name and stat in (None, row['stat'])
             for name, stat in primary)


def bh_qvalues(p_values: list) -> list:
  """Benjamini-Hochberg adjusted p-values, in the order of p_values."""
  m = len(p_values)
  order = sorted(range(m), key=lambda i: p_values[i])
  q_values = [1.0] * m
  running = 1.0
  for rank in range(m, 0, -1):
    i = order[rank - 1]
    running = min(running, p_values[i] * m / rank)
    q_values[i] = running
  return q_values


def apply_multiplicity(rows: list, primary: list):
  """Marks primary rows and downgrades uncorrected diagnostic findings."""
  for row in rows:
    row['primary'] = is_primary(row, primary)
    row['q'] = None
  if not primary:
    return
  tested = [
      r for r in rows
      if not r['primary'] and r['diffs'] and r['tag'] not in ('[UNCHANGED]',
                                                              'N/A')
  ]
  for row, q in zip(tested, bh_qvalues([r['sign_p'] for r in tested])):
    row['q'] = q
    if row['tag'] in SIGNIFICANT_TAGS and q >= FDR_Q:
      old_tag = row['tag']
      row['tag_reason'] = f'diagnostic, BH q={q:.2g}; uncorrected {old_tag}'
      row['tag'] = '[FLAG]'


def _enum_bucket_stats(pairs: list, phase: str, name: str) -> list:
  """Returns ordered bucket stat names with count > 0 for an enum histogram."""
  if name not in ENUM_BUCKET_NAMES:
    return []
  active_lows = set()
  for pair in pairs:
    for side in ('A_phases', 'B_phases'):
      h = pair[side].get(phase, {}).get(name)
      if not h:
        continue
      for b in h.get('buckets', []):
        if b.get('count', 0) > 0:
          active_lows.add(b.get('low', 0))
  mapping = ENUM_BUCKET_NAMES[name]
  return [mapping.get(low, f'bucket_{low}') for low in sorted(active_lows)]


def build_rows(ab_pairs: list, noop_pairs: list, phase: str, *, boot: Bootstrap,
               order_key, min_rel_effect: float) -> list:
  names = set()
  max_count = {}
  for pair in ab_pairs + noop_pairs:
    for side in ('A_phases', 'B_phases'):
      for name, h in pair[side].get(phase, {}).items():
        names.add(name)
        max_count[name] = max(max_count.get(name, 0), h.get('count', 0))
  rows = []
  for name in sorted(names, key=order_key):
    if is_enum(name):
      stats = ['count'] + _enum_bucket_stats(ab_pairs + noop_pairs, phase, name)
    elif max_count.get(name, 0) <= 1:
      stats = ['value']
    elif is_sampled(name):
      stats = ['mean']
    else:
      stats = ['count', 'sum', 'mean']
    for stat in stats:
      a_values, b_values, diffs = paired_values(ab_pairs, phase, name, stat)
      noop_a, noop_b, noop_diffs = paired_values(noop_pairs, phase, name, stat)
      row = {
          'phase': phase,
          'name': name,
          'stat': stat,
          'n': len(diffs),
          'a_values': a_values,
          'b_values': b_values,
          'diffs': diffs,
          'polarity': polarity(name, stat),
          'median': statistics.median(diffs) if diffs else None,
          'ci': boot.median_ci(diffs) if diffs else None,
          'sign_p': sign_test_p(diffs),
          'noop_n': len(noop_diffs),
          'noop_a_values': noop_a,
          'noop_b_values': noop_b,
          'noop_diffs': noop_diffs,
          'noop_median': statistics.median(noop_diffs) if noop_diffs else None,
          'noop_ci': boot.median_ci(noop_diffs) if noop_diffs else None,
          'adj_ci': None,
          'rel': None,
      }
      if diffs and noop_diffs:
        row['adj_ci'] = boot.difference_ci(diffs, noop_diffs)
      if diffs:
        control = statistics.median(a_values)
        if control:
          row['rel'] = row['median'] / abs(control)
      row['tag'], row['tag_reason'] = tag_row(row, min_rel_effect)
      row['noop_tag'] = ''
      if noop_diffs:
        row['noop_tag'] = tag_row(
            {
                'diffs': noop_diffs,
                'a_values': noop_a,
                'b_values': noop_b,
                'median': row['noop_median'],
                'ci': row['noop_ci'],
                'adj_ci': None,
                'rel': None,
                'polarity': row['polarity'],
            }, 0.0)[0]
      rows.append(row)
  return rows


def fmt(value) -> str:
  if value is None:
    return '-'
  if isinstance(value, int) or float(value).is_integer():
    return f'{int(value):,}'
  if abs(value) >= 100:
    return f'{value:,.0f}'
  if abs(value) >= 10:
    return f'{value:.1f}'
  return f'{value:.2f}'


def fmt_signed(value) -> str:
  if value is None:
    return '-'
  text = fmt(value)
  return text if text.startswith('-') or value == 0 else '+' + text


def fmt_dist(values: list) -> str:
  if not values:
    return '-'
  return (f'{fmt(statistics.median(values))} '
          f'[{fmt(min(values))}–{fmt(max(values))}]')


def fmt_ci(ci) -> str:
  return f'[{fmt_signed(ci[0])}, {fmt_signed(ci[1])}]' if ci else '-'


def signs(diffs: list) -> str:
  return (f'{sum(d < 0 for d in diffs)}/{sum(d > 0 for d in diffs)}/'
          f'{sum(d == 0 for d in diffs)} (p={sign_test_p(diffs):.2g})')


def _table_row(cells: list) -> str:
  return '| ' + ' | '.join(str(c) for c in cells) + ' |'


def _row_cells(row: dict, has_noop: bool) -> list:
  name, stat, rel = row['name'], row['stat'], row['rel']
  star = '★ ' if row.get('primary') else ''
  cells = [
      f'{star}`{name}`', stat, row['n'],
      fmt_dist(row['a_values']),
      fmt_dist(row['b_values']),
      fmt_signed(row['median']) + ' ' + fmt_ci(row['ci']),
      f'{rel:+.1%}' if rel is not None else '-',
      signs(row['diffs'])
  ]
  if has_noop:
    noop_n = row['noop_n']
    cells.append(
        fmt_signed(row['noop_median']) + ' ' + fmt_ci(row['noop_ci']) +
        f' (n={noop_n})' if noop_n else '-')
  tag, reason = row['tag'], row['tag_reason']
  cells.append(f'**{tag}**' + (f' ({reason})' if reason else ''))
  return cells


def render_phase(rows: list, phase: str, labels: tuple, has_noop: bool) -> list:
  out = [f'### {PHASE_TITLES.get(phase, phase)}', '']
  counts = {}
  for r in rows:
    counts[r['tag']] = counts.get(r['tag'], 0) + 1
  out += [
      'Tags: ' + ', '.join(f'{t} {n}' for t, n in sorted(counts.items())), ''
  ]
  header = [
      'Histogram', 'Stat', 'n', f'{labels[0]} median [min–max]',
      f'{labels[1]} median [min–max]', 'Δ median [95% CI]', 'Δ%',
      '−/+/= (sign test)'
  ]
  align = [':---', ':---', '---:', '---:', '---:', '---:', '---:', ':---:']
  if has_noop:
    header.append('No-Op Δ median [95% CI]')
    align.append('---:')
  header.append('Tag')
  align.append(':---')
  out += [_table_row(header), _table_row(align)]
  for r in sorted(rows, key=lambda row: not row.get('primary')):
    if r['tag'] not in ('[UNCHANGED]', 'N/A'):
      out.append(_table_row(_row_cells(r, has_noop)))
  unchanged = [r for r in rows if r['tag'] == '[UNCHANGED]']
  if unchanged:
    items = []
    for r in unchanged:
      name, stat = r['name'], r['stat']
      control = fmt(statistics.median(r['a_values']))
      items.append(f'`{name}` {stat} = {control}')
    out += [
        '', f'**[UNCHANGED]** ({len(unchanged)} rows, every paired Δ = 0): ' +
        ', '.join(items)
    ]
  missing = [r for r in rows if r['tag'] == 'N/A']
  if missing:
    out += [
        '', f'No paired data ({len(missing)} rows): ' +
        ', '.join('`' + r['name'] + '` ' + r['stat'] for r in missing)
    ]
  out.append('')
  return out


def render_noop_phase(rows: list, phase: str) -> list:
  usable = [r for r in rows if r['noop_n']]
  if not usable:
    return []
  flagged = [
      r for r in usable if r['noop_tag'] not in ('[UNCHANGED]', '[NOISE]')
  ]
  out = [
      f'### No-Op noise floor: {PHASE_TITLES.get(phase, phase)}', '',
      f'{len(flagged)} of {len(usable)} rows have a No-Op 95% CI that excludes '
      '0 (expected: about 5% by chance).', '',
      _table_row([
          'Histogram', 'Stat', 'n', 'Δ median [95% CI]', 'max abs Δ',
          'max abs Δ %', 'Tag'
      ]),
      _table_row([':---', ':---', '---:', '---:', '---:', '---:', ':---'])
  ]
  for r in usable:
    if r['noop_tag'] == '[UNCHANGED]':
      continue
    max_abs = max(abs(d) for d in r['noop_diffs'])
    control = statistics.median(r['a_values']) if r['a_values'] else 0
    out.append(
        _table_row([
            '`' + r['name'] + '`', r['stat'], r['noop_n'],
            fmt_signed(r['noop_median']) + ' ' + fmt_ci(r['noop_ci']),
            fmt(max_abs), f'{max_abs / abs(control):.1%}' if control else '-',
            r['noop_tag']
        ]))
  out.append('')
  return out


def _load_session(args) -> dict:
  path = os.path.join(args.results_dir, f'{args.experiment}_session.json')
  if not os.path.exists(path):
    return {}
  with open(path, encoding='utf-8') as f:
    return json.load(f)


def _pair_table(ab_pairs: list, noop_pairs: list) -> list:
  all_pairs = ab_pairs + noop_pairs
  out = [
      '',
      _table_row(['Pair', 'Order', 'Mode', 'Exit flush A / B']),
      _table_row([':---', ':---:', ':---:', ':---'])
  ]
  for p in all_pairs:
    methods = [(p[side].get('exit_flush') or {}).get('method', '-')
               for side in ('A', 'B')]
    out.append(
        _table_row([
            p['A']['group'] + ' ' + str(p['pair']), p['order'],
            p['A'].get('mode', ''), ' / '.join(methods)
        ]))
  return out


def render_report(args, data: dict) -> str:
  """data: runs, attempts, prewarm, ab_pairs, noop_pairs, phase_rows."""
  ab_pairs, noop_pairs = data['ab_pairs'], data['noop_pairs']
  labels, flags = ('A', 'B'), ('', '')
  if ab_pairs:
    first = ab_pairs[0]
    labels = (first['A'].get('label', 'A'), first['B'].get('label', 'B'))
    flags = (first['A'].get('flags', ''), first['B'].get('flags', ''))
  session = _load_session(args)
  device = session.get('device', {})
  out = [f'## A/B report: `{args.experiment}`', '']
  if ab_pairs:
    out.append(f'* Arms: **{labels[0]}** `{flags[0]}` vs **{labels[1]}** '
               f'`{flags[1]}`.')
  if device:
    name, serial = device.get('device'), device.get('serial')
    package, version = device.get('package'), device.get('version_name')
    installed = device.get('last_update_time')
    tar_sha = session.get('device_tar_sha256', '')[:16]
    started, finished = session.get('started_at'), session.get('finished_at')
    status = session.get('status')
    out.append(f'* Device: `{name}` (`{serial}`), {package} {version} '
               f'(installed {installed}); golden tar sha256 `{tar_sha}…`; '
               f'session {started} → {finished} ({status}).')
  attempts = data['attempts']
  n_runs, n_prewarm = len(data['runs']), len(data['prewarm'])
  out.append(f'* Runs: {n_runs} valid of {len(attempts)} launches '
             f'(+{n_prewarm} discarded prewarm); '
             f'{len(ab_pairs)} A/B pairs, {len(noop_pairs)} No-Op pairs; '
             f'bootstrap {args.bootstrap} resamples, seed {args.seed}.')
  for a in attempts:
    if not a.get('valid'):
      run_id, attempt, reason = a['run_id'], a['attempt'], a.get('reason')
      out.append(f'  * Discarded `{run_id}` try {attempt}: {reason}')
  out += _pair_table(ab_pairs, noop_pairs)
  primary = data.get('primary') or []
  if primary:
    items = ', '.join(f'`{n}`' + (f' {s}' if s else '') for n, s in primary)
    method = (f'* Pre-registered primary rows (★): {items}. Their tags are not '
              'adjusted. Every other row is a diagnostic: a significant tag '
              'needs a Benjamini-Hochberg q < 0.05 (sign test, over the '
              "phase's diagnostic rows), otherwise it becomes [FLAG].")
  else:
    method = ('* No pre-registered primary rows: diagnostic tags are not '
              'corrected for multiple comparisons.')
  code = session.get('code', {})
  if code:
    git = code.get('git', {})
    apk = device.get('apk_sha256', {})
    apk_text = ', '.join(f'{k} `{v[:12]}…`' for k, v in sorted(apk.items()))
    commit_short = git.get('commit', '?')[:11]
    dirty_suffix = ' (dirty)' if git.get('dirty') else ''
    apk_unchanged = session.get('apk_unchanged', '?')
    out.append(f'* Provenance: harness `{commit_short}`{dirty_suffix}; '
               f'APK {apk_text}; APK unchanged during session: '
               f'{apk_unchanged}.')
  out += [
      '', method, '',
      'Δ = Treatment − Control per pair (median over pairs, 95% '
      'percentile-bootstrap CI of the median). Tag rules: see '
      '`android_ab_report.py --help`.', ''
  ]
  for phase, rows in data['phase_rows'].items():
    out += render_phase(rows, phase, labels, bool(noop_pairs))
  for phase, rows in data['phase_rows'].items():
    out += render_noop_phase(rows, phase)
  return '\n'.join(out)


def parse_args(argv=None):
  parser = argparse.ArgumentParser(
      description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
  parser.add_argument('--results-dir', required=True)
  parser.add_argument('--experiment', required=True)
  parser.add_argument('--out', help='Write the markdown report here.')
  parser.add_argument('--json', help='Write all rows as JSON here.')
  parser.add_argument('--histogram-file', default=DEFAULT_HISTOGRAM_FILE)
  parser.add_argument('--bootstrap', type=int, default=10000)
  parser.add_argument('--seed', type=int, default=20261021)
  parser.add_argument(
      '--min-rel-effect',
      type=float,
      default=0.01,
      help='Median effects smaller than this fraction of the Control median '
      'are tagged [NOISE] (default: 0.01).')
  parser.add_argument(
      '--primary',
      default=None,
      help='Pre-registered primary rows, comma-separated "Histogram" or '
      '"Histogram:stat" (default: primary_metrics from the session JSON). '
      'With primary rows, uncorrected diagnostic findings become [FLAG].')
  return parser.parse_args(argv)


def main(argv=None) -> int:
  args = parse_args(argv)
  runs, pair_records, attempts, prewarm = load_experiment(
      args.results_dir, args.experiment)
  if not runs:
    print(f'No valid runs for experiment {args.experiment!r}.', file=sys.stderr)
    return 1
  ab_pairs = build_pairs(args.results_dir, runs, pair_records, 'ab')
  noop_pairs = build_pairs(args.results_dir, runs, pair_records, 'noop')
  boot = Bootstrap(args.bootstrap, args.seed)
  order_key = histogram_order(args.histogram_file)
  phases = []
  for pair in ab_pairs + noop_pairs:
    for phase in pair['A_phases']:
      if phase not in phases:
        phases.append(phase)
  phase_rows = {
      phase:
          build_rows(
              ab_pairs,
              noop_pairs,
              phase,
              boot=boot,
              order_key=order_key,
              min_rel_effect=args.min_rel_effect) for phase in phases
  }
  primary_spec = args.primary
  if primary_spec is None:
    primary_spec = ','.join(_load_session(args).get('primary_metrics') or [])
  primary = parse_primary(primary_spec)
  for rows in phase_rows.values():
    apply_multiplicity(rows, primary)
  report = render_report(
      args, {
          'runs': runs,
          'attempts': attempts,
          'prewarm': prewarm,
          'ab_pairs': ab_pairs,
          'noop_pairs': noop_pairs,
          'phase_rows': phase_rows,
          'primary': primary,
      })
  if args.out:
    with open(args.out, 'w', encoding='utf-8') as f:
      f.write(report + '\n')
    print(f'Wrote {args.out}')
  else:
    print(report)
  if args.json:
    with open(args.json, 'w', encoding='utf-8') as f:
      json.dump(phase_rows, f, indent=1)
  return 0


if __name__ == '__main__':
  sys.exit(main())
