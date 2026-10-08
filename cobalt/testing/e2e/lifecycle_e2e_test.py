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
"""Host E2E tests for Cobalt preload and Starboard lifecycle signals."""

import signal
import sys

import pytest


def test_preload_then_focus(launch_cobalt, wait_for_visibility):
  proc = launch_cobalt('--preload')
  wait_for_visibility('hidden')
  proc.send_signal(signal.SIGCONT)  # Starboard: Focus.
  wait_for_visibility('visible')


def test_conceal_focus_terminate(launch_cobalt, wait_for_visibility):
  proc = launch_cobalt()
  wait_for_visibility('visible')
  proc.send_signal(signal.SIGUSR1)  # Starboard: Conceal.
  wait_for_visibility('hidden')
  proc.send_signal(signal.SIGCONT)
  wait_for_visibility('visible')
  proc.send_signal(signal.SIGTERM)
  assert proc.wait(timeout=10) in (0, -signal.SIGTERM)


if __name__ == '__main__':
  sys.exit(pytest.main([__file__, '-v', *sys.argv[1:]]))
