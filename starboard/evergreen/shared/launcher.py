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

from starboard.tools import abstract_launcher
from starboard.tools import paths

_BASE_STAGING_DIRECTORY = 'evergreen_staging'
_CRASHPAD_TARGET = 'crashpad_handler'
_DEFAULT_LOADER_TARGET = 'elf_loader_sandbox'


class Launcher(abstract_launcher.AbstractLauncher):
  """Class for launching Evergreen targets on arbitrary platforms.

  This Evergreen launcher leverages the platform-specific launchers to start the
  Evergreen loader on a particular device. A staging directory is built that
  resembles a typical install directory, with the Evergreen target and its
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

    self.use_compressed_library = kwargs.get('use_compressed_library')

    # The relationship of loader platforms and configurations to evergreen
    # platforms and configurations is many-to-many. We need a separate directory
    # for each of them, i.e. linux-x64x11_debug__evergreen-x64_gold.
    loader_build_name = f'{self.loader_platform}_{self.loader_config}'
    target_build_name = f'{platform}_{config}'
    self.combined_config = f'{loader_build_name}__{target_build_name}'

    self.staging_directory = os.path.join(
        os.path.dirname(self.out_directory), _BASE_STAGING_DIRECTORY,
        self.combined_config)

    # TODO: Figure out a place for the launcher to do this prep work that is not
    # part of the constructor.
    self._StageTargetsAndContents()

    # Ensure the path, relative to the content of the ELF Loader, to the
    # Evergreen target and its content are passed as command line switches.
    library_path_value = os.path.join('app', self.target_name, 'lib',
                                      f'lib{self.target_name}')
    library_path_param = f'--evergreen_library={library_path_value}'
    if self.use_compressed_library:
      if self.target_name != 'cobalt':
        raise ValueError(
            '|use_compressed_library| only expected with |target_name| cobalt')
      library_path_param += '.lz4'
    else:
      library_path_param += '.so'

    target_command_line_params = [
        library_path_param,
        f'--evergreen_content=app/{self.target_name}/content'
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
             config:      devel             +-- install
             target_name: nplb                   +-- nplb
                                                      +-- content
                                                      +-- nplb

    This function effectively builds a directory structure that matches both of
    these expectations while minimizing the hard copies made.

    Note: The Linux launcher does not yet look in the install directory and
          instead runs the target from the top of its out-directory. This will
          be changed in the future.
    """
    if os.path.exists(self.staging_directory):
      shutil.rmtree(self.staging_directory)
    os.makedirs(self.staging_directory)

    # TODO(b/267568637): Make the Linux launcher run from the install_directory.
    if 'linux' in self.loader_platform:
      self._StageTargetsAndContentsLinux()
    else:
      self._StageTargetsAndContentsRaspi()

  def _StageTargetsAndContentsLinux(self):
    """Stage targets and their contents for GN builds for Linux platforms."""
    content_subdir = os.path.join('usr', 'share', 'cobalt')

    # Copy loader content and binaries
    loader_install_path = os.path.join(self.loader_out_directory, 'install')

    loader_content_src = os.path.join(loader_install_path, content_subdir)
    loader_content_dst = os.path.join(self.staging_directory, 'content')
    shutil.copytree(loader_content_src, loader_content_dst)

    loader_target_src = os.path.join(loader_install_path, 'bin',
                                     self.loader_target)
    loader_target_dst = os.path.join(self.staging_directory, self.loader_target)
    shutil.copy(loader_target_src, loader_target_dst)

    crashpad_target_src = os.path.join(loader_install_path, 'bin',
                                       _CRASHPAD_TARGET)
    crashpad_target_dst = os.path.join(self.staging_directory, _CRASHPAD_TARGET)
    shutil.copy(crashpad_target_src, crashpad_target_dst)

    # Copy target content and binary
    target_install_path = os.path.join(self.out_directory, 'install')
    target_staging_dir = os.path.join(self.staging_directory, 'content', 'app',
                                      self.target_name)

    target_content_src = os.path.join(target_install_path, content_subdir)
    target_content_dst = os.path.join(target_staging_dir, 'content')
    shutil.copytree(target_content_src, target_content_dst)

    shlib_name = f'lib{self.target_name}'
    shlib_name += '.lz4' if self.use_compressed_library else '.so'
    target_binary_src = os.path.join(target_install_path, 'lib', shlib_name)
    target_binary_dst = os.path.join(target_staging_dir, 'lib', shlib_name)
    os.makedirs(os.path.join(target_staging_dir, 'lib'))
    shutil.copy(target_binary_src, target_binary_dst)

  def _StageTargetsAndContentsRaspi(self):
    """Stage targets and their contents for GN builds for Raspi platforms."""
    # TODO(b/218889313): `content` is hardcoded on raspi and must be in the same
    # directory as the binaries.
    if 'raspi' in self.loader_platform:
      content_subdir = 'content'
      bin_subdir = ''
    else:
      content_subdir = os.path.join('usr', 'share', 'cobalt')
      bin_subdir = 'bin'

    # Copy loader content and binaries to
    loader_install_path = os.path.join(self.loader_out_directory, 'install')
    # raspi is a packaged platform and the loader target is the staging target.
    staging_directory_loader = os.path.join(self.staging_directory, 'install',
                                            self.loader_target)

    loader_content_src = os.path.join(loader_install_path, content_subdir)
    loader_content_dst = os.path.join(staging_directory_loader, 'content')
    shutil.copytree(loader_content_src, loader_content_dst)

    loader_binary_src = os.path.join(loader_install_path, bin_subdir)
    loader_target_src = os.path.join(loader_binary_src, self.loader_target,
                                     self.loader_target)
    loader_target_dst = os.path.join(staging_directory_loader,
                                     self.loader_target)
    shutil.copy(loader_target_src, loader_target_dst)

    crashpad_target_src = os.path.join(loader_binary_src, _CRASHPAD_TARGET,
                                       _CRASHPAD_TARGET)
    crashpad_target_dst = os.path.join(staging_directory_loader,
                                       _CRASHPAD_TARGET)
    os.makedirs(crashpad_target_dst)
    shutil.copy(crashpad_target_src, crashpad_target_dst)

    # Copy target content and binary
    target_install_path = os.path.join(self.out_directory, 'install')
    target_staging_dir = os.path.join(staging_directory_loader, 'content',
                                      'app', self.target_name)
    os.makedirs(target_staging_dir)

    # TODO(b/218889313): Reset the content path for the evergreen artifacts.
    content_subdir = os.path.join('usr', 'share', 'cobalt')
    target_content_src = os.path.join(target_install_path, content_subdir)
    target_content_dst = os.path.join(target_staging_dir, 'content')
    shutil.copytree(target_content_src, target_content_dst)

    shlib_name = f'lib{self.target_name}'
    shlib_name += '.lz4' if self.use_compressed_library else '.so'
    target_binary_src = os.path.join(target_install_path, 'lib', shlib_name)
    target_binary_dst = os.path.join(target_staging_dir, 'lib', shlib_name)
    os.makedirs(os.path.join(target_staging_dir, 'lib'))
    shutil.copy(target_binary_src, target_binary_dst)

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
