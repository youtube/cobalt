#!/usr/bin/python
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
"""UWP API."""

from __future__ import print_function

from glob import glob
import os
import pprint
from six.moves import input
import subprocess
import sys


_PS_SCRIPT_LIST_APPS_IN_PACKAGE = \
    os.path.realpath(os.path.join(
        os.path.dirname(__file__), 'list_apps_in_uwp_package.ps1'))

_PS_SCRIPT_LIST_RUNNING_PROCESSES = \
    os.path.realpath(os.path.join(
        os.path.dirname(__file__), 'list_running_processes.ps1'))

_PS_SCRIPT_REMOVE_UWP_PACKAGE = \
    os.path.realpath(os.path.join(
        os.path.dirname(__file__), 'remove_uwp_package.ps1'))

_PS_SCRIPT_REGISTER_UWP_PACKAGE = \
    os.path.realpath(os.path.join(
        os.path.dirname(__file__), 'register_uwp_package.ps1'))

_PS_SCRIPT_LIST_UWP_PACKAGES = \
    os.path.realpath(os.path.join(
        os.path.dirname(__file__), 'list_uwp_packages.ps1'))

_PS_SCRIPT_STOP_PROCESS_BY_ID = \
    os.path.realpath(os.path.join(
        os.path.dirname(__file__), 'stop_process_by_id.ps1'))


def ExecutePowershellScript(script_file, args_str=''):
  try:
    cmd = 'powershell ' + os.path.abspath(script_file)
    if args_str:
      cmd += ' ' + args_str
    out = subprocess.check_output(cmd, shell=True, universal_newlines=True)
  except Exception as e:
    raise e
  return out


#----------- Command Generation
def GetPackageDir(package_full_name):
  """ Queries the os to get the directory for the given package name. """
  return os.path.join(
      os.path.expandvars('%LocalAppData%'), 'Packages', package_full_name)


def GetPackageAppMap(package_filter='', app_name_filter=''):
  """Returns a map of {full_package_name: { AppName0: { ...

  }, ...}} |package_filter| is an optional parameter, only packages
  containing the package_filter name will be returned.

  |app_name_filter| is an optional parameter, only app names, aliases or ids
  containing app_name_filter will be returned.
  """
  result = ExecutePowershellScript(_PS_SCRIPT_LIST_APPS_IN_PACKAGE,
                                   package_filter)
  package_map = {}
  for line in result.split('\n'):
    try:
      if not line:
        continue
      vals = line.split(':')
      if len(vals) != 5:
        continue

      # Note that valid lines start with '^' so that errors may be filtered out.
      if vals[0] != '^':
        continue
      (unused_anchor, package_name, id_, executable_name, alias_name) = vals

      if len(package_filter) > 0:
        if not package_filter in package_name:
          continue

      if len(app_name_filter) > 0:
        app_found = (app_name_filter in id_) or \
                    (app_name_filter in executable_name) or \
                    (app_name_filter in alias_name)

        if not app_found:
          continue

      app_map = package_map.setdefault(package_name, {})
      props = app_map.setdefault(id_, {})
      props.setdefault('exe', executable_name)
      props.setdefault('aliases', []).append(alias_name)
    except Exception as e:  # pylint: disable=broad-except
      print(str(e) + ' for line "' + line + '"')
  return package_map


def GetRunningProcessMap(exe_name_filter=''):
  """Returns a dictionary mapping of int(process_id) -> str(exe_name).

  Note that many process id's may have the same exe_name!
  """
  lines = ExecutePowershellScript(_PS_SCRIPT_LIST_RUNNING_PROCESSES)

  if exe_name_filter is None:
    exe_name_filter = ''
  exe_name_filter = exe_name_filter.lower()
  exe_name_filter = RemoveSuffix(exe_name_filter, '.exe')

  process_map = {}
  for line in lines.split('\n'):
    try:
      if not line:
        continue
      (pid, name) = line.split(':')
      name = name.lower()
      if exe_name_filter and name != exe_name_filter:
        continue
      process_map[int(pid)] = name
    except Exception as e:  # pylint: disable=broad-except
      print(str(e) + ' for line "' + line + '"')
  return process_map


def RemoveSuffix(name, suffix_str):
  if name is None:
    return name
  if not name.lower().endswith(suffix_str):
    return name
  return name[0:-len(suffix_str)]


#############################################################
# UwpApi acts wraps Windows10 shell commands in a friendly api.
class UwpApi:
  """UWP API class."""

  def __init__(self, logger):
    self.logger = logger
    #self.cached_installed_apps = {}

  def GetAllInstalledPackageNames(self):
    infos = self.GetInstalledPackageInfos()
    names = []
    for props in infos.values():
      try:
        names.append(props['Name'])
      except KeyError as ke:
        print(ke)
        continue
    return names

  # Resolves to the best possible package name, given the partial_package_name.
  def ResolveFullPackageName(self, partial_package_name):
    package_map = GetPackageAppMap(partial_package_name)
    best_package_match = None
    for package_name in package_map:
      if not best_package_match:
        best_package_match = package_name
      else:
        diff_curr = abs(len(package_name) - len(partial_package_name))
        diff_prev = abs(len(best_package_match) - len(partial_package_name))
        if diff_curr < diff_prev:
          # Smallest possible match allowed.
          best_package_match = package_name
    return best_package_match

  def GetRunningProcesses(self, exe_filter_name=None):
    return GetRunningProcessMap(exe_filter_name)

  def GetProcessIds(self, exe_name):
    exe_name = exe_name.lower()
    exe_name = RemoveSuffix(exe_name, '.exe')
    proc_map = GetRunningProcessMap(exe_name)
    out_ids = []
    for id_, name in proc_map.items():
      if name == exe_name:
        out_ids.append(id_)
    return out_ids

  def IsExeRunning(self, exe_name):
    ids = self.GetProcessIds(exe_name)
    return len(ids) > 0

  def ResolveExeNameToAliases(self, partial_package_name, exe_name):
    packages = GetPackageAppMap(partial_package_name)

    for apps_dict in packages.values():
      for props in apps_dict.values():
        try:
          if exe_name == props.get('exe', ''):
            aliases = props.get('aliases', [])
            return aliases
        except KeyError:
          pass
    return None

  def GetAppsInPackage(self, package_name):
    package_app_map = GetPackageAppMap(package_name)
    output = []
    for apps_dict in package_app_map.values():
      for app_name in apps_dict:  # key is app name
        output.append(app_name)
    return output

  def GetAppsExesInPackage(self, package_name):
    package_app_map = GetPackageAppMap(package_name)
    output = []
    for apps_dict in package_app_map.values():
      for app_dict in apps_dict.values():  # key is app name
        try:
          output.append(app_dict['exe'])
        except KeyError:
          pass
    return output

  def GetAppAliasesInPackage(self, package_name):
    package_app_map = GetPackageAppMap(package_name)
    output = []
    for apps_dict in package_app_map.values():
      for app_name in apps_dict:  # key is app name
        try:
          aliases = apps_dict[app_name]['aliases']
          output.extend(aliases)
        except Exception:  # pylint: disable=broad-except
          pass
    return output

  def IsAppInstalled(self, package_name, id_):
    return id_ in self.GetAppsInPackage(package_name)

  def GetPackageInfo(self, package_name):
    infos = self.GetInstalledPackageInfos()
    for key, val in infos.items():
      if package_name in key:
        return val
    return None

  def GetInstalledPackageInfos(self):
    """Finds all the app packages and returns all their info.

    Doesn't include the installed app names."""

    def GetKeyVal(line):
      try:
        key, val = line.split(':', 1)
        key = key.strip()
        val = val.strip()
      except ValueError:
        return '', ''
      return key, val

    output = ExecutePowershellScript(_PS_SCRIPT_LIST_UWP_PACKAGES)
    infos = []
    current = {}

    for line in output.splitlines():
      key, val = GetKeyVal(line)
      if key == 'Name':
        if len(current) > 0:
          infos.append(current)
          current = {}
      if key and val:
        current[key] = val
    infos.append(current)

    output = {}
    for info in infos:
      output[info['PackageFullName']] = info
    return output

  # Will throw subprocess.CalledProcessError if any folders/files are
  # locked.
  def RemovePackage(self, package_name):
    package_name = '*' + package_name.replace('_', '*') + '*'
    ExecutePowershellScript(_PS_SCRIPT_REMOVE_UWP_PACKAGE, package_name)

  def InstallPackage(self, package_path):
    if not package_path:
      raise IOError('Could not find starboard directory.')
    full_path = os.path.join(package_path, 'AppxManifest.xml')
    ExecutePowershellScript(_PS_SCRIPT_REGISTER_UWP_PACKAGE, full_path)

  def StartAppByAliasName(self, app_alias_name, args):
    cmd = 'start ' + app_alias_name + ':' + ';'.join(args)
    self.Log('\nExecute command:\n  ' + cmd + '\n')
    subprocess.Popen(cmd, shell=True)  # pylint: disable=consider-using-with
    self.Log('  ...Process started\n')

  def KillAppByExeName(self, exe_name):
    for id_ in self.GetProcessIds(exe_name):
      self.KillAppByProcessId(id_)

  def KillAppByProcessId(self, proc_id):
    ExecutePowershellScript(_PS_SCRIPT_STOP_PROCESS_BY_ID, str(proc_id))

  # Private section
  # Sends to the test framework output file.
  def Log(self, s):
    if self.logger:
      self.logger.write(str(s))
      self.logger.flush()
    else:
      print(str(s), end='')

  def LogLn(self, s):
    self.Log(str(s) + '\n')


#############################################################
def InteractiveCommandLineMode():
  api = UwpApi(logger=None)  # Prints to stdout

  def PrintInstalledExecutables():
    package_name = input('Which package? ')
    package_map = GetPackageAppMap(package_name)

    pprint.pprint(package_map)

  def PrintInstalledPackages():
    v = input('Just names (y) or full package infos (n)?')
    if 'y' in v or 'Y' in v:
      package_names = api.GetAllInstalledPackageNames()
      print('Installed Packages:')
      for name in sorted(package_names, key=lambda s: s.lower()):
        print('  ' + name)
      return
    else:
      infos = api.GetInstalledPackageInfos()
      for key in sorted(infos, key=lambda s: s.lower()):
        print(key + ':')
        pprint.pprint(infos[key])
        print('')

  def PrintInspectPackage():
    package_name = input('Which package? ')
    info = api.GetPackageInfo(package_name)
    pprint.pprint(info)

  def PrintFilesInPackage():
    package_name = input('Which package? ')
    info = api.GetPackageInfo(package_name)
    if not info:
      print('ERROR could not find package name ' + package_name)
    pprint.pprint(glob(os.path.join(info['InstallLocation'], '*')))

  def InstallPackage():
    appx_dir = input('Path to appx directory: ')
    try:
      api.InstallPackage(appx_dir)
    except Exception:  # pylint: disable=broad-except
      print('Failed to install ' + appx_dir + ', maybe uninstall first?')

  def UninstallPackage():
    package_name = input('Which package: ')
    api.RemovePackage(package_name)

  def KillRunningProgramByNameOrId():
    answer = input('By (i)d or by (n)ame: ')
    if 'i' in answer:
      id_ = input('process id: ')
      api.KillAppByProcessId(id_)
    elif 'n' in answer:
      app_name = input('Exe name: ')
      api.KillAppByExeName(app_name)
    else:
      print('Unexpected response: ' + answer)

  def PrintRunningProcesses():
    filter_name = None
    if 'y' in input('filter by name (y/n)?: '):
      filter_name = input('exe name: ')
    running_procs = api.GetRunningProcesses(filter_name)

    if len(running_procs) < 1:
      print('No running processes found.')
    else:
      print('Found processes:')
      for key in sorted(running_procs):
        print(f'  {key}: {running_procs[key]}')

  while True:
    print('What would you like to do?')
    print('  [0] Show all installed packages.')
    print('  [1] See all installed executables in an installed package')
    print('  [2] Inspect a package by name')
    print('  [3] List directories and files in the package')
    print('  [4] Install package')
    print('  [5] Uninstall package')
    print('  [6] Run from alias')
    print('  [7] Kill process by exe name or id')
    print('  [8] Show all running processes')
    print('  [q] Quit')
    val = input('> ')

    if val in ('', 'q'):
      print('quitting.')
      sys.exit(0)
    elif '0' == val:
      PrintInstalledPackages()
    elif '1' == val:
      PrintInstalledExecutables()
    elif '2' == val:
      PrintInspectPackage()
    elif '3' == val:
      PrintFilesInPackage()
    elif '4' == val:
      InstallPackage()
    elif '5' == val:
      UninstallPackage()
    elif '6' == val:
      alias_name = input('Which app alias: ')
      args = input('Args: ')
      api.StartAppByAliasName(alias_name, args)
    elif '7' == val:
      KillRunningProgramByNameOrId()
    elif '8' == val:
      PrintRunningProcesses()
    else:
      print('Unexpected answer: ' + val)
    input('press the return key to continue....')


if __name__ == '__main__':
  InteractiveCommandLineMode()
