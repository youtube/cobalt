#!/usr/bin/env vpython3
# Copyright 2025 The Cobalt Authors. All Rights Reserved.
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
  result = asyncio.run(evaluate("document.visibilityState", 9222))
"""

import argparse
import asyncio
import json
import sys

import requests
import websockets


async def get_websocket_url(port):
  """Retrieves the websocket debugger URL from the local Cobalt instance."""
  try:
    # Use a synchronous requests call for the JSON list, as it's simple.
    response = requests.get(f'http://localhost:{port}/json', timeout=10)
    targets = response.json()
    for target in targets:
      if target.get('type') == 'page' and target.get('webSocketDebuggerUrl'):
        return target['webSocketDebuggerUrl']
  except (RuntimeError, ValueError) as e:
    print(f'Error connecting to devtools on port {port}: {e}', file=sys.stderr)
  return None


async def evaluate(expression, port):
  """Evaluates a JS expression via the Chrome DevTools Protocol."""
  ws_url = await get_websocket_url(port)
  if not ws_url:
    return 'ERROR: No websocket URL'

  try:
    async with websockets.connect(ws_url) as websocket:
      # Send a simple Runtime.evaluate message.
      message = {
          'id': 1,
          'method': 'Runtime.evaluate',
          'params': {
              'expression': expression,
              'returnByValue': True
          }
      }
      await websocket.send(json.dumps(message))
      response = await websocket.recv()
      result = json.loads(response)
      if 'result' in result and 'result' in result['result']:
        return result['result']['result'].get('value')
      return f'ERROR: {response}'
  except (RuntimeError, ValueError) as e:
    print(f'CDP Error: {e}', file=sys.stderr)
    return f'ERROR: {e}'


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--port', type=int, default=9222)
  parser.add_argument('expression', type=str)
  args = parser.parse_args()

  result = asyncio.run(evaluate(args.expression, args.port))
  print(result)


if __name__ == '__main__':
  main()
