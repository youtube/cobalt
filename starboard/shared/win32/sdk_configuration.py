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
import sys


import starboard.shared.win32.sdk.installer as sdk_installer


_DEFAULT_SDK_BIN_DIR = 'C:\\Program Files (x86)\\Windows Kits\\10\\bin'
_DEFAULT_MSVC_TOOLS_VERSION = '14.15.26726'
_DEFAULT_WIN_SDK_VERSION = '10.0.17763.0'


# Platform specific api versions should be added here.
def _GetWinSdkVersionForPlatform(platform_name):
  return _DEFAULT_WIN_SDK_VERSION


def _GetMsvcToolVersionForPlatform(platform_name):
  return _DEFAULT_MSVC_TOOLS_VERSION


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


def _GetVersionedWinmdIncludes(win32_sdk_path, sdk_version,
                               vs_install_dir_with_version,
                               platform_name):
  root_path = win32_sdk_path + '/References/' + sdk_version
  winmd_files_dict = {
    '10.0.17134.0': {
      'FoundationContract': \
          '/Windows.Foundation.FoundationContract/3.0.0.0'
          '/Windows.Foundation.FoundationContract.winmd',
      'UniversalApiContract': \
          '/Windows.Foundation.UniversalApiContract/6.0.0.0'
          '/Windows.Foundation.UniversalApiContract.winmd',
      # Additional files for xb1/uwp platforms.
      # Xbox One Platform Extension SDK 17095.1000.
      'ApplicationResourceContract': \
          '/Windows.Xbox.ApplicationResourcesContract/1.0.0.0'
          '/Windows.Xbox.ApplicationResourcesContract.winmd',
      'ViewManagementViewScalingContract': \
          '/Windows.UI.ViewManagement.ViewManagementViewScalingContract/'
          '1.0.0.0/Windows.UI.ViewManagement'
          '.ViewManagementViewScalingContract.winmd',
    }
  }
  # 10.0.17763.0 is same as 10.0.17134.0 but with a change in the
  # UniversalApiContract version.
  winmd_files_dict['10.0.17763.0'] = dict(winmd_files_dict['10.0.17134.0'])
  winmd_files_dict['10.0.17763.0']['UniversalApiContract'] = \
      winmd_files_dict['10.0.17763.0']['UniversalApiContract']\
      .replace('6.0.0.0', '7.0.0.0')

  try:
    winmd_files = winmd_files_dict[sdk_version]
  except KeyError as err:
    logging.critical("sdk version " + sdk_version + " is not defined.")
    raise

  out_winmd_files = [
    vs_install_dir_with_version + '/lib/x86/store/references'
                                  '/platform.winmd',
    root_path + winmd_files['FoundationContract'],
    root_path + winmd_files['UniversalApiContract'],
  ]
  if 'xb1' in platform_name:
    out_winmd_files = out_winmd_files + [
      root_path + winmd_files['ApplicationResourceContract'],
      root_path + winmd_files['ViewManagementViewScalingContract'],
    ]
  out_winmd_files = [f.replace('/', os.path.sep) for f in out_winmd_files]
  return out_winmd_files


def _Print(s):
  sys.stdout.write(s + '\n')


class SdkConfiguration:
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

  def __init__(self, platform_name):
    self.required_sdk_version = _GetWinSdkVersionForPlatform(platform_name)
    self.msvc_tool_version = _GetMsvcToolVersionForPlatform(platform_name)
    # Maybe override Windows SDK bin directory with environment variable.
    self.windows_sdk_bin_dir = _SelectBestPath('WindowsSdkBinPath', _DEFAULT_SDK_BIN_DIR)
    self.windows_sdk_host_tools = os.path.join(
        self.windows_sdk_bin_dir, self.required_sdk_version, 'x64')

    # Note that sdk_installer.InstallSdkIfNecessary() does not handle
    # the mappedProgramFiles or %WindowsSdkBinPath%
    if not os.path.isdir(self.windows_sdk_host_tools):
      sdk_installer.InstallSdkIfNecessary('winsdk', self.required_sdk_version)

    self.windows_sdk_path = os.path.dirname(self.windows_sdk_bin_dir)
    # Fix path to the SDK root, regardless if environment variables point to
    # bin or parent dir.
    if os.path.basename(self.windows_sdk_path) == 'bin':
      self.windows_sdk_path = os.path.abspath(os.path.join(self.windows_sdk_path,'..'))

    # Maybe override Visual Studio install directory with environment variable.
    self.vs_install_dir = \
        _SelectBestPath('VSINSTALLDIR',
                        _GetBestBuildToolsDirectory(self.msvc_tool_version))

    self.vs_install_dir_with_version = (self.vs_install_dir
        + '\\VC\\Tools\\MSVC\\' + self.msvc_tool_version)
    self.vs_host_tools_path = (self.vs_install_dir_with_version
        + '\\bin\\HostX64\\x64')

    self.ucrtbased_dll_path = \
        'C:\\Program Files (x86)\\Microsoft SDKs\\Windows Kits\\10\\' + \
        'ExtensionSDKs\\Microsoft.UniversalCRT.Debug\\' + self.required_sdk_version + \
        '\\Redist\\Debug\\x64\\ucrtbased.dll'

    self.versioned_winmd_files = \
        _GetVersionedWinmdIncludes(self.windows_sdk_path,
                                   self.required_sdk_version,
                                   self.vs_install_dir_with_version,
                                   platform_name)

    _Print('Windows SDK Path:              %s' \
           % os.path.abspath(self.windows_sdk_host_tools))
    _Print('Visual Studio Path:            %s' \
           % os.path.abspath(self.vs_install_dir_with_version))
    _Print('Visual Studio Host Tools Path: %s' \
           % os.path.abspath(self.vs_host_tools_path))

    if not os.path.isdir(self.vs_host_tools_path):
      sdk_installer.InstallSdkIfNecessary('msvc', self.msvc_tool_version)

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
