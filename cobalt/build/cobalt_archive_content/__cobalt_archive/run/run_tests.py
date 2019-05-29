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

   This will invoke starboard/tools/testing/test_runner.py, which runs the
   specified test with a test filter list for the patform.

   Note that this is different from run.py will invoke
   starboard/tools/example/app_launcher_client.py which does NOT filter any
   tests.

   Example: python __cobalt_archive/run/run_tests.py nplb nb_test

   Example: python __cobalt_archive/run/run_tests.py (runs all tests)
"""


import sys
sys.path.insert(0, '.')
from impl import trampoline # pylint: disable=relative-import,g-import-not-at-top


TRAMPOLINE = [
    'python starboard/tools/testing/test_runner.py',
    '{target_names_arg}',
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
  # Interpret all tail args as the target_name param.
  tail_args = ['--target_name %s' % a for a in unresolve_args]
  resolved_cmd = ' '.join([resolved_cmd] + tail_args)
  return resolved_cmd


if __name__ == '__main__':
  trampoline.RunThenExit(_ResolveTrampoline())
