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
"""GN script_executable wrapper for generating tvOS build graphs on Linux.

When `cobalt/build/gn.py` runs for a tvOS platform on a Linux host (for example,
to run `gn analyze` in CI), Apple Xcode/SDK CLI tools are not installed.
This wrapper intercepts GN `exec_script` calls to Chromium's Apple SDK discovery
scripts (`sdk_info.py` and `find_sdk.py`) and returns placeholder SDK metadata,
while forwarding all other scripts to Python unchanged.
"""

import os
import sys


def main() -> int:
  if sys.platform.startswith('linux') and len(sys.argv) >= 2:
    script_path = sys.argv[1].replace('\\', '/')
    if script_path.endswith('/build/config/apple/sdk_info.py'):
      print('machine_os_build="linux"')
      print('sdk_build="0"')
      print('sdk_path="//"')
      print('sdk_platform_path="//"')
      print('sdk_version="26.0"')
      print('toolchains_path="//"')
      print('xcode_build="0"')
      print('xcode_version="2600"')
      print('xcode_version_int=2600')
      return 0
    if script_path.endswith('/build/mac/find_sdk.py'):
      print('//')
      print('//')
      print('24E241')
      print('15.4')
      return 0

  os.execv(sys.executable, [sys.executable] + sys.argv[1:])
  return 0


if __name__ == '__main__':
  sys.exit(main())
