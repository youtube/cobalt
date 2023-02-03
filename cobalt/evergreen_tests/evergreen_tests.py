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

from starboard.tools import abstract_launcher
from starboard.tools import command_line
from starboard.tools import log_level
from starboard.tools.paths import REPOSITORY_ROOT


def _Exec(cmd, env=None):
  """Executes a command in a subprocess and returns the result."""
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


def _RunTests(arg_parser, args, use_compressed_system_image):
  """Runs an instance of the Evergreen tests for the provided configuration."""
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
      loader_out_directory=launcher_params.loader_out_directory,
      use_compressed_library=use_compressed_system_image)

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

  # /bin/bash is executed since run_all_tests.sh may not be executable.
  command = ['/bin/bash', run_all_tests]

  if launcher.device_id:
    # The automated tests run on this host should target a remote device.
    command.append('-d')
    command.append(launcher.device_id)

  if args.public_key_auth:
    command.append('-a')
    command.append('public-key')
  elif args.password_auth:
    command.append('-a')
    command.append('password')

  if use_compressed_system_image:
    command.append('-c')

  command.append(args.loader_platform)

  return _Exec(command, env)


def main():
  arg_parser = argparse.ArgumentParser()
  arg_parser.add_argument(
      '--no-can_mount_tmpfs',
      dest='can_mount_tmpfs',
      action='store_false',
      help='A temporary filesystem cannot be mounted on the target device.')
  authentication_method = arg_parser.add_mutually_exclusive_group()
  authentication_method.add_argument(
      '--public-key-auth',
      help='Public key authentication should be used with the remote device.',
      action='store_true')
  authentication_method.add_argument(
      '--password-auth',
      help='Password authentication should be used with the remote device.',
      action='store_true')
  arg_parser.add_argument(
      '--no-rerun_using_compressed_system_image',
      dest='rerun_using_compressed_system_image',
      action='store_false',
      help='Do not run a second instance of the tests with a compressed system '
      'image.')
  command_line.AddLauncherArguments(arg_parser)
  args = arg_parser.parse_args()

  log_level.InitializeLogging(args)

  uncompressed_system_image_result = _RunTests(arg_parser, args, False)
  if args.rerun_using_compressed_system_image:
    compressed_system_image_result = _RunTests(arg_parser, args, True)
    return uncompressed_system_image_result or compressed_system_image_result

  return uncompressed_system_image_result


if __name__ == '__main__':
  sys.exit(main())
