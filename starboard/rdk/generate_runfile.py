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

import os
import sys

sys.path.append(
    os.path.abspath(os.path.join(os.path.dirname(__file__), '../..')))
import starboard.build.util.generate_runfile

_RDK_PRE_RUN = """
# TODO: Remove this when westeros-init is started by default.
if subprocess.run(['pgrep', 'westeros-init'], capture_output=True).returncode != 0:
    subprocess.Popen(['/usr/bin/westeros-init'])
import time
time.sleep(2)
subprocess.run(['rdkDisplay', 'create'])
"""

_RDK_POST_RUN = """
subprocess.run(['rdkDisplay', 'remove'])
"""

if __name__ == '__main__':
  sys.argv.extend(['--pre-run', _RDK_PRE_RUN])
  sys.argv.extend(['--post-run', _RDK_POST_RUN])
  starboard.build.util.generate_runfile.generate()
