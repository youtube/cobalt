#!/usr/bin/python

# Copyright 2021 The Cobalt Authors. All Rights Reserved.
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
"""Prepares the host for and then runs the Cobalt Evergreen automated tests."""

import argparse
import logging
import os
import subprocess
import sys

import _env  # pylint: disable=unused-import

from starboard.tools import abstract_launcher
from starboard.tools import command_line
from starboard.tools import log_level
from starboard.tools.paths import REPOSITORY_ROOT

_DEFAULT_PLATFORM_UNDER_TEST = 'linux'


def _Exec(cmd, env=None):
  try:
    msg = 'Executing:\n    ' + ' '.join(cmd)
    logging.info(msg)
    p = subprocess.Popen(  # pylint: disable=consider-using-with
        cmd,
        env=env,
        stderr=subprocess.STDOUT,
        universal_newlines=True)
    p.wait()
    return p.returncode
  except KeyboardInterrupt:
    p.kill()
    return 1


def main():
  arg_parser = argparse.ArgumentParser()
  arg_parser.add_argument(
      '--no-can_mount_tmpfs',
      dest='can_mount_tmpfs',
      action='store_false',
      help='A temporary filesystem cannot be mounted on the target device.')
  arg_parser.add_argument(
      '--platform_under_test',
      default=_DEFAULT_PLATFORM_UNDER_TEST,
      help='The platform to run the tests on (e.g., linux or raspi).')
  command_line.AddLauncherArguments(arg_parser)
  args = arg_parser.parse_args()

  log_level.InitializeLogging(args)

  launcher_params = command_line.CreateLauncherParams(arg_parser)

  # Creating an instance of the Evergreen abstract launcher implementation
  # generates a staging area in the 'out' directory with a predictable layout
  # and all of the desired binaries.
  launcher = abstract_launcher.LauncherFactory(
      launcher_params.platform,
      'cobalt',
      launcher_params.config,
      device_id=launcher_params.device_id,
      target_params=launcher_params.target_params,
      out_directory=launcher_params.out_directory,
      loader_platform=launcher_params.loader_platform,
      loader_config=launcher_params.loader_config,
      loader_target='loader_app',
      loader_out_directory=launcher_params.loader_out_directory)

  # The automated tests use the |OUT| environment variable as the path to a
  # known directory structure containing the desired binaries. This path is
  # generated in the Evergreen abstract launcher implementation.
  env = os.environ.copy()
  env['OUT'] = launcher.staging_directory

  args = arg_parser.parse_args()

  # The automated tests also use the |CAN_MOUNT_TMPFS| environment variable.
  # They assume a temporary filesystem can be mounted unless it's set to false.
  env['CAN_MOUNT_TMPFS'] = '1' if args.can_mount_tmpfs else '0'

  run_all_tests = os.path.join(REPOSITORY_ROOT,
                               'starboard/evergreen/testing/run_all_tests.sh')
  command = [run_all_tests, args.platform_under_test]

  if launcher.device_id:
    # The automated tests run on this host should target a remote device.
    command.append(launcher.device_id)

  return _Exec(command, env)


if __name__ == '__main__':
  sys.exit(main())
