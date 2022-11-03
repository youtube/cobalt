#
# Copyright 2016 Google Inc. All Rights Reserved.
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
#
"""Utility to prepend the top-level source directory to sys.path.

Since this may be used outside of gclient or git environment (such as part of a
tarball), the path to the root must be hardcoded.
"""

import os
import sys

import os
import sys

_SCRIPT_DIR = os.path.dirname(__file__)
_TOP_LEVEL_DIR = os.path.join(_SCRIPT_DIR, os.pardir, os.pardir, os.pardir)
sys.path.insert(0, _TOP_LEVEL_DIR)

# Add blink's python tools to the path.

sys.path.append(
    os.path.normpath(
        os.path.join(_TOP_LEVEL_DIR, 'third_party', 'blink', 'Tools',
                     'Scripts')))

sys.path.append(
    os.path.normpath(
        os.path.join(_TOP_LEVEL_DIR, 'third_party', 'blink', 'Source',
                     'bindings', 'scripts')))
