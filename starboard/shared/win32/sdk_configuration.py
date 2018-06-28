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

import logging
import os

import sdk_installer

class SdkConfiguration:
  required_sdk_version = '10.0.17134.0'

  # Default Windows SDK bin directory.
  windows_sdk_bin_dir = 'C:\\Program Files (x86)\\Windows Kits\\10\\bin'

  # windows_sdk_host_tools will be set to, eg,
  # 'C:\\Program Files (x86)\\Windows Kits\\10\\bin\10.0.15063.0'

  # windows_sdk_path will be set to, eg
  # 'C:\\Program Files (x86)\\Windows Kits\\10'

  # Default Visual Studio Install directory.
  vs_install_dir = ('C:\\Program Files (x86)\\Microsoft Visual Studio'
                    + '\\2017\\Professional')

  # vs_install_dir_with_version will be set to, eg
  # "C:\Program Files (x86)\Microsoft Visual Studio\2017\Professional\VC\Tools
  # \MSVC\14.10.25017

  # vs_host_tools_path will be set to, eg
  # "C:\Program Files (x86)\Microsoft Visual Studio\2017\Professional\VC\Tools
  # \MSVC\14.10.25017\bin\HostX64\x64

  def __init__(self):
    # Maybe override Windows SDK bin directory with environment variable.
    windows_sdk_bin_var = 'WindowsSdkBinPath'
    if windows_sdk_bin_var in os.environ:
      self.windows_sdk_bin_dir = os.environ[windows_sdk_bin_var]
    elif not os.path.exists(self.windows_sdk_bin_dir):
      # If the above fails, this is our last guess.
      self.windows_sdk_bin_dir = self.windows_sdk_bin_dir.replace(
        'Program Files (x86)', 'mappedProgramFiles')

    self.windows_sdk_host_tools = os.path.join(
        self.windows_sdk_bin_dir, self.required_sdk_version, 'x64')

    # Note that sdk_installer.InstallSdkIfNecessary() does not handle
    # the mappedProgramFiles or %WindowsSdkBinPath%
    if not os.path.isdir(self.windows_sdk_host_tools):
      sdk_installer.InstallSdkIfNecessary(self.required_sdk_version)

    self.windows_sdk_path = os.path.dirname(self.windows_sdk_bin_dir)

    # Maybe override Visual Studio install directory with environment variable.
    vs_install_dir_var = 'VSINSTALLDIR'
    if vs_install_dir_var in os.environ:
      self.vs_install_dir = os.environ[vs_install_dir_var]
    elif not os.path.exists(self.vs_install_dir):
      # If the above fails, this is our last guess.
      self.vs_install_dir = self.vs_install_dir.replace('Program Files (x86)',
                                                        'mappedProgramFiles')

    self.vs_install_dir_with_version = (self.vs_install_dir
        + '\\VC\\Tools\\MSVC' + '\\14.10.25017')
    self.vs_host_tools_path = (self.vs_install_dir_with_version
        + '\\bin\\HostX64\\x64')

    if not os.path.exists(self.vs_host_tools_path):
      logging.critical('Expected Visual Studio path \"%s\" not found.',
                       self.vs_host_tools_path)
    self._CheckWindowsSdkVersion()

  def _CheckWindowsSdkVersion(self):
    if os.path.exists(self.windows_sdk_host_tools):
      return True

    if os.path.exists(self.windows_sdk_bin_dir):
      contents = os.listdir(self.windows_sdk_bin_dir)
      contents = [content for content in contents
                  if os.path.isdir(
                    os.path.join(self.windows_sdk_bin_dir, content))]
      non_sdk_dirs = ['arm', 'arm64', 'x64', 'x86']
      installed_sdks = [content for content in contents
                        if content not in non_sdk_dirs]
      logging.critical('Windows SDK versions \"%s\" found." \"%s\" required.',
                       installed_sdks, self.required_sdk_version)
    else:
      logging.critical('Windows SDK versions \"%s\" required.',
                       self.required_sdk_version)
    return False