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
"""Common test utilities, runner, and CDP client for Cobalt Linux E2E tests."""

import abc
import base64
import ctypes
import json
import logging
import os
import shutil
import signal
import socket
import struct
import subprocess
import sys
import time
import urllib.error
import urllib.parse
import urllib.request

logger = logging.getLogger('cobalt_test_common')


def is_display_working():
  """Checks if a functional X11 display is currently available."""
  if not os.environ.get('DISPLAY'):
    return False

  # Try xdpyinfo first if present
  if shutil.which('xdpyinfo'):
    try:
      res = subprocess.run(
          ['xdpyinfo'],
          stdout=subprocess.DEVNULL,
          stderr=subprocess.DEVNULL,
          timeout=2,
          check=False,
      )
      return res.returncode == 0
    except (subprocess.SubprocessError, OSError):
      pass

  # Fallback to libX11.so.6 via ctypes
  try:
    x11 = ctypes.cdll.LoadLibrary('libX11.so.6')
    x11.XOpenDisplay.argtypes = [ctypes.c_char_p]
    x11.XOpenDisplay.restype = ctypes.c_void_p
    x11.XCloseDisplay.argtypes = [ctypes.c_void_p]
    x11.XCloseDisplay.restype = ctypes.c_int
    disp = x11.XOpenDisplay(None)
    if disp:
      x11.XCloseDisplay(disp)
      return True
    return False
  except (OSError, AttributeError):
    return False


def ensure_display():
  """Ensures working display is present; re-execs with xvfb-run if needed."""
  if is_display_working():
    return

  if os.environ.get('COBALT_XVFB_ACTIVE') == '1':
    return

  xvfb_path = shutil.which('xvfb-run')
  if not xvfb_path:
    logger.warning(
        'No working DISPLAY detected and xvfb-run is not installed. Cobalt may'
        ' fail to initialize graphics.')
    return

  logger.info(
      'No working DISPLAY found. Automatically re-launching under xvfb-run...')
  server_args = '-screen 0 1920x1080x24i +render +extension GLX -noreset'
  os.environ['COBALT_XVFB_ACTIVE'] = '1'
  args = [
      'xvfb-run',
      '-a',
      f'--server-args={server_args}',
      sys.executable,
  ] + sys.argv
  os.execvp('xvfb-run', args)


def resolve_executable(platform=None,
                       config='qa',
                       custom_executable=None,
                       out_dir=None):
  """Resolves the executable command based on configuration."""
  if custom_executable:
    return custom_executable

  search_roots = []
  if out_dir:
    search_roots.append(out_dir)
  if os.environ.get('OUT_DIR'):
    search_roots.append(os.environ['OUT_DIR'])
  if os.environ.get('GITHUB_WORKSPACE'):
    search_roots.append(os.path.join(os.environ['GITHUB_WORKSPACE'], 'out'))
  search_roots.extend(['./out', '../out'])

  for root in search_roots:
    modular_qa = os.path.join(root, 'linux-x64x11-modular_qa', 'cobalt_loader')
    modular_devel = os.path.join(root, 'linux-x64x11-modular_devel',
                                 'cobalt_loader')
    evergreen_qa = os.path.join(root, 'evergreen-x64_qa', 'loader_app')
    evergreen_devel = os.path.join(root, 'evergreen-x64_devel', 'loader_app')

    def format_evergreen(loader, lib_dir):
      lib = os.path.join(lib_dir, 'libcobalt.so')
      lib_arg = lib if os.path.isfile(lib) else 'libcobalt.so'
      content_arg = lib_dir if os.path.isdir(lib_dir) else '.'
      return (f'{loader} --evergreen_content={content_arg}'
              f' --evergreen_library={lib_arg}')

    # Also check if root itself is the specific out directory
    if os.path.isfile(os.path.join(root, 'cobalt_loader')):
      return os.path.join(root, 'cobalt_loader')
    if os.path.isfile(os.path.join(root, 'loader_app')):
      return format_evergreen(os.path.join(root, 'loader_app'), root)

    if platform in ('modular', 'linux-x64x11-modular'):
      target = modular_qa if config == 'qa' else modular_devel
      if os.path.isfile(target):
        return target
      if os.path.isfile(modular_devel):
        return modular_devel

    if platform in ('evergreen', 'evergreen-x64'):
      target = evergreen_qa if config == 'qa' else evergreen_devel
      if os.path.isfile(target):
        return format_evergreen(target, os.path.dirname(target))
      if os.path.isfile(evergreen_devel):
        return format_evergreen(evergreen_devel,
                                os.path.dirname(evergreen_devel))

    # Auto-detection
    if os.path.isfile(modular_qa):
      return modular_qa
    if os.path.isfile(evergreen_qa):
      return format_evergreen(evergreen_qa, os.path.dirname(evergreen_qa))
    if os.path.isfile(modular_devel):
      return modular_devel
    if os.path.isfile(evergreen_devel):
      return format_evergreen(evergreen_devel, os.path.dirname(evergreen_devel))

  raise FileNotFoundError(
      'Could not find any Cobalt executable in search paths. Please specify'
      ' --executable, --out-dir, or build modular/evergreen first.')


class SimpleWebSocket:
  """Minimal synchronous RFC 6455 WebSocket client.

  Uses only the Python standard library.
  """

  def __init__(self, url, timeout=10.0):
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

  def _handshake(self):
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

  def _read_exact(self, n):
    data = bytearray()
    while len(data) < n:
      chunk = self.sock.recv(n - len(data))
      if not chunk:
        raise ConnectionError('WebSocket connection closed unexpectedly')
      data.extend(chunk)
    return bytes(data)

  def send_text(self, text):
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

  def recv_text(self):
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

  def close(self):
    try:
      self.sock.close()
    except OSError:
      pass

  def __enter__(self):
    return self

  def __exit__(self, exc_type, exc_val, exc_tb):
    self.close()


class CDPClient:
  """Client for interacting with Cobalt via the Chrome DevTools Protocol."""

  def __init__(self, host='localhost', port=9223):
    self.host = host
    self.port = port
    self.base_url = f'http://{self.host}:{self.port}'

  def get_ws_url(self, timeout=5.0):
    """Fetches the websocket debugger URL from /json endpoint."""
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

  def wait_for_ready(self, total_wait=60.0, interval=1.0):
    """Waits until the CDP endpoint responds with a usable page target."""
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

  def evaluate(self, expression, timeout=30.0):
    """Synchronously evaluates a JavaScript expression via CDP."""
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

  def wait_for_state(self, expr, expected, timeout=30.0, interval=0.5):
    """Polls a JS expression until it equals expected."""
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

  def pop_event(self, expected_event, timeout=10.0, interval=0.5):
    """Waits for window.event_log to have items, pops one, and asserts match."""
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


class CobaltRunner:
  """Manages execution and lifecycle signaling of Cobalt instances."""

  def __init__(self, executable, log_file, port=9223):
    self.executable_cmd = executable
    self.log_file = log_file
    self.port = port
    self.proc = None
    self.log_handle = None

  def cleanup_existing(self):
    """Kills orphaned cobalt processes to prevent port/resource conflicts."""
    logger.info('Cleaning up any orphaned Cobalt processes...')
    for proc_name in ['cobalt_loader', 'loader_app', 'wait_for_state.sh']:
      subprocess.run(
          ['pkill', '-9', '-f', proc_name],
          stdout=subprocess.DEVNULL,
          stderr=subprocess.DEVNULL,
          check=False,
      )
    time.sleep(0.5)

  def launch(self, extra_args):
    """Launches Cobalt in a background process."""
    self.cleanup_existing()
    if os.path.exists(self.log_file):
      os.remove(self.log_file)

    cmd = (
        self.executable_cmd if isinstance(self.executable_cmd, list) else
        self.executable_cmd.split())
    cmd = list(cmd) + extra_args

    logger.info('Launching Cobalt: %s', ' '.join(cmd))
    env = os.environ.copy()
    if 'ASAN_OPTIONS' not in env:
      env['ASAN_OPTIONS'] = 'exitcode=0:detect_leaks=0'
    # pylint: disable=consider-using-with
    self.log_handle = open(self.log_file, 'w', encoding='utf-8')
    try:
      # pylint: disable=subprocess-popen-preexec-fn
      self.proc = subprocess.Popen(
          cmd,
          stdout=self.log_handle,
          stderr=subprocess.STDOUT,
          preexec_fn=os.setpgrp,
          env=env,
      )
    except Exception:
      self.log_handle.close()
      self.log_handle = None
      raise
    logger.info('Launched PID: %d. Output: %s', self.proc.pid, self.log_file)
    return self.proc.pid

  def send_signal(self, sig, name=None):
    """Sends a signal to the Cobalt process."""
    if not self.proc or self.proc.poll() is not None:
      raise RuntimeError('Cobalt process is not running!')
    sig_name = name or sig.name if hasattr(sig, 'name') else str(sig)
    logger.info('Sending %s to PID %d...', sig_name, self.proc.pid)
    os.kill(self.proc.pid, sig)

  def wait_for_exit(self, timeout=15.0):
    """Waits for Cobalt to exit cleanly, escalating to SIGKILL on timeout."""
    if not self.proc or self.proc.poll() is not None:
      return 0 if not self.proc else self.proc.poll()

    start = time.time()
    while time.time() - start < timeout:
      ret = self.proc.poll()
      if ret is not None:
        logger.info('Cobalt exited cleanly with code: %d', ret)
        if self.log_handle:
          self.log_handle.close()
        allowed_codes = (0, 143, -signal.SIGPWR, -signal.SIGTERM)
        if ret not in allowed_codes:
          raise AssertionError(
              f'Cobalt exited with unexpected non-zero code {ret}')
        return ret
      time.sleep(0.5)

    logger.warning('Cobalt did not stop after %ss. Escalating to SIGKILL...',
                   timeout)
    try:
      os.kill(self.proc.pid, signal.SIGKILL)
    except ProcessLookupError:
      pass
    if self.log_handle:
      self.log_handle.close()
    return -9

  def terminate_and_wait(self, timeout=15.0):
    """Sends SIGPWR and waits for Cobalt to terminate cleanly."""
    if not self.proc or self.proc.poll() is not None:
      return 0 if not self.proc else self.proc.poll()

    logger.info('Sending SIGPWR (graceful stop) to PID %d...', self.proc.pid)
    try:
      self.send_signal(signal.SIGPWR, 'SIGPWR')
    except ProcessLookupError:
      pass
    return self.wait_for_exit(timeout)

  def get_log_content(self):
    """Reads and returns the complete log content."""
    if os.path.exists(self.log_file):
      with open(self.log_file, 'r', encoding='utf-8', errors='replace') as f:
        return f.read()
    return ''


class LifecycleController(abc.ABC):
  """Abstract interface for injecting lifecycle transitions into Cobalt."""

  @abc.abstractmethod
  def blur(self) -> None:
    """Transitions the application to blurred (loss of focus)."""

  @abc.abstractmethod
  def focus(self) -> None:
    """Transitions the application to focused / active."""

  @abc.abstractmethod
  def conceal(self) -> None:
    """Transitions the application to concealed (hidden/background)."""

  @abc.abstractmethod
  def freeze(self) -> None:
    """Transitions the application to frozen (suspended)."""

  @abc.abstractmethod
  def resume(self) -> None:
    """Transitions the application from frozen/concealed back to visible."""

  @abc.abstractmethod
  def stop(self) -> None:
    """Gracefully requests the application to terminate."""

  @abc.abstractmethod
  def low_memory(self) -> None:
    """Injects a low-memory warning event."""


class PosixSignalLifecycleController(LifecycleController):
  """Controls application lifecycle via POSIX signals (suspend_signals.cc)."""

  def __init__(self, runner: CobaltRunner):
    self.runner = runner

  def blur(self) -> None:
    logger.info('Sending SIGWINCH (BLUR)...')
    self.runner.send_signal(signal.SIGWINCH, 'SIGWINCH')

  def focus(self) -> None:
    logger.info('Sending SIGCONT (FOCUS)...')
    self.runner.send_signal(signal.SIGCONT, 'SIGCONT')

  def conceal(self) -> None:
    logger.info('Sending SIGUSR1 (CONCEAL)...')
    self.runner.send_signal(signal.SIGUSR1, 'SIGUSR1')

  def freeze(self) -> None:
    logger.info('Sending SIGTSTP (FREEZE)...')
    self.runner.send_signal(signal.SIGTSTP, 'SIGTSTP')

  def resume(self) -> None:
    logger.info('Sending SIGCONT (RESUME)...')
    self.runner.send_signal(signal.SIGCONT, 'SIGCONT')

  def stop(self) -> None:
    logger.info('Sending SIGPWR (STOP)...')
    self.runner.send_signal(signal.SIGPWR, 'SIGPWR')

  def low_memory(self) -> None:
    logger.info('Sending SIGUSR2 (LOW_MEMORY)...')
    self.runner.send_signal(signal.SIGUSR2, 'SIGUSR2')


def create_lifecycle_controller(platform: str,
                                runner: CobaltRunner) -> LifecycleController:
  """Factory returning the appropriate LifecycleController for the platform."""
  # Currently Linux Modular and Evergreen use POSIX signals via
  # suspend_signals.cc.
  del platform  # Unused until Android/RDK adapters are added.
  return PosixSignalLifecycleController(runner)
