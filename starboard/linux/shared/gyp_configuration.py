# Copyright 2017 The Cobalt Authors. All Rights Reserved.
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
"""Starboard Linux shared platform configuration."""

import os

from starboard.build import platform_configuration


class LinuxConfiguration(platform_configuration.PlatformConfiguration):
  """Starboard Linux platform configuration."""

  def __init__(self, platform):
    super(LinuxConfiguration, self).__init__(platform)
    self.AppendApplicationConfigurationPath(os.path.dirname(__file__))

  def GetLauncherPath(self):
    """Gets the path to the launcher module for this platform."""
    return os.path.dirname(__file__)

  def GetTestEnvVariables(self):
    # Due to fragile nature of dynamic TLS tracking, in particular LSAN reliance
    # on GLIBC private APIs, tracking TLS leaks is unstable and can trigger
    # sporadic false positives and/or crashes, depending on the used compiler,
    # direct library dependencies and runtime-loaded dynamic libraries. Hence,
    # TLS leak tracing in LSAN is turned off.
    # For reference, https://sourceware.org/ml/libc-alpha/2018-02/msg00567.html
    # When failing, 'LeakSanitizer has encountered a fatal error' message would
    # be printed at test shutdown, and env var LSAN_OPTIONS=verbosity=2 would
    # further point to 'Scanning DTLS range ..' prior to crash.
    return {'ASAN_OPTIONS': 'intercept_tls_get_addr=0'}
