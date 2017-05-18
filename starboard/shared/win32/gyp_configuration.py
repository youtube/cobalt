# Copyright 2016 Google Inc. All Rights Reserved.
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

import config.starboard
import gyp_utils

required_sdk_version = '10.0.15063.0'

# Default Windows SDK bin directory.
windows_sdk_bin_dir = 'C:\\Program Files (x86)\\Windows Kits\\10\\bin'

# Maybe override Windows SDK bin directory with environment variable.
windows_sdk_bin_var = 'WindowsSdkBinPath'
if windows_sdk_bin_var in os.environ:
  windows_sdk_bin_dir = os.environ[windows_sdk_bin_var]
elif not os.path.exists(windows_sdk_bin_dir):
  # If the above fails, this is our last guess.
  windows_sdk_bin_dir = windows_sdk_bin_dir.replace('Program Files (x86)',
                                                    'mappedProgramFiles')

# Default Visual Studio Install directory.
vs_install_dir = ('C:\\Program Files (x86)\\Microsoft Visual Studio'
                  + '\\2017\\Professional')
# Maybe override Visual Studio install directory with environment variable.
vs_install_dir_var = 'VSINSTALLDIR'
if vs_install_dir_var in os.environ:
  vs_install_dir = os.environ[vs_install_dir_var]
elif not os.path.exists(vs_install_dir):
  # If the above fails, this is our last guess.
  vs_install_dir = vs_install_dir.replace('Program Files (x86)',
                                          'mappedProgramFiles')


vs_install_dir_with_version = (vs_install_dir + '\\VC\\Tools\\MSVC'
                               + '\\14.10.25017')
vs_cl_path = vs_install_dir_with_version + '\\bin\\HostX64\\x64'



def _CheckVisualStudioVersion():
  if os.path.exists(vs_cl_path):
    return True
  logging.critical('Expected Visual Studio path \"%s\" not found.',
                   vs_cl_path)

def _QuotePath(path):
  return '"' + path + '"'

def _CheckWindowsSdkVersion():
  required_sdk_bin_dir = os.path.join(windows_sdk_bin_dir,
                                      required_sdk_version)
  if os.path.exists(required_sdk_bin_dir):
    return True

  if os.path.exists(windows_sdk_bin_dir):
    contents = os.listdir(windows_sdk_bin_dir)
    contents = [content for content in contents
                if os.path.isdir(os.path.join(windows_sdk_bin_dir, content))]
    non_sdk_dirs = ['arm', 'arm64', 'x64', 'x86']
    installed_sdks = [content for content in contents
                      if content not in non_sdk_dirs]
    logging.critical('Windows SDK versions \"%s\" found." \"%s\" required.',
                     installed_sdks, required_sdk_version)
  else:
    logging.critical('Windows SDK versions \"%s\" required.',
                     required_sdk_version)
  return False


class PlatformConfig(config.starboard.PlatformConfigStarboard):
  """Starboard Microsoft Windows platform configuration."""

  def __init__(self, platform):
    super(PlatformConfig, self).__init__(platform)
    _CheckWindowsSdkVersion()
    _CheckVisualStudioVersion()

  def GetVariables(self, configuration):
    variables = super(PlatformConfig, self).GetVariables(configuration)
    windows_sdk_path = os.path.abspath(os.path.join(windows_sdk_bin_dir,
                                                    os.pardir))
    variables.update({
      'visual_studio_install_path': vs_install_dir_with_version,
      'windows_sdk_path': windows_sdk_path,
      'windows_sdk_version': required_sdk_version,
      })
    return variables

  def GetEnvironmentVariables(self):
    cl = _QuotePath(os.path.join(vs_cl_path, 'cl.exe'))
    lib = _QuotePath(os.path.join(vs_cl_path, 'lib.exe'))
    link = _QuotePath(os.path.join(vs_cl_path, 'link.exe'))
    rc = _QuotePath(os.path.join(windows_sdk_bin_dir, required_sdk_version,
                                 'x64', 'rc.exe'))
    env_variables = {
        'AR' : lib,
        'AR_HOST' : lib,
        'CC': cl,
        'CXX': cl,
        'LD': link,
        'RC': rc,
        'VS_INSTALL_DIR': vs_install_dir,
        'CC_HOST': cl,
        'CXX_HOST': cl,
        'LD_HOST': link,
        'ARFLAGS_HOST': 'rcs',
        'ARTHINFLAGS_HOST': 'rcsT',
    }
    return env_variables

  def GetBuildFormat(self):
    """Returns the desired build format."""
    # The comma means that ninja, msvs_makefile, will be chained and use the
    # same input information so that .gyp files will only have to be parsed
    # once.
    return 'ninja,msvs_makefile'

  def GetGeneratorVariables(self, configuration):
    """Returns a dict of generator variables for the given configuration."""
    _ = configuration
    generator_variables = {
        'msvs_version': 2017,
        'msvs_platform': 'x64',
        'msvs_template_prefix': 'win/',
        'msvs_deploy_dir': '',
        'qtcreator_session_name_prefix': 'cobalt',
    }
    return generator_variables
