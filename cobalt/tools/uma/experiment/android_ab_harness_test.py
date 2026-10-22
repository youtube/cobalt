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
"""Unit tests for the Storage Lab Verification Harness (Homefeed & WPR)."""

import argparse
import io
import json
import os
import sys
import tempfile
import unittest
from unittest.mock import MagicMock, patch

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
UMA_DIR = os.path.dirname(SCRIPT_DIR)
for p in (SCRIPT_DIR, UMA_DIR):
  if p not in sys.path:
    sys.path.insert(0, p)

import android_ab_harness as ab_harness  # pylint: disable=wrong-import-position
import android_ab_report as ab_report  # pylint: disable=wrong-import-position
import android_cdp_puller as cdp_puller  # pylint: disable=wrong-import-position
import android_uma_dump as uma_dump  # pylint: disable=wrong-import-position


def _make_dump_lines(num_histograms: int = 25) -> str:
  histograms = [{
      'name': f'Cobalt.Storage.Metric{i}',
      'count': 1,
      'sum': 10,
      'buckets': [{
          'low': 10,
          'high': 11,
          'count': 1
      }],
  } for i in range(num_histograms)]
  return uma_dump.format_line('Cobalt.Storage',
                              {'result': {
                                  'histograms': histograms
                              }}, '2026-10-22 00:00:00')


class HomeAndWprRecipeTest(unittest.TestCase):
  """Tests for Basic YouTube TV Home Cold-Start + Exit CUJ (homefeed & wpr)."""

  def test_parse_args_homefeed_defaults(self):
    args = ab_harness.parse_args([
        '--results-dir=/tmp/out',
        '--experiment=ws1_home',
        '--mode=homefeed',
        '--arm-a-flags=--disable-features=CobaltLocalStorageOptimization',
        '--arm-b-flags=--enable-features=CobaltLocalStorageOptimization',
    ])
    self.assertEqual(args.mode, 'homefeed')
    self.assertEqual(args.poll_interval_s, 35.0)
    self.assertEqual(args.pairs, 3)
    self.assertEqual(args.landing_gate_s, 14.0)
    self.assertTrue(args.no_dpad_nav)
    self.assertTrue(args.exit_flush)
    self.assertTrue(args.pull_script.endswith('android_cdp_puller.py'))

  def test_pull_command_homefeed_and_warmup_passthrough(self):
    args = ab_harness.parse_args([
        '--results-dir=/tmp/out',
        '--experiment=ws3b_home',
        '--mode=homefeed',
        '--poll-interval-s=20',
        '--serial=localhost:40501',
        '--arm-a-flags=--disable-features=CobaltGpuShaderDiskCache',
        '--arm-b-flags=--enable-features=CobaltGpuShaderDiskCache',
        '--arm-a-warmup-runs=1',
        '--arm-b-warmup-runs=1',
        '--warmup-duration-s=25',
    ])
    harness = ab_harness.Harness(args, MagicMock())
    self.assertEqual(harness._warmup_runs_for_arm('ab', 'A'), 1)  # pylint: disable=protected-access
    self.assertEqual(harness._warmup_runs_for_arm('ab', 'B'), 1)  # pylint: disable=protected-access

    cmd_b = harness._pull_command(  # pylint: disable=protected-access
        args.arm_b_flags,
        '/tmp/out/b.txt',
        'homefeed',
        args.poll_interval_s,
        warmup_runs=harness._warmup_runs_for_arm('ab', 'B'))  # pylint: disable=protected-access
    self.assertIn('--mode=homefeed', cmd_b)
    self.assertIn('--poll-interval-s=20.0', cmd_b)
    self.assertIn('--no-dpad-nav', cmd_b)
    self.assertIn('--nav-end-s=0', cmd_b)
    self.assertIn('--exit-flush', cmd_b)
    self.assertIn('--warmup-runs=1', cmd_b)
    self.assertIn('--warmup-duration-s=25.0', cmd_b)

  def test_parse_args_and_pull_command_wpr_mode(self):
    args = ab_harness.parse_args([
        '--results-dir=/tmp/out',
        '--experiment=ws5_wpr',
        '--mode=wpr',
        '--wpr-archive=/tmp/yt_home.wprgo',
    ])
    self.assertEqual(args.mode, 'wpr')
    self.assertEqual(args.network, 'replay')
    self.assertEqual(args.poll_interval_s, 20.0)
    self.assertEqual(args.pairs, 3)
    self.assertTrue(args.no_dpad_nav)
    self.assertEqual(args.nav_end_s, 0.0)
    self.assertEqual(args.landing_gate_s, 0.0)
    self.assertTrue(args.exit_flush)

    harness = ab_harness.Harness(args, MagicMock())
    cmd = harness._pull_command(  # pylint: disable=protected-access
        '--enable-features=CobaltCodeCacheAndHttpCacheOptimization',
        '/tmp/out/wpr.txt',
        'wpr',
        args.poll_interval_s,
    )
    self.assertIn('--mode=wpr', cmd)
    self.assertIn('--network=replay', cmd)
    self.assertIn('--poll-interval-s=20.0', cmd)
    self.assertIn('--no-dpad-nav', cmd)
    self.assertIn('--nav-end-s=0', cmd)
    self.assertIn('--wpr-archive=/tmp/yt_home.wprgo', cmd)
    self.assertIn('--exit-flush', cmd)

  def test_normalize_extra_args_preserves_wpr_commas(self):
    extra = cdp_puller.build_wpr_extra_args(
        '--enable-features=FeatureA,FeatureB')
    normalized = cdp_puller._normalize_extra_args(extra)  # pylint: disable=protected-access
    self.assertIn(
        '--enable-features=FeatureA;FeatureB;'
        'CobaltMetricsInterval:memory-metrics-interval/10', normalized)
    self.assertIn(
        f'--host-resolver-rules={uma_dump.DEFAULT_WPR_HOST_RESOLVER_RULES}',
        normalized)
    spki = uma_dump.DEFAULT_WPR_SPKI_LIST
    self.assertIn(f'--ignore-certificate-errors-spki-list={spki}', normalized)

  @patch('android_cdp_puller.time.sleep')
  @patch('android_cdp_puller.run_adb_command')
  def test_no_dpad_nav_suppresses_scrolling(self, mock_run_adb, mock_sleep):  # pylint: disable=unused-argument
    mock_run_adb.return_value = ('', '')
    cdp_puller._wait_with_android_navigation(  # pylint: disable=protected-access
        25.0, quiet=True, no_dpad_nav=True)
    mock_run_adb.assert_not_called()

  @patch('android_cdp_puller.run_adb_command')
  @patch('android_cdp_puller.socket.create_connection')
  @patch('android_cdp_puller.subprocess.Popen')
  def test_wpr_server_record_and_replay_lifecycle(self, mock_popen, mock_sock,
                                                  mock_adb):
    proc = MagicMock()
    proc.poll.return_value = None
    mock_popen.return_value = proc
    mock_sock.return_value.__enter__.return_value = MagicMock()
    mock_adb.return_value = ('', '')

    with tempfile.TemporaryDirectory() as tmp:
      wpr_dir = os.path.join(tmp, 'wpr_go')
      os.makedirs(os.path.join(wpr_dir, 'src'), exist_ok=True)
      for fname in ('wpr_cert.pem', 'wpr_key.pem', 'ecdsa_cert.pem',
                    'ecdsa_key.pem'):
        with open(os.path.join(wpr_dir, fname), 'w', encoding='utf-8') as f:
          f.write('test')
      wpr_go_src = os.path.join(wpr_dir, 'src', 'wpr.go')
      with open(wpr_go_src, 'w', encoding='utf-8') as f:
        f.write('package main')

      cert_arg, key_arg = cdp_puller.locate_wpr_cert_and_key(wpr_dir)
      self.assertIn('wpr_cert.pem', cert_arg)
      self.assertNotIn('ecdsa_cert.pem', cert_arg)
      cmd_prefix, cwd = cdp_puller.locate_wpr_go_command(
          wpr_dir=wpr_dir, telemetry_bin_dir=os.path.join(tmp, 'no_bin'))
      self.assertEqual(cmd_prefix, ['go', 'run', wpr_go_src])
      self.assertEqual(cwd, wpr_dir)

      archive = os.path.join(tmp, 'cold_start.wprgo')
      with open(archive, 'wb') as f:
        f.write(b'wpr-data')
      with patch(
          'android_cdp_puller.locate_wpr_cert_and_key',
          return_value=(cert_arg, key_arg)), patch(
              'android_cdp_puller.locate_wpr_go_command',
              return_value=(cmd_prefix, cwd)):
        with cdp_puller.WprServer(
            mode='replay',
            archive_path=archive,
            http_port=8080,
            https_port=8081,
            serial='localhost:40501',
            quiet=True):
          pass
    popen_cmd = mock_popen.call_args.args[0]
    self.assertIn('replay', popen_cmd)
    self.assertIn('--http_port=8080', popen_cmd)
    self.assertIn('--https_port=8081', popen_cmd)
    proc.send_signal.assert_called_once()


class CdpPullerLandingGateAndExitFlushTest(unittest.TestCase):
  """Tests for android_cdp_puller.py URL params, landing gate, and S1/S2."""

  @patch('android_cdp_puller.run_adb_command')
  def test_launch_cobalt_url_params_and_app_url(self, mock_run_adb):
    mock_run_adb.return_value = ('', '')
    self.assertTrue(
        cdp_puller.launch_cobalt(
            'com.google.android.youtube.tv',
            'dev.cobalt.app.MainActivity',
            'http://test.url',
            quiet=True,
            url_params='force_startup_screen=none, hl=en,gl=US',
            app_url='https://www.youtube.com/tv?a=1,2'))
    cmd = mock_run_adb.call_args[0][0]
    self.assertIn("--esa url_params 'force_startup_screen=none,hl=en,gl=US'",
                  cmd)
    self.assertIn(',--url=https://www.youtube.com/tv?a=1%2C2\'', cmd)

  def test_landing_gate_passes_and_fails_on_picker(self):
    session = MagicMock()
    session.evaluate.return_value = json.dumps({
        'href': 'https://www.youtube.com/tv?hl=en#/',
        'title': 'YouTube',
        'text': 'Recommended a@b.com Trending',
    })
    focus = 'mCurrentFocus=Window{1 u0 com.google.android.youtube.tv/x}'
    with patch(
        'android_cdp_puller._find_page_websocket_url',
        return_value='ws://p'), patch(
            'android_cdp_puller._CdpSession', return_value=session), patch(
                'android_cdp_puller.run_adb_command',
                return_value=(focus,
                              None)), patch('android_cdp_puller.time.sleep'):
      ok_res = cdp_puller.probe_landing_via_cdp(
          'android', 9222, 'com.google.android.youtube.tv', 'Watch as guest')
      self.assertTrue(ok_res['ok'])
      self.assertEqual(ok_res['snippet'], 'Recommended <email> Trending')

      session.evaluate.return_value = json.dumps({
          'href': 'https://www.youtube.com/tv',
          'title': 'YouTube',
          'text': "Who's watching? Watch as guest",
      })
      fail_res = cdp_puller.probe_landing_via_cdp(
          'android', 9222, 'com.google.android.youtube.tv', 'Watch as guest')
      self.assertFalse(fail_res['ok'])
      self.assertTrue(fail_res['fail_text_found'])

  def test_capture_session_and_exit_flush_writes_s1_and_s2(self):
    session = MagicMock()
    session.call.return_value = {
        'result': {
            'histograms': [{
                'name': 'Cobalt.Storage.Cookie.CommitDuration',
                'count': 2,
                'sum': 20,
                'buckets': [{
                    'low': 10,
                    'high': 11,
                    'count': 2
                }],
            }]
        }
    }
    session.evaluate.return_value = 'h5vcc.system.exit'
    with tempfile.TemporaryDirectory() as tmp:
      out_path = os.path.join(tmp, 'run.txt')
      args = argparse.Namespace(output_file=out_path, quiet=True)
      out_buf = io.StringIO()
      with patch('android_cdp_puller._CdpSession', return_value=session), \
           patch('android_cdp_puller.time.sleep'):
        cdp_puller._capture_session_and_exit_flush(  # pylint: disable=protected-access
            'ws://page', ['Cobalt.Storage.Cookie.CommitDuration'], args,
            out_buf)
      self.assertTrue(os.path.exists(os.path.join(tmp, 'run.s1.txt')))
      self.assertTrue(os.path.exists(os.path.join(tmp, 'run.s2.txt')))
      _, annotations = uma_dump.parse_dump_lines(
          out_buf.getvalue().splitlines())
      self.assertEqual(annotations[uma_dump.EXIT_FLUSH_KEY]['method'],
                       'h5vcc.system.exit')


class DeviceKeepAwakeTest(unittest.TestCase):
  """Tests for sabrina keep-awake enforcement in DeviceSession."""

  def test_session_env_runs_all_keep_awake_commands(self):
    device = MagicMock()
    device.shell.return_value = '1'
    with ab_harness.DeviceSession(
        device, keep_awake=True, disable_mediashell=False):
      pass
    shell_cmds = [c.args[0] for c in device.shell.call_args_list]
    for expected in ab_harness.KEEP_AWAKE_COMMANDS:
      self.assertIn(expected, shell_cmds)


class EnumBucketAndExitFlushTest(unittest.TestCase):
  """Tests per-bucket enum reporting and --exit-flush in homefeed/wpr modes."""

  def test_enum_bucket_reporting_and_homefeed_exit_flush(self):
    with tempfile.TemporaryDirectory() as tmp:
      manifest = []
      for pair, order in ((1, 'AB'), (2, 'BA'), (3, 'AB')):
        for slot, arm in enumerate(order):
          base = f'home_ab_p{pair:02d}_{arm}'
          v8_buckets = ([{
              'low': 0,
              'high': 1,
              'count': 4
          }] if arm == 'A' else [{
              'low': 2,
              'high': 3,
              'count': 4
          }])
          s1_line = uma_dump.format_line(
              'V8', {
                  'result': {
                      'histograms': [
                          {
                              'name': 'V8.CompileScript.CacheBehaviour',
                              'count': 4,
                              'sum': 0 if arm == 'A' else 8,
                              'buckets': v8_buckets,
                          },
                          {
                              'name':
                                  'LevelDBEnv.SyncResult',
                              'count':
                                  10 if arm == 'A' else 0,
                              'sum':
                                  0,
                              'buckets': [{
                                  'low': 0,
                                  'high': 1,
                                  'count': 10 if arm == 'A' else 0
                              }],
                          },
                      ]
                  }
              })
          s2_line = uma_dump.format_line(
              'V8', {
                  'result': {
                      'histograms': [
                          {
                              'name': 'V8.CompileScript.CacheBehaviour',
                              'count': 4,
                              'sum': 0 if arm == 'A' else 8,
                              'buckets': v8_buckets,
                          },
                          {
                              'name':
                                  'LevelDBEnv.SyncResult',
                              'count':
                                  11 if arm == 'A' else 1,
                              'sum':
                                  0,
                              'buckets': [{
                                  'low': 0,
                                  'high': 1,
                                  'count': 11 if arm == 'A' else 1
                              }],
                          },
                      ]
                  }
              })
          for suffix, content in (('.txt', s1_line), ('.s1.txt', s1_line),
                                  ('.s2.txt', s2_line)):
            with open(
                os.path.join(tmp, base + suffix), 'w', encoding='utf-8') as f:
              f.write(content)
          manifest.append({
              'type': 'run',
              'experiment': 'home',
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
              'dump': f'{base}.txt',
          })
        manifest.append({
            'type': 'pair',
            'experiment': 'home',
            'group': 'ab',
            'pair': pair,
            'order': order,
        })
      with open(
          os.path.join(tmp, 'manifest.jsonl'), 'w', encoding='utf-8') as f:
        f.writelines(json.dumps(r) + '\n' for r in manifest)
      rows_path = os.path.join(tmp, 'rows.json')
      rc = ab_report.main([
          '--results-dir', tmp, '--experiment', 'home', '--json', rows_path,
          '--bootstrap', '200'
      ])
      self.assertEqual(rc, 0)
      with open(rows_path, encoding='utf-8') as f:
        phase_rows = json.load(f)
      self.assertIn('homefeed', phase_rows)
      self.assertIn('exit_flush', phase_rows)
      hf = {(r['name'], r['stat']): r for r in phase_rows['homefeed']}
      self.assertEqual(hf[('V8.CompileScript.CacheBehaviour', 'count')]['tag'],
                       '[UNCHANGED]')
      self.assertEqual(
          hf[('V8.CompileScript.CacheBehaviour', 'ProduceCodeCache')]['tag'],
          '[IMPROVED]')
      self.assertEqual(
          hf[('V8.CompileScript.CacheBehaviour', 'ConsumeCodeCache')]['tag'],
          '[IMPROVED]')
      ef = {(r['name'], r['stat']): r for r in phase_rows['exit_flush']}
      self.assertEqual(ef[('LevelDBEnv.SyncResult', 'count')]['tag'],
                       '[UNCHANGED]')


if __name__ == '__main__':
  unittest.main()
