#!/usr/bin/python
#
# Copyright 2017 Google Inc. All Rights Reserved.
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

import importlib
import os
import sys

if "environment" in sys.modules:
  environment = sys.modules["environment"]
else:
  env_path = os.path.abspath(
      os.path.join(os.path.dirname(__file__), os.pardir))
  if env_path not in sys.path:
    sys.path.append(env_path)
  environment = importlib.import_module("environment")

from starboard.tools import abstract_launcher
from starboard.tools import command_line


def main():
  parser = command_line.CreateParser()
  args = parser.parse_args()
  extra_args = {}

  if not args.device_id:
    args.device_id = None

  extra_args = {}
  if args.target_params:
    extra_args["target_params"] = args.target_params.split(" ")

  launcher = abstract_launcher.LauncherFactory(
      args.platform, args.target_name, args.config,
      args.device_id, extra_args, out_directory=args.out_directory)
  return launcher.Run()

if __name__ == "__main__":
  sys.exit(main())
