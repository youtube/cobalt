#!/usr/bin/python
#
# Copyright 2017 The Cobalt Authors. All Rights Reserved.
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
"""Client to launch executables via the new launcher logic."""

import argparse
import logging
import signal
import sys

import _env  # pylint: disable=unused-import, relative-import
from starboard.tools import abstract_launcher
from starboard.tools import command_line
from starboard.tools.util import SetupDefaultLoggingConfig


def main():
  SetupDefaultLoggingConfig()
  arg_parser = argparse.ArgumentParser()
  command_line.AddLauncherArguments(arg_parser)
  arg_parser.add_argument(
      "-t", "--target_name", required=True, help="Name of executable target.")
  launcher_params = command_line.CreateLauncherParams(arg_parser)

  launcher = abstract_launcher.LauncherFactory(
      launcher_params.platform,
      launcher_params.target_name,
      launcher_params.config,
      device_id=launcher_params.device_id,
      target_params=launcher_params.target_params,
      out_directory=launcher_params.out_directory,
      loader_platform=launcher_params.loader_platform,
      loader_config=launcher_params.loader_config,
      loader_out_directory=launcher_params.loader_out_directory)

  def Abort(signum, frame):
    del signum, frame  # Unused.
    sys.stderr.write("Killing thread\n")
    launcher.Kill()
    sys.exit(1)

  signal.signal(signal.SIGINT, Abort)

  return launcher.Run()


if __name__ == "__main__":
  sys.exit(main())
