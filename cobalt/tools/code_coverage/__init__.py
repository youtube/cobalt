#!/usr/bin/env python3
# Copyright 2025 The Cobalt Authors. All Rights. Reserved.
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
"""
Init tests for the run_coverage.py script.
"""

import os
import sys

_SRC_ROOT_PATH = os.path.join(
    os.path.abspath(os.path.dirname(__file__)), os.path.pardir, os.path.pardir,
    os.path.pardir)
if _SRC_ROOT_PATH not in sys.path:
  sys.path.insert(0, _SRC_ROOT_PATH)
