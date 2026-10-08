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
"""Attach-mode Linux E2E test runner (Chromedriver & CDP lifecycle)."""

import argparse
import asyncio
import contextlib
import fnmatch
import os
import signal
import subprocess
import sys
import time
import xml.etree.ElementTree as ET
from selenium import webdriver
from selenium.webdriver.chrome.options import Options

sys.path.insert(0, os.path.abspath(os.path.dirname(__file__)))
import cdp_js_helper

CANDIDATE_BINS = (
    'out/linux-x64x11_devel/cobalt',
    'out/linux-x64x11-modular_devel/cobalt_loader',
    'out/evergreen-x64_devel/cobalt_loader',
)


@contextlib.contextmanager
def launch_cobalt(binary, port, *extra_args):
  assert os.path.exists(binary), f'Cobalt binary not found: {binary}'
  cmd = [binary, f'--remote-debugging-port={port}', '--no-sandbox', *extra_args]
  proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
  try:
    yield proc
  finally:
    proc.terminate()
    try:
      proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
      proc.kill()
      proc.wait()


def test_chromedriver_smoke_attach(binary, port):
  with launch_cobalt(binary, port):
    opts = Options()
    opts.debugger_address = f'127.0.0.1:{port}'
    driver = webdriver.Chrome(options=opts)
    try:
      driver.get('https://www.youtube.com/tv')
      assert driver.title is not None
    finally:
      driver.quit()


def test_linux_lifecycle_and_preload(binary, port):
  with launch_cobalt(binary, port, '--preload') as proc:
    time.sleep(2)
    assert asyncio.run(cdp_js_helper.evaluate('document.visibilityState', 'localhost', port)) == 'hidden'
    proc.send_signal(signal.SIGCONT)
    time.sleep(2)
    assert asyncio.run(cdp_js_helper.evaluate('document.visibilityState', 'localhost', port)) == 'visible'
    proc.send_signal(signal.SIGPWR)
    time.sleep(1)


TESTS = [
    ('test_chromedriver_smoke_attach', test_chromedriver_smoke_attach),
    ('test_linux_lifecycle_and_preload', test_linux_lifecycle_and_preload),
]


def main():
  p = argparse.ArgumentParser()
  p.add_argument('--cobalt-bin', default=next((b for b in CANDIDATE_BINS if os.path.exists(b)), CANDIDATE_BINS[0]))
  p.add_argument('--port', type=int, default=9222)
  p.add_argument('--junit-xml', default=None)
  p.add_argument('--gtest_output', default='')
  p.add_argument('--gtest_filter', default='*')
  p.add_argument('--gtest_shard_index', type=int, default=0)
  p.add_argument('--gtest_total_shards', type=int, default=1)
  args, _ = p.parse_known_args()

  xml_out = args.junit_xml or args.gtest_output.removeprefix('xml:')
  selected = [
      (name, fn) for i, (name, fn) in enumerate(TESTS)
      if i % args.gtest_total_shards == args.gtest_shard_index
      and fnmatch.fnmatch(f'LinuxE2ETests.{name}', args.gtest_filter.replace(':', '*'))
  ]

  suite = ET.Element('testsuite', name='LinuxE2ETests', tests=str(len(selected)))
  failures = 0
  for name, fn in selected:
    t0 = time.time()
    tc = ET.SubElement(suite, 'testcase', name=name, classname='LinuxE2ETests')
    try:
      fn(args.cobalt_bin, args.port)
    except Exception as e:
      failures += 1
      ET.SubElement(tc, 'failure', message=str(e)).text = str(e)
    tc.set('time', f'{time.time() - t0:.3f}')

  suite.set('failures', str(failures))
  if xml_out:
    os.makedirs(os.path.dirname(os.path.abspath(xml_out)), exist_ok=True)
    root = ET.Element('testsuites', suite.attrib)
    root.append(suite)
    ET.ElementTree(root).write(xml_out, encoding='utf-8', xml_declaration=True)
  sys.exit(bool(failures))


if __name__ == '__main__':
  main()
