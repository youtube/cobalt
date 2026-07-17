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
"""A helper to interact with Cobalt's CDP interface via JS.

Usage from shell:
  vpython3 cobalt/tools/cdp_js_helper.py --port 9222 "document.visibilityState"

Usage from python:
  import asyncio
  from cobalt.tools.cdp_js_helper import evaluate
  result = asyncio.run(evaluate("document.visibilityState", "localhost", 9222))
"""

import argparse
import asyncio
import json
import sys
import time

import requests
import websockets


async def get_websocket_url(host, port):
  """Retrieves the websocket debugger URL from the local Cobalt instance."""
  try:
    # Use a synchronous requests call for the JSON list, as it's simple.
    response = requests.get(f'http://{host}:{port}/json', timeout=10)
    targets = response.json()
    for target in targets:
      if target.get('type') == 'page' and target.get('webSocketDebuggerUrl'):
        return target['webSocketDebuggerUrl']
    print(f'CDP targets found but no page target: {targets}', file=sys.stderr)
  except (requests.exceptions.RequestException, ValueError) as e:
    print(
        f'Error connecting to devtools on {host}:{port}: {e}', file=sys.stderr)
  return None


async def evaluate(expression, host, port):
  """Evaluates a JavaScript expression via CDP."""
  ws_url = await get_websocket_url(host, port)
  if not ws_url:
    return 'ERROR: No websocket URL'

  try:
    async with websockets.connect(ws_url, close_timeout=5) as websocket:
      # Send a simple Runtime.evaluate message.
      message = {
          'id': 1,
          'method': 'Runtime.evaluate',
          'params': {
              'expression': expression,
              'returnByValue': True
          }
      }
      await asyncio.wait_for(websocket.send(json.dumps(message)), timeout=30)
      response = await asyncio.wait_for(websocket.recv(), timeout=30)
      result = json.loads(response)
      if 'result' in result and 'result' in result['result']:
        val = result['result']['result'].get('value')
        # If the value is not a string (e.g. boolean), convert to string.
        if val is True:
          return 'True'
        if val is False:
          return 'False'
        if isinstance(val, (dict, list)):
          return json.dumps(val, separators=(',', ':'))
        return str(val) if val is not None else 'None'
      return f'ERROR: {response}'
  except (RuntimeError, ValueError, asyncio.TimeoutError, OSError) as e:
    print(f'CDP Error: {type(e).__name__}: {e}', file=sys.stderr)
    return f'ERROR: {type(e).__name__}: {e}'


async def wait_for_cdp(host, port, timeout, total_wait):
  """Waits for the CDP endpoint to become available."""
  start_time = time.monotonic()
  while (time.monotonic() - start_time) < total_wait:
    try:
      ws_url = await get_websocket_url(host, port)
      if ws_url:
        async with websockets.connect(ws_url, close_timeout=1) as websocket:
          # Try to verify JS responsiveness, but don't fail if it times out
          # (e.g. if the app is preloaded/suspended and JS is throttled).
          try:
            message = {
                'id': 999,
                'method': 'Runtime.evaluate',
                'params': {
                    'expression': '1+1',
                    'returnByValue': True
                }
            }
            await asyncio.wait_for(
                websocket.send(json.dumps(message)), timeout=2)
            await asyncio.wait_for(websocket.recv(), timeout=2)
            return 'SUCCESS'
          except asyncio.TimeoutError:
            # If we connected but JS is non-responsive, it's still "ready" for
            # connection.
            return 'SUCCESS'
    except (RuntimeError, ValueError, asyncio.TimeoutError, OSError):
      pass
    await asyncio.sleep(timeout)
  return 'FAILURE'


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--host', type=str, default='localhost')
  parser.add_argument('--port', type=int, default=9222)
  parser.add_argument(
      '--wait', action='store_true', help='Wait for CDP to be ready')
  parser.add_argument(
      '--wait-timeout',
      type=float,
      default=1.0,
      help='Interval between connection attempts')
  parser.add_argument(
      '--wait-total',
      type=float,
      default=30.0,
      help='Total time to wait for CDP')
  parser.add_argument('expression', type=str, nargs='?', default='')
  args = parser.parse_args()

  if args.wait:
    result = asyncio.run(
        wait_for_cdp(args.host, args.port, args.wait_timeout, args.wait_total))
  else:
    if not args.expression:
      parser.error('expression is required when not using --wait')
    result = asyncio.run(evaluate(args.expression, args.host, args.port))
  print(result)


if __name__ == '__main__':
  main()
