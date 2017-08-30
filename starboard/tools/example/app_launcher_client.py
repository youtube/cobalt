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

import argparse

from starboard.tools import abstract_launcher

arg_parser = argparse.ArgumentParser(
    description="Runs application/tool executables.")
arg_parser.add_argument(
    "-p",
    "--platform",
    help="Device platform, eg 'linux-x64x11'.")
arg_parser.add_argument(
    "-t",
    "--target_name",
    help="Name of executable.")
arg_parser.add_argument(
    "-c",
    "--config",
    choices=["debug", "devel", "qa", "gold"],
    help="Build config (eg, 'qa' or 'devel')")
arg_parser.add_argument(
    "-d",
    "--device_id",
    help="Devkit or IP address for the target device.")
arg_parser.add_argument(
    "--target_params",
    help="Command line arguments to pass to the executable."
         " Because different executables could have differing command"
         " line syntax, list all arguments exactly as you would to the"
         " executable between a set of double quotation marks.")


def main():
  args = arg_parser.parse_args()
  extra_args = {}

  if not args.device_id:
    args.device_id = None

  extra_args = {}
  if args.target_params:
    extra_args["target_params"] = args.target_params.split(" ")

  launcher = abstract_launcher.LauncherFactory(args.platform,
                                               args.target_name, args.config,
                                               args.device_id, extra_args)
  return launcher.Run()

if __name__ == "__main__":
  sys.exit(main())
