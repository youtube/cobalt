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
"""Dumps UMA histograms from a running Cobalt instance using CDP."""

import argparse
import asyncio
import json
import sys
import urllib.error
import urllib.request

import websockets


async def get_histograms(ws_url, delta=False):
  """Connects to CDP and retrieves histograms."""
  try:
    async with websockets.connect(ws_url, max_size=2**24) as websocket:
      command = {
          "id": 1,
          "method": "Browser.getHistograms",
          "params": {
              "query": "",
              "delta": delta
          },
      }
      await websocket.send(json.dumps(command))
      raw_response = await websocket.recv()
      response = json.loads(raw_response)

      if "error" in response:
        error_msg = response.get("error")
        print(f"Error from DevTools Protocol: {error_msg}")
        return None

      return response.get("result", {}).get("histograms", [])
  except Exception as e:  # pylint: disable=broad-exception-caught
    print(f"Error during WebSocket communication: {e}")
    return None


async def main():
  parser = argparse.ArgumentParser(description="Dump histograms from CDP.")
  parser.add_argument(
      "--port", type=int, default=9222, help="CDP port (default: 9222)")
  parser.add_argument(
      "--delta", action="store_true", help="Get histograms since last call")
  parser.add_argument(
      "--output", type=str, default="histograms.json", help="Output file")
  parser.add_argument("--filter", type=str, default="", help="Filter by name")
  args = parser.parse_args()

  # Find CDP target
  try:
    ws_url = None
    for path in ["/json/version", "/json"]:
      try:
        with urllib.request.urlopen(
            f"http://localhost:{args.port}{path}", timeout=2) as resp:
          data = json.loads(resp.read().decode())
          if path == "/json/version":
            ws_url = data.get("webSocketDebuggerUrl")
          else:
            target = next((t for t in data if t["type"] in ["browser", "page"]),
                          data[0] if data else None)
            ws_url = target.get("webSocketDebuggerUrl") if target else None
          if ws_url:
            break
      except (urllib.error.URLError, json.JSONDecodeError, KeyError,
              IndexError):
        continue

    if not ws_url:
      print(f"Error: Could not find CDP target on port {args.port}.")
      sys.exit(1)
  except Exception:  # pylint: disable=broad-exception-caught
    print(f"Error: Failed to connect to localhost:{args.port}.")
    sys.exit(1)

  # Get and filter histograms
  histograms = await get_histograms(ws_url, delta=args.delta)
  if histograms is None:
    sys.exit(1)

  if args.filter:
    histograms = [
        h for h in histograms
        if args.filter.lower() in h.get("name", "").lower()
    ]

  # Save and report
  with open(args.output, "w", encoding="utf-8") as f:
    json.dump(histograms, f, indent=2)

  print(f"Dumped {len(histograms)} histograms to {args.output}")


if __name__ == "__main__":
  try:
    asyncio.run(main())
  except KeyboardInterrupt:
    pass
  except Exception as e:  # pylint: disable=broad-exception-caught
    print(f"Fatal error: {e}")
