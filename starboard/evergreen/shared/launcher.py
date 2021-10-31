# Copyright 2019 The Cobalt Authors. All Rights Reserved.
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
"""Evergreen implementation of Starboard launcher abstraction."""

import logging
import os
import shutil

import _env  # pylint: disable=unused-import
from starboard.tools import abstract_launcher
from starboard.tools import paths
from starboard.tools import port_symlink

_BASE_STAGING_DIRECTORY = 'evergreen_staging'
_CRASHPAD_TARGET = 'crashpad_handler'
_DEFAULT_LOADER_TARGET = 'elf_loader_sandbox'


class Launcher(abstract_launcher.AbstractLauncher):
  """Class for launching Evergreen targets on arbitrary platforms.

  This Evergreen launcher leverages the platform-specific launchers to start the
  Evergreen loader on a particular device. A staging directory is built that
  resembles a typical deploy directory, with the Evergreen target and its
  content included in the Evergreen loader's content. The platform-specific
  launcher, given command-line switches to tell the Evergreen loader where the
  Evergreen target and its content are, can run the loader without needing to
  know about Evergreen per-se.

  The Evergreen launcher does not require its own implementation for many
  abstract launcher methods and trampolines them to the platform-specific
  launcher.
  """

  def __init__(self, platform, target_name, config, device_id, **kwargs):
    # TODO: Remove this injection of 'detect_leaks=0' once the memory leaks when
    #       running executables in Evergreen mode have been resolved.
    env_variables = kwargs.get('env_variables') or {}
    asan_options = env_variables.get('ASAN_OPTIONS', '')
    asan_options = [
        opt for opt in asan_options.split(':') if 'detect_leaks' not in opt
    ]
    asan_options.append('detect_leaks=0')
    env_variables['ASAN_OPTIONS'] = ':'.join(asan_options)
    kwargs['env_variables'] = env_variables

    # pylint: disable=super-with-arguments
    super(Launcher, self).__init__(platform, target_name, config, device_id,
                                   **kwargs)

    self.loader_platform = kwargs.get('loader_platform')
    if not self.loader_platform:
      raise ValueError('|loader_platform| cannot be |None|.')

    self.loader_config = kwargs.get('loader_config')
    if not self.loader_config:
      raise ValueError('|loader_config| cannot be |None|.')

    self.loader_target = kwargs.get('loader_target')
    if not self.loader_target:
      self.loader_target = _DEFAULT_LOADER_TARGET

    self.loader_out_directory = kwargs.get('loader_out_directory')
    if not self.loader_out_directory:
      self.loader_out_directory = paths.BuildOutputDirectory(
          self.loader_platform, self.loader_config)

    # The relationship of loader platforms and configurations to evergreen
    # platforms and configurations is many-to-many. We need a separate directory
    # for each of them, i.e. linux-x64x11_debug__evergreen-x64_gold.
    self.combined_config = '{}_{}__{}_{}'.format(self.loader_platform,
                                                 self.loader_config, platform,
                                                 config)

    self.staging_directory = os.path.join(
        os.path.dirname(self.out_directory), _BASE_STAGING_DIRECTORY,
        self.combined_config)

    # TODO: Figure out a place for the launcher to do this prep work that is not
    # part of the constructor.
    self._StageTargetsAndContents()

    # Ensure the path, relative to the content of the ELF Loader, to the
    # Evergreen target and its content are passed as command line switches.
    target_command_line_params = [
        '--evergreen_library=app/{}/lib/lib{}.so'.format(
            self.target_name, self.target_name),
        '--evergreen_content=app/{}/content'.format(self.target_name)
    ]

    if self.target_command_line_params:
      target_command_line_params.extend(self.target_command_line_params)

    self.launcher = abstract_launcher.LauncherFactory(
        self.loader_platform,
        self.loader_target,
        self.loader_config,
        device_id,
        target_params=target_command_line_params,
        output_file=self.output_file,
        out_directory=self.staging_directory,
        coverage_directory=self.coverage_directory,
        env_variables=self.env_variables,
        log_targets=False)

  def Run(self):
    """Redirects to the ELF Loader platform's abstract loader implementation."""

    return_code = 1

    logging.info('-' * 32)
    logging.info('Starting to run target: %s', self.target_name)
    logging.info('=' * 32)

    try:

      return_code = self.launcher.Run()
    except Exception:  # pylint: disable=broad-except
      logging.exception('Error occurred while running test.')

    logging.info('-' * 32)
    logging.info('Finished running target: %s', self.target_name)
    logging.info('=' * 32)

    return return_code

  def Kill(self):
    """Redirects to the ELF Loader platform's abstract loader implementation."""
    self.launcher.Kill()

  def _StageTargetsAndContents(self):
    """Stage targets and their contents in our staging directory.

    The existing launcher infrastructure expects a specific directory structure
    regardless of whether or not we are running locally:

             platform:    raspi-2          raspi-2_devel
             config:      devel             +-- deploy
             target_name: nplb                   +-- nplb
                                                      +-- content
                                                      +-- nplb

    This function effectively builds a directory structure that matches both of
    these expectations while minimizing the hard copies made.

    Note: The Linux launcher does not yet look in the deploy directory and
          instead runs the target from the top of its out-directory. This will
          be changed in the future.
    """

    if os.path.exists(self.staging_directory):
      shutil.rmtree(self.staging_directory)
    os.makedirs(self.staging_directory)

    # <outpath>/deploy/elf_loader_sandbox
    staging_directory_loader = os.path.join(self.staging_directory, 'deploy',
                                            self.loader_target)

    # <outpath>/deploy/elf_loader_sandbox/content/app/nplb/
    staging_directory_evergreen = os.path.join(staging_directory_loader,
                                               'content', 'app',
                                               self.target_name)

    # Make a hard copy of the ELF Loader's deploy directory in the location
    # specified by |staging_directory_loader|. A symbolic link here would cause
    # future symbolic links to fall through to the original out-directories.
    shutil.copytree(
        os.path.join(self.loader_out_directory, 'deploy', self.loader_target),
        staging_directory_loader)
    shutil.copy(
        os.path.join(self.loader_out_directory, 'deploy', _CRASHPAD_TARGET,
                     _CRASHPAD_TARGET), staging_directory_loader)

    port_symlink.MakeSymLink(
        os.path.join(self.out_directory, 'deploy', self.target_name),
        staging_directory_evergreen)

    # TODO: Make the Linux launcher run from the deploy directory, no longer
    #       create these symlinks, and remove the NOTE from the docstring.
    port_symlink.MakeSymLink(
        os.path.join(staging_directory_loader, self.loader_target),
        os.path.join(self.staging_directory, self.loader_target))
    port_symlink.MakeSymLink(
        os.path.join(staging_directory_loader, _CRASHPAD_TARGET),
        os.path.join(self.staging_directory, _CRASHPAD_TARGET))
    port_symlink.MakeSymLink(
        os.path.join(staging_directory_loader, 'content'),
        os.path.join(self.staging_directory, 'content'))

  def SupportsSuspendResume(self):
    return self.launcher.SupportsSuspendResume()

  def SendResume(self):
    return self.launcher.SendResume()

  def SendSuspend(self):
    return self.launcher.SendSuspend()

  def SendConceal(self):
    return self.launcher.SendConceal()

  def SendFocus(self):
    return self.launcher.SendFocus()

  def SendFreeze(self):
    return self.launcher.SendFreeze()

  def SendStop(self):
    return self.launcher.SendStop()

  def SupportsDeepLink(self):
    return self.launcher.SupportsDeepLink()

  def SendDeepLink(self, link):
    return self.launcher.SendDeepLink(link)

  def GetStartupTimeout(self):
    return self.launcher.GetStartupTimeout()

  def GetHostAndPortGivenPort(self, port):
    return self.launcher.GetHostAndPortGivenPort(port)

  def GetTargetPath(self):
    return self.launcher.GetTargetPath()

  def GetDeviceIp(self):
    return self.launcher.GetDeviceIp()

  def GetDeviceOutputPath(self):
    return self.launcher.GetDeviceOutputPath()
