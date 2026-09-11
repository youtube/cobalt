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
"""X11 platform display detection and virtual display (Xvfb) lifecycle."""

import ctypes
import logging
import os
import shutil
import subprocess
import sys

try:
  from cobalt.tools.e2e.platform_display import PlatformDisplay
except ImportError:
  from platform_display import PlatformDisplay  # type: ignore[no-redef]

logger = logging.getLogger('x11_display')


class X11Display(PlatformDisplay):
  """Manages X11 display availability and Xvfb fallback."""

  def is_working(self) -> bool:
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

  def ensure_display(self) -> None:
    """Ensures working display is present; re-execs with xvfb-run if needed."""
    if self.is_working():
      return

    if os.environ.get('COBALT_XVFB_ACTIVE') == '1':
      return

    xvfb_path = shutil.which('xvfb-run')
    if not xvfb_path:
      logger.warning(
          'No working DISPLAY detected and xvfb-run is not installed. Cobalt'
          ' may fail to initialize graphics.')
      return

    logger.info(
        'No working DISPLAY found. Automatically re-launching under xvfb-run...'
    )
    server_args = '-screen 0 1920x1080x24i +render +extension GLX -noreset'
    os.environ['COBALT_XVFB_ACTIVE'] = '1'
    args = [
        'xvfb-run',
        '-a',
        f'--server-args={server_args}',
        sys.executable,
    ] + sys.argv
    os.execvp('xvfb-run', args)
