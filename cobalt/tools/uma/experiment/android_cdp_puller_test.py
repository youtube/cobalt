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


if __name__ == '__main__':
  unittest.main()
