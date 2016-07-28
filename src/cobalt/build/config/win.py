# Copyright 2014 Google Inc. All Rights Reserved.
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
"""Windows platform configuration for gyp_cobalt."""

import logging
import os

import base


def CreatePlatformConfig():
  try:
    return _PlatformConfig()
  except RuntimeError as e:
    logging.critical(e)
    return None


class _PlatformConfig(base.PlatformConfigBase):
  """Windows platform configuration."""

  def __init__(self):
    super(_PlatformConfig, self).__init__('win')

  def GetBuildFormat(self):
    """Returns the desired build format."""
    # The comma means that ninja and msvs_makefile will be chained and use the
    # same input information so that .gyp files will only have to be parsed
    # once.
    return 'ninja,msvs_makefile'

  def GetEnvironmentVariables(self):
    cc = 'cl.exe'
    ld = 'link.exe'

    env_variables = {
        'CC': cc,
        'CXX': cc,
        'CXX_ZW': cc,
        'LD': ld,
    }
    return env_variables

  def GetVariables(self, config):
    """Returns a dict of GYP variables for the given configuration."""
    _ = config
    variables = {
        'use_widevine': 0,
        'use_openssl': 1,
    }
    static_analysis = os.environ.get('USE_STATIC_ANALYSIS')
    if static_analysis:
      variables['static_analysis'] = 'true' if int(static_analysis) else ''
    return variables

  def GetGeneratorVariables(self, config):
    _ = config
    generator_variables = {
        'msvs_version': 2012,
        'msvs_platform': 'Win32',
        'msvs_template_prefix': 'win/',
        # This is appended to out/(Win_Debug/..) to set the $(OutDir)
        # environment variable in MSVS solution.
        'msvs_deploy_dir': '',
    }
    return generator_variables
