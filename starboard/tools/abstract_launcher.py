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


def GetGypModuleForPlatform(platform):
  """Gets the module containing a platform's GYP configuration.

  Args:
    platform:  Platform on which the app will be run, ex. "linux-x64x11".

  Returns:
    The module containing the platform's GYP configuration.

  Raises:
    RuntimeError:  The specified platform does not exist.
  """
  platform_dict = platform_module.GetAllPorts()
  if platform in platform_dict:
    platform_path = platform_dict[platform]
    if platform_path not in sys.path:
      sys.path.append(platform_path)
    gyp_module = importlib.import_module("gyp_configuration")
    return gyp_module
  else:
    raise RuntimeError("Specified platform does not exist.")


def _GetLauncherForPlatform(platform):
  """Gets the module containing a platform's concrete launcher implementation.

  Args:
    platform: Platform on which the app will be run, ex. "linux-x64x11".

  Returns:
    The module containing the platform's launcher implementation.
  """

  gyp_config = GetGypModuleForPlatform(platform).CreatePlatformConfig()
  if not hasattr(gyp_config, "GetLauncher"):
    return None
  else:
    return gyp_config.GetLauncher()


def DynamicallyBuildOutDirectory(platform, config):
  """Constructs the location used to store executable targets/their components.

  Args:
    platform: The platform to run the executable on, ex. "linux-x64x11".
    config: The build configuration, ex. "qa".

  Returns:
    The path to the directory containing executables and/or their components.
  """
  path = os.path.abspath(
      os.path.join(os.path.dirname(__file__),
                   os.pardir, os.pardir, "out",
                   "{}_{}".format(platform, config)))
  return path


def GetDefaultTargetPath(platform, config, target_name):
  """Constructs the default path to an executable target.

  The default path takes the form of:

    "/path/to/out/<platform>_<config>/target_name"

  Args:
    platform: The platform to run the executable on, ex. "linux-x64x11".
    config: The build configuration, ex. "qa".
    target_name:  The name of the executable target, ex. "cobalt"

  Returns:
    The path to an executable target.
  """
  return os.path.join(DynamicallyBuildOutDirectory(platform, config),
                      target_name)


def LauncherFactory(platform, target_name, config, device_id, args,
                    output_file=sys.stdout, out_directory=None):
  """Creates the proper launcher based upon command line args.

  Args:
    platform:  The platform on which the app will run.
    target_name:  The name of the executable target (ex. "cobalt").
    config:  Type of configuration used by the launcher (ex. "qa", "devel").
    device_id:  The identifier for the devkit being used.
    args:  Any extra arguments to be passed on a platform-specific basis.
    output_file:  The open file to which the launcher should write its output.
    out_directory:  Path to directory where tool/test targets and/or their
      components are stored.

  Returns:
    An instance of the concrete launcher class for the desired platform.

  Raises:
    RuntimeError: The platform does not exist, or there is no project root.
  """

  if not out_directory:
    out_directory = DynamicallyBuildOutDirectory(platform, config)

  #  Creates launcher for provided platform if the platform has a valid port
  launcher_module = _GetLauncherForPlatform(platform)

  #  TODO: Refactor when all old launchers have been deleted
  #  If a platform that does not have a new launcher is provided, attempt
  #  to create an adapter to the old one.
  if not launcher_module:
    old_path = os.path.abspath(os.path.join(os.path.dirname(__file__),
                                            os.pardir, os.pardir, "tools",
                                            "lbshell"))
    if os.path.exists(os.path.join(old_path, "app_launcher.py")):
      sys.path.append(old_path)
      bridge_module = importlib.import_module("app_launcher_bridge")
      return bridge_module.LauncherAdapter(platform, target_name, config,
                                           device_id, args, output_file,
                                           out_directory)
    else:
      raise RuntimeError("No launcher implemented for the given platform.")
  else:
    return launcher_module.Launcher(platform, target_name, config, device_id,
                                    args, output_file, out_directory)


class AbstractLauncher(object):
  """Class that specifies all required behavior for Cobalt app launchers."""

  __metaclass__ = abc.ABCMeta

  def __init__(self, platform, target_name, config, device_id,
               args, output_file, out_directory):
    self.platform = platform
    self.target_name = target_name
    self.config = config
    self.device_id = device_id
    self.args = args
    self.out_directory = out_directory
    self.output_file = output_file
    self.target_command_line_params = []
    if "target_params" in args:
      self.target_command_line_params.extend(args["target_params"])

    # Launchers that need different startup timeout times should reassign
    # this variable during initialization.
    self.startup_timeout_seconds = 2 * 60

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

  def GetStartupTimeout(self):
    """Gets the number of seconds to wait before assuming a launcher timeout."""
    return self.startup_timeout_seconds

  def GetHostAndPortGivenPort(self, port):
    """Creates a host/port tuple for use on the target device.

    This is used to gain access to a service on the target device, and
    can be overridden when the host/port of that service is only accessible
    at runtime and/or via forwarding.

    Args:
      port:  Port number for the desired service on the target device.

    Returns:
      (Host, port) tuple for use in connecting to the target device.
    """
    return self.device_id, port
