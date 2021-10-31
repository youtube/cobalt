# Copyright 2018 The Cobalt Authors. All Rights Reserved.
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
"""Common code for building various types of targets, typically tests."""

import logging
import subprocess

APP_LAUNCHER_TARGET = 'app_launcher_zip'


def BuildTargets(targets, out_directory, dry_run=False, extra_build_flags=None):
  """Builds all specified targets.

  Args:
    targets: Gyp targets to be built
    out_directory: Directory to build to
    dry_run: Whether to run ninja with -n (dry run)
    extra_build_flags: Array of additional command line flags to pass to ninja

  Returns:
    The return code of builds, or 1 on unknown failure
  """
  if not targets:
    logging.info('No targets specified, nothing to build.')
    return

  args_list = ['ninja', '-C', out_directory]
  if dry_run:
    args_list.append('-n')

  args_list.append(APP_LAUNCHER_TARGET)
  args_list.extend(['{}_deploy'.format(test_name) for test_name in targets])
  if extra_build_flags:
    args_list.extend(extra_build_flags)

  logging.info('Building targets with command: %s', str(args_list))

  try:
    # We set shell=True because otherwise Windows doesn't recognize
    # PATH properly.
    #   https://bugs.python.org/issue15451
    # We flatten the arguments to a string because with shell=True, Linux
    # doesn't parse them properly.
    #   https://bugs.python.org/issue6689
    return subprocess.check_call(' '.join(args_list), shell=True)
  except KeyboardInterrupt:
    return 1
