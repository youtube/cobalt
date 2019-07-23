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


"""A trampoline for running the unit tests from a Cobalt Archive.

   This will invoke starboard/tools/testing/test_runner.py with
   --platform_tests_only, which only runs a subset of tests related to platform
   include nplb, starboard_platform_tests and others.

   Example: python __cobalt_archive/run/run_platform_tests.py
"""


import sys
sys.path.insert(0, '.')
from impl import trampoline # pylint: disable=relative-import,g-import-not-at-top


TRAMPOLINE = [
    'python starboard/tools/testing/test_runner.py',
    '--platform_tests_only',
    '{platform_arg}',
    '{config_arg}',
    '{device_id_arg}',
    '{target_params_arg}',
]


def _ResolveTrampoline(argv=None):
  if argv == None:
    argv = sys.argv[1:]
  resolved_cmd, unresolve_args = trampoline.ResolveTrampoline(
      TRAMPOLINE, argv=argv)
  return resolved_cmd


if __name__ == '__main__':
  trampoline.RunThenExit(_ResolveTrampoline())
