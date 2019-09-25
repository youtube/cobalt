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
import os
import subprocess

APP_LAUNCHER_TARGET = 'app_launcher_zip'


def BuildTargets(targets, out_directory, dry_run=False, extra_build_flags=[]):
  """Builds all specified targets.

  Args:
    extra_build_flags: Additional command line flags to pass to ninja.
  """
  if not targets:
    logging.info('No targets specified, nothing to build.')
    return

  args_list = ["ninja", "-C", out_directory]
  if dry_run:
    args_list.append("-n")

  args_list.append(APP_LAUNCHER_TARGET)
  args_list.extend(["{}_deploy".format(test_name) for test_name in targets])
  args_list.extend(extra_build_flags)

  if "TEST_RUNNER_BUILD_FLAGS" in os.environ:
    args_list.append(os.environ["TEST_RUNNER_BUILD_FLAGS"])

  logging.info('Building targets with command: %s', str(args_list))

  try:
    # We set shell=True because otherwise Windows doesn't recognize
    # PATH properly.
    #   https://bugs.python.org/issue15451
    # We flatten the arguments to a string because with shell=True, Linux
    # doesn't parse them properly.
    #   https://bugs.python.org/issue6689
    return subprocess.check_call(" ".join(args_list), shell=True)
  except KeyboardInterrupt:
    return 1
