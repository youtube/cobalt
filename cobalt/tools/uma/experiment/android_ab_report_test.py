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

import json
import os
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

  def test_stat_value(self):
    h = _hist('X', 4, 10)
    self.assertEqual(ab_report.stat_value(h, 'count'), 4)
    self.assertEqual(ab_report.stat_value(h, 'sum'), 10)
    self.assertEqual(ab_report.stat_value(h, 'mean'), 2.5)
    self.assertEqual(ab_report.stat_value(None, 'count'), 0)
    self.assertIsNone(ab_report.stat_value(None, 'value'))

  def test_bootstrap_ci(self):
    boot = ab_report.Bootstrap(2000, 1)
    self.assertEqual(boot.median_ci([-5, -5, -5]), (-5, -5))
    low, high = boot.median_ci([-3, 1, 2, 4, -1, 0])
    self.assertLessEqual(low, 0)
    self.assertGreaterEqual(high, 0)
    self.assertEqual(
        ab_report.Bootstrap(2000, 1).median_ci([-3, 1, 2, 4, -1, 0]),
        (low, high))

  def _row(self, diffs, ci, *, adj_ci=None, rel=-0.5, pol=-1, a=None, b=None):
    a = a if a is not None else [10] * len(diffs)
    b = b if b is not None else [x + d for x, d in zip(a, diffs)]
    return {
        'diffs': diffs,
        'ci': ci,
        'adj_ci': adj_ci,
        'rel': rel,
        'polarity': pol,
        'a_values': a,
        'b_values': b,
        'median': sorted(diffs)[len(diffs) // 2] if diffs else None
    }

  def test_tag_rules(self):
    tag = ab_report.tag_row
    five_down = [-5, -4, -5, -6, -5]
    self.assertEqual(tag(self._row([0, 0], (0, 0)), 0.01)[0], '[UNCHANGED]')
    self.assertEqual(
        tag(self._row([-5, -5], (-5, -5)), 0.01), ('[NOISE]', 'n < 3'))
    self.assertEqual(tag(self._row([-1, 2, 1], (-1, 2)), 0.01)[0], '[NOISE]')
    self.assertEqual(
        tag(self._row(five_down, (-6, -4), adj_ci=(-6, 1)), 0.01),
        ('[NOISE]', 'within No-Op spread'))
    self.assertEqual(
        tag(self._row(five_down, (-6, -4), rel=-0.001), 0.01)[0], '[NOISE]')
    self.assertEqual(
        tag(self._row(five_down, (-6, -4), pol=0), 0.01)[0], '[CHANGED]')
    self.assertEqual(tag(self._row(five_down, (-6, -4)), 0.01)[0], '[IMPROVED]')
    self.assertEqual(
        tag(self._row([5, 4, 5, 6, 5], (4, 6), rel=0.5), 0.01)[0],
        '[REGRESSED]')
    self.assertEqual(
        tag(self._row([-2, -2, -2], (-2, -2), a=[10, 20, 30]), 0.01)[1],
        'arm ranges overlap (n < 5)')
    self.assertEqual(
        tag(
            self._row([11, 16, 21], (11, 21),
                      rel=0.4,
                      a=[68, 53, 53],
                      b=[79, 69, 74]), 0.01),
        ('[NOISE]', 'arm ranges overlap (n < 5)'))
    self.assertEqual(
        tag(
            self._row([-5, -6, -7], (-7, -5),
                      rel=-0.21,
                      a=[28, 28, 29],
                      b=[23, 22, 22]), 0.01)[0], '[IMPROVED]')
    self.assertEqual(
        tag(self._row([-5, -5, -5], (-5, -5), a=[6, 6, 6]), 0.01)[0],
        '[IMPROVED]')
    self.assertEqual(tag(self._row([], None), 0.01)[0], 'N/A')

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


class ReportEndToEndTest(unittest.TestCase):
  """Builds a homefeed + exit-flush report from synthetic S1/S2 snapshots."""

  def test_homefeed_and_exit_flush_report(self):
    with tempfile.TemporaryDirectory() as tmp:
      manifest = []
      for pair, order in ((1, 'AB'), (2, 'BA'), (3, 'AB')):
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
      with open(
          os.path.join(tmp, 'manifest.jsonl'), 'w', encoding='utf-8') as f:
        f.writelines(json.dumps(r) + '\n' for r in manifest)
      out = os.path.join(tmp, 'report.md')
      rows_path = os.path.join(tmp, 'rows.json')
      self.assertEqual(
          ab_report.main([
              '--results-dir', tmp, '--experiment', 'hf', '--out', out,
              '--json', rows_path, '--bootstrap', '500'
          ]), 0)
      with open(rows_path, encoding='utf-8') as f:
        rows = json.load(f)
      by_key = {(r['name'], r['stat']): r for r in rows['homefeed']}
      commit_count = by_key[('Cobalt.Storage.Cookie.CommitDuration', 'count')]
      self.assertEqual(commit_count['diffs'], [-5, -5, -5])
      self.assertEqual(commit_count['tag'], '[IMPROVED]')
      self.assertEqual(by_key[('LevelDBEnv.SyncResult', 'count')]['tag'],
                       '[UNCHANGED]')
      flush = {(r['name'], r['stat']): r for r in rows['exit_flush']}
      pause = flush[('Cobalt.Storage.OnPause.FlushDuration', 'value')]
      self.assertEqual(pause['diffs'], [1, 2, 3])
      self.assertEqual(pause['tag'], '[REGRESSED]')
      with open(out, encoding='utf-8') as f:
        self.assertIn('**[IMPROVED]**', f.read())


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


if __name__ == '__main__':
  unittest.main()
