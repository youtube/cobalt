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
"""Wrapper to generate build files from .gyp files."""

import logging
import os
import shlex
import sys

import _env  # pylint: disable=unused-import
from starboard.tools import build
from starboard.tools import paths
from starboard.tools import platform


def _Dedupe(sequence):
  visited = set()
  return [x for x in sequence if not (x in visited or visited.add(x))]


def _GetHostOS():
  """Returns the name of the hosting OS."""
  host_os_names = {
      'linux2': 'linux',
      'linux3': 'linux',
      'darwin': 'linux',
      'win32': 'win',
  }

  if sys.platform not in host_os_names:
    if sys.platform == 'cygwin':
      logging.error('There may be several incompatibilities when trying to '
                    'build from within cygwin. When using a bash shell is '
                    'preferred, consider using msysgit instead (see '
                    'https://msysgit.github.io).')
    raise RuntimeError('Unsupported platform: %s' % sys.platform)

  return host_os_names[sys.platform]


def _AppendIncludes(includes, args):
  for include in includes:
    args.append('-I{}'.format(include))


def _AppendVariables(variables, args):
  for var in variables:
    args.append('-D{}={}'.format(var, variables[var]))


def _AppendGeneratorVariables(variables, args):
  for var in variables:
    args.append('-G{}={}'.format(var, variables[var]))


class GypRunner(object):
  """Responsible for running GYP."""

  def __init__(self, options):
    options.build_configs = _Dedupe(options.build_configs)
    self.options = options
    self.common_args = []
    self.platform_configuration = build.GetPlatformConfig(options.platform)
    if not self.platform_configuration:
      raise RuntimeError('Unable to load PlatformConfiguration.')

    env_vars = self.platform_configuration.GetEnvironmentVariables()
    self.app_configuration = (
        self.platform_configuration.GetApplicationConfiguration(
            options.application))

    if not self.app_configuration:
      raise RuntimeError('Unable to load an ApplicationConfiguration.')

    app_env_vars = self.app_configuration.GetEnvironmentVariables()
    if app_env_vars:
      env_vars.update(app_env_vars)
    os.environ.update(env_vars)
    self._MakeCommonArguments()

  def BuildConfigs(self):
    """Builds all configs specified in the options."""
    if not self.options.build_configs:
      raise RuntimeError('No build config types specified.')

    for config_name in self.options.build_configs:
      logging.info('Building config: %s', config_name)
      result = self.BuildConfig(config_name)
      if result:
        return result
    return 0

  def BuildConfig(self, config_name):
    """Builds the GYP file for a given config."""

    # Make a copy of the common arguments.
    args = self.common_args[:]

    configuration_variables = {
        # Default deploy script. Certain platforms may choose to change this.
        'include_path_platform_deploy_gypi':
            'starboard/build/default_no_deploy.gypi'
    }
    configuration_variables.update(
        self.platform_configuration.GetVariables(config_name))
    app_variables = self.app_configuration.GetVariables(config_name)
    if app_variables:
      configuration_variables.update(app_variables)
    _AppendVariables(configuration_variables, args)

    # Add the symbolizer path for ASAN to allow translation of callstacks.
    asan_symbolizer_path = ''
    if configuration_variables.get('use_asan', 0) == 1:
      compiler_commandline = shlex.split(os.environ.get('CC', ''))
      for path in compiler_commandline:
        if os.path.isfile(path):
          test_path = os.path.join(os.path.dirname(path), 'llvm-symbolizer')
          if os.path.isfile(test_path):
            asan_symbolizer_path = test_path
            break
    asan_variables = {
        'asan_symbolizer_path': asan_symbolizer_path,
    }
    _AppendVariables(asan_variables, args)

    # Add config specific generator variables.
    generator_config = '{}_{}'.format(self.options.platform, config_name)

    generator_variables = {
        'config': generator_config,
    }
    generator_variables.update(
        self.platform_configuration.GetGeneratorVariables(config_name))
    app_generator_variables = (
        self.app_configuration.GetGeneratorVariables(config_name))
    if app_generator_variables:
      generator_variables.update(app_generator_variables)
    _AppendGeneratorVariables(generator_variables, args)

    build_file = self.options.build_file
    if not build_file:
      build_file = self.app_configuration.GetDefaultTargetBuildFile()

    if not build_file:
      build_file = self.platform_configuration.GetDefaultTargetBuildFile()

    if not build_file:
      raise RuntimeError('Unable to determine target GYP file.')

    # That target build file goes last.
    args.append(build_file)
    logging.debug('GYP arguments: %s', args)
    return build.GetGyp().main(args)

  def _MakeCommonArguments(self):
    """Makes a list of GYP arguments that are common to all configurations."""

    platform_name = self.platform_configuration.GetName()
    if not platform.IsValid(platform_name):
      raise RuntimeError('Invalid platform: %s' % platform_name)

    source_tree_dir = paths.REPOSITORY_ROOT
    self.common_args = [
        # Set the build format.
        '--format={}-{}'.format(self.platform_configuration.GetBuildFormat(),
                                platform_name),

        # Set the depth argument to the top level directory. This will define
        # the DEPTH variable which is the relative path between the directory
        # containing the processed build file and the top level directory.
        '--depth={}'.format(source_tree_dir),

        # Set the top of the source tree.
        '--toplevel-dir={}'.format(source_tree_dir),
    ]

    # Pass through the debug options.
    for debug in self.options.debug:
      self.common_args.append('--debug=%s' % debug)

    if self.options.check:
      self.common_args.append('--check')

    # Append common GYP variables.
    common_variables = {
        'OS': 'starboard',
        'CC_HOST': os.environ.get('CC_HOST', os.environ.get('CC', '')),
        'host_os': _GetHostOS(),
        'starboard_path': os.path.relpath(platform.Get(platform_name).path,
                                          source_tree_dir),
        'starboard_platform_name': platform_name,
    }

    _AppendVariables(common_variables, self.common_args)

    # Append generator variables.
    generator_variables = {
        # Set the output folder name; affects all generators but MSVS.
        'output_dir': 'out',
    }
    _AppendGeneratorVariables(generator_variables, self.common_args)

    # Append GYPI includes which will be included by GYP before any processed
    # build file.
    gyp_includes = [
        os.path.join(paths.REPOSITORY_ROOT, 'starboard', 'build',
                     'base_configuration.gypi'),

        # TODO: Remove dependency on common.gypi by moving the required bits
        # into base_configuration.gypi.
        os.path.join(paths.REPOSITORY_ROOT, 'build', 'common.gypi'),
    ]
    gyp_includes.extend(self.platform_configuration.GetIncludes())
    if self.app_configuration:
      gyp_includes[:0] = self.app_configuration.GetPreIncludes()
      gyp_includes.extend(self.app_configuration.GetPostIncludes())
    _AppendIncludes(gyp_includes, self.common_args)
