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
"""Unit tests for android_uma_dump, android_ab_report and android_ab_harness."""

import io
import json
import math
import os
import statistics
import sys
import tempfile
import unittest
from unittest import mock

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
if SCRIPT_DIR not in sys.path:
  sys.path.insert(0, SCRIPT_DIR)

import android_ab_harness as ab_harness  # pylint: disable=wrong-import-position
import android_ab_report as ab_report  # pylint: disable=wrong-import-position
import android_uma_dump as uma_dump  # pylint: disable=wrong-import-position


def _hist(name, count, total, buckets=None):
  return {
      'name':
          name,
      'count':
          count,
      'sum':
          total,
      'buckets':
          buckets or [{
              'low': 0,
              'high': 1,
              'count': count
          }] if count else []
  }


def _write_dump(path, histograms, annotations=None):
  with open(path, 'w', encoding='utf-8') as f:
    for h in histograms:
      f.write(
          uma_dump.format_line(h['name'], {
              'id': 1,
              'result': {
                  'histograms': [h]
              }
          }))
    for key, data in (annotations or {}).items():
      uma_dump.write_annotation(f, key, data)


class UmaDumpTest(unittest.TestCase):

  def test_parse_skips_annotations(self):
    lines = [
        uma_dump.format_line('A',
                             {'result': {
                                 'histograms': [_hist('A', 2, 5)]
                             }}),
        uma_dump.format_line(
            uma_dump.LANDING_GATE_KEY,
            {'result': {
                uma_dump.LANDING_GATE_KEY: {
                    'ok': True
                }
            }}), 'garbage line\n'
    ]
    histograms, annotations = uma_dump.parse_dump_lines(lines)
    self.assertEqual(list(histograms), ['A'])
    self.assertEqual(annotations[uma_dump.LANDING_GATE_KEY], {'ok': True})

  def test_matches_query_and_parse_dot_boundary(self):
    self.assertTrue(
        uma_dump.matches_query('Cobalt.Storage.Cookie.CommitDuration',
                               'Cobalt.Storage.Cookie.CommitDuration'))
    self.assertFalse(
        uma_dump.matches_query(
            'Cobalt.Storage.Cookie.CommitDurationPerOperation',
            'Cobalt.Storage.Cookie.CommitDuration'))
    self.assertTrue(
        uma_dump.matches_query('HttpCache.Pattern.Image', 'HttpCache.Pattern'))
    self.assertFalse(
        uma_dump.matches_query('HttpCache.PatternX', 'HttpCache.Pattern'))
    self.assertTrue(
        uma_dump.matches_query('Cobalt.Startup.MilestoneDuration.First',
                               'Cobalt.Startup.MilestoneDuration.'))
    lines = [
        uma_dump.format_line(
            'Cobalt.Storage.Cookie.CommitDuration', {
                'result': {
                    'histograms': [
                        _hist('Cobalt.Storage.Cookie.CommitDuration', 4, 65),
                        _hist(
                            'Cobalt.Storage.Cookie.CommitDurationPerOperation',
                            4, 25),
                    ]
                }
            })
    ]
    histograms, _ = uma_dump.parse_dump_lines(lines)
    self.assertEqual(list(histograms), ['Cobalt.Storage.Cookie.CommitDuration'])

  def test_snapshot_delta(self):
    s0 = {
        'C': {
            'name': 'C',
            'count': 3,
            'sum': 30,
            'buckets': [{
                'low': 10,
                'high': 12,
                'count': 3
            }]
        },
        'Same': _hist('Same', 4, 4),
    }
    s1 = {
        'C': {
            'name':
                'C',
            'count':
                5,
            'sum':
                55,
            'buckets': [{
                'low': 10,
                'high': 12,
                'count': 4
            }, {
                'low': 14,
                'high': 17,
                'count': 1
            }]
        },
        'Same': _hist('Same', 4, 4),
        'New': _hist('New', 1, 7),
    }
    delta = uma_dump.snapshot_delta(s0, s1)
    self.assertEqual(sorted(delta), ['C', 'New'])
    self.assertEqual(delta['C']['count'], 2)
    self.assertEqual(delta['C']['sum'], 25)
    self.assertEqual(delta['C']['buckets'], [{
        'low': 10,
        'high': 12,
        'count': 1
    }, {
        'low': 14,
        'high': 17,
        'count': 1
    }])
    self.assertEqual(delta['New']['sum'], 7)


class ReportLogicTest(unittest.TestCase):

  def test_classification(self):
    self.assertTrue(ab_report.is_enum('HttpCache.Pattern.Image'))
    self.assertFalse(ab_report.is_enum('HttpCache.PatternX'))
    self.assertEqual(
        ab_report.polarity('Cobalt.Storage.Cookie.CommitDuration', 'count'), -1)
    self.assertEqual(ab_report.polarity('HttpCache.Pattern', 'count'), 0)
    self.assertEqual(
        ab_report.polarity('Cobalt.Storage.Cookie.PendingOperationsAtCommit',
                           'mean'), 0)
    self.assertEqual(
        ab_report.polarity('DOMStorage.CommitSizeBytesAggregated', 'sum'), -1)
    self.assertEqual(
        ab_report.polarity('Cobalt.Storage.OnPause.FlushDuration', 'value'), -1)
    self.assertFalse(ab_report.is_sampled('Memory.ParkableImage.Write.Size'))
    self.assertTrue(
        ab_report.is_sampled(
            'Memory.Experimental.Browser2.Small.ParkableImage.OnDiskSize'))
    self.assertEqual(
        ab_report.polarity('Memory.ParkableImage.Write.Size', 'count'), -1)
    self.assertEqual(
        ab_report.polarity('Memory.ParkableImage.Write.Size', 'sum'), -1)
    self.assertEqual(
        ab_report.polarity(
            'Memory.Experimental.Browser2.Small.ParkableImage.OnDiskSize',
            'mean'), -1)
    self.assertEqual(
        ab_report.polarity(
            'Memory.Experimental.Browser2.Small.ParkableImage.UnparkedSize',
            'mean'), 0)
    self.assertEqual(
        ab_report.polarity('Memory.ParkableImage.OnDiskSize.5min', 'value'), -1)
    self.assertEqual(
        ab_report.polarity('Memory.ParkableImage.UnparkedSize.5min', 'value'),
        0)
    self.assertEqual(
        ab_report.polarity('Memory.ParkableImage.Read.Throughput', 'count'), -1)
    self.assertEqual(
        ab_report.polarity('Memory.ParkableImage.Read.Throughput', 'mean'), 1)
    self.assertEqual(
        ab_report.polarity('Memory.ParkableImage.Read.Throughput', 'sum'), 0)

  def test_stat_value(self):
    h = _hist('X', 4, 10)
    self.assertEqual(ab_report.stat_value(h, 'count'), 4)
    self.assertEqual(ab_report.stat_value(h, 'sum'), 10)
    self.assertEqual(ab_report.stat_value(h, 'mean'), 2.5)
    self.assertEqual(ab_report.stat_value(None, 'count'), 0)
    self.assertIsNone(ab_report.stat_value(None, 'value'))

  def test_bootstrap_difference_ci(self):
    boot = ab_report.Bootstrap(2000, 1)
    self.assertEqual(boot.difference_ci([-5] * 6, [0] * 6), (-5, -5))
    values, baseline = [-3, 1, 2, 4, -1, 0], [0, 1, -1, 0, 2, -2]
    low, high = boot.difference_ci(values, baseline)
    self.assertLessEqual(low, 0)
    self.assertGreaterEqual(high, 0)
    self.assertEqual(
        ab_report.Bootstrap(2000, 1).difference_ci(values, baseline),
        (low, high))

  def test_exact_median_ci(self):
    ci = ab_report.exact_median_ci
    self.assertIsNone(ci([]))
    self.assertIsNone(ci([1, 2, 3, 4, 5]))  # n = 5: no 95% CI exists.
    self.assertEqual(ci([5, 1, 4, 2, 6, 3]), (1, 6))  # n = 6: [min, max].
    self.assertEqual(ci(list(range(10, 0, -1))), (2, 9))  # 2nd and 9th of 10.
    self.assertIsNone(ci([0] * (ab_report.MIN_PAIRS - 1)))
    self.assertIsNotNone(ci([0] * ab_report.MIN_PAIRS))
    # (x_(k), x_(n+1-k)) is the narrowest order-statistic CI with at least 95%
    # coverage: P(Binomial(n, 1/2) < k) <= 2.5% < P(Binomial(n, 1/2) <= k).
    for n in range(ab_report.MIN_PAIRS, 41):
      low, high = ci(list(range(n)))
      k = low + 1
      self.assertEqual(high, n - k)
      tail = sum(math.comb(n, i) for i in range(k)) / 2**n
      self.assertLessEqual(tail, 0.025)
      self.assertGreater(tail + math.comb(n, k) / 2**n, 0.025)

  def _row(self, diffs, *, adj_ci=None, rel=-0.5, pol=-1):
    return {
        'diffs': diffs,
        'adj_ci': adj_ci,
        'rel': rel,
        'polarity': pol,
        'median': statistics.median(diffs) if diffs else None,
    }

  def test_tag_rules(self):
    tag = ab_report.tag_row
    six_down = [-5, -4, -5, -6, -5, -5]
    self.assertEqual(tag(self._row([]), 0.01), ('N/A', 'no paired data'))
    self.assertEqual(tag(self._row([0, 0]), 0.01)[0], '[UNCHANGED]')
    # n <= 5 is never significant, however large and consistent the effect;
    # e.g. the 3-pair A/A row whose old bootstrap CI [+504, +67,884] excluded 0.
    for diffs in ([5, 4, 5, 6, 5], [-5] * 5, [18168, 504, 67884], [-2, -2]):
      self.assertEqual(
          tag(self._row(diffs, rel=0.5), 0.01),
          ('[NOISE]', 'n <= 5: no 95% CI of the median'))
    # n = 6: the exact CI is [min, max], so every difference needs one sign.
    self.assertEqual(
        tag(self._row([5, 4, 5, 6, 5, 7], rel=0.5), 0.01), ('[REGRESSED]', ''))
    self.assertEqual(tag(self._row(six_down), 0.01), ('[IMPROVED]', ''))
    self.assertEqual(
        tag(self._row([-5, -4, -5, -6, -5, 3]), 0.01),
        ('[NOISE]', 'CI includes 0'))
    # n = 10: the exact CI is (2nd, 9th order statistic), so one difference
    # may have the other sign, but not two.
    self.assertEqual(tag(self._row([-5] * 9 + [3]), 0.01), ('[IMPROVED]', ''))
    self.assertEqual(
        tag(self._row([-5] * 8 + [3, 3]), 0.01), ('[NOISE]', 'CI includes 0'))
    self.assertEqual(
        tag(self._row(six_down, adj_ci=(-6, 1)), 0.01),
        ('[NOISE]', 'No-Op-adjusted bootstrap CI includes 0'))
    self.assertEqual(
        tag(self._row(six_down, rel=-0.001), 0.01), ('[NOISE]', 'effect < 1%'))
    self.assertEqual(
        tag(self._row(six_down, pol=0), 0.01),
        ('[CHANGED]', 'no better/worse direction'))

  def test_config_echoes_are_changed_not_improved(self):
    echoes = {
        'SimpleCache.Http.MaxCacheSizeOnInit': (25600, 12288),
        'SimpleCache.Code.MaxCacheSizeOnInit': (5120, 10240),
        'Cobalt.Storage.GpuShaderCache.LoadedEntriesCount': (97, 32),
        'Cobalt.Storage.GpuShaderCache.LoadedBytesKB': (675, 210),
    }
    for name in echoes:
      self.assertEqual(ab_report.polarity(name, 'value'), 0)
    self.assertEqual(
        ab_report.polarity('SimpleCache.Code.CacheSizeOnInit', 'value'), 1)
    pairs = [{
        f'{arm}_phases': {
            'homefeed': {
                name: _hist(name, 1, values[i])
                for name, values in echoes.items()
            }
        }
        for i, arm in enumerate('AB')
    }
             for _ in range(ab_report.MIN_PAIRS)]
    rows = ab_report.build_rows(
        pairs, [],
        'homefeed',
        boot=ab_report.Bootstrap(100, 1),
        order_key=ab_report.histogram_order(None),
        min_rel_effect=0.01)
    self.assertEqual({r['name']: (r['stat'], r['tag']) for r in rows},
                     {name: ('value', '[CHANGED]') for name in echoes})

  def test_unchanged_primary_rows_are_starred(self):

    def row(name, stat, value, primary):
      return {
          'name': name,
          'stat': stat,
          'tag': '[UNCHANGED]',
          'tag_reason': '',
          'a_values': [value] * 6,
          'primary': primary,
      }

    lines = ab_report.render_phase([
        row('Other.Count', 'count', 4, False),
        row('Cobalt.Storage.DirCache.StartupTotalSizeKB', 'value', 7859, True),
    ], 'homefeed', ('A1', 'A2'), False)
    header = lines[4]
    self.assertIn('| Δ median [exact 95% CI; n ≤ 5: min–max range] |', header)
    self.assertIn('| Δ / \\|median A1\\| |', header)
    summary = next(line for line in lines if line.startswith('**[UNCHANGED]**'))
    self.assertIn(
        ': ★ `Cobalt.Storage.DirCache.StartupTotalSizeKB` value = 7,859, '
        '`Other.Count` count = 4', summary)

  def test_delta_cells_label_range_and_exact_ci(self):
    self.assertEqual(
        ab_report.fmt_median_ci(18168, [18168, 504, 67884], None),
        '+18,168 range [+504, +67,884]')
    diffs = list(range(1, 11))
    self.assertEqual(
        ab_report.fmt_median_ci(5.5, diffs, ab_report.exact_median_ci(diffs)),
        '+5.50 [+2, +9]')
    self.assertEqual(ab_report.fmt_median_ci(None, [], None), '-')
    self.assertEqual(
        ab_report.fmt_noop_ci({
            'noop_median': -2,
            'noop_ci': None
        }), '-2 [n/a]')

  def test_noop_section_uses_exact_ci(self):

    def row(name, noop_diffs):
      median = statistics.median(noop_diffs)
      return {
          'name':
              name,
          'stat':
              'count',
          'a_values': [10] * 6,
          'noop_n':
              len(noop_diffs),
          'noop_diffs':
              noop_diffs,
          'noop_median':
              median,
          'noop_ci':
              ab_report.exact_median_ci(noop_diffs),
          'noop_tag':
              ab_report.tag_row(
                  {
                      'diffs': noop_diffs,
                      'median': median,
                      'adj_ci': None,
                      'rel': None,
                      'polarity': -1
                  }, 0.0)[0],
      }

    six, three = row('Six', [1, 2, 1, 3, 1, 2]), row('Three', [1, 2, 3])
    text = '\n'.join(ab_report.render_noop_phase([six, three], 'homefeed'))
    self.assertIn(
        '1 of 1 rows with n ≥ 6 have a No-Op exact 95% CI that excludes 0 '
        '(expected: at most 5% by chance). Rows with n ≤ 5 (CI n/a): 1.', text)
    self.assertIn('| `Six` | count | 6 | +1.50 [+1, +3] |', text)
    self.assertIn('| `Three` | count | 3 | +2 [n/a] |', text)
    text = '\n'.join(ab_report.render_noop_phase([three], 'homefeed'))
    self.assertIn('No-Op CI n/a: every row has n ≤ 5 No-Op pairs', text)

  def test_sign_test(self):
    self.assertAlmostEqual(ab_report.sign_test_p([-1] * 6), 0.03125)
    self.assertEqual(ab_report.sign_test_p([0, 0]), 1.0)
    self.assertAlmostEqual(ab_report.sign_test_p([-1, -1, 1]), 1.0)

  def test_parse_primary(self):
    self.assertEqual(
        ab_report.parse_primary(' A.B:count, C.D ,,'), [('A.B', 'count'),
                                                        ('C.D', None)])
    self.assertEqual(
        ab_report.parse_primary('exit_flush:A.B:sum, homefeed:C.D'),
        [('exit_flush:A.B', 'sum'), ('homefeed:C.D', None)])
    primary = ab_report.parse_primary('exit_flush:A.B:sum, C.D:count')
    self.assertFalse(
        ab_report.is_primary({
            'phase': 'homefeed',
            'name': 'A.B',
            'stat': 'sum'
        }, primary))
    self.assertTrue(
        ab_report.is_primary(
            {
                'phase': 'exit_flush',
                'name': 'A.B',
                'stat': 'sum'
            }, primary))
    self.assertTrue(
        ab_report.is_primary(
            {
                'phase': 'homefeed',
                'name': 'C.D',
                'stat': 'count'
            }, primary))
    self.assertEqual(ab_report.parse_primary(''), [])

  def test_primary_mean_matches_one_sample_value_row(self):

    def row(stat, phase='homefeed'):
      return {'phase': phase, 'name': 'A.B', 'stat': stat}

    primary = ab_report.parse_primary('homefeed:A.B:mean')
    self.assertTrue(ab_report.is_primary(row('value'), primary))
    self.assertTrue(ab_report.is_primary(row('mean'), primary))
    self.assertFalse(ab_report.is_primary(row('count'), primary))
    self.assertFalse(ab_report.is_primary(row('value', 'exit_flush'), primary))
    # One-way alias: ':value' never matches a multi-sample 'mean' row.
    self.assertFalse(
        ab_report.is_primary(row('mean'), ab_report.parse_primary('A.B:value')))
    self.assertEqual(
        ab_report.unmatched_primary(
            {'homefeed': [row('value'), row('count')]},
            ab_report.parse_primary('A.B:mean, A.B:sum, X, exit_flush:A.B')),
        ['A.B:sum', 'X', 'exit_flush:A.B'])

  def test_bh_qvalues(self):
    q = ab_report.bh_qvalues([0.01, 0.04, 0.03, 0.5])
    self.assertAlmostEqual(q[0], 0.04)
    self.assertAlmostEqual(q[2], 0.0533333, places=5)
    self.assertAlmostEqual(q[1], 0.0533333, places=5)
    self.assertAlmostEqual(q[3], 0.5)

  def test_apply_multiplicity(self):

    def row(name, stat, tag, sign_p, diffs=(-1, -1, -1, -1, -1, -1)):
      return {
          'name': name,
          'stat': stat,
          'tag': tag,
          'tag_reason': '',
          'sign_p': sign_p,
          'diffs': list(diffs)
      }

    rows = [
        row('Cookie', 'count', '[IMPROVED]', 0.03125),
        row('Diag', 'sum', '[IMPROVED]', 0.03125),
        row('Other', 'mean', '[NOISE]', 1.0),
        row('Same', 'count', '[UNCHANGED]', 1.0, diffs=(0, 0)),
    ]
    legacy = [dict(r) for r in rows]
    ab_report.apply_multiplicity(legacy, [])
    self.assertEqual([r['tag'] for r in legacy],
                     ['[IMPROVED]', '[IMPROVED]', '[NOISE]', '[UNCHANGED]'])
    ab_report.apply_multiplicity(rows, [('Cookie', 'count')])
    self.assertTrue(rows[0]['primary'])
    self.assertEqual(rows[0]['tag'], '[IMPROVED]')
    self.assertEqual(rows[1]['tag'], '[FLAG]')
    self.assertIn('BH q=0.062', rows[1]['tag_reason'])
    self.assertEqual(rows[2]['tag'], '[NOISE]')
    self.assertIsNone(rows[3]['q'])

  def test_summary_lists_p0_rows_regressed_before_bh(self):

    def row(name, tier, tag, median, sign_p=0.03125):
      return {
          'phase': 'homefeed',
          'name': name,
          'stat': 'value',
          'tier': tier,
          'tag': tag,
          'tag_reason': '',
          'sign_p': sign_p,
          'diffs': [median] * 6,
          'median': median,
          'rel': None,
      }

    rows = [
        row('Cobalt.Storage.OnPause.FlushDuration', 'P0', '[REGRESSED]', 4),
        row('Cobalt.Storage.UserData.StartupTotalSizeKB2', 'P0', '[REGRESSED]',
            7),
        row('V8.CompileSerializeMicroSeconds', 'P0', '[IMPROVED]', -5),
        row('LevelDBEnv.SyncResult', 'P1', '[REGRESSED]', 2),
        row('Noise.A', 'P2', '[NOISE]', 1, sign_p=1.0),
        row('Noise.B', 'P2', '[NOISE]', 1, sign_p=1.0),
    ]
    ab_report.apply_multiplicity(
        rows, [('Cobalt.Storage.OnPause.FlushDuration', None)])
    tags = ['[REGRESSED]', '[FLAG]', '[FLAG]', '[FLAG]', '[NOISE]', '[NOISE]']
    self.assertEqual([r['tag'] for r in rows], tags)
    summary = '\n'.join(ab_report.render_ranked_summary({'homefeed': rows}))
    self.assertEqual([r['tag'] for r in rows], tags)
    self.assertIn(
        '* **P0 rows tagged [REGRESSED] before BH correction**: homefeed: ★ '
        '`Cobalt.Storage.OnPause.FlushDuration` value +4 [REGRESSED]; '
        'homefeed: `Cobalt.Storage.UserData.StartupTotalSizeKB2` value +7 '
        '[FLAG].\n', summary)
    summary = '\n'.join(ab_report.render_ranked_summary({'homefeed': rows[2:]}))
    self.assertIn(
        '* **P0 rows tagged [REGRESSED] before BH correction**: none.\n',
        summary)


class ReportEndToEndTest(unittest.TestCase):
  """Builds a homefeed + exit-flush report from synthetic S1/S2 snapshots."""

  def _run_report(self, tmp: str, num_pairs: int, *extra_args):
    """Writes num_pairs ABBA pairs; returns (rows by phase, report text)."""
    manifest = []
    for pair in range(1, num_pairs + 1):
      order = 'AB' if pair % 2 else 'BA'
      for slot, arm in enumerate(order):
        base = f'hf_ab_p{pair:02d}_{arm}'
        commits = 8 if arm == 'A' else 3
        s1 = [
            _hist('Cobalt.Storage.Cookie.CommitDuration', commits,
                  14 * commits),
            _hist('LevelDBEnv.SyncResult', 68, 0),
        ]
        s2 = s1[:1] + [
            _hist('LevelDBEnv.SyncResult', 70, 0),
            _hist('Cobalt.Storage.OnPause.FlushDuration', 1, 70 +
                  (pair if arm == 'B' else 0)),
        ]
        for tag, hs in (('s1', s1), ('s2', s2)):
          _write_dump(os.path.join(tmp, f'{base}.{tag}.txt'), hs)
        _write_dump(os.path.join(tmp, f'{base}.txt'), s1)
        manifest.append({
            'type': 'run',
            'experiment': 'hf',
            'mode': 'homefeed',
            'group': 'ab',
            'pair': pair,
            'slot': slot,
            'arm': arm,
            'label': 'Control' if arm == 'A' else 'Treatment',
            'flags': '',
            'run_id': base,
            'attempt': 1,
            'valid': True,
            'dump': f'{base}.txt'
        })
      manifest.append({
          'type': 'pair',
          'experiment': 'hf',
          'group': 'ab',
          'pair': pair,
          'order': order,
      })
    with open(os.path.join(tmp, 'manifest.jsonl'), 'w', encoding='utf-8') as f:
      f.writelines(json.dumps(r) + '\n' for r in manifest)
    out = os.path.join(tmp, 'report.md')
    rows_path = os.path.join(tmp, 'rows.json')
    self.assertEqual(
        ab_report.main([
            '--results-dir', tmp, '--experiment', 'hf', '--out', out, '--json',
            rows_path, '--bootstrap', '500', *extra_args
        ]), 0)
    with open(rows_path, encoding='utf-8') as f:
      rows = json.load(f)
    with open(out, encoding='utf-8') as f:
      return rows, f.read()

  def test_homefeed_and_exit_flush_report(self):
    with tempfile.TemporaryDirectory() as tmp:
      rows, report = self._run_report(tmp, 6)
    by_key = {(r['name'], r['stat']): r for r in rows['homefeed']}
    commit_count = by_key[('Cobalt.Storage.Cookie.CommitDuration', 'count')]
    self.assertEqual(commit_count['diffs'], [-5] * 6)
    self.assertEqual(commit_count['ci'], [-5, -5])
    self.assertEqual(commit_count['tag'], '[IMPROVED]')
    self.assertEqual(by_key[('LevelDBEnv.SyncResult', 'count')]['tag'],
                     '[UNCHANGED]')
    flush = {(r['name'], r['stat']): r for r in rows['exit_flush']}
    pause = flush[('Cobalt.Storage.OnPause.FlushDuration', 'value')]
    self.assertEqual(pause['diffs'], [1, 2, 3, 4, 5, 6])
    self.assertEqual(pause['ci'], [1, 6])
    self.assertEqual(pause['tag'], '[REGRESSED]')
    self.assertIn('**[IMPROVED]**', report)
    self.assertIn('| -5 [-5, -5] |', report)
    self.assertIn('exact 95% CI of the median', report)
    self.assertNotIn('percentile-bootstrap', report)
    self.assertNotIn('Only 6 A/B pairs', report)

  def test_three_pairs_are_never_significant(self):
    with tempfile.TemporaryDirectory() as tmp:
      rows, report = self._run_report(tmp, 3)
    by_key = {(r['name'], r['stat']): r for r in rows['homefeed']}
    commit_count = by_key[('Cobalt.Storage.Cookie.CommitDuration', 'count')]
    self.assertEqual(commit_count['diffs'], [-5, -5, -5])
    self.assertIsNone(commit_count['ci'])
    self.assertEqual((commit_count['tag'], commit_count['tag_reason']),
                     ('[NOISE]', 'n <= 5: no 95% CI of the median'))
    self.assertIn('| -5 range [-5, -5] |', report)
    self.assertIn('* Only 3 A/B pairs: a 95% CI of the median needs n ≥ 6',
                  report)
    self.assertNotIn('**[IMPROVED]**', report)
    self.assertNotIn('**[REGRESSED]**', report)

  def test_primary_mean_alias_and_unmatched_warnings(self):
    primary = ('exit_flush:Cobalt.Storage.OnPause.FlushDuration:mean,'
               'Cobalt.Storage.Cookie.CommitDuration:value,Typo.Hist')
    with tempfile.TemporaryDirectory() as tmp, mock.patch(
        'sys.stdout', new_callable=io.StringIO) as stdout:
      rows, report = self._run_report(tmp, 6, '--primary', primary)
    pause = next(r for r in rows['exit_flush']
                 if r['name'] == 'Cobalt.Storage.OnPause.FlushDuration')
    self.assertEqual((pause['stat'], pause['primary'], pause['tag']),
                     ('value', True, '[REGRESSED]'))
    self.assertIn(
        '* **WARNING:** primary entries that match no row (check the phase, '
        'name and stat), so they are not ★: '
        '`Cobalt.Storage.Cookie.CommitDuration:value`, `Typo.Hist`.', report)
    self.assertIn(
        'WARNING: primary entries that match no row (check the phase, name '
        'and stat): Cobalt.Storage.Cookie.CommitDuration:value, Typo.Hist\n',
        stdout.getvalue())
    self.assertIn(
        '* **P0 rows tagged [REGRESSED] before BH correction**: exit_flush: ★ '
        '`Cobalt.Storage.OnPause.FlushDuration` value +3.50 (+5.0%) '
        '[REGRESSED].', report)


class HarnessLogicTest(unittest.TestCase):

  def _args(self, *extra, results_dir='out'):
    return ab_harness.parse_args(
        ['--results-dir', results_dir, '--experiment', 'e', *extra])

  def test_build_schedule(self):
    self.assertEqual(
        ab_harness.build_schedule(3, 2), [('ab', 1, ('A', 'B')),
                                          ('noop', 1, ('A', 'B')),
                                          ('ab', 2, ('B', 'A')),
                                          ('noop', 2, ('B', 'A')),
                                          ('ab', 3, ('A', 'B'))])
    # The default is the smallest n with a 95% CI and balances AB / BA.
    self.assertEqual(ab_harness.DEFAULT_PAIRS, ab_report.MIN_PAIRS)
    orders = [
        o for _, _, o in ab_harness.build_schedule(ab_harness.DEFAULT_PAIRS, 0)
    ]
    self.assertEqual(orders.count(('A', 'B')), orders.count(('B', 'A')))

  def test_spec(self):
    with tempfile.TemporaryDirectory() as tmp:
      spec_path = os.path.join(tmp, 'spec.json')
      with open(spec_path, 'w', encoding='utf-8') as f:
        json.dump(
            {
                'description':
                    'Cookie homefeed window',
                'results_dir':
                    tmp,
                'experiment':
                    'ws2',
                'mode':
                    'homefeed',
                'pairs':
                    4,
                'primary_metrics':
                    ['Cobalt.Storage.Cookie.CommitDuration:count'],
            }, f)
      args = ab_harness.parse_args(['--spec', spec_path, '--pairs', '5'])
      self.assertEqual((args.experiment, args.mode, args.pairs),
                       ('ws2', 'homefeed', 5))
      self.assertEqual(args.primary_metrics,
                       'Cobalt.Storage.Cookie.CommitDuration:count')
      self.assertEqual(args.spec_doc['description'], 'Cookie homefeed window')
      with open(spec_path, 'w', encoding='utf-8') as f:
        json.dump({'experiment': 'x', 'paris': 3}, f)
      with mock.patch('sys.stderr'), self.assertRaises(SystemExit):
        ab_harness.parse_args(['--spec', spec_path, '--results-dir', tmp])
    with mock.patch('sys.stderr'), self.assertRaises(SystemExit):
      ab_harness.parse_args(['--experiment', 'e'])

  def test_preflight_problems(self):
    device = mock.MagicMock()
    device.shell.return_value = ('Filesystem 1K-blocks Used Available\n'
                                 '/dev/block/userdata 4000000 3900000 102400')
    args = self._args('--expect-version', '7.30.1')
    problems = ab_harness.preflight_problems(device, args, {
        'version_name': '7.29.0',
        'apk_sha256': {}
    })
    self.assertEqual(len(problems), 3)
    self.assertIn('expected 7.30.1', problems[0])
    self.assertIn('no installed APK', problems[1])
    self.assertIn('100 MB free', problems[2])
    device.shell.return_value = 'h\n/dev/block/userdata 4000000 10 563200'
    self.assertEqual(
        ab_harness.preflight_problems(device, args, {
            'version_name': '7.30.1',
            'apk_sha256': {
                'base.apk': 'ab'
            }
        }), [])

  def test_validate_run(self):
    params = 'force_startup_screen=none,hl=en'
    good_landing = {
        'ok': True,
        'text_len': 100,
        'href': 'https://www.youtube.com/tv?hl=en&force_startup_screen=none#/'
    }
    with tempfile.TemporaryDirectory() as tmp:
      dump = os.path.join(tmp, 'run.txt')
      histograms = [_hist(f'H{i}', 1, 1) for i in range(25)]
      _write_dump(dump, histograms, {uma_dump.LANDING_GATE_KEY: good_landing})
      info = ab_harness.validate_run('homefeed', dump, 0, params)
      self.assertTrue(info['valid'], info['reason'])

      self.assertFalse(
          ab_harness.validate_run('homefeed', dump, 0,
                                  params + ',gl=US')['valid'])
      picker = dict(good_landing, ok=False, reason='picker')
      _write_dump(dump, histograms, {uma_dump.LANDING_GATE_KEY: picker})
      info = ab_harness.validate_run('homefeed', dump,
                                     uma_dump.LANDING_GATE_EXIT_CODE, params)
      self.assertFalse(info['valid'])
      self.assertIn('landing gate', info['reason'])
      empty = dict(good_landing, text_len=0)
      _write_dump(dump, histograms, {uma_dump.LANDING_GATE_KEY: empty})
      self.assertFalse(
          ab_harness.validate_run('homefeed', dump, 0, params)['valid'])

      # A deep-linked watch page is not the Home CUJ, even though its
      # url_params (appended inside the fragment) pass the substring check.
      watch = dict(
          good_landing,
          href='https://www.youtube.com/tv#/watch?v=1La4QzGeaaQ'
          '&force_startup_screen=none&hl=en')
      _write_dump(dump, histograms, {uma_dump.LANDING_GATE_KEY: watch})
      info = ab_harness.validate_run('homefeed', dump, 0, params)
      self.assertFalse(info['valid'])
      self.assertIn('landing gate: not on Home', info['reason'])
      self.assertTrue(ab_harness.validate_run('wpr', dump, 0, params)['valid'])

      _write_dump(
          dump, histograms, {
              uma_dump.LANDING_GATE_KEY: good_landing,
              uma_dump.EXIT_FLUSH_KEY: {
                  'method': 'h5vcc.system.exit',
                  'ok': False,
                  'error': 'Connection closed',
              },
          })
      ef_fail = ab_harness.validate_run(
          'homefeed', dump, 0, params, exit_flush=True)
      self.assertFalse(ef_fail['valid'])
      self.assertIn('exit flush:', ef_fail['reason'])

  def test_paired_values_skips_missing_phase(self):
    pairs = [
        {
            'A_phases': {
                'exit_flush': {
                    'H': _hist('H', 2, 20)
                }
            },
            'B_phases': {
                'exit_flush': {
                    'H': _hist('H', 2, 10)
                }
            },
        },
        {
            'A_phases': {},
            'B_phases': {
                'exit_flush': {
                    'H': _hist('H', 2, 10)
                }
            },
        },
    ]
    a_vals, b_vals, diffs = ab_report.paired_values(pairs, 'exit_flush', 'H',
                                                    'count')
    self.assertEqual((a_vals, b_vals, diffs), ([2], [2], [0]))

  def test_run_abort_hint(self):
    with tempfile.TemporaryDirectory() as tmp:
      harness = ab_harness.Harness(
          self._args('--pairs', '1', results_dir=tmp), None)
      failed = {
          'valid': False,
          'run_id': 'e_ab_p01_A',
          'attempt': 3,
          'reason': 'landing gate: picker',
      }
      with mock.patch.object(harness, 'run_slot', return_value=failed):
        with self.assertRaisesRegex(RuntimeError, 're-capture the golden tar'):
          harness.run()

  def test_single_sample_eliminated_in_one_arm_reports_count(self):
    pairs = [{
        'A_phases': {
            'homefeed': {
                'Sql.Database.Success.OpenInternalTime.DIPS':
                    _hist('Sql.Database.Success.OpenInternalTime.DIPS', 1,
                          6400),
                'ServiceWorker.Database.OpenResult':
                    _hist('ServiceWorker.Database.OpenResult', 1, 0),
                'Cobalt.Storage.UserData.StartupTotalSizeKB':
                    _hist('Cobalt.Storage.UserData.StartupTotalSizeKB', 1, 683),
            }
        },
        'B_phases': {
            'homefeed': {
                'Cobalt.Storage.UserData.StartupTotalSizeKB':
                    _hist('Cobalt.Storage.UserData.StartupTotalSizeKB', 1, 103),
            }
        },
    } for _ in range(ab_report.MIN_PAIRS)]
    rows = ab_report.build_rows(
        pairs, [],
        'homefeed',
        boot=ab_report.Bootstrap(100, 1),
        order_key=ab_report.histogram_order(None),
        min_rel_effect=0.01)
    by_name = {(r['name'], r['stat']): r for r in rows}
    self.assertIn(('Sql.Database.Success.OpenInternalTime.DIPS', 'count'),
                  by_name)
    self.assertNotIn(('Sql.Database.Success.OpenInternalTime.DIPS', 'value'),
                     by_name)
    dips = by_name[('Sql.Database.Success.OpenInternalTime.DIPS', 'count')]
    self.assertEqual((dips['n'], dips['median'], dips['tag']),
                     (ab_report.MIN_PAIRS, -1, '[IMPROVED]'))
    sw = by_name[('ServiceWorker.Database.OpenResult', 'count')]
    self.assertEqual((sw['n'], sw['median'], sw['tag']),
                     (ab_report.MIN_PAIRS, -1, '[IMPROVED]'))
    size = by_name[('Cobalt.Storage.UserData.StartupTotalSizeKB', 'value')]
    self.assertEqual(size['tag'], '[IMPROVED]')

  def test_ranked_tier_ordering_and_verdict_summary(self):
    tiers = ab_report.parse_histogram_tiers(ab_report.DEFAULT_HISTOGRAM_FILE)
    p0_names = [q for tier, q in tiers if tier == 'P0']
    self.assertEqual(p0_names, [
        'Cobalt.Startup.Time.NavigationDispatchToCommit',
        'ServiceWorker.Storage.ReadInitialDataFromDB.Time',
        'SimpleCache.Http.IndexRestoreTime',
        'V8.CompileSerializeMicroSeconds',
        'Cookie.TimeBlockedOnLoad',
        'Cobalt.Storage.OnPause.FlushDuration',
        'Cobalt.Storage.Exit.CookieFlushDuration',
        'Cobalt.Responsiveness.IOJankyIntervalsPerMinute',
        'Cobalt.LocalStorage.DatabaseOpenError',
        'Cobalt.Storage.UserData.StartupTotalSizeKB2',
        'Cobalt.Storage.DirCache.StartupTotalSizeKB',
    ])
    tier_of = ab_report.histogram_tier_lookup(ab_report.DEFAULT_HISTOGRAM_FILE)
    self.assertEqual(
        tier_of('Cobalt.Startup.Time.NavigationDispatchToCommit'), 'P0')
    self.assertEqual(
        tier_of('Sql.Database.Success.OpenInternalTime.DIPS'), 'P1')
    self.assertEqual(
        tier_of('Cobalt.Storage.ProcessIo.StartupMajorPageFaults'), 'P2')
    rows = [
        {
            'phase': 'homefeed',
            'name': 'Memory.Total.ResidentSet',
            'stat': 'mean',
            'tier': 'P2',
            'primary': True,
            'n': 6,
            'a_values': [380] * 6,
            'b_values': [380] * 6,
            'diffs': [-1] * 6,
            'median': -1,
            'ci': (-1, -1),
            'rel': -0.003,
            'tag': '[NOISE]',
            'tag_reason': 'effect < 1%',
        },
        {
            'phase': 'homefeed',
            'name': 'Cobalt.Storage.Cookie.CommitDuration',
            'stat': 'count',
            'tier': 'P1',
            'primary': False,
            'n': 6,
            'a_values': [8] * 6,
            'b_values': [7] * 6,
            'diffs': [-1, 0, -1, 0, -1, 0],
            'median': -0.5,
            'ci': (-1, 0),
            'rel': -0.0625,
            'tag': '[NOISE]',
            'tag_reason': 'CI includes 0',
        },
        {
            'phase': 'homefeed',
            'name': 'Sql.Database.Success.OpenInternalTime.DIPS',
            'stat': 'count',
            'tier': 'P1',
            'primary': True,
            'n': 6,
            'a_values': [1] * 6,
            'b_values': [0] * 6,
            'diffs': [-1] * 6,
            'median': -1,
            'ci': (-1, -1),
            'rel': -1.0,
            'tag': '[IMPROVED]',
            'tag_reason': '',
        },
        {
            'phase': 'homefeed',
            'name': 'Cookie.TimeBlockedOnLoad',
            'stat': 'value',
            'tier': 'P0',
            'primary': False,
            'n': 6,
            'a_values': [20] * 6,
            'b_values': [15] * 6,
            'diffs': [-5] * 6,
            'median': -5,
            'ci': (-5, -5),
            'rel': -0.25,
            'tag': '[IMPROVED]',
            'tag_reason': '',
        },
    ]
    phase_lines = ab_report.render_phase(rows, 'homefeed', ('Control', 'Treat'),
                                         False)
    table_rows = [l for l in phase_lines if l.startswith('| ') and '`' in l]
    self.assertEqual([
        'Cookie.TimeBlockedOnLoad',
        'Sql.Database.Success.OpenInternalTime.DIPS',
        'Cobalt.Storage.Cookie.CommitDuration',
        'Memory.Total.ResidentSet',
    ], [l.split('`')[1] for l in table_rows])
    summary = '\n'.join(ab_report.render_ranked_summary({'homefeed': rows}))
    self.assertIn('### Ranked verdict summary', summary)
    self.assertIn('`Cookie.TimeBlockedOnLoad`', summary)
    self.assertIn('★ `Sql.Database.Success.OpenInternalTime.DIPS`', summary)


if __name__ == '__main__':
  unittest.main()
