#!/usr/bin/env vpython3
# Copyright 2026 The Cobalt Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""End-to-End integration tests for Cobalt Lifecycle & Preload on Linux.

Verifies Starboard lifecycle transitions controlled by POSIX signals (SIGCONT,
SIGWINCH, SIGUSR1, SIGTSTP, SIGPWR) and asserts that standard W3C/DOM lifecycle
events (focus, blur, visibilitychange, freeze, resume) are dispatched accurately
to the web application via DevTools (CDP).
"""

import argparse
import base64
import logging
import os
import shutil
import signal
import sys
import time

_TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
if _TOOLS_DIR not in sys.path:
  sys.path.insert(0, _TOOLS_DIR)

# pylint: disable=wrong-import-position
from test_common import CDPClient, CobaltRunner, create_lifecycle_controller, ensure_display, resolve_executable

logging.basicConfig(
    level=logging.INFO,
    format='[TEST] %(asctime)s - %(levelname)s - %(message)s',
    datefmt='%H:%M:%S',
)
logger = logging.getLogger('test_lifecycle_e2e')


def run_preload_test(executable, platform=None, host='localhost', port=9223):
  """Runs the Preload E2E integration test."""
  logger.info('========================================')
  logger.info('Starting PRELOAD Integration Test')
  logger.info('========================================')

  log_file = 'preload_run.log'
  runner = CobaltRunner(executable, log_file, port=port)
  controller = create_lifecycle_controller(platform, runner)
  cdp = CDPClient(host=host, port=port)

  test_html = (
      '<html><body><h1>Preload Test</h1><input autofocus></body></html>')
  test_url = 'data:text/html;base64,' + base64.b64encode(
      test_html.encode('utf-8')).decode('ascii')

  try:
    runner.launch([
        '--preload',
        f'--url={test_url}',
        f'--remote-debugging-port={port}',
        '--v=1',
    ])

    logger.info('Waiting for DevTools to become available...')
    cdp.wait_for_ready(total_wait=60.0)

    # Allow app initialization to stabilize
    logger.info('Allowing initial preloaded launch to stabilize...')
    time.sleep(3)

    logger.info('Verifying initial preloaded state (Hidden & Not Focused)...')
    cdp.wait_for_state('document.visibilityState', 'hidden', timeout=120.0)
    cdp.wait_for_state('document.hasFocus()', False, timeout=120.0)

    logger.info('Revealing application...')
    controller.resume()

    logger.info('Verifying revealed state (Visible & Focused)...')
    cdp.wait_for_state('document.visibilityState', 'visible', timeout=120.0)
    cdp.wait_for_state('document.hasFocus()', True, timeout=120.0)

    # Allow window surface and rendering to stabilize before stopping
    time.sleep(2)

    logger.info('Stopping Cobalt gracefully...')
    controller.stop()
    runner.wait_for_exit(timeout=15.0)

    logger.info('SUCCESS: Preload and reveal verified!')
    return True
  finally:
    if runner.proc and runner.proc.poll() is None:
      try:
        os.kill(runner.proc.pid, signal.SIGKILL)
      except ProcessLookupError:
        pass


def run_lifecycle_test(executable, platform=None, host='localhost', port=9223):
  """Runs the comprehensive Lifecycle Transitions E2E integration test."""
  logger.info('========================================')
  logger.info('Starting LIFECYCLE Integration Test')
  logger.info('========================================')

  log_file = 'lifecycle_run.log'
  runner = CobaltRunner(executable, log_file, port=port)
  controller = create_lifecycle_controller(platform, runner)
  cdp = CDPClient(host=host, port=port)

  # Clean user storage/caches to prevent database locks in CI
  if os.environ.get('CI') == 'true' or os.environ.get(
      'GITHUB_ACTIONS') == 'true':
    for storage_dir in [
        os.path.expanduser('~/.cobalt_storage'),
        os.path.expanduser('~/.cobalt'),
        os.path.expanduser('~/.config/cobalt'),
    ]:
      if os.path.exists(storage_dir):
        shutil.rmtree(storage_dir, ignore_errors=True)

  html_payload = '<html><body><h1>Test</h1><input autofocus></body></html>'
  b64_html = base64.b64encode(html_payload.encode('utf-8')).decode('utf-8')

  try:
    runner.launch([
        f'--url=data:text/html;base64,{b64_html}',
        f'--remote-debugging-port={port}',
    ])

    logger.info('Waiting for DevTools to become available...')
    cdp.wait_for_ready(total_wait=60.0)

    # Stabilize V8 / DOM layout
    logger.info('Allowing app initialization to stabilize...')
    time.sleep(5)

    logger.info('Verifying initial state (Visible & Focused)...')
    cdp.wait_for_state('document.visibilityState', 'visible', timeout=120.0)
    cdp.wait_for_state('document.hasFocus()', True, timeout=120.0)

    logger.info('Injecting in-page event logger...')
    injection_script = """
    window.event_log = [];
    document.addEventListener('visibilitychange', () => {
      console.log('JS_EVENT: visibilitychange ' + document.visibilityState);
      window.event_log.push({type: 'visibilitychange', visibility: document.visibilityState});
    });
    window.addEventListener('focus', () => {
      console.log('JS_EVENT: focus');
      window.event_log.push({type: 'focus'});
    });
    window.addEventListener('blur', () => {
      console.log('JS_EVENT: blur');
      window.event_log.push({type: 'blur'});
    });
    document.addEventListener('freeze', () => {
      console.log('JS_EVENT: freeze');
      window.event_log.push({type: 'freeze'});
    });
    document.addEventListener('resume', () => {
      console.log('JS_EVENT: resume');
      window.event_log.push({type: 'resume'});
    });
    window.focus();
    """
    cdp.evaluate(injection_script)

    # 1. Blur transition
    logger.info('Step 1: Blurring application...')
    controller.blur()
    cdp.wait_for_state('document.hasFocus()', False, timeout=10.0)
    cdp.pop_event({'type': 'blur'})

    # 2. Focus transition
    logger.info('Step 2: Focusing application...')
    controller.focus()
    cdp.wait_for_state('document.hasFocus()', True, timeout=10.0)
    cdp.pop_event({'type': 'focus'})

    # 3. Conceal transition
    logger.info('Step 3: Concealing application...')
    controller.conceal()
    cdp.wait_for_state('document.hasFocus()', False, timeout=10.0)
    cdp.wait_for_state('document.visibilityState', 'hidden', timeout=10.0)
    cdp.pop_event({'type': 'blur'})
    cdp.pop_event({'type': 'visibilitychange', 'visibility': 'hidden'})

    # 4. Freeze transition
    logger.info('Step 4: Freezing application...')
    controller.freeze()
    time.sleep(2)

    # 5. Resume & Reveal & Focus
    logger.info('Step 5: Resuming and revealing application...')
    controller.resume()
    cdp.wait_for_state('document.visibilityState', 'visible', timeout=20.0)
    cdp.wait_for_state('document.hasFocus()', True, timeout=20.0)

    # Pop remaining queued events: freeze, resume, visibilitychange (visible),
    # and focus.
    cdp.pop_event({'type': 'freeze'})
    cdp.pop_event({'type': 'resume'})

    ev3 = cdp.pop_event({})
    ev4 = cdp.pop_event({})
    remaining_types = {ev3.get('type'), ev4.get('type')}
    if remaining_types != {'visibilitychange', 'focus'}:
      raise AssertionError(
          f'Expected visibilitychange and focus, got: {remaining_types}')
    for ev in (ev3, ev4):
      if ev.get('type') == 'visibilitychange' and ev.get(
          'visibility') != 'visible':
        raise AssertionError(
            f'Expected visibilitychange visibility="visible", got: {ev}')

    # Cross-verify stdout/stderr console logs
    logger.info('Verifying console log events...')
    logs = runner.get_log_content()
    if 'JS_EVENT: freeze' not in logs:
      raise AssertionError('Missing "JS_EVENT: freeze" in stdout/stderr log')
    if 'JS_EVENT: resume' not in logs:
      raise AssertionError('Missing "JS_EVENT: resume" in stdout/stderr log')
    if 'JS_EVENT: visibilitychange visible' not in logs:
      raise AssertionError(
          'Missing "JS_EVENT: visibilitychange visible" in stdout/stderr log')
    focus_count = logs.count('JS_EVENT: focus')
    if focus_count < 2:
      raise AssertionError(
          f'Expected >= 2 occurrences of "JS_EVENT: focus", found {focus_count}'
      )

    # 6. Graceful Shutdown
    logger.info('Step 6: Stopping Cobalt gracefully...')
    controller.stop()
    runner.wait_for_exit(timeout=15.0)

    logger.info('SUCCESS: Lifecycle transitions verified!')
    return True
  finally:
    if runner.proc and runner.proc.poll() is None:
      try:
        os.kill(runner.proc.pid, signal.SIGKILL)
      except ProcessLookupError:
        pass


def main():
  ensure_display()
  parser = argparse.ArgumentParser(
      description='Run Cobalt Linux Lifecycle E2E tests.')
  parser.add_argument(
      '--test',
      choices=['all', 'preload', 'lifecycle'],
      default='all',
      help='Which test to execute (default: all)',
  )
  parser.add_argument(
      '--platform',
      choices=['modular', 'evergreen', 'linux-x64x11-modular', 'evergreen-x64'],
      default=None,
      help='Platform target to test',
  )
  parser.add_argument(
      '--modular',
      action='store_const',
      const='modular',
      dest='platform',
      help='Alias for --platform modular',
  )
  parser.add_argument(
      '--evergreen',
      action='store_const',
      const='evergreen',
      dest='platform',
      help='Alias for --platform evergreen',
  )
  parser.add_argument(
      '--config',
      choices=['qa', 'devel', 'debug', 'gold'],
      default='qa',
      help='Build configuration (default: qa)',
  )
  parser.add_argument(
      '-e',
      '--executable',
      type=str,
      default=None,
      help='Path to custom executable or command line',
  )
  parser.add_argument(
      '--host',
      type=str,
      default='localhost',
      help='DevTools host (default: localhost)',
  )
  parser.add_argument(
      '-p',
      '--port',
      type=int,
      default=9223,
      help='DevTools port (default: 9223)',
  )
  parser.add_argument(
      '--out-dir',
      type=str,
      default=None,
      help='Build output directory',
  )
  args = parser.parse_args()

  executable = resolve_executable(
      platform=args.platform,
      config=args.config,
      custom_executable=args.executable,
      out_dir=args.out_dir,
  )
  logger.info('Using executable: %s', executable)

  tests_to_run = []
  if args.test in ('all', 'preload'):
    tests_to_run.append(('preload', run_preload_test))
  if args.test in ('all', 'lifecycle'):
    tests_to_run.append(('lifecycle', run_lifecycle_test))

  failed = []
  for name, test_func in tests_to_run:
    try:
      test_func(
          executable, platform=args.platform, host=args.host, port=args.port)
    except Exception as e:  # pylint: disable=broad-exception-caught
      logger.exception('FAILED: Test %s encountered an error: %s', name, e)
      failed.append(name)

  if failed:
    logger.error('The following tests FAILED: %s', ', '.join(failed))
    sys.exit(1)

  logger.info('All requested tests PASSED successfully!')
  sys.exit(0)


if __name__ == '__main__':
  main()
