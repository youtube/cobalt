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
"""Minimal synchronous RFC 6455 WebSocket client using standard library."""

import base64
import os
import socket
import struct
import urllib.parse


class SimpleWebSocket:
  """Minimal synchronous RFC 6455 WebSocket client.

  Uses only the Python standard library.
  """

  def __init__(self, url: str, timeout: float = 10.0):
    """Connects to the WebSocket server and performs the RFC 6455 handshake.

    Args:
      url: ws:// or http:// URL of the WebSocket endpoint.
      timeout: Socket timeout in seconds.

    Raises:
      ConnectionError: If connection or handshake fails.
    """
    parsed = urllib.parse.urlparse(url)
    self.host = parsed.hostname
    self.port = parsed.port or 80
    self.path = parsed.path or '/'
    if parsed.query:
      self.path += '?' + parsed.query
    self.timeout = timeout
    self.sock = socket.create_connection((self.host, self.port),
                                         timeout=timeout)
    self._handshake()

  def _handshake(self) -> None:
    """Executes the HTTP 101 Switching Protocols WebSocket handshake."""
    key = base64.b64encode(os.urandom(16)).decode('ascii')
    req = (f'GET {self.path} HTTP/1.1\r\n'
           f'Host: {self.host}:{self.port}\r\n'
           'Upgrade: websocket\r\n'
           'Connection: Upgrade\r\n'
           f'Sec-WebSocket-Key: {key}\r\n'
           'Sec-WebSocket-Version: 13\r\n\r\n')
    self.sock.sendall(req.encode('ascii'))
    resp = b''
    while b'\r\n\r\n' not in resp:
      chunk = self.sock.recv(4096)
      if not chunk:
        raise ConnectionError('WebSocket handshake failed: EOF')
      resp += chunk
    status_line = resp.split(b'\r\n')[0].decode('utf-8', errors='replace')
    if ' 101 ' not in status_line:
      raise ConnectionError(f'WebSocket handshake failed: {status_line}')

  def _read_exact(self, n: int) -> bytes:
    """Reads exactly n bytes from the socket."""
    data = bytearray()
    while len(data) < n:
      chunk = self.sock.recv(n - len(data))
      if not chunk:
        raise ConnectionError('WebSocket connection closed unexpectedly')
      data.extend(chunk)
    return bytes(data)

  def send_text(self, text: str) -> None:
    """Sends a UTF-8 text frame masked per RFC 6455 client requirements.

    Args:
      text: The text payload to transmit.
    """
    payload = text.encode('utf-8')
    header = bytearray([0x81])  # FIN + text opcode
    length = len(payload)
    mask = os.urandom(4)
    if length < 126:
      header.append(0x80 | length)
    elif length <= 0xFFFF:
      header.append(0x80 | 126)
      header.extend(struct.pack('!H', length))
    else:
      header.append(0x80 | 127)
      header.extend(struct.pack('!Q', length))
    header.extend(mask)
    masked_payload = bytes(b ^ mask[i % 4] for i, b in enumerate(payload))
    self.sock.sendall(header + masked_payload)

  def recv_text(self) -> str:
    """Receives and unmasks the next text frame.

    Returns:
      Decoded UTF-8 string payload from the frame.

    Raises:
      ConnectionError: If server closes the connection or socket terminates.
      ValueError: If an unexpected or unsupported opcode is received.
    """
    while True:
      b1, b2 = self._read_exact(2)
      opcode = b1 & 0x0F
      is_masked = bool(b2 & 0x80)
      payload_len = b2 & 0x7F
      if payload_len == 126:
        payload_len = struct.unpack('!H', self._read_exact(2))[0]
      elif payload_len == 127:
        payload_len = struct.unpack('!Q', self._read_exact(8))[0]
      mask = self._read_exact(4) if is_masked else None
      payload = self._read_exact(payload_len)
      if mask:
        payload = bytes(b ^ mask[i % 4] for i, b in enumerate(payload))

      if opcode == 0x8:  # CLOSE
        self.sock.close()
        raise ConnectionError('WebSocket closed by server')
      if opcode == 0x9:  # PING
        pong_hdr = bytearray([0x8A, 0x80 | len(payload)])
        pmask = os.urandom(4)
        pong_hdr.extend(pmask)
        pong_hdr.extend(bytes(b ^ pmask[i % 4] for i, b in enumerate(payload)))
        self.sock.sendall(pong_hdr)
        continue
      if opcode in (0x1, 0x0):  # TEXT or CONTINUATION
        return payload.decode('utf-8')
      raise ValueError(
          f'Unsupported or unhandled WebSocket opcode: {opcode:#x}')

  def close(self) -> None:
    """Closes the underlying socket connection."""
    try:
      self.sock.close()
    except OSError:
      pass

  def __enter__(self) -> 'SimpleWebSocket':
    return self

  def __exit__(self, exc_type, exc_val, exc_tb) -> None:
    self.close()
