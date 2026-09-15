#!/usr/bin/env vpython3
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
"""Backward-compatibility shim for cdp_js_helper."""

import sys

try:
  from cobalt.tools.lib.cdp_js_helper import (
      evaluate,
      get_websocket_url,
      main,
      wait_for_cdp,
  )
except ImportError:
  import os
  _lib_dir = os.path.join(os.path.dirname(__file__), 'lib')
  if _lib_dir not in sys.path:
    sys.path.insert(0, _lib_dir)
  from cdp_js_helper import (  # type: ignore[no-redef]
      evaluate, get_websocket_url, main, wait_for_cdp,
  )

__all__ = ['evaluate', 'get_websocket_url', 'main', 'wait_for_cdp']

if __name__ == '__main__':
  main()
