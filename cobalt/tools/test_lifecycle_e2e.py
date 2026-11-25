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
import sys
import time
from typing import Optional
import zlib

_TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
if _TOOLS_DIR not in sys.path:
  sys.path.insert(0, _TOOLS_DIR)

# Compact 3-frame VP9 WebM clip (from media/test/data/four-colors-vp9.webm,
# sliced cleanly at the end of SimpleBlock #2 at 0x590 with matching EBML
# Segment/Cluster lengths), zlib-compressed and base64-encoded so local runs
# and CI sparse checkouts exercise the exact same valid WebM byte stream.
_VP9_WEBM_ZLIB_B64 = (
    'eNqTcr2/mJEBDOSd2hoZnb4D8adGFqfPjRxOTS3lqUm5Tu2NTE6tjUwSwQ3pEKWskYK+s0sc'
    'rH13dwevbhH1XJkWvKbxqe/uHiBPLGRddvCaJkZlCFco5HA6iFsO4coE7wbJirK+gVo6m2Hw'
    'ApDHoM400rq+sZnfycG3odcnsSzN1ELPyEDP0MAg3BGV79LZ4bBwClgLKCCguj3WQRn21xsZ'
    'i482Ms5pZFDaOqc5NS+9rTUsPizAsrmRUflxcwvL9wlXH0DVCm9oYj6wq4lJJnRDR+j2RsbQ'
    'HY1MoNCESDPGFRdDFU5JPgBlMaSfgLJEXRd3+zp6+QfFOwU5+rm4tLfkFpgYwaVFXBf3+nr6'
    'AaXDXIOCPf39XNobDeGy8q6LJzr7+wY4hng6+bhCTAh2ae8AGZFYloxQKOW6uN3Vz9nfxTXI'
    'pR01IOCuK4K7jiUZ6HO4Xm3XxT0eQIN9XIPi/Rx9XV3aZzoWFOSkKoRlpqTmK/impmQmKngk'
    '5qXkpBbBNakiWzgDaGEy0EJjU5CFCjmZSWUFFbplBZZwu61w2K3kurjDJTQI6D+Qz6cYGFiB'
    'kJGekakBBADDwHlbKUQ1U87zRobFTtKNDAwNTZ7NTgzWHxS3sVioyEhMEWDUqJt7P/GdftAD'
    'Q179tPsHw5at1xD2idJcvdrXYXdigtGRt6sTqhtAphyomaCr63T6JFd5wX7tsG3/y3QdFC2V'
    'pu2ITdnex99c8WbjjofbehRYHY77PGepDzjFOPPwWvvg+y1mzOYH5jz++1yU89vcdbrvT0sq'
    'MvnPjoicwDT5+nUG9gdK0y5vvLTgQl4Dw7UFi5TnOpuqCuyNkEsQXdLtzj+X0av3d8SvS65L'
    'zPRtbz24sW7NOa7IG476s/c4qVUe2bq8csnZc/HyHEbLjv7oWSAfwKmZ/SlZMrUlc9Xit5Kf'
    'fYIYcxinSTNdbV6olh1/7YNZQ80EQy7m+tcymR6qv4ys7z5b/NvzSuC3JP2XPB+Ywl9WXovJ'
    '+v/n0j2lC3/eiByObz2qWRQ53eq1p/S1Rfry035obbcLk2lgSI//IJsoKC8VfWaGZ9mbaJZn'
    'q1cdE7hSfzdl6e9VL8MjP+47y/LIS2aFrIfrch2FxO9qIY1XFv4LF3l9jOGd1vzf97Osr2sb'
    'PNqz4IZUVZVsqP/T/3k9musep9j/M7I592j3psPnHsh6vj84Zc3nUtvlsXzNwYpnZ1apLK7t'
    '8nM68+uD/ZWA8O/8y6OtdqvrdCWrqyziObzS6KGPVNmiQNsK+Vca2r/bO5mKVJLDNK78j3r7'
    '9rn5nFgl7wwXCZ09J4qWXIvLnLBt+/n1AapNB10bn6Zu6wxJMT3dHtuyrWFhdinD4mWNDMEM'
    'bQwOk+ZwBDUysDCorVlkPusCOOLZciufPTjAkNspZJmxeGEjw3KouoiDDIwNCgzoSgBEu9BY')


def _get_vp9_webm_base64() -> str:
  """Returns a base64-encoded VP9 WebM test stream."""
  raw_bytes = zlib.decompress(base64.b64decode(_VP9_WEBM_ZLIB_B64))
  return base64.b64encode(raw_bytes).decode('ascii')


def _verify_sbplayer_destroyed_before_conceal_complete(runner: 'CobaltRunner',
                                                       destroy_nth: int,
                                                       conceal_nth: int,
                                                       label: str) -> None:
  """Asserts the Nth SbPlayerDestroy log precedes the Mth conceal completion."""
  conceal_line = None
  destroy_line = None

  # Wait up to 10s for the Mth final "Transition to kConcealed complete" line
  # to be flushed. Match lines ending with "Transition to kConcealed complete"
  # so intermediate "Transition to kConcealed complete (target: kStarted)" logs
  # emitted during resume from kFrozen are excluded.
  for _ in range(20):
    lines = runner.get_log_content().splitlines()
    conceal_matches = [
        idx for idx, line in enumerate(lines, start=1)
        if line.rstrip().endswith('Transition to kConcealed complete')
    ]
    if len(conceal_matches) >= conceal_nth:
      conceal_line = conceal_matches[conceal_nth - 1]
      break
    time.sleep(0.5)

  lines = runner.get_log_content().splitlines()
  destroy_matches = [
      idx for idx, line in enumerate(lines, start=1)
      if 'Destroying SbPlayerPrivateImpl. There are 0 players.' in line
  ]
  if len(destroy_matches) >= destroy_nth:
    destroy_line = destroy_matches[destroy_nth - 1]

  if conceal_line is None:
    raise AssertionError(
        f"[{label}] 'Transition to kConcealed complete' (#{conceal_nth}) not"
        f' found in {runner.log_file}')
  if destroy_line is None:
    raise AssertionError(
        f"[{label}] 'Destroying SbPlayerPrivateImpl' (#{destroy_nth}) was not"
        f' logged before conceal completed (line {conceal_line})')
  if destroy_line >= conceal_line:
    raise AssertionError(
        f"[{label}] 'Destroying SbPlayerPrivateImpl' (line {destroy_line})"
        " occurred after 'Transition to kConcealed complete'"
        f' (line {conceal_line})')
  logger.info(
      'SUCCESS [%s]: SbPlayerDestroy (line %d) < Transition to kConcealed'
      ' complete (line %d)', label, destroy_line, conceal_line)


# pylint: disable=wrong-import-position
try:
  from cobalt.tools.lib.cdp_client import CDPClient
  from cobalt.tools.e2e.cobalt_runner import CobaltRunner
  from cobalt.tools.e2e.common import (
      create_lifecycle_controller,
      ensure_display,
      resolve_executable,
  )
except ImportError:
  from test_common import (  # type: ignore[no-redef]
      CDPClient, CobaltRunner, create_lifecycle_controller, ensure_display,
      resolve_executable,
  )

logging.basicConfig(
    level=logging.INFO,
    format='[TEST] %(asctime)s - %(levelname)s - %(message)s',
    datefmt='%H:%M:%S',
)
logger = logging.getLogger('test_lifecycle_e2e')


def run_preload_test(executable: str,
                     platform: Optional[str] = None,
                     host: str = 'localhost',
                     port: int = 9223) -> bool:
  """Runs the Preload E2E integration test.

  Args:
    executable: Path or command line to launch Cobalt.
    platform: Optional platform identifier.
    host: DevTools host.
    port: DevTools remote debugging port.

  Returns:
    True if the preload test succeeds.
  """
  logger.info('========================================')
  logger.info('Starting PRELOAD Integration Test')
  logger.info('========================================')

  log_file = 'preload_run.log'
  with CobaltRunner(executable, log_file, port=port) as runner:
    controller = create_lifecycle_controller(platform, runner)
    cdp = CDPClient(host=host, port=port)

    test_html = (
        '<html><body><h1>Preload Test</h1><input autofocus></body></html>')
    test_url = 'data:text/html,' + test_html

    runner.launch([
        '--preload',
        f'--url={test_url}',
        f'--remote-debugging-port={port}',
        '--v=1',
    ])

    logger.info('Waiting for DevTools to become available...')
    cdp.wait_for_ready(total_wait=60.0)

    logger.info('Waiting for preloaded document to complete loading...')
    cdp.wait_for_state('document.readyState', 'complete', timeout=60.0)

    logger.info('Verifying initial preloaded state (Hidden & Not Focused)...')
    cdp.wait_for_state('document.visibilityState', 'hidden', timeout=120.0)
    cdp.wait_for_state('document.hasFocus()', False, timeout=120.0)
    cdp.wait_for_state(
        "window.location.search.includes('launch=preload')",
        True,
        timeout=120.0,
    )

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


def run_lifecycle_test(executable: str,
                       platform: Optional[str] = None,
                       host: str = 'localhost',
                       port: int = 9223) -> bool:
  """Runs the comprehensive Lifecycle Transitions E2E integration test.

  Args:
    executable: Path or command line to launch Cobalt.
    platform: Optional platform identifier.
    host: DevTools host.
    port: DevTools remote debugging port.

  Returns:
    True if all lifecycle transitions are verified successfully.
  """
  logger.info('========================================')
  logger.info('Starting LIFECYCLE Integration Test')
  logger.info('========================================')

  log_file = 'lifecycle_run.log'
  with CobaltRunner(executable, log_file, port=port) as runner:
    controller = create_lifecycle_controller(platform, runner)
    cdp = CDPClient(host=host, port=port)

    # Clean user storage/caches to prevent database locks in CI
    if os.environ.get('CI') == 'true' or os.environ.get(
        'GITHUB_ACTIONS') == 'true':
      home_dir = os.path.expanduser('~')
      for rel_dir in ['.cobalt_storage', '.cobalt', '.config/cobalt']:
        storage_dir = os.path.join(home_dir, rel_dir)
        if os.path.exists(storage_dir):
          shutil.rmtree(storage_dir, ignore_errors=True)

    html_payload = '<html><body><h1>Test</h1><input autofocus></body></html>'
    b64_html = base64.b64encode(html_payload.encode('utf-8')).decode('utf-8')

    runner.launch([
        f'--url=data:text/html;base64,{b64_html}',
        '--autoplay-policy=no-user-gesture-required',
        f'--remote-debugging-port={port}',
    ])

    logger.info('Waiting for DevTools to become available...')
    cdp.wait_for_ready(total_wait=60.0)

    logger.info('Waiting for document to complete loading...')
    cdp.wait_for_state('document.readyState', 'complete', timeout=60.0)

    logger.info('Verifying initial state (Visible & Focused)...')
    cdp.wait_for_state('document.visibilityState', 'visible', timeout=120.0)
    cdp.wait_for_state('document.hasFocus()', True, timeout=120.0)

    logger.info('Injecting in-page event logger...')
    injection_script = """
    window.event_log = [];
    document.addEventListener('visibilitychange', () => {
      console.log('JS_EVENT: visibilitychange ' + document.visibilityState);
      window.event_log.push({
        type: 'visibilitychange',
        visibility: document.visibilityState
      });
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
    document.onfreeze = () => {
      console.log('JS_EVENT: onfreeze');
      window.event_log.push({type: 'onfreeze'});
    };
    document.addEventListener('resume', () => {
      console.log('JS_EVENT: resume');
      window.event_log.push({type: 'resume'});
    });
    document.onresume = () => {
      console.log('JS_EVENT: onresume');
      window.event_log.push({type: 'onresume'});
    };
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

    logger.info(
        'Verifying freeze and onfreeze events fired BEFORE resume (while'
        ' frozen)...')
    frozen_logs = runner.get_log_content()
    if 'JS_EVENT: freeze' not in frozen_logs:
      raise AssertionError(
          'freeze event not found in logs before resume (fired too late or not'
          ' at all)')
    if 'JS_EVENT: onfreeze' not in frozen_logs:
      raise AssertionError(
          'document.onfreeze event not found in logs before resume (fired too'
          ' late or not at all)')
    if 'JS_EVENT: resume' in frozen_logs:
      raise AssertionError('Unexpected resume event in logs while frozen')

    # 5. Resume & Reveal & Focus
    logger.info('Step 5: Resuming and revealing application...')
    controller.resume()
    cdp.wait_for_state('document.visibilityState', 'visible', timeout=20.0)
    cdp.wait_for_state('document.hasFocus()', True, timeout=20.0)

    logger.info(
        'Verifying strict FIFO event sequence across freeze and resume...')
    cdp.pop_event({'type': 'freeze'})
    cdp.pop_event({'type': 'onfreeze'})
    cdp.pop_event({'type': 'resume'})
    cdp.pop_event({'type': 'onresume'})
    cdp.pop_event({'type': 'visibilitychange', 'visibility': 'visible'})
    cdp.pop_event({'type': 'focus'})

    # Cross-verify stdout/stderr console logs
    logger.info('Verifying console log events...')
    logs = runner.get_log_content()
    if 'JS_EVENT: freeze' not in logs:
      raise AssertionError('Missing "JS_EVENT: freeze" in stdout/stderr log')
    if 'JS_EVENT: onfreeze' not in logs:
      raise AssertionError('Missing "JS_EVENT: onfreeze" in stdout/stderr log')
    if 'JS_EVENT: resume' not in logs:
      raise AssertionError('Missing "JS_EVENT: resume" in stdout/stderr log')
    if 'JS_EVENT: onresume' not in logs:
      raise AssertionError('Missing "JS_EVENT: onresume" in stdout/stderr log')
    if 'JS_EVENT: visibilitychange visible' not in logs:
      raise AssertionError(
          'Missing "JS_EVENT: visibilitychange visible" in stdout/stderr log')
    focus_count = logs.count('JS_EVENT: focus')
    if focus_count < 2:
      raise AssertionError(
          f'Expected >= 2 occurrences of "JS_EVENT: focus", found {focus_count}'
      )

    # 6. Media Playback Conceal/Reveal Scenarios
    # Verifies:
    #   Scenario 1: Active playback on conceal pauses, destroys SbPlayer before
    #     "Transition to kConcealed complete", and auto-resumes on reveal.
    #   Scenario 2: Web-app-paused playback on conceal destroys SbPlayer before
    #     "Transition to kConcealed complete", and stays paused on reveal.
    #   Scenario 3: Web-app-removed <video> on conceal destroys SbPlayer before
    #     "Transition to kConcealed complete".
    logger.info('Step 6: Running Media Playback Conceal/Reveal Scenarios...')
    webm_b64 = _get_vp9_webm_base64()

    logger.info('Scenario 1: Launching MSE VP9 playback...')
    cdp.evaluate(f"""
      window.startTestVideo = async () => {{
        const existing = document.getElementById('test_video');
        if (existing) existing.remove();
        const v = document.createElement('video');
        v.id = 'test_video';
        v.loop = true;
        v.muted = true;
        document.body.appendChild(v);
        const ms = new MediaSource();
        v.src = URL.createObjectURL(ms);
        await new Promise(
            r => ms.addEventListener('sourceopen', r, {{once: true}}));
        const sb = ms.addSourceBuffer('video/webm; codecs="vp9"');
        const raw = atob('{webm_b64}');
        const bytes = new Uint8Array(raw.length);
        for (let i = 0; i < raw.length; i++) bytes[i] = raw.charCodeAt(i);
        sb.appendBuffer(bytes);
        await new Promise(
            r => sb.addEventListener('updateend', r, {{once: true}}));
        ms.endOfStream();
        await v.play();
        return true;
      }};
      window.startTestVideo();
    """)

    cdp.wait_for_state(
        '(() => { const v = document.getElementById("test_video");'
        ' return Boolean(v && !v.paused && v.readyState >= 2); })()',
        True,
        timeout=20.0,
    )

    if 'Creating SbPlayerPrivateImpl. There are 1 players.' not in (
        runner.get_log_content()):
      raise AssertionError('[Scenario 1] SbPlayerPrivateImpl was not created')

    # Note: 1st conceal in run_lifecycle_test was in Step 3 (without media).
    # Therefore this media conceal is conceal #2, and SbPlayerDestroy #1.
    logger.info('Scenario 1: Concealing during active playback...')
    controller.conceal()
    cdp.wait_for_state('document.visibilityState', 'hidden', timeout=10.0)
    _verify_sbplayer_destroyed_before_conceal_complete(
        runner, destroy_nth=1, conceal_nth=2, label='Scenario 1')

    logger.info('Scenario 1: Revealing and verifying auto-resume...')
    controller.resume()
    cdp.wait_for_state('document.visibilityState', 'visible', timeout=10.0)
    cdp.wait_for_state(
        "document.getElementById('test_video').paused", False, timeout=10.0)

    # Scenario 2: Web app explicitly calls video.pause() on conceal.
    logger.info(
        'Scenario 2: Registering visibilitychange listener that pauses video...'
    )
    cdp.evaluate("""
      document.addEventListener('visibilitychange', function onHidePause() {
        if (document.visibilityState === 'hidden') {
          document.removeEventListener('visibilitychange', onHidePause);
          document.getElementById('test_video').pause();
        }
      });
    """)

    logger.info('Scenario 2: Concealing application...')
    controller.conceal()
    cdp.wait_for_state('document.visibilityState', 'hidden', timeout=10.0)
    _verify_sbplayer_destroyed_before_conceal_complete(
        runner, destroy_nth=2, conceal_nth=3, label='Scenario 2')

    logger.info('Scenario 2: Revealing and verifying video remains paused...')
    controller.resume()
    cdp.wait_for_state('document.visibilityState', 'visible', timeout=10.0)
    time.sleep(1)
    cdp.wait_for_state(
        "document.getElementById('test_video').paused", True, timeout=5.0)

    # Scenario 3: Web app removes <video> on conceal.
    logger.info('Scenario 3: Resuming playback and registering visibilitychange'
                ' listener that removes <video>...')
    cdp.evaluate("""
      document.getElementById('test_video').play();
      document.addEventListener('visibilitychange', function onHideRemove() {
        if (document.visibilityState === 'hidden') {
          document.removeEventListener('visibilitychange', onHideRemove);
          const v = document.getElementById('test_video');
          if (v) {
            v.removeAttribute('src');
            v.load();
            v.remove();
          }
        }
      });
    """)
    cdp.wait_for_state(
        "document.getElementById('test_video').paused", False, timeout=10.0)

    logger.info('Scenario 3: Concealing application...')
    controller.conceal()
    cdp.wait_for_state('document.visibilityState', 'hidden', timeout=10.0)
    _verify_sbplayer_destroyed_before_conceal_complete(
        runner, destroy_nth=3, conceal_nth=4, label='Scenario 3')

    # Reveal before graceful shutdown so the app finishes in a visible state.
    controller.resume()
    cdp.wait_for_state('document.visibilityState', 'visible', timeout=10.0)

    # 7. Graceful Shutdown
    logger.info('Step 7: Stopping Cobalt gracefully...')
    controller.stop()
    runner.wait_for_exit(timeout=15.0)

    logger.info('SUCCESS: Lifecycle transitions verified!')
    return True


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
