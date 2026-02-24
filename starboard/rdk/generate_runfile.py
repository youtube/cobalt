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
"""
Generate runfiles for evergreen on rdk.
"""
import argparse
import os
import stat

_TEMPLATE = """#!/usr/bin/env python3
# TODO: Remove when lab devices are updated to RDK_V6_20251218+.
os.environ.update({{
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
    "ASAN_OPTIONS": "exitcode=0",
}})

# TODO: Remove this when westeros-init is started by default.
if subprocess.run(['pgrep', 'westeros-init'], capture_output=True).returncode != 0:
    subprocess.Popen(['/usr/bin/westeros-init'])
import time
time.sleep(2)

subprocess.run(['rdkDisplay', 'create'])

import os
import subprocess
import sys

command = [
    os.path.join(os.path.dirname(__file__), 'elf_loader_sandbox'),
    '--evergreen_content=.', '--evergreen_library={library}.so'
] + sys.argv[1:]
try:
    result = subprocess.run(command, check=False)
    sys.exit(result.returncode)
except subprocess.CalledProcessError:
    # A subprocess failed, so don't log the python traceback.
    raise SystemExit(1)
except Exception as e:
    print("An unexpected error occurred: " + str(e), file=sys.stderr)
    sys.exit(1)

subprocess.run(['rdkDisplay', 'remove'])
"""

parser = argparse.ArgumentParser()
parser.add_argument('--output', type=str, required=True)
parser.add_argument('--library', type=str, required=True)
args = parser.parse_args()

with open(args.output, 'w', encoding='utf-8') as f:
  target_dir = os.path.dirname(args.output)
  f.write(_TEMPLATE.format(target_dir=target_dir, library=args.library))
current_permissions = stat.S_IMODE(os.stat(args.output).st_mode)
new_permissions = current_permissions | stat.S_IXUSR | stat.S_IXGRP
os.chmod(args.output, new_permissions)
