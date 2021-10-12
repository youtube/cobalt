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
#
"""Abstraction for running Cobalt development tools."""

import abc
import os
import sys

import _env  # pylint: disable=unused-import
from starboard.tools import build
from starboard.tools import paths

ARG_NOINSTALL = "noinstall"
ARG_SYSTOOLS = "systools"
ARG_DRYRUN = "dryrun"


def _GetLauncherForPlatform(platform_name):
  """Gets the module containing a platform's concrete launcher implementation.

  Args:
    platform_name: Platform on which the app will be run, ex. "linux-x64x11".

  Returns:
    The module containing the platform's launcher implementation.
  """

  gyp_config = build.GetPlatformConfig(platform_name)
  if not hasattr(gyp_config, "GetLauncher"):
    return None
  else:
    return gyp_config.GetLauncher()


def LauncherFactory(platform_name,
                    target_name,
                    config,
                    device_id=None,
                    target_params=None,
                    output_file=None,
                    out_directory=None,
                    env_variables=None,
                    **kwargs):
  """Creates the proper launcher based upon command line args.

  Args:
    platform_name:  The platform on which the app will run.
    target_name:  The name of the executable target (ex. "cobalt").
    config:  Type of configuration used by the launcher (ex. "qa", "devel").
    device_id:  The identifier for the devkit being used.  Can be None.
    target_params: Command line arguments to the executable.  Can be None.
    output_file: Open file object used for storing the launcher's output. If
      None, sys.stdout is used.
    out_directory: Directory containing the executable target. If None is
      provided, the path to the directory is dynamically generated.
    env_variables:  Environment variables for the executable.
    **kwargs:  Additional parameters to be passed to the launcher.

  Returns:
    An instance of the concrete launcher class for the desired platform.

  Raises:
    RuntimeError: The platform does not exist, or there is no project root.
  """

  #  Creates launcher for provided platform if the platform has a valid port
  launcher_module = _GetLauncherForPlatform(platform_name)

  if not launcher_module:
    raise RuntimeError("Cannot load launcher for given platform.")

  return launcher_module.Launcher(
      platform_name,
      target_name,
      config,
      device_id,
      target_params=target_params,
      output_file=output_file,
      out_directory=out_directory,
      env_variables=env_variables,
      **kwargs)


class AbstractLauncher(object):
  """Class that specifies all required behavior for Cobalt app launchers."""

  __metaclass__ = abc.ABCMeta

  def __init__(self, platform_name, target_name, config, device_id, **kwargs):
    self.platform_name = platform_name
    self.target_name = target_name
    self.config = config
    self.device_id = device_id

    #  The following pattern makes sure that variables will be initialized
    #  properly whether a kwarg is passed in with a value of None or it
    #  is not passed in at all.
    out_directory = kwargs.get("out_directory", None)
    if not out_directory:
      out_directory = paths.BuildOutputDirectory(platform_name, config)
    self.out_directory = out_directory
    self.coverage_directory = kwargs.get("coverage_directory", out_directory)

    output_file = kwargs.get("output_file", None)
    if not output_file:
      output_file = sys.stdout
    self.output_file = output_file

    target_command_line_params = kwargs.get("target_params", None)
    if target_command_line_params is None:
      target_command_line_params = []
    self.target_command_line_params = target_command_line_params

    env_variables = kwargs.get("env_variables", None)
    if env_variables is None:
      env_variables = {}
    self.env_variables = env_variables

    launcher_args = kwargs.get("launcher_args", None)
    if launcher_args is None:
      launcher_args = []
    self.launcher_args = launcher_args

    # Launchers that need different startup timeout times should reassign
    # this variable during initialization.
    self.startup_timeout_seconds = 2 * 60

  @abc.abstractmethod
  def Run(self):
    """Runs the launcher's executable.

    Must be implemented in subclasses.

    Returns:
      The return code from the launcher's executable.
    """
    pass

  @abc.abstractmethod
  def Kill(self):
    """Kills the launcher. Must be implemented in subclasses."""
    pass

  @abc.abstractmethod
  def GetDeviceIp(self):
    """Gets the device IP. Must be implemented in subclasses."""
    pass

  @abc.abstractmethod
  def GetDeviceOutputPath(self):
    """Writable path where test targets can output files"""
    pass

  def SupportsSuspendResume(self):
    return False

  # The Send*() functions are guaranteed to resolve sequences of calls (e.g.
  # SendSuspend() followed immediately by SendResume()) in the order that they
  # are sent.
  def SendResume(self):
    """Sends resume signal to the launcher's executable.

    Raises:
      RuntimeError: Resume signal not supported on platform.
    """
    raise RuntimeError("Resume not supported for this platform.")

  def SendSuspend(self):
    """sends suspend signal to the launcher's executable.

    When implementing this function, please coordinate with other functions
    sending platform signals(e.g. SendResume()) to ensure Cobalt receives these
    signals in the same order they are sent.

    Raises:
      RuntimeError: Suspend signal not supported on platform.
    """
    raise RuntimeError("Suspend not supported for this platform.")

  def SendConceal(self):
    """sends conceal signal to the launcher's executable.

    Raises:
      RuntimeError: Conceal signal not supported on platform.
    """
    raise RuntimeError("Conceal not supported for this platform.")

  def SendFocus(self):
    """sends focus signal to the launcher's executable.

    Raises:
      RuntimeError: focus signal not supported on platform.
    """
    raise RuntimeError("Focus not supported for this platform.")

  def SendFreeze(self):
    """sends freeze signal to the launcher's executable.

    Raises:
      RuntimeError: Freeze signal not supported on platform.
    """
    raise RuntimeError("Freeze not supported for this platform.")

  def SendStop(self):
    """sends stop signal to the launcher's executable.

    Raises:
      RuntimeError: Stop signal not supported on platform.
    """
    raise RuntimeError("Stop not supported for this platform.")

  def SupportsDeepLink(self):
    return False

  def SendDeepLink(self, link):
    """Sends deep link to the launcher's executable.

    Args:
      link:  Link to send to the executable.

    Returns:
      True if the link was sent, False if it wasn't.

    Raises:
      RuntimeError: Deep link not supported on platform.
    """
    raise RuntimeError(
        "Deep link not supported for this platform (link {} sent).".format(
            link))

  # Not like SendSuspendResume sending signals to cobalt, system suspend and
  # resume send system signals to suspend and resume cobalt process.
  def SupportsSystemSuspendResume(self):
    return False

  def SendSystemResume(self):
    """sends a system signal to the resume the launcher's executable.

    Raises:
      RuntimeError: System resume signal not supported on platform.
    """
    raise RuntimeError("System resume signal not supported for this platform.")

  def SendSystemSuspend(self):
    """sends a system signal to suspend the current running executable.

    Raises:
      RuntimeError: System suspend signal not supported on platform.
    """
    raise RuntimeError("System suspend signal not supported for this platform.")

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

  def GetTargetPath(self):
    """Constructs the path to an executable target.

    The default path returned by this method takes the form of:

      "/path/to/out/<platform>_<config>/target_name"

    Returns:
      The path to an executable target.
    """
    return os.path.abspath(os.path.join(self.out_directory, self.target_name))
