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
"""CDP DevTools Automated Keyboard Scrolling Driver for Cobalt.

Dispatches native keypress events over raw websockets to simulate scrolling.
"""

import argparse
import json
import logging
import socket
import sys
import time
import urllib.request


def send_raw_ws_frame(sock, payload):
  """Encapsulates and sends a raw WebSocket text frame (RFC 6455)."""
  data = payload.encode("utf-8")
  length = len(data)

  header = bytearray()
  header.append(0x81)  # Fin bit set + Text opcode (1)

  if length <= 125:
    header.append(length | 0x80)  # Mask bit set
  elif length <= 65535:
    header.append(126 | 0x80)
    header.extend(length.to_bytes(2, byteorder="big"))
  else:
    header.append(127 | 0x80)
    header.extend(length.to_bytes(8, byteorder="big"))

  # Standard 4-byte masking key (randomized)
  mask = bytearray([0x11, 0x22, 0x33, 0x44])
  header.extend(mask)

  # Mask payload
  masked_data = bytearray(length)
  for i in range(length):
    masked_data[i] = data[i] ^ mask[i % 4]

  sock.sendall(header + masked_data)


def dispatch_cdp_key(sock, key_code):
  """Sends rawKeyDown and keyUp CDP commands sequentially."""
  # 1. Dispatch KeyDown
  cmd_down = {
      "id": int(time.time() * 1000),
      "method": "Input.dispatchKeyEvent",
      "params": {
          "type": "rawKeyDown",
          "windowsVirtualKeyCode": key_code,
          "unmodifiedText": "",
          "text": "",
      },
  }
  send_raw_ws_frame(sock, json.dumps(cmd_down))
  time.sleep(0.05)

  # 2. Dispatch KeyUp
  cmd_up = {
      "id": int(time.time() * 1000) + 1,
      "method": "Input.dispatchKeyEvent",
      "params": {
          "type": "keyUp",
          "windowsVirtualKeyCode": key_code,
          "unmodifiedText": "",
          "text": "",
      },
  }
  send_raw_ws_frame(sock, json.dumps(cmd_up))


def get_websocket_url(port):
  """Queries Cobalt DevTools for the WebSocket debugger URL."""
  try:
    req = urllib.request.Request(f"http://127.0.0.1:{port}/json/list")
    with urllib.request.urlopen(req, timeout=3) as response:
      pages = json.loads(response.read().decode())
      if pages and len(pages) > 0:
        return pages[0].get("webSocketDebuggerUrl")
  except Exception as e:  # pylint: disable=broad-exception-caught
    logging.warning("  ⚠️ Error locating DevTools websocket: %s", e)
  return None


def main():
  parser = argparse.ArgumentParser(description="CDP Automated Scrolling Driver")
  parser.add_argument("--port", type=int, default=9222, help="CDP debug port")
  parser.add_argument(
      "--duration",
      type=int,
      default=60,
      help="Total scroll campaign duration in seconds")
  parser.add_argument(
      "--interval",
      type=float,
      default=2.0,
      help="Seconds between scroll keypresses")
  parser.add_argument(
      "--key", type=int, default=40, help="D-pad Down Keycode (Default: 40)")
  args = parser.parse_args()

  logging.basicConfig(level=logging.INFO, format="%(message)s")

  logging.info("🔍 Locating Cobalt DevTools on port %s...", args.port)
  ws_url = get_websocket_url(args.port)
  if not ws_url:
    logging.error("❌ Failure: DevTools websocket not active. Exiting.")
    sys.exit(1)

  logging.info("🔌 WebSocket debugger URL: %s", ws_url)
  # Parse host/port/path from ws://127.0.0.1:9222/devtools/page/UUID
  path = "/" + ws_url.split("/", 3)[3]

  # Manually perform simple raw WebSocket handshake
  logging.info("⚡ Connecting raw TCP socket...")
  sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
  sock.settimeout(5)
  sock.connect(("127.0.0.1", args.port))

  handshake = (f"GET {path} HTTP/1.1\r\n"
               f"Host: 127.0.0.1:{args.port}\r\n"
               f"Upgrade: websocket\r\n"
               f"Connection: Upgrade\r\n"
               f"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
               f"Sec-WebSocket-Version: 13\r\n\r\n")
  sock.sendall(handshake.encode("utf-8"))

  # Read handshake response headers
  res = sock.recv(4096)
  if b"101 Switching Protocols" not in res:
    res_clean = res.decode(errors="ignore")
    logging.error("❌ Failure: Handshake rejected: %s", res_clean)
    sys.exit(1)
  logging.info("🟢 WebSocket connection established successfully! 🟢")

  logging.info(
      "🚀 Starting automated scroll campaign (Duration: %ss, Key: %s)...",
      args.duration, args.key)
  start_time = time.time()
  while time.time() - start_time < args.duration:
    logging.info("   -> Dispatching DPAD scroll event (Keycode: %s)...",
                 args.key)
    dispatch_cdp_key(sock, args.key)
    time.sleep(args.interval)

  logging.info("🏁 Scroll campaign complete! Closing socket connection.")
  sock.close()


if __name__ == "__main__":
  main()
