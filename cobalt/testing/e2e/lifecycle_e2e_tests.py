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
"""Lifecycle (preload / focus / visibility) E2E tests for Cobalt on Linux."""

import asyncio
import os
import signal
import subprocess
import sys
import time
import unittest

import gtest_main

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'tools'))
import cdp_js_helper  # pylint: disable=wrong-import-position

PORT = 9223


class LifecycleTest(unittest.TestCase):

  def start(self, *flags):
    self.cobalt = subprocess.Popen([
        gtest_main.flag('cobalt', 'out/linux-x64x11_devel/cobalt'),
        f'--remote-debugging-port={PORT}', '--no-sandbox', *flags
    ])
    self.addCleanup(self.cobalt.kill)

  def wait_for(self, expr, expected, timeout=30):
    deadline = time.time() + timeout
    while (value := asyncio.run(cdp_js_helper.evaluate(expr, 'localhost', PORT))
          ) != expected and time.time() < deadline:
      time.sleep(0.5)
    self.assertEqual(value, expected, expr)

  def test_preload_reveal_stop(self):
    self.start('--preload')
    self.wait_for('document.visibilityState', 'hidden')
    self.wait_for('document.hasFocus()', 'False')
    self.cobalt.send_signal(signal.SIGCONT)  # Reveal + focus.
    self.wait_for('document.visibilityState', 'visible')
    self.wait_for('document.hasFocus()', 'True')
    self.cobalt.send_signal(signal.SIGPWR)  # Stop.
    self.assertIn(self.cobalt.wait(timeout=30), (0, -signal.SIGTERM))

  def test_blur_focus_conceal(self):
    self.start()
    self.wait_for('document.visibilityState', 'visible')
    self.wait_for('document.hasFocus()', 'True')
    self.cobalt.send_signal(signal.SIGWINCH)  # Blur.
    self.wait_for('document.hasFocus()', 'False')
    self.cobalt.send_signal(signal.SIGCONT)  # Focus.
    self.wait_for('document.hasFocus()', 'True')
    self.cobalt.send_signal(signal.SIGUSR1)  # Conceal.
    self.wait_for('document.visibilityState', 'hidden')


if __name__ == '__main__':
  gtest_main.main()
