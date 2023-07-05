#!/usr/bin/python
#
# Copyright 2018 The Cobalt Authors. All Rights Reserved.
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
"""UWP implementation of Starboard launcher abstraction."""

from __future__ import print_function

import fnmatch
import os
import subprocess
import sys
import time

from starboard.shared.win32 import mini_dump_printer
from starboard.tools import abstract_launcher
from starboard.xb1.tools import uwp_api

APPX_NAME = 'YouTube'
PACKAGE_NAME = 'GoogleInc.YouTube'
PACKAGE_FULL_NAME = PACKAGE_NAME + '_yfg5n0ztvskxp'


#----------- Global Functions
def GlobalVars():
  return GlobalVars.__dict__


# First call returns True, otherwise return false.
def FirstRun():
  v = GlobalVars()
  if 'first_run' in v:
    return False
  v['first_run'] = False
  return True


# Repeatedly tests to see if a file path exists. Returns True
# on success, or else False after timeout time with no success
# has elapsed.
def WaitForFileToAppear(file_path, timeout):
  start_time = time.process_time()
  while True:
    if os.path.exists(file_path):
      return True
    if (time.process_time() - start_time) > timeout:
      return False
    time.sleep(.01)


def FindAppxDirectoryOrRaise(target_dir):
  appx_paths = [target_dir, os.path.join(target_dir, 'appx')]

  final_path = None
  for p in appx_paths:
    if os.path.exists(os.path.join(p, 'AppxManifest.xml')):
      final_path = p
      break

  if not final_path or not os.path.exists(final_path):
    msg = ('Could not find AppxManifest in paths ' + str(appx_paths) + '.' +
           '\nPlease run again with --out_directory <APPX_DIR>')
    raise IOError(msg)
  return final_path


# Input:  "--url=https://www.youtube.com/tv?v=1FmJdQiKy_w,--install=True"
# Output: {'url': 'https://www.youtube.com/tv?v=1FmJdQiKy_w', 'install': 'True'}
def CommandArgsToDict(s):
  if not s:
    return {}

  array = s.split(',')
  out = {}
  for v in array:
    elems = v.split('=', 1)
    if len(elems) < 1:
      continue
    key = elems[0]

    if key.startswith('--'):
      key = key[2:]
    val = None
    if len(elems) > 1:
      val = elems[1]
    out[key] = val
  return out


# Returns full names.
def GlobFiles(dir_path, file_pattern):
  if not os.path.exists(dir_path):
    return []
  files = [
      os.path.join(dir_path, f)
      for f in os.listdir(dir_path)
      if fnmatch.fnmatch(f, file_pattern)
  ]
  return files


###########################################################################
class Launcher(abstract_launcher.AbstractLauncher):
  """Run an application on UWP target."""

  def __init__(self, platform, target_name, config, device_id, **kwargs):
    super().__init__(platform, target_name, config, device_id, **kwargs)

    self.UwpApi = uwp_api.UwpApi(None)  # pylint: disable=invalid-name
    self.package_dir = uwp_api.GetPackageDir(PACKAGE_FULL_NAME)
    local_cache_path = os.path.join(self.package_dir, 'LocalCache')
    self.executable = self.GetTargetPath()
    self.kill_called = False

    self.stdout_file_name = target_name + '_stdout.txt'
    self.stdout_file_path = os.path.join(local_cache_path,
                                         self.stdout_file_name)
    self.uwp_app_exe_name = self.target_name + '.exe'
    self.uwp_app_exe_path = self.GetTargetPath() + '.exe'

    self.uwp_dump_file_pattern = '*' + self.uwp_app_exe_name + '*.dmp'

    self.uwp_app_dmp_path = os.path.join(os.environ['LOCALAPPDATA'],
                                         'CrashDumps')

    # Clear out dump files from a previous run.
    self.DeleteFiles(self.uwp_app_dmp_path, self.uwp_dump_file_pattern)

    self.args = ['--xb1_log_file=' + self.stdout_file_name]

    # Merge target params into the user_params.
    user_params = kwargs.get('target_params', '')
    if user_params and len(user_params) > 0:
      user_params_dict = CommandArgsToDict(','.join(user_params))
      for key, val in user_params_dict.items():
        if val:
          self.args.append('--' + str(key) + '=' + str(val))
        else:
          self.args.append('--' + str(key))

    if FirstRun():
      self.InitInstallAppxPackage()
      # Hack to prevent UWP app from launching a window that will block
      # progress in unit tests.
      first_run_file = os.path.join(local_cache_path, 'first_run')
      dirname = os.path.dirname(first_run_file)
      if not os.path.exists(dirname):
        os.makedirs(dirname)
      with open(first_run_file, 'w', encoding='utf-8') as _:
        pass

  # Install Appx Package.
  def InitInstallAppxPackage(self):
    target_dir = os.path.dirname(self.GetTargetPath())
    final_path = FindAppxDirectoryOrRaise(target_dir)
    self.Output('final_appx path: ' + final_path)
    try:
      self.UwpApi.RemovePackage(PACKAGE_NAME)
      self.UwpApi.InstallPackage(final_path)
    except subprocess.CalledProcessError as e:
      msg = ('Could not complete remove / install cycle for \n' +
             PACKAGE_FULL_NAME + '.\n' + 'This error is usually ' +
             'caused by a file being open in the package ' +
             'directory which is currently:\n  ' +
             os.path.abspath(self.package_dir))
      raise IOError(msg) from e

  # Sends to the test framework output file.
  def Output(self, s):
    self.output_file.write(str(s))
    self.output_file.flush()

  # Waits for the test execution to finish by inspecting the watchdog_log_path.
  # On error, this function will throw an IOError.
  def WaitForExecutionCompletion(self, exe_name, log_path):
    if not WaitForFileToAppear(log_path, 30):
      raise IOError('----> ERROR - expected log file ' + log_path +
                    ' was not generated')

    log_file = None
    try:
      log_file = open(log_path, 'r', encoding='utf-8')  # pylint: disable=consider-using-with

      while self.UwpApi.IsExeRunning(exe_name) and not self.kill_called:
        self.Output(''.join(log_file.readlines()))
    except Exception as e:
      raise IOError(str(e)) from e

    finally:
      time.sleep(.5)  # Give time for the |log_file| to finish writing.
      if log_file:
        self.Output(''.join(log_file.readlines()))
        log_file.close()

  # Note that alias resolving is done in the launcher since it
  # appears that multiple aliases are linked to the same binary.
  # However, only the aliases with '-starboard' seem to work.
  def ResolveAliasName(self, exe_name):
    app_aliases = self.UwpApi.ResolveExeNameToAliases(PACKAGE_NAME, exe_name)
    if not app_aliases:
      return []
    # Prefer alias names that have "-starboard" in them. Other alias names
    # that map to the same binary seem not to work.
    for name in app_aliases:
      if '-starboard' in name:
        return name
    return app_aliases[0]

  def Run(self):
    crashed = False
    app_alias = self.ResolveAliasName(self.uwp_app_exe_name)
    if not app_alias:
      msg = ('ERROR - "' + self.uwp_app_exe_name + '" is not installed.\n' +
             'Installed Apps:\n' + '  ' +
             str(self.UwpApi.GetAppsExesInPackage(PACKAGE_NAME)) + '\n' +
             'Maybe you need to build and/or deploy ' + self.uwp_app_exe_name +
             ' first?')
      raise IOError(msg)

    try:
      self.UwpApi.StartAppByAliasName(app_alias, self.args)
      self.WaitForExecutionCompletion(self.uwp_app_exe_name,
                                      self.stdout_file_path)
    except KeyboardInterrupt:
      self.Output('ctrl-c captured, quitting app...')
    except IOError as io_err:
      self.Output('IOError while running ' + app_alias + ', reason: ' +
                  str(io_err))
    except KeyError as key_err:
      self.Output('Error while running app ' + app_alias + ': ' + str(key_err))
    finally:
      self.UwpApi.KillAppByExeName(self.uwp_app_exe_name)
      crashed = self.DetectAndHandleCrashDump()
    return crashed

  def Kill(self):
    self.kill_called = True
    sys.stderr.write('\n***Killing UWP Launcher***\n')
    self.UwpApi.KillAppByExeName(self.uwp_app_exe_name)

  def DeleteFiles(self, dir_path, glob_pattern):
    dump_files = GlobFiles(dir_path, glob_pattern)
    for f in dump_files:
      try:
        os.remove(f)
        self.Output('Removed ' + f)
      except Exception as err:  # pylint: disable=broad-except
        self.Output(f'Failed to remove {f} error:{err!r}')

  def DetectAndHandleCrashDump(self):
    dump_files = GlobFiles(self.uwp_app_dmp_path, self.uwp_dump_file_pattern)
    for dmp_file in dump_files:
      mini_dump_printer.PrintMiniDump(dmp_file, self.output_file)
    return len(dump_files) > 0

  def GetDeviceIp(self):
    """Gets the device IP. TODO: Implement."""
    return None

  def GetDeviceOutputPath(self):
    # TODO: Implement
    return None
