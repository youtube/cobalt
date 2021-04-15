# Copyright 2016 The Cobalt Authors. All Rights Reserved.
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
"""Starboard win32 shared platform configuration for gyp_cobalt."""

import logging
import os
import re
import subprocess
import sys

import config.base
import starboard.shared.win32.sdk_configuration as win_sdk_configuration
from starboard.tools import cache
from starboard.tools.paths import STARBOARD_ROOT
from starboard.tools.testing import test_filter
from starboard.tools.toolchain import cmd
from starboard.tools.toolchain import msvc
from starboard.tools.toolchain import python

MSVS_VERSION = 2017;


def GetWindowsVersion():
  out = subprocess.check_output('ver', universal_newlines=True, shell=True)
  lines = [l for l in out.split('\n') if l]
  for l in lines:
    m = re.search(r'Version\s([0-9\.]+)', out)
    if m and m.group(1):
      major, minor, build = m.group(1).split('.')[0:3]
      return (int(major), int(minor), int(build))
  raise IOError('Could not retrieve windows version')


def GetCompilerOptionsFromWinSdk(win_sdk):
  """Convert mandatory include files in win_sdk to compiler options."""
  force_include_files = win_sdk.versioned_winmd_files
  missing_files = []
  for f in force_include_files:
    if not os.path.exists(f):
      missing_files.append(f)
  if missing_files:
    logging.critical('\n***** Missing files *****: \n' + \
                     '\n'.join(missing_files) + \
                     '\nCompiling may have problems.\n')
  # /FU"path" will force include that path for every file compiled.
  compiler_options = ['/FU' + _QuotePath(f) for f in force_include_files]
  return compiler_options


def _QuotePath(path):
  return '"' + path + '"'


class Win32SharedConfiguration(config.base.PlatformConfigBase):
  """Starboard Microsoft Windows platform configuration."""

  def __init__(self,
               platform,
               sdk_checker_fcn=None,
               sabi_json_path='starboard/sabi/default/sabi.json'):
    super(Win32SharedConfiguration, self).__init__(platform)
    self.sdk_name = self.GetName()
    self.vmware = 'vmware' in self.GetVideoProcessorDescription().lower()
    self.sdk_checker_fcn = sdk_checker_fcn
    self.sabi_json_path = sabi_json_path
    # Sets sccache as build accelerator.
    self.build_accelerator = self.GetBuildAccelerator(
        cache.Accelerator.SCCACHE)

  def GetSdk(self):
    # Lazy load sdk to avoid any sdk checks running until it is used.
    d = self.GetSdk.__dict__
    if not 'sdk' in d:
      d['sdk'] = win_sdk_configuration.SdkConfiguration(self.sdk_name)
      if self.sdk_checker_fcn:
        self.sdk_checker_fcn()
    return d['sdk']

  def GetVideoProcessorDescription(self):
    try:
      cmd = ('powershell -command "(Get-WmiObject '
             'Win32_VideoController).Description"')
      return subprocess.check_output(cmd, shell=True).splitlines()[0]
    except Exception as err:
      logging.error('Could not detect video card because: ' + str(err))
      return 'UNKNOWN'

  def AdditionalPlatformCompilerOptions(self):
    return GetCompilerOptionsFromWinSdk(self.GetSdk())

  def GetVariables(self, configuration):
    sdk = self.GetSdk()
    variables = super(Win32SharedConfiguration,
                      self).GetVariables(configuration)
    compiler_options = ' '.join(self.AdditionalPlatformCompilerOptions())
    variables.update({
        'additional_platform_compiler_options': compiler_options,
        'include_path_platform_deploy_gypi': 'starboard/win/win32/platform_deploy.gypi',
        'msvc_redist_version': sdk.msvc_redist_version,
        'ucrtbased_dll_path': sdk.ucrtbased_dll_path,
        'visual_studio_base_path': sdk.vs_install_dir,
        'visual_studio_install_path': sdk.vs_install_dir_with_version,
        'windows_sdk_path': sdk.windows_sdk_path,
        'windows_sdk_version': sdk.required_sdk_version,
    })
    return variables

  def GetEnvironmentVariables(self):
    sdk = self.GetSdk()
    cl = self.build_accelerator + ' ' + _QuotePath(os.path.join(
             sdk.vs_host_tools_path, 'cl.exe'))
    lib = _QuotePath(os.path.join(sdk.vs_host_tools_path, 'lib.exe'))
    link = _QuotePath(os.path.join(sdk.vs_host_tools_path, 'link.exe'))
    ml = _QuotePath(os.path.join(sdk.vs_host_tools_path, 'ml64.exe'))
    rc = _QuotePath(os.path.join(sdk.windows_sdk_host_tools, 'rc.exe'))
    env_variables = {
        'AR': lib,
        'AR_HOST': lib,
        'CC': cl,
        'CXX': cl,
        'LD': link,
        'RC': rc,
        'ML': ml,
        'VS_INSTALL_DIR': sdk.vs_install_dir,
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
        'msvs_version': MSVS_VERSION,
        'msvs_platform': 'x64',
        'msvs_template_prefix': 'win/',
        'msvs_deploy_dir': '',
        'qtcreator_session_name_prefix': 'cobalt',
    }
    return generator_variables

  def GetToolchain(self):
    sys.path.append(os.path.join(STARBOARD_ROOT, 'shared', 'msvc', 'uwp'))
    from msvc_toolchain import MSVCUWPToolchain  # pylint: disable=g-import-not-at-top,g-bad-import-order
    return MSVCUWPToolchain()

  def GetTestFilters(self):
    """Gets all tests to be excluded from a unit test run.

    Returns:
      A list of initialized TestFilter objects.
    """
    filters = super(Win32SharedConfiguration, self).GetTestFilters()
    for target, tests in self.__FILTERED_TESTS.iteritems():
      filters.extend(test_filter.TestFilter(target, test) for test in tests)
    return filters

  __FILTERED_TESTS = {}

  def GetPathToSabiJsonFile(self):
    return self.sabi_json_path

  def GetToolsetToolchain(self, cc_path, cxx_path, asm_path, ar_path, ld_path,
                          **kwargs):
    arch = 'environment.x64'
    gyp_defines = []
    gyp_include_dirs = []
    gyp_cflags = []
    gyp_cflags_cc = []
    gyp_libflags = []
    gyp_ldflags = []

    if 'spec' in kwargs and 'config_name' in kwargs:
      spec = kwargs['spec']
      config_name = kwargs['config_name']
      compiler_settings = self.GetCompilerSettings(
          spec, self.GetGeneratorVariables(None))
      arch = 'environment.' + compiler_settings.GetArch(config_name)
      gyp_defines = gyp_defines + compiler_settings.GetDefines(config_name)
      gyp_include_dirs = compiler_settings.ProcessIncludeDirs(
          gyp_include_dirs, config_name)
      cflags = compiler_settings.GetCflags(config_name)
      cflags_c = compiler_settings.GetCflagsC(config_name)
      cflags_cc = compiler_settings.GetCflagsCC(config_name)
      gyp_cflags = gyp_cflags + cflags + cflags_c
      gyp_cflags_cc = gyp_cflags_cc + cflags + cflags_cc

      if 'gyp_path_to_ninja' in kwargs:
        gyp_path_to_ninja = kwargs['gyp_path_to_ninja']
        libflags = compiler_settings.GetLibFlags(config_name, gyp_path_to_ninja)
        gyp_libflags = gyp_libflags + libflags

        keys = [
            'expand_special', 'gyp_path_to_unique_output',
            'compute_output_file_name'
        ]
        if all(k in kwargs for k in keys):
          expand_special = kwargs['expand_special']
          gyp_path_to_unique_output = kwargs['gyp_path_to_unique_output']
          compute_output_file_name = kwargs['compute_output_file_name']
          manifest_name = gyp_path_to_unique_output(
              compute_output_file_name(spec))
          is_executable = spec['type'] == 'executable'
          ldflags, manifest_files = compiler_settings.GetLdFlags(
              config_name,
              gyp_path_to_ninja=gyp_path_to_ninja,
              expand_special=expand_special,
              manifest_name=manifest_name,
              is_executable=is_executable)
          gyp_ldflags = gyp_ldflags + ldflags

    max_concurrent_linkers = kwargs.get('max_concurrent_processes', None)

    return [
        msvc.CCompiler(
            path=cc_path,
            gyp_defines=gyp_defines,
            gyp_include_dirs=gyp_include_dirs,
            gyp_cflags=gyp_cflags),
        msvc.CxxCompiler(
            path=cxx_path,
            gyp_defines=gyp_defines,
            gyp_include_dirs=gyp_include_dirs,
            gyp_cflags=gyp_cflags_cc),
        msvc.AssemblerWithCPreprocessor(
            path=asm_path,
            arch=arch,
            gyp_defines=gyp_defines,
            gyp_include_dirs=gyp_include_dirs),
        msvc.StaticThinLinker(
            path=ar_path,
            arch=arch,
            gyp_libflags=gyp_libflags,
            max_concurrent_processes=max_concurrent_linkers),
        msvc.StaticLinker(
            path=ar_path,
            arch=arch,
            gyp_libflags=gyp_libflags,
            max_concurrent_processes=max_concurrent_linkers),
        msvc.ExecutableLinker(
            path=ld_path,
            arch=arch,
            gyp_ldflags=gyp_ldflags,
            max_concurrent_processes=max_concurrent_linkers),
        msvc.SharedLibraryLinker(
            path=ld_path,
            arch=arch,
            gyp_ldflags=gyp_ldflags,
            max_concurrent_processes=max_concurrent_linkers),
        python.Copy(),
        python.Stamp(),
        cmd.Shell(quote=False),
    ]

  def GetCompilerSettings(self, spec, generator_flags):
    sys.path.append(os.path.join(STARBOARD_ROOT, 'shared', 'msvc', 'uwp'))
    # pylint: disable=g-import-not-at-top,g-bad-import-order
    from msvc_toolchain import MsvsSettings
    return MsvsSettings(spec, generator_flags)
