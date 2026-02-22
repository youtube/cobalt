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
# TODO: Remove when lab devices are updated to RDK_V6_20251218+.
os.environ.update({
    "HOME": "{target_dir}/home",
    "XDG_RUNTIME_DIR": "/run",
    "LD_PRELOAD": "/usr/lib/libwesteros_gl.so.0.0.0",
    "WESTEROS_GL_USE_BEST_MODE": "1",
    "WESTEROS_GL_MAX_MODE": "3840x2160",
    "WESTEROS_GL_GRAPHICS_MAX_SIZE": "1920x1080",
    "WESTEROS_GL_USE_AMLOGIC_AVSYNC": "1",
    "WESTEROS_GL_REFRESH_PRIORITY": "F,80",
    "WESTEROS_SINK_AMLOGIC_USE_DMABUF": "1",
    "WESTEROS_SINK_USE_FREERUN": "1",
    "WESTEROS_SINK_USE_ESSRMGR": "1",
    "WESTEROS_GL_USE_REFRESH_LOCK": "1",
    "WESTEROS_GL_USE_UEVENT_HOTPLUG": "1",
    "RDKSHELL_KEYMAP_FILE": "/etc/rdkshell_keymapping.json",
    "ESSOS_NO_EVENT_LOOP_THROTTLE": "1",
    "AVPK_SKIP_HDMI_VALIDATION": "1",
    "WAYLAND_DISPLAY": "wayland-0",
    # TODO: Move?
    "ASAN_OPTIONS": "exitcode=0"
})

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
