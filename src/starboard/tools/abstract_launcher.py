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
"""Abstraction for running Cobalt development tools."""

import importlib
import os
import sys

if "environment" in sys.modules:
  environment = sys.modules["environment"]
else:
  env_path = os.path.abspath(os.path.dirname(__file__))
  if env_path not in sys.path:
    sys.path.append(env_path)
  environment = importlib.import_module("environment")


import abc

import starboard.tools.platform as platform_module


def _GetLauncherForPlatform(platform_path):
  """Gets the module containing a platform's concrete launcher implementation.

  Args:
    platform_path:  Path to the location of a valid starboard platform.

  Returns:
    The module containing the platform's launcher implementation.
  """
  if platform_path not in sys.path:
    sys.path.append(platform_path)
  gyp_module = importlib.import_module("gyp_configuration")
  return gyp_module.CreatePlatformConfig().GetLauncher()


def LauncherFactory(platform, target_name, config, device_id, args):
  """Creates the proper launcher based upon command line args.

  Args:
    platform:  The platform on which the app will run
    target_name:  The name of the executable target (ex. "cobalt")
    config:  Type of configuration used by the launcher (ex. "qa", "devel")
    device_id:  The identifier for the devkit being used
    args:  Any extra arguments to be passed on a platform-specific basis

  Returns:
    An instance of the concrete launcher class for the desired platform

  Raises:
    RuntimeError: The platform does not exist, or there is no project root.
  """

  #  Creates launcher for provided platform if the platform has a valid port
  platform_dict = platform_module.GetAllPorts()
  if platform in platform_dict:
    platform_path = platform_dict[platform]
    launcher_module = _GetLauncherForPlatform(platform_path)
    return launcher_module.Launcher(platform, target_name, config, device_id,
                                    args)
  else:
    raise RuntimeError("Specified platform does not exist.")


class AbstractLauncher(object):
  """Class that specifies all required behavior for Cobalt app launchers."""

  __metaclass__ = abc.ABCMeta

  def __init__(self, platform, target_name, config, device_id, args):
    self.platform = platform
    self.target_name = target_name
    self.config = config
    self.device_id = device_id
    self.args = args
    self.target_command_line_params = []
    if "target_params" in args:
      self.target_command_line_params.extend(args["target_params"])

  @abc.abstractmethod
  def Run(self):
    """Runs the launcher's executable.  Must be implemented in subclasses.

    Returns:
      The return code from the launcher's executable.
    """
    pass

  @abc.abstractmethod
  def Kill(self):
    """Kills the launcher. Must be implemented in subclasses."""
    pass
