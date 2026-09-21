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
"""Unit tests for CobaltRunner exit crash and hang detection."""

import os
import sys
import tempfile
import time
import unittest
from unittest import mock

_E2E_DIR = os.path.dirname(os.path.abspath(__file__))
_SRC_DIR = os.path.abspath(os.path.join(_E2E_DIR, '..', '..', '..'))
if _SRC_DIR not in sys.path:
  sys.path.insert(0, _SRC_DIR)

# pylint: disable=wrong-import-position
from cobalt.tools.e2e.cobalt_runner import CobaltRunner


class CobaltRunnerTest(unittest.TestCase):
  """Tests CobaltRunner process exit validation."""

  def setUp(self):
    super().setUp()
    # pylint: disable=consider-using-with
    temp_dir = self.enterContext(tempfile.TemporaryDirectory())
    self.log_file = os.path.join(temp_dir, 'cobalt_test.log')

  @mock.patch.object(CobaltRunner, 'cleanup_existing')
  def test_clean_exit_succeeds(self, _):
    with CobaltRunner([sys.executable, '-c', 'import sys; sys.exit(0)'],
                      self.log_file) as runner:
      runner.launch([])
      self.assertEqual(runner.wait_for_exit(timeout=5.0), 0)

  @mock.patch.object(CobaltRunner, 'cleanup_existing')
  def test_hang_on_exit_raises_assertion_error(self, _):
    with CobaltRunner([sys.executable, '-c', 'import time; time.sleep(30)'],
                      self.log_file) as runner:
      runner.launch([])
      with self.assertRaisesRegex(AssertionError, 'Cobalt hung on exit'):
        runner.wait_for_exit(timeout=0.5)
      # Verify the hung process was killed.
      time.sleep(0.1)
      self.assertIsNotNone(runner.proc.poll())

  @mock.patch.object(CobaltRunner, 'cleanup_existing')
  def test_immediate_crash_before_wait_raises_assertion_error(self, _):
    with CobaltRunner([sys.executable, '-c', 'import sys; sys.exit(1)'],
                      self.log_file) as runner:
      runner.launch([])
      # Wait until the process has already exited before calling wait_for_exit.
      deadline = time.time() + 5.0
      while runner.proc.poll() is None and time.time() < deadline:
        time.sleep(0.05)
      self.assertIsNotNone(runner.proc.poll())
      with self.assertRaisesRegex(
          AssertionError,
          'Cobalt crashed or exited uncleanly with non-zero code'):
        runner.wait_for_exit(timeout=5.0)

  @mock.patch.object(CobaltRunner, 'cleanup_existing')
  def test_signal_crash_on_exit_raises_assertion_error(self, _):
    script = 'import os, signal, time; time.sleep(0.3); os.kill(os.getpid(), 6)'
    with CobaltRunner([sys.executable, '-c', script], self.log_file) as runner:
      runner.launch([])
      with self.assertRaisesRegex(
          AssertionError,
          'Cobalt crashed or exited uncleanly with non-zero code -6'):
        runner.wait_for_exit(timeout=5.0)

  @mock.patch.object(CobaltRunner, 'cleanup_existing')
  def test_crash_marker_in_log_raises_assertion_error(self, _):
    script = 'print("ERROR: AddressSanitizer: heap-use-after-free")'
    with CobaltRunner([sys.executable, '-c', script], self.log_file) as runner:
      runner.launch([])
      with self.assertRaisesRegex(AssertionError,
                                  'Crash marker "ERROR: AddressSanitizer:"'):
        runner.wait_for_exit(timeout=5.0)

  @mock.patch.object(CobaltRunner, 'cleanup_existing')
  def test_asan_options_does_not_mask_exit_code(self, _):
    script = 'import os, sys; print(os.environ.get("ASAN_OPTIONS", ""))'
    with mock.patch.dict(os.environ, {}, clear=False):
      os.environ.pop('ASAN_OPTIONS', None)
      with CobaltRunner([sys.executable, '-c', script],
                        self.log_file) as runner:
        runner.launch([])
        runner.wait_for_exit(timeout=5.0)
        logs = runner.get_log_content()
        self.assertIn('detect_leaks=0', logs)
        self.assertNotIn('exitcode=0', logs)


if __name__ == '__main__':
  unittest.main()
