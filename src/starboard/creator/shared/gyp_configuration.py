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
"""Starboard Creator Ci20 platform configuration for gyp_cobalt."""

import logging
import imp
import os
import sys

import config.base
#import config.starboard
import gyp_utils
from starboard.tools.testing import test_filter
#import starboard.tools.testing.test_filter as test_filter


class PlatformConfig(config.starboard.PlatformConfigStarboard):
  """Starboard ci20 platform configuration."""

  def __init__(self, platform):
    super(PlatformConfig, self).__init__(platform)
    # This must be called for building, but not for running tests
    if 'CI20_HOME' in os.environ:
      self.host_compiler_environment = gyp_utils.GetHostCompilerEnvironment(0)

  def _GetCi20Home(self):
    try:
      ci20_home = os.environ['CI20_HOME']
    except KeyError:
      logging.critical('ci20 builds require the `CI20_HOME\' '
                       'environment variable to be set.')
      sys.exit(1)
    return ci20_home

  def GetVariables(self, configuration):
    ci20_home = self._GetCi20Home()

    relative_sysroot = os.path.join('sysroot')
    sysroot = os.path.join(ci20_home, relative_sysroot)

    if not os.path.isdir(sysroot):
      logging.critical(
          'ci20 builds require $CI20_HOME/%s to be a valid directory.',
          relative_sysroot)
      sys.exit(1)
    variables = super(PlatformConfig, self).GetVariables(configuration)
    variables.update({
        'clang': 1,
        'sysroot': sysroot,
    })

    return variables

  def GetEnvironmentVariables(self):
    env_variables = self.host_compiler_environment
    env_variables = {
      'CC': self.host_compiler_environment['CC_host'],
      'CXX': self.host_compiler_environment['CXX_host'],
      'CC_host': 'gcc',
      'CXX_host': 'g++',
      'LD_host': 'g++',
      'ARFLAGS_host': 'rcs',
      'ARTHINFLAGS_host': 'rcsT',
    }

    return env_variables

  def GetLauncher(self):
    """Gets the module used to launch applications on this platform."""
    module_path = os.path.abspath(os.path.join(
        os.path.dirname(__file__), 'launcher.py'))
    launcher_module = imp.load_source('launcher', module_path)
    return launcher_module

