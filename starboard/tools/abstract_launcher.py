#!/usr/bin/python
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
# limitations under the License."""
"""Abstraction for running Cobalt development tools."""

import abc
import imp
import os
import re
import sys

platform_module = imp.load_source(
    "platform", os.path.abspath(
        os.path.join(os.path.dirname(__file__), "platform.py")))


def _GetAllPlatforms(path):
  """Retrieves information about all available Cobalt ports.

  Args:
    path:  Root path that will be crawled to find ports.

  Returns:
    Dict mapping available cobalt ports to their respective location in
    the filesystem.
  """
  platform_dict = {}
  for port in platform_module.PlatformInfo.EnumeratePorts(path):
    port_name = re.search(".*starboard-(.*)", port.port_name).group(1)
    platform_dict[port_name] = port.path
  return platform_dict


def _GetProjectRoot():
  """Gets the root of this project.

  Returns:
    Path to the root of this project

  Raises:
    RuntimeError: There is no root.
  """
  current_path = os.path.normpath(os.path.dirname(__file__))
  while not os.access(os.path.join(current_path, ".gclient"), os.R_OK):
    next_path = os.path.dirname(current_path)
    if next_path == current_path:
      current_path = None
      break
    current_path = next_path
  client_root = current_path

  if not client_root:
    raise RuntimeError("No project root declared.")

  return client_root


def _GetLauncherForPlatform(platform_path):
  """Gets the module containing a platform's concrete launcher implementation.

  Args:
    platform_path:  Path to the location of a valid starboard platform.

  Returns:
    The module containing the platform's launcher implementation.
  """

  # Necessary because gyp_configuration modules use relative imports.
  # "cobalt/build" needs to be in sys.path to keep the imports working
  sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__),
                                               os.pardir, os.pardir,
                                               "cobalt", "build")))

  # Necessary because the gyp_configuration for linux-x64x11 imports
  # directly from "starboard/".  All of this import logic will eventually be
  # moved to a configuration system.
  sys.path.append(os.path.abspath(os.path.join(
      os.path.dirname(__file__), os.pardir, os.pardir)))

  module_path = os.path.abspath(os.path.join(platform_path,
                                             "gyp_configuration.py"))

  gyp_module = imp.load_source("platform_module", module_path)
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
  client_root = _GetProjectRoot()
  platform_dict = _GetAllPlatforms(client_root)
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
    """Runs the launcher's executable.  Must be implemented in subclasses."""
    pass

  @abc.abstractmethod
  def Kill(self):
    """Kills the launcher. Must be implemented in subclasses."""
    pass
