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
"""Client for interacting with Cobalt via Chrome DevTools Protocol."""

import json
import logging
import time
from typing import Any, Dict, Optional
import urllib.error
import urllib.parse
import urllib.request

try:
  from cobalt.tools.lib.simple_web_socket import SimpleWebSocket
except ImportError:
  from simple_web_socket import SimpleWebSocket  # type: ignore[no-redef]

logger = logging.getLogger('cobalt_cdp_client')


class CDPClient:
  """Client for interacting with Cobalt via the Chrome DevTools Protocol."""

  def __init__(self, host: str = 'localhost', port: int = 9223):
    """Initializes the CDP client.

    Args:
      host: DevTools server hostname.
      port: DevTools remote debugging port.
    """
    self.host = host
    self.port = port
    self.base_url = f'http://{self.host}:{self.port}'

  def get_ws_url(self, timeout: float = 5.0) -> Optional[str]:
    """Fetches the websocket debugger URL from /json endpoint.

    Args:
      timeout: HTTP request timeout in seconds.

    Returns:
      The WebSocket debugger URL string if found, None otherwise.
    """
    try:
      req = urllib.request.Request(f'{self.base_url}/json')
      with urllib.request.urlopen(req, timeout=timeout) as resp:
        targets = json.loads(resp.read().decode('utf-8'))
        for target in targets:
          if target.get('type') == 'page' and target.get(
              'webSocketDebuggerUrl'):
            return target['webSocketDebuggerUrl']
        logger.debug('Targets found but no page target: %s', targets)
    except (urllib.error.URLError, ValueError, TimeoutError) as e:
      logger.debug('Error querying %s/json: %s', self.base_url, e)
    return None

  def wait_for_ready(self,
                     total_wait: float = 60.0,
                     interval: float = 1.0) -> str:
    """Waits until the CDP endpoint responds with a usable page target.

    Args:
      total_wait: Total seconds to wait for readiness.
      interval: Polling interval in seconds.

    Returns:
      The WebSocket debugger URL of the target page.

    Raises:
      TimeoutError: If endpoint is not ready within total_wait seconds.
    """
    start = time.time()
    while time.time() - start < total_wait:
      ws_url = self.get_ws_url(timeout=2.0)
      if ws_url:
        logger.info('CDP ready at: %s', ws_url)
        return ws_url
      time.sleep(interval)
    raise TimeoutError(
        f'DevTools on {self.host}:{self.port} did not become ready within'
        f' {total_wait}s.')

  def evaluate(self, expression: str, timeout: float = 30.0) -> Any:
    """Synchronously evaluates a JavaScript expression via CDP.

    Args:
      expression: The JavaScript expression to evaluate in page context.
      timeout: Seconds to wait for evaluation completion.

    Returns:
      The evaluated result value (or None).

    Raises:
      RuntimeError: If WebSocket URL is missing, evaluation fails, or a JS error
        occurs.
      TimeoutError: If the CDP response is not received within timeout.
    """
    ws_url = self.get_ws_url()
    if not ws_url:
      raise RuntimeError(f'No websocket debugger URL on {self.base_url}')

    with SimpleWebSocket(ws_url, timeout=timeout) as ws:
      msg = {
          'id': 1,
          'method': 'Runtime.evaluate',
          'params': {
              'expression': expression,
              'returnByValue': True,
          },
      }
      ws.send_text(json.dumps(msg))

      start = time.time()
      while time.time() - start < timeout:
        raw_resp = ws.recv_text()
        resp_data = json.loads(raw_resp)
        if resp_data.get('id') == 1:
          if 'error' in resp_data:
            err_msg = resp_data['error']
            raise RuntimeError(f'CDP evaluation error: {err_msg}')
          result = resp_data.get('result', {}).get('result', {})
          if result.get('subtype') == 'error':
            desc = result.get('description', result)
            raise RuntimeError(f'JavaScript error: {desc}')
          return result.get('value')
      raise TimeoutError(f'Timed out waiting for CDP response to: {expression}')

  def wait_for_state(self,
                     expr: str,
                     expected: Any,
                     timeout: float = 30.0,
                     interval: float = 0.5) -> bool:
    """Polls a JS expression until it equals expected.

    Args:
      expr: JavaScript expression to poll.
      expected: Expected value.
      timeout: Seconds to wait for expression to equal expected.
      interval: Polling interval in seconds.

    Returns:
      True if the condition is met.

    Raises:
      TimeoutError: If condition is not met within timeout seconds.
    """
    start = time.time()
    last_val = None
    last_err = None
    logger.info('Waiting for %s == %r (timeout: %ss)...', expr, expected,
                timeout)
    while time.time() - start < timeout:
      try:
        last_val = self.evaluate(expr, timeout=10.0)
        last_err = None
        if last_val == expected:
          elapsed = round(time.time() - start, 2)
          logger.info('SUCCESS: %s is %r (took %ss)', expr, expected, elapsed)
          return True
      except (RuntimeError, TimeoutError, OSError) as e:
        last_err = e
        logger.debug('Exception during evaluate(%s): %s', expr, e)
      time.sleep(interval)
    err_info = f' (last error: {last_err})' if last_err else ''
    raise TimeoutError(
        f"Timed out waiting for '{expr}' to be {expected!r}. Last value:"
        f' {last_val!r}{err_info}')

  def pop_event(self,
                expected_event: Dict[str, Any],
                timeout: float = 10.0,
                interval: float = 0.5) -> Dict[str, Any]:
    """Waits for window.event_log to have items, pops one, and asserts match.

    Args:
      expected_event: Dictionary of key-value pairs that the popped event must
        match.
      timeout: Seconds to wait for matching event.
      interval: Polling interval in seconds.

    Returns:
      The popped event dictionary.

    Raises:
      AssertionError: If popped object is not a dict or does not match
        expected_event.
      TimeoutError: If no matching event appears before timeout.
    """
    start = time.time()
    last_err = None
    logger.info('Waiting to pop event matching: %s...', expected_event)
    while time.time() - start < timeout:
      try:
        event = self.evaluate(
            'window.event_log && window.event_log.length > 0 ?'
            ' window.event_log.shift() : null')
        last_err = None
        if event is not None:
          if not isinstance(event, dict):
            raise AssertionError(f'Expected event object, got: {event}')
          for key, val in expected_event.items():
            if event.get(key) != val:
              raise AssertionError(
                  f'Event mismatch! Expected {key}={val!r}, got'
                  f' {key}={event.get(key)!r}. Full event: {event}')
          logger.info('SUCCESS: Popped expected event %s', event)
          return event
      except (RuntimeError, TimeoutError, OSError) as e:
        last_err = e
        logger.debug('Exception while waiting for event_log: %s', e)
      time.sleep(interval)
    err_info = f' (last error: {last_err})' if last_err else ''
    raise TimeoutError(
        f'Timed out waiting to pop event {expected_event}{err_info}')
