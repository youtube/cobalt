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
"""Fixtures for Cobalt host E2E tests."""

import asyncio
import os
import subprocess
import sys
import time

import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', '..'))
from cobalt.tools import cdp_js_helper  # pylint: disable=wrong-import-position

PORT = 9222  # Cobalt's default --remote-debugging-port in non-release builds.


def pytest_addoption(parser):
  parser.addoption('--cobalt')
  parser.addoption('--chromedriver')
  # Passed by CI to all host JUnit-lane tests; XML comes via PYTEST_ADDOPTS.
  parser.addoption('--json-results-file')


@pytest.fixture
def launch_cobalt(pytestconfig):
  """Yields launch(*args), which starts Cobalt and waits for DevTools."""
  procs = []

  def launch(*args):
    # pylint: disable-next=consider-using-with
    procs.append(subprocess.Popen([pytestconfig.getoption('cobalt'), *args]))
    ready = asyncio.run(cdp_js_helper.wait_for_cdp('127.0.0.1', PORT, 0.5, 30))
    assert ready == 'SUCCESS'
    return procs[-1]

  yield launch
  for proc in procs:
    proc.kill()
    proc.wait()


@pytest.fixture
def wait_for_visibility():
  """Returns wait(state), which polls document.visibilityState via CDP."""

  def wait(state, timeout=30):
    deadline = time.monotonic() + timeout
    while (actual := asyncio.run(
        cdp_js_helper.evaluate('document.visibilityState', '127.0.0.1',
                               PORT))) != state:
      assert time.monotonic() < deadline, f'Got {actual}, want {state}'
      time.sleep(0.5)

  return wait
