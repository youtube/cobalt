#!/usr/bin/env python3
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
"""Backward-compatibility shim for junit_mini_parser_test."""

import os
import sys
import unittest

_lib_dir = os.path.join(os.path.dirname(__file__), 'lib')
if _lib_dir not in sys.path:
  sys.path.insert(0, _lib_dir)

try:
  from cobalt.tools.lib.junit_mini_parser_test import *  # pylint: disable=wildcard-import
except ImportError:
  from junit_mini_parser_test import *  # type: ignore[no-redef] # pylint: disable=wildcard-import

if __name__ == '__main__':
  unittest.main()
