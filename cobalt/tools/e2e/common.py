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
"""E2E test utilities, runner, and platform facade for Cobalt."""

import logging
from typing import List, Optional, Union

try:
  from cobalt.tools.e2e.cobalt_runner import CobaltRunner
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
  import os
  import sys
  _e2e_dir = os.path.dirname(__file__)
  _tools_dir = os.path.dirname(_e2e_dir)
  _lib_dir = os.path.join(_tools_dir, 'lib')
  if _e2e_dir not in sys.path:
    sys.path.insert(0, _e2e_dir)
  if _lib_dir not in sys.path:
    sys.path.insert(0, _lib_dir)
  from cdp_client import CDPClient  # type: ignore[no-redef]
  from cobalt_runner import CobaltRunner  # type: ignore[no-redef]
  from executable_resolver import ExecutableResolver  # type: ignore[no-redef]
  from lifecycle_controller import LifecycleController  # type: ignore[no-redef]
  from linux_executable_resolver import (  # type: ignore[no-redef]
      LinuxExecutableResolver,)
  from platform_display import PlatformDisplay  # type: ignore[no-redef]
  from posix_signal_lifecycle_controller import (  # type: ignore[no-redef]
      PosixSignalLifecycleController,)
  from simple_web_socket import SimpleWebSocket  # type: ignore[no-redef]
  from x11_display import X11Display  # type: ignore[no-redef]

logger = logging.getLogger('cobalt_e2e_common')

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


def get_platform_display(platform: Optional[str] = None) -> PlatformDisplay:
  """Returns the PlatformDisplay implementation for the specified platform.

  Args:
    platform: Optional platform identifier string.

  Returns:
    A PlatformDisplay instance for the target platform.
  """
  # Default to X11; future platforms (Wayland, Android, RDK) will be added here.
  del platform
  return X11Display()


def get_executable_resolver(
    platform: Optional[str] = None) -> ExecutableResolver:
  """Returns the ExecutableResolver implementation for the platform.

  Args:
    platform: Optional platform identifier string.

  Returns:
    An ExecutableResolver instance configured for the platform.
  """
  return LinuxExecutableResolver(platform=platform)


def create_lifecycle_controller(
    platform: Optional[str] = None,
    runner: Optional[CobaltRunner] = None) -> LifecycleController:
  """Factory returning the appropriate LifecycleController for the platform.

  Args:
    platform: Target platform identifier.
    runner: Running Cobalt instance manager.

  Returns:
    A LifecycleController instance.

  Raises:
    ValueError: If runner is None.
  """
  del platform  # Unused until Android/RDK adapters are added.
  if runner is None:
    raise ValueError(
        'runner must not be None for PosixSignalLifecycleController')
  return PosixSignalLifecycleController(runner)


def is_display_working(platform: Optional[str] = None) -> bool:
  """Checks if a functional display is currently available.

  Args:
    platform: Optional platform identifier.

  Returns:
    True if a display is working and usable, False otherwise.
  """
  return get_platform_display(platform).is_working()


def ensure_display(platform: Optional[str] = None) -> None:
  """Ensures working display is present; re-execs with virtual display.

  Args:
    platform: Optional platform identifier.
  """
  get_platform_display(platform).ensure_display()


def resolve_executable(platform: Optional[str] = None,
                       config: str = 'qa',
                       custom_executable: Optional[str] = None,
                       out_dir: Optional[str] = None) -> Union[str, List[str]]:
  """Resolves the executable command based on configuration.

  Args:
    platform: Platform identifier ('modular', 'evergreen', etc.).
    config: Build configuration ('qa', 'devel', etc.).
    custom_executable: Explicit user-specified executable path or command.
    out_dir: Custom output directory to search.

  Returns:
    A path or command string / list to launch Cobalt.

  Raises:
    FileNotFoundError: If no suitable executable can be found.
  """
  resolver = get_executable_resolver(platform)
  return resolver.resolve(
      config=config, custom_executable=custom_executable, out_dir=out_dir)
