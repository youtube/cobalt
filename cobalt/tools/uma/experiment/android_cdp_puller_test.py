#!/usr/bin/env vpython3
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
"""Unit tests for cobalt/tools/uma/experiment/android_cdp_puller.py."""

import contextlib
import io
import json
import os
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
# pylint: disable=wrong-import-position,protected-access
import android_cdp_puller as cdp_puller


class CdpPullerTest(unittest.TestCase):

  def test_normalize_extra_args_appends_memory_feature(self):
    normalized = cdp_puller._normalize_extra_args(
        '--enable-features=CobaltLocalStorageOptimization')
    self.assertEqual(
        normalized,
        [
            '--enable-features=CobaltLocalStorageOptimization;'
            'CobaltMetricsInterval:memory-metrics-interval/10'
        ],
    )

  def test_launch_cobalt_logs_home_launch_command(self):
    self.assertEqual(cdp_puller.TARGET_URL, 'https://www.youtube.com/tv')
    for package, url_arg in (
        ('com.google.android.youtube.tv', "-d 'https://www.youtube.com/tv'"),
        (cdp_puller.DEFAULT_COBALT_PACKAGE_NAME,
         "--url=https://www.youtube.com/tv'"),
    ):
      out = io.StringIO()
      with mock.patch.object(
          cdp_puller, 'run_adb_command',
          return_value=('', None)) as run_adb, contextlib.redirect_stdout(out):
        self.assertTrue(
            cdp_puller.launch_cobalt(
                package,
                cdp_puller.DEFAULT_COBALT_ACTIVITY_NAME,
                cdp_puller.TARGET_URL,
                quiet=False,
                url_params='force_startup_screen=none'))
      command = run_adb.call_args[0][0]
      self.assertIn(f'Launch command: {command}\n', out.getvalue())
      self.assertIn(url_arg, command)
      self.assertIn('--remote-allow-origins=*', command)
      self.assertIn(
          '--enable-features=CobaltMetricsInterval:memory-metrics-interval/10',
          command)
      self.assertNotIn('#/watch', command)

  def test_build_wpr_extra_args_adds_host_resolver_and_spki(self):
    args = cdp_puller.build_wpr_extra_args(
        '--enable-features=CobaltLocalStorageOptimization')
    self.assertIn('--host-resolver-rules=MAP * 127.0.0.1:8081', args)
    self.assertIn('--ignore-certificate-errors-spki-list=', args)

  def test_locate_wpr_cert_and_key_single_rsa(self):
    with tempfile.TemporaryDirectory() as tmp:
      rsa_cert = os.path.join(tmp, 'wpr_cert.pem')
      rsa_key = os.path.join(tmp, 'wpr_key.pem')
      with open(rsa_cert, 'w', encoding='utf-8') as f:
        f.write('cert')
      with open(rsa_key, 'w', encoding='utf-8') as f:
        f.write('key')
      cert, key = cdp_puller.locate_wpr_cert_and_key(tmp, include_ecdsa=False)
      self.assertEqual(cert, rsa_cert)
      self.assertEqual(key, rsa_key)

  def test_probe_landing_via_cdp_rejects_guest_picker(self):
    fake_session = mock.MagicMock()
    fake_session.evaluate.return_value = json.dumps({
        'href': 'https://www.youtube.com/tv',
        'title': 'YouTube',
        'text': 'Sign in\nWatch as guest',
    })
    with mock.patch.object(cdp_puller, 'run_adb_command'), mock.patch.object(
        cdp_puller,
        '_find_page_websocket_url',
        return_value='ws://127.0.0.1:9222/devtools/page/1'), mock.patch.object(
            cdp_puller, '_CdpSession',
            return_value=fake_session), mock.patch.object(
                cdp_puller,
                'is_package_in_foreground_android',
                return_value=True):
      result = cdp_puller.probe_landing_via_cdp(
          'android', 9222, 'com.google.android.youtube.tv', 'Watch as guest')
      self.assertFalse(result['ok'])
      self.assertIn('account picker', result['reason'])

  def test_probe_landing_via_cdp_passes_home_page(self):
    fake_session = mock.MagicMock()
    fake_session.evaluate.return_value = json.dumps({
        'href': 'https://www.youtube.com/tv#/',
        'title': 'YouTube',
        'text': 'Recommended\nHome\nSubscriptions',
    })
    with mock.patch.object(cdp_puller, 'run_adb_command'), mock.patch.object(
        cdp_puller,
        '_find_page_websocket_url',
        return_value='ws://127.0.0.1:9222/devtools/page/1'), mock.patch.object(
            cdp_puller, '_CdpSession',
            return_value=fake_session), mock.patch.object(
                cdp_puller,
                'is_package_in_foreground_android',
                return_value=True):
      result = cdp_puller.probe_landing_via_cdp(
          'android', 9222, 'com.google.android.youtube.tv', 'Watch as guest')
      self.assertTrue(result['ok'])
      self.assertEqual(result['reason'], '')

  def test_snapshot_histograms_filters_non_dot_boundary_substring_matches(self):
    fake_session = mock.MagicMock()
    fake_session.call.side_effect = [
        {
            'id': 1,
            'result': {
                'histograms': [
                    {
                        'name': 'Cobalt.Storage.Cookie.CommitDuration',
                        'count': 4,
                        'sum': 75,
                        'buckets': [],
                    },
                    {
                        'name':
                            ('Cobalt.Storage.Cookie.CommitDurationPerOperation'
                            ),
                        'count': 4,
                        'sum': 25,
                        'buckets': [],
                    },
                ]
            },
        },
        {
            'id': 2,
            'result': {
                'histograms': [
                    {
                        'name': 'HttpCache.Pattern',
                        'count': 55,
                        'sum': 0,
                        'buckets': [],
                    },
                    {
                        'name': 'HttpCache.Pattern.MainFrame',
                        'count': 1,
                        'sum': 0,
                        'buckets': [],
                    },
                    {
                        'name': 'HttpCache.PatternExtra',
                        'count': 3,
                        'sum': 0,
                        'buckets': [],
                    },
                ]
            },
        },
    ]
    lines, merged = cdp_puller._snapshot_histograms(
        fake_session,
        ['Cobalt.Storage.Cookie.CommitDuration', 'HttpCache.Pattern'])
    self.assertEqual(
        sorted(merged),
        [
            'Cobalt.Storage.Cookie.CommitDuration',
            'HttpCache.Pattern',
            'HttpCache.Pattern.MainFrame',
        ],
    )
    self.assertEqual(len(lines), 2)
    parsed, _ = cdp_puller.uma_dump.parse_dump_lines(lines)
    self.assertEqual(sorted(parsed), sorted(merged))

  def test_snapshot_histograms_reconnects_on_closed_websocket(self):
    first_session = mock.MagicMock()
    first_session.call.side_effect = RuntimeError(
        'Connection is already closed')
    second_session = mock.MagicMock()
    second_session.call.return_value = {
        'id': 1,
        'result': {
            'histograms': [{
                'name': 'LocalStorage.DatabaseOpen',
                'count': 1,
                'sum': 0,
                'buckets': [],
            }]
        },
    }
    reconnect = mock.MagicMock(return_value=second_session)
    lines, merged = cdp_puller._snapshot_histograms(
        first_session, ['LocalStorage.DatabaseOpen'], reconnect=reconnect)
    reconnect.assert_called_once_with(first_session)
    self.assertEqual(sorted(merged), ['LocalStorage.DatabaseOpen'])
    self.assertEqual(len(lines), 1)

  def test_capture_single_snapshot_strips_wildcard_and_reconnects(self):
    broken = mock.MagicMock()
    broken.call.side_effect = RuntimeError('socket closed')
    recovered = mock.MagicMock()
    recovered.call.return_value = {
        'id': 1,
        'result': {
            'histograms': [{
                'name': 'Sql.Database.Success.OpenInternalTime.DIPS',
                'count': 1,
                'sum': 12,
                'buckets': [{
                    'low': 10,
                    'high': 15,
                    'count': 1
                }],
            }]
        },
    }
    buf = io.StringIO()
    args = mock.MagicMock(
        device_id='tv-serial-1', platform='android', port=9222)
    with mock.patch.object(
        cdp_puller, '_CdpSession', return_value=broken), mock.patch.object(
            cdp_puller, '_reconnect_cdp_session',
            return_value=recovered) as reconnect:
      cdp_puller._capture_single_snapshot(
          'ws://127.0.0.1:9222/devtools/page/1',
          ['Sql.Database.Success.OpenInternalTime.*'], args, buf)
    reconnect.assert_called_once()
    recovered.call.assert_called_once_with(
        'Browser.getHistograms',
        {'query': 'Sql.Database.Success.OpenInternalTime.'})
    parsed, _ = cdp_puller.uma_dump.parse_dump_lines(
        buf.getvalue().splitlines())
    self.assertIn('Sql.Database.Success.OpenInternalTime.DIPS', parsed)

  def test_run_warmup_cycles_captures_launch_1_and_passes_device_serial(self):
    fake_session = mock.MagicMock()
    fake_session.call.return_value = {
        'id': 1,
        'result': {
            'histograms': [{
                'name': 'Cobalt.Storage.ProcessIo.StartupWriteKB',
                'count': 1,
                'sum': 5600,
                'buckets': [{
                    'low': 5000,
                    'high': 6000,
                    'count': 1
                }],
            }]
        },
    }
    with tempfile.TemporaryDirectory() as tmp:
      w1_path = os.path.join(tmp, 'slot.w1.txt')
      args = mock.MagicMock(
          warmup_runs=2,
          warmup_duration_s=1.0,
          warmup_output_file=w1_path,
          device_id='sabrina:5555',
          package_name='com.google.android.youtube.tv',
          platform='android',
          port=9222,
          no_dpad_nav=True,
          nav_end_s=0.0,
          quiet=True,
      )
      with mock.patch.dict(
          os.environ, {}, clear=False), mock.patch.object(
              cdp_puller, 'is_package_running_android',
              return_value=False), mock.patch.object(
                  cdp_puller,
                  '_start_and_wait_for_cobalt_android',
                  return_value=True), mock.patch.object(
                      cdp_puller,
                      '_wait_with_android_navigation'), mock.patch.object(
                          cdp_puller, 'stop_package'), mock.patch.object(
                              cdp_puller.time, 'sleep'), mock.patch.object(
                                  cdp_puller,
                                  'get_websocket_url',
                                  return_value='ws://127.0.0.1:9222/page'
                              ), mock.patch.object(
                                  cdp_puller,
                                  '_CdpSession',
                                  return_value=fake_session), mock.patch.object(
                                      cdp_puller,
                                      'run_adb_command',
                                      return_value=('', None)) as run_adb:
        os.environ.pop('ANDROID_SERIAL', None)
        cdp_puller._run_warmup_cycles(
            args, ['Cobalt.Storage.ProcessIo.StartupWriteKB'])
        self.assertEqual(os.environ.get('ANDROID_SERIAL'), 'sabrina:5555')
      home_calls = [
          c.args[0]
          for c in run_adb.call_args_list
          if 'KEYCODE_HOME' in c.args[0]
      ]
      self.assertEqual(home_calls, [[
          'adb', '-s', 'sabrina:5555', 'shell', 'input', 'keyevent',
          'KEYCODE_HOME'
      ]] * 2)
      # Only Warmup Run 1 (transitional first launch) is written to .w1.txt.
      self.assertEqual(fake_session.call.call_count, 1)
      parsed, _ = cdp_puller.uma_dump.parse_dump(w1_path)
      self.assertEqual(parsed['Cobalt.Storage.ProcessIo.StartupWriteKB']['sum'],
                       5600)


if __name__ == '__main__':
  unittest.main()
