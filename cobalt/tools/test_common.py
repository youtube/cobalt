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
"""Backward-compatibility shim re-exporting Cobalt E2E support classes."""

try:
  from cobalt.tools.e2e.cobalt_runner import CobaltRunner
  from cobalt.tools.e2e.common import (
      create_lifecycle_controller,
      ensure_display,
      get_executable_resolver,
      get_platform_display,
      is_display_working,
      resolve_executable,
  )
  from cobalt.tools.e2e.executable_resolver import ExecutableResolver
  from cobalt.tools.e2e.lifecycle_controller import LifecycleController
  from cobalt.tools.e2e.linux_executable_resolver import LinuxExecutableResolver
  from cobalt.tools.e2e.platform_display import PlatformDisplay
  from cobalt.tools.e2e.posix_signal_lifecycle_controller import (
      PosixSignalLifecycleController,)
  from cobalt.tools.e2e.x11_display import X11Display
  from cobalt.tools.lib.cdp_client import CDPClient
  from cobalt.tools.lib.simple_web_socket import SimpleWebSocket
except ImportError:
  # Fallback when running directly inside cobalt/tools
  import os
  import sys
  _tools_dir = os.path.dirname(__file__)
  _e2e_dir = os.path.join(_tools_dir, 'e2e')
  _lib_dir = os.path.join(_tools_dir, 'lib')
  if _tools_dir not in sys.path:
    sys.path.insert(0, _tools_dir)
  if _e2e_dir not in sys.path:
    sys.path.insert(0, _e2e_dir)
  if _lib_dir not in sys.path:
    sys.path.insert(0, _lib_dir)
  from cdp_client import CDPClient  # type: ignore[no-redef]
  from cobalt_runner import CobaltRunner  # type: ignore[no-redef]
  from common import (  # type: ignore[no-redef]
      create_lifecycle_controller, ensure_display, get_executable_resolver,
      get_platform_display, is_display_working, resolve_executable,
  )
  from executable_resolver import ExecutableResolver  # type: ignore[no-redef]
  from lifecycle_controller import LifecycleController  # type: ignore[no-redef]
  from linux_executable_resolver import (  # type: ignore[no-redef]
      LinuxExecutableResolver,)
  from platform_display import PlatformDisplay  # type: ignore[no-redef]
  from posix_signal_lifecycle_controller import (  # type: ignore[no-redef]
      PosixSignalLifecycleController,)
  from simple_web_socket import SimpleWebSocket  # type: ignore[no-redef]
  from x11_display import X11Display  # type: ignore[no-redef]

__all__ = [
    'CDPClient',
    'CobaltRunner',
    'ExecutableResolver',
    'LifecycleController',
    'LinuxExecutableResolver',
    'PlatformDisplay',
    'PosixSignalLifecycleController',
    'SimpleWebSocket',
    'X11Display',
    'create_lifecycle_controller',
    'ensure_display',
    'get_executable_resolver',
    'get_platform_display',
    'is_display_working',
    'resolve_executable',
]
