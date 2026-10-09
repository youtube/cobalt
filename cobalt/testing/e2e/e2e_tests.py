#!/usr/bin/env python3
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
"""Host E2E tests (Lifecycle + Chromedriver) using pure Python stdlib and CDP."""

import argparse
import asyncio
import json
import os
import signal
import subprocess
import sys
import time
import unittest
import urllib.request

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '../../tools')))
import cdp_js_helper

_CANDIDATE_BINARIES = (
    'out/linux-x64x11_devel/cobalt',
    'out/linux-x64x11-modular_devel/cobalt_loader',
    'out/evergreen-x64_devel/cobalt_loader',
    'out/Default/cobalt',
)


class W3CWebDriver:
  """Minimal W3C WebDriver HTTP client over urllib.request."""

  def __init__(self, url='http://127.0.0.1:9515'):
    self.url, self.sid = url, None

  def _req(self, method, path, body=None):
    data = json.dumps(body).encode() if body is not None else None
    req = urllib.request.Request(
        f'{self.url}{path}',
        data=data,
        headers={'Content-Type': 'application/json'},
        method=method)
    with urllib.request.urlopen(req, timeout=30) as resp:
      return json.loads(resp.read() or b'{}').get('value')

  def new_session(self, capabilities):
    res = self._req('POST', '/session', {'capabilities': {'alwaysMatch': capabilities}})
    self.sid = res['sessionId']
    return self.sid

  def get(self, url):
    return self._req('POST', f'/session/{self.sid}/url', {'url': url})

  def get_title(self):
    return self._req('GET', f'/session/{self.sid}/title')

  def quit(self):
    if self.sid:
      try:
        self._req('DELETE', f'/session/{self.sid}')
      finally:
        self.sid = None


class CobaltProcess:
  """Manages Cobalt process execution and POSIX lifecycle signals."""

  def __init__(self, port=9222):
    self.binary = next(
        (b for b in _CANDIDATE_BINARIES if os.access(b, os.X_OK)),
        _CANDIDATE_BINARIES[0])
    self.port = port
    self.proc = None

  def start(self, preload=False):
    cmd = [self.binary, f'--remote-debugging-port={self.port}', '--no-sandbox']
    if preload:
      cmd.append('--preload')
    self.proc = subprocess.Popen(
        cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, preexec_fn=os.setsid)

  def signal(self, sig):
    if self.proc and self.proc.poll() is None:
      os.kill(self.proc.pid, sig)

  def stop(self):
    if self.proc and self.proc.poll() is None:
      self.proc.terminate()
      try:
        self.proc.wait(timeout=5)
      except subprocess.TimeoutExpired:
        self.proc.kill()
        self.proc.wait()
    self.proc = None


class HostE2ETest(unittest.TestCase):
  """Cobalt Linux E2E tests for lifecycle signals and W3C Chromedriver."""

  def setUp(self):
    self.proc = CobaltProcess(port=9222)
    if not os.access(self.proc.binary, os.X_OK):
      self.skipTest(f'Cobalt binary {self.proc.binary} not found.')

  def tearDown(self):
    self.proc.stop()

  def wait_js(self, expr, expected, timeout=15):
    deadline = time.time() + timeout
    while time.time() < deadline:
      if str(asyncio.run(cdp_js_helper.evaluate(expr, 'localhost', self.proc.port))) == str(expected):
        return True
      time.sleep(0.5)
    return False

  def test_preload_and_reveal_cycle(self):
    self.proc.start(preload=True)
    self.assertTrue(self.wait_js('document.visibilityState', 'hidden'))
    self.assertTrue(self.wait_js('document.hasFocus()', 'False'))

    self.proc.signal(signal.SIGCONT)
    self.assertTrue(self.wait_js('document.visibilityState', 'visible'))
    self.assertTrue(self.wait_js('document.hasFocus()', 'True'))

    self.proc.signal(signal.SIGPWR)
    self.proc.proc.wait(timeout=10)
    self.assertIsNotNone(self.proc.proc.poll())

  def test_lifecycle_signals(self):
    self.proc.start()
    self.assertTrue(self.wait_js('document.visibilityState', 'visible'))
    self.assertTrue(self.wait_js('document.hasFocus()', 'True'))

    self.proc.signal(signal.SIGWINCH)
    self.assertTrue(self.wait_js('document.hasFocus()', 'False'))

    self.proc.signal(signal.SIGCONT)
    self.assertTrue(self.wait_js('document.hasFocus()', 'True'))

    self.proc.signal(signal.SIGUSR1)
    self.assertTrue(self.wait_js('document.visibilityState', 'hidden'))

  def test_chromedriver_attach_and_navigate(self):
    self.proc.start()
    driver = W3CWebDriver()
    try:
      try:
        sid = driver.new_session(
            {'goog:chromeOptions': {'debuggerAddress': f'127.0.0.1:{self.proc.port}'}})
      except Exception as e:
        self.skipTest(f'Chromedriver not listening on 9515: {e}')
      self.assertIsNotNone(sid)
      driver.get('https://www.youtube.com/tv')
      self.assertIn('YouTube', driver.get_title())
    finally:
      driver.quit()


if __name__ == '__main__':
  parser = argparse.ArgumentParser()
  parser.add_argument('--json-results-file')
  args, _ = parser.parse_known_args()
  suite = unittest.defaultTestLoader.loadTestsFromModule(sys.modules[__name__])
  results, ok = [], True
  for group in suite:
    for test in group:
      t0, res = time.time(), unittest.TestResult()
      test.run(res)
      err = res.failures + res.errors
      ok &= not err
      results.append({
          'name': test._testMethodName,
          'suite_name': type(test).__name__,
          'result': 'FAILED' if err else 'PASSED',
          'duration': time.time() - t0,
          'output': err[0][1] if err else '',
      })
  if args.json_results_file:
    os.makedirs(os.path.dirname(os.path.abspath(args.json_results_file)), exist_ok=True)
    with open(args.json_results_file, 'w', encoding='utf-8') as f:
      json.dump({'tests': results}, f, indent=2)
  sys.exit(0 if ok else 1)
