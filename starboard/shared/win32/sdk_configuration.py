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

import starboard.shared.win32.sdk.installer as sdk_installer

_DEFAULT_SDK_BIN_DIR = 'C:\\Program Files (x86)\\Windows Kits\\10\\bin'
_MSVC_TOOLS_VERSION = '14.14.26428'
_WIN_SDK_VERSION = '10.0.17134.0'

def _SelectBestPath(os_var_name, path):
  if os_var_name in os.environ:
    return os.environ[os_var_name]
  if os.path.exists(path):
    return path
  new_path = path.replace('Program Files (x86)', 'mappedProgramFiles')
  if os.path.exists(new_path):
    return new_path
  return path

def _GetBestBuildToolsDirectory(msvc_version):
  paths = (
    'C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\Professional',
    'C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\Enterprise',
    'C:\\Program Files (x86)\\Microsoft Visual Studio\\2017\\Community',
    # TODO: Add support for using msvc build tools without visual studio.
  )
  for p in paths:
    if os.path.isdir(p):
      msvc_tools_dir = p + '\\VC\\Tools\\MSVC\\' + msvc_version + '\\bin\\HostX64\\x64'
      if os.path.isdir(msvc_tools_dir):
        return p
  return paths[0]
  
class SdkConfiguration:
  required_sdk_version = _WIN_SDK_VERSION
  # windows_sdk_host_tools will be set to, eg,
  # 'C:\\Program Files (x86)\\Windows Kits\\10\\bin\10.0.15063.0'

  # windows_sdk_path will be set to, eg
  # 'C:\\Program Files (x86)\\Windows Kits\\10'

  # vs_install_dir_with_version will be set to, eg
  # "C:\Program Files (x86)\Microsoft Visual Studio\2017\Professional\VC\Tools
  # \MSVC\14.10.25017

  # vs_host_tools_path will be set to, eg
  # "C:\Program Files (x86)\Microsoft Visual Studio\2017\Professional\VC\Tools
  # \MSVC\14.10.25017\bin\HostX64\x64

  def __init__(self):
    # Maybe override Windows SDK bin directory with environment variable.
    self.windows_sdk_bin_dir = _SelectBestPath('WindowsSdkBinPath', _DEFAULT_SDK_BIN_DIR)
    self.windows_sdk_host_tools = os.path.join(
        self.windows_sdk_bin_dir, _WIN_SDK_VERSION, 'x64')

    # Note that sdk_installer.InstallSdkIfNecessary() does not handle
    # the mappedProgramFiles or %WindowsSdkBinPath%
    if not os.path.isdir(self.windows_sdk_host_tools):
      sdk_installer.InstallSdkIfNecessary('winsdk', _WIN_SDK_VERSION)

    self.windows_sdk_path = os.path.dirname(self.windows_sdk_bin_dir)

    # Maybe override Visual Studio install directory with environment variable.
    self.vs_install_dir = \
        _SelectBestPath('VSINSTALLDIR',
                        _GetBestBuildToolsDirectory(_MSVC_TOOLS_VERSION))

    self.vs_install_dir_with_version = (self.vs_install_dir
        + '\\VC\\Tools\\MSVC' + '\\' + _MSVC_TOOLS_VERSION)
    self.vs_host_tools_path = (self.vs_install_dir_with_version
        + '\\bin\\HostX64\\x64')

    logging.critical('Windows SDK Path:              ' + os.path.abspath(self.windows_sdk_host_tools))
    logging.critical('Visual Studio Path:            ' + os.path.abspath(self.vs_install_dir_with_version))
    logging.critical('Visual Studio Host Tools Path: ' + os.path.abspath(self.vs_host_tools_path))

    if not os.path.isdir(self.vs_host_tools_path):
      sdk_installer.InstallSdkIfNecessary('msvc', _MSVC_TOOLS_VERSION)

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
                       installed_sdks, _WIN_SDK_VERSION)
    else:
      logging.critical('Windows SDK versions \"%s\" required.',
                       _WIN_SDK_VERSION)
    return False
