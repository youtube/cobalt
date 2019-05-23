#!/usr/bin/env python
#
# Copyright 2019 The Cobalt Authors. All Rights Reserved.
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


"""A trampoline that launches starboard/tools/example/app_launcher.py.


   Example: python __cobalt_archive/run/run.py -t cobalt
"""


import sys
sys.path.insert(0, '.')
import impl.trampoline  # pylint: disable=relative-import,g-import-not-at-top


TRAMPOLINE = [
    'python starboard/tools/example/app_launcher_client.py',
    '{target_names_arg}',
    '{platform_arg}',
    '{config_arg}',
    '{device_id_arg}',
    '{target_params_arg}',
]


if __name__ == '__main__':
  impl.trampoline.RunTrampolineThenExit(TRAMPOLINE)
