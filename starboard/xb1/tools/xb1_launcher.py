#!/usr/bin/python
#
"""This script uses the UWP Device Portal API [1] to run unit tests on the Xbox.

This module is designed to be integrated with the abstract_launcher framework.
It does the following when it is asked to run an app:
    1.  Uninstall existing appx.
    2.  Upload and install new application.
    3.  Wait for the install to finish
    4.  Run the application
    5.  Wait for the application to finish running
    6.  Fetch the content and write it to the appropriate place.

When combined with starboard/tools/example/app_launcher_client.py, this module
enables changing target names (e.g. nplb), the Xbox used for testing, and

Examples:
python starboard/tools/example/app_launcher_client.py --p xb1 -t nplb
  -c debug -d 172.31.164.179 --target_params="--gtest_filter=*SbSocket*ait*"

python starboard/tools/example/app_launcher_client.py --p xb1 -t nplb
  -c debug -d 172.31.164.179
  --target_params="--gtest_filter=*Sb*Win*"

Caveats:
  Since we cannot launch write to a location any of the user directories like
LocalCache before the application is run for the first time, we have to bake
in the commandline arguments at install time.  Thus, we have to reinstall
the package each time we want to run the binary with different arguments.

Sample output (truncated for brevity):
  Uninstalling App (if it exists).
  Uninstall successfully started
  Installing on NAVREETXB1
  Uploading 6 files in appx/...Done.
  .
  .
  Uploading 80 files in appx/content/data/icu/icudt56l/zone...Done.
  Skipping appx/microsoft.system.package.metadata since it has 0 files.
  Done uploading.
  Uploading args. [ --gtest_filter=*Sb*Win*; ]
  Uploading 1 files in appx/content/data/arguments...Done.
  Done uploading.
  Done registering.
  Installation is running..
  Installation complete.
  Starting GoogleInc.YouTube::nplb
  App started successfully.
  nplb is running.
  nplb is running.
  Fetching log..
  Found 12 installed apps.
  [1800:61781033300:INFO:application_uwp.cc(465)] Starting nplb.exe
  Note: Google Test filter = *Sb*Win*
  [==========] Running 7 tests from 4 test cases.
  [----------] Global test environment set-up.
  [----------] 2 tests from SbWindowCreateTest
  [ RUN      ] SbWindowCreateTest.SunnyDayDefault
  [       OK ] SbWindowCreateTest.SunnyDayDefault (1 ms)
  .
  .
  .
  [----------] Global test environment tear-down
  [==========] 7 tests from 4 test cases ran. (5644 ms total)
  [  PASSED  ] 7 tests.
  Killing Process (if running).
  Found 12 installed apps.
  Log file found. Deleting.

[1]
https://docs.microsoft.com/en-us/windows/uwp/debug-test-perf/device-portal-api-core
"""

from __future__ import print_function

import os
import subprocess
import sys
import time

from starboard.shared.win32 import mini_dump_printer
from starboard.tools import abstract_launcher
from starboard.tools import net_args
from starboard.tools import net_log
from starboard.xb1.tools import packager
from starboard.xb1.tools import xb1_network_api

_ARGS_DIRECTORY = 'content/data/arguments'
_STARBOARD_ARGUMENTS_FILE = 'starboard_arguments.txt'
_DEFAULT_PACKAGE_NAME = 'GoogleInc.YouTube'
_DEFAULT_APPX_NAME = 'cobalt.appx'
_DEFAULT_STAGING_APP_NAME = 'appx'
_XB1_LOG_FILE_PARAM = 'xb1_log_file'
_XB1_PORT = 11443
_XB1_NET_LOG_PORT = 49353
_XB1_NET_ARG_PORT = 49355

_PROCESS_TIMEOUT = 60 * 5.0
_PROCESS_KILL_TIMEOUT_SECONDS = 5.0

_RESTART_TIMEOUT = 180


def ToAppxFriendlyName(app_name):
  if app_name == 'cobalt':
    app_name = 'App'
  else:
    app_name = app_name.replace('_', '') + 'App'
  return app_name


def GlobalVars():
  return GlobalVars.__dict__


# First call returns True, otherwise return false.
def FirstRun():
  v = GlobalVars()
  if 'first_run' in v:
    return False
  v['first_run'] = False
  return True


def GetXb1NetworkApiFor(device_id, port):
  v = GlobalVars()
  network_key = str(device_id) + str(port)
  if network_key not in v:
    v[network_key] = xb1_network_api.Xb1NetworkApi(device_id, port)
  return v[network_key]


def TryGetAppxDirectory(target_dir):
  appx_path = os.path.join(target_dir, _DEFAULT_STAGING_APP_NAME)
  if os.path.exists(os.path.join(appx_path, 'AppxManifest.xml')):
    return appx_path
  return None


def TryGetTargetPath(appx_path, target_name):
  if not appx_path:
    return None
  target_path = os.path.join(appx_path, target_name + '.exe')
  if os.path.exists(target_path):
    return target_path
  return None


def TryGetPackagedBinary(target_path):
  path = os.path.join(target_path, 'package', _DEFAULT_APPX_NAME)
  if os.path.exists(path):
    return path
  return None


def CheckEnvironment():
  v = GlobalVars()
  if 'os_checked' not in v:
    v['os_checked'] = True
    path = os.path.abspath(__file__)
    if path.startswith('/cygdrive'):
      # os.path.abspath() does not work in cygwin, maybe other
      # path operations don't work either.
      raise OSError('\n  **** cygwin not supported ****')


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


def Str2Bool(s):
  if not s:
    return False
  s = s.lower()
  return s in ('true', '1')


class Launcher(abstract_launcher.AbstractLauncher):
  """Class for launching Cobalt/tools on Xbox."""

  def __init__(self, platform, target_name, config, device_id, **kwargs):
    """Launcher constructor.

    Args:
      platform: Platform name (e.g. xb1, or xb1-future)
      target_name: Target name (e.g. "nplb")
      config: Config name (e.g. debug, devel, qa, gold)
      device_id: IP address of the device (e.g. 172.2.3.4)
    """
    super().__init__(platform, target_name, config, device_id, **kwargs)

    CheckEnvironment()
    self._network_api = GetXb1NetworkApiFor(device_id, _XB1_PORT)
    self._network_api.SetLoggingFunction(self._Log)
    self._logfile_name = target_name + '_stdout.txt'
    # Global argument will be visible to every launched application.
    # --net_args_wait_for_connection will instruct the app to wait
    # until net_args can connect via a socket and receive the command
    # arguments.
    # --net_log_wait_for_connection will instruct the app to wait until
    # the netlog attaches.
    self._global_args = (';--net_args_wait_for_connection'
                         ';--net_log_wait_for_connection')
    # Once net_args is connected, the following _target_args will be sent
    # over the wire.
    self._target_args = ['--' + _XB1_LOG_FILE_PARAM + '=' + self._logfile_name]
    self._do_deploy = True
    self._do_restart = True
    self._do_run = True
    self._device_id = device_id
    self._platform = platform
    self._config = config

    # Note that target_params is an array of strings, not a string.
    user_params = kwargs.get('target_params', '')
    if user_params and len(user_params) > 0:
      self.InitTargetParams(','.join(user_params))

    self._package_root_dir = os.path.split(self.GetTargetPath())[0]

    # Ensure that target is built and ready to be packaged.
    self._source_appx_path = TryGetAppxDirectory(self._package_root_dir)
    self._source_exe_path = TryGetTargetPath(self._source_appx_path,
                                             target_name)
    if not self._source_exe_path:
      raise IOError(f'Target {target_name} not found in appx directory.')

  def InitTargetParams(self, user_params_str):
    args_dict = CommandArgsToDict(user_params_str)
    # If --restart is present, consume the field
    self._do_restart = Str2Bool(args_dict.pop('restart', 'True'))

    # If --deploy is present, then consume the value and
    # remove from the rest of the arguments.
    if 'deploy' in args_dict:
      val = args_dict['deploy']
      del args_dict['deploy']
      if val is not None:
        self._do_deploy = Str2Bool(val)

    # If --run is present, then consume the value and
    # remove from the rest of the arguments.
    if 'run' in args_dict:
      val = args_dict['run']
      del args_dict['run']
      if val is not None:
        self._do_run = Str2Bool(val)
    # If there are still arguments then add them to the target
    # args.
    if len(args_dict) > 0:
      args = []
      for key, val in args_dict.items():
        entry = '--' + key
        if val is not None:
          entry += '=' + val
        args.append(entry)
      self._target_args.extend(args)

  def _Log(self, s):
    try:
      self.output_file.write(s)
    except UnicodeEncodeError:
      s = s.encode('ascii', 'replace').decode()
      self.output_file.write(s)
    self.output_file.flush()

  def _LogLn(self, s):
    try:
      # Attempt to convert to a string if the type is bytes.
      s = s.decode()
    except (UnicodeDecodeError, AttributeError):
      pass
    self._Log(s + '\n')

  def InstallStarboardArgument(self, global_args):
    args_path = [self._source_appx_path]
    args_path.extend(_ARGS_DIRECTORY.split('/'))
    args_path.append(_STARBOARD_ARGUMENTS_FILE)
    args_path = os.path.join(*args_path)  # splat args_path into flat list.

    args_dir_path = os.path.dirname(args_path)
    if not os.path.exists(args_dir_path):
      os.makedirs(args_dir_path)

    if not global_args:
      if os.path.exists(args_path):
        os.remove(args_path)
        self._LogLn('Removed global command argument file: ' + args_path)
    else:
      with open(args_path, 'w', encoding='utf-8') as fd:
        fd.write(global_args)
      self._LogLn('Installed global command arguments: \"' + global_args +
                  '\" to:\n' + os.path.realpath(args_path))

  def GetSerial(self):
    try:
      poll = self._network_api.GetDeviceInfo()
      return poll['SerialNumber']
    except IOError:
      return ''

  def RestartDevkit(self):
    dev_info = self._network_api.GetDeviceInfo()
    serial = dev_info['SerialNumber']
    console_type = dev_info['ConsoleType']
    self._Log('Connected to: ')
    self._LogLn(self._network_api.ComputerName() + ' Type:' + console_type +
                ' Serial:' + serial + ', triggering restart')
    start = time.time()
    self._network_api.Restart()

    while self.GetSerial():  # Wait for at least one failed poll
      time.sleep(1)

    while self.GetSerial() != serial:
      elapsed = int(time.time() - start)
      if elapsed > _RESTART_TIMEOUT:
        raise RuntimeError('Timed out waiting for restart, '
                           f'timeout:{_RESTART_TIMEOUT}')
      self._LogLn(f'Waiting for restart, elapsed seconds:{elapsed} '
                  f'( timeout: {_RESTART_TIMEOUT} )')
      time.sleep(1)

    time.sleep(10)
    self._Log('Restart complete: ')
    self._LogLn(self._network_api.ComputerName())

  def SignIn(self):
    self._Log('Connected to: ')
    self._LogLn(self._network_api.ComputerName())

    users = self._network_api.GetXboxLiveUserInfos().get('Users', [])

    if len(users) == 0:
      raise IOError('Please add at least one user to the test accounts on\n' +
                    'the Xbox developer homescreen.')

    # If we find an existing user that is signed in, then use that.
    for user in users:
      if user.get('SignedIn', False):
        return

    # Else, sign in the first user.
    self._network_api.SetXboxLiveSignedInUserState(users[0]['EmailAddress'],
                                                   True)

  def WinAppDeployCmd(self, command):
    try:
      out = subprocess.check_output('WinAppDeployCmd ' + command + ' -ip ' +
                                    self.GetDeviceIp()).decode()
    except subprocess.CalledProcessError as e:
      self._LogLn(e.output)
      raise e

    return out

  def PackageCobalt(self):
    # Package all targets into an appx.
    package_parameters = {
        'source_dir': self.out_directory,
        'output_dir': os.path.join(self.out_directory, 'package'),
        'publisher': None,
        'product': 'youtube',
    }
    if not os.path.exists(package_parameters['output_dir']):
      os.makedirs(package_parameters['output_dir'])
    packager.Package(**package_parameters)

  def UninstallSubPackages(self):
    # Check for sub-packages left over. Force uninstall any that exist.
    uninstalled_packages = []
    packages = self._network_api.GetInstalledPackages()
    for package in packages:
      try:
        package_full_name = package['PackageFullName']
        if package_full_name.find(_DEFAULT_PACKAGE_NAME) != -1:
          if package_full_name not in uninstalled_packages:
            self._LogLn('Existing YouTube app found on device. Uninstalling: ' +
                        package_full_name)
            uninstalled_packages.append(package_full_name)
            self.WinAppDeployCmd('uninstall -package ' + package_full_name)
      except KeyError:
        # Some packages don't include all fields. Ignore those.
        pass
      except subprocess.CalledProcessError as err:
        self._LogLn(err.output)

  def Deploy(self):
    # starboard_arguments.txt is packaged with the appx. It instructs the app
    # to wait for the NetArgs thread to send command-line args via the socket.
    self.InstallStarboardArgument(self._global_args)

    self.PackageCobalt()
    appx_package_file = TryGetPackagedBinary(self._package_root_dir)
    if not appx_package_file:
      raise IOError('Packaged appx not found in package directory. Perhaps '
                    'package_cobalt script did not complete successfully.')

    existing_package = self.CheckPackageIsDeployed()
    if existing_package:
      self._LogLn('Existing YouTube app found on device. Uninstalling.')
      self.WinAppDeployCmd('uninstall -package ' + existing_package)

    self._LogLn('Deleting temporary files')
    self._network_api.ClearTempFiles()

    try:
      self._LogLn('Installing appx file ' + appx_package_file)
      self.WinAppDeployCmd('install -file ' + appx_package_file)
    except subprocess.CalledProcessError:
      # Install exited with non-zero status code, clear everything out, restart,
      # and attempt another install.
      self._LogLn('Error installing appx. Attempting a clean install...')
      self.UninstallSubPackages()
      self.RestartDevkit()
      self.WinAppDeployCmd('install -file ' + appx_package_file)

    # Cleanup starboard arguments file.
    self.InstallStarboardArgument(None)

  # Validate that app was installed correctly by checking to make sure
  # that the full package name can now be found.
  def CheckPackageIsDeployed(self):
    package_list = self.WinAppDeployCmd('list')
    package_index = package_list.find(_DEFAULT_PACKAGE_NAME)
    if package_index == -1:
      return False
    return package_list[package_index:].split('\n')[0].strip()

  def Run(self):
    # Only upload and install Appx on the first run.
    if FirstRun():
      if self._do_restart:
        self.RestartDevkit()

      if not self._network_api.IsInDevMode():
        raise IOError('\n\n**** Please set the XBOX at ' + self._device_id +
                      ' to dev mode!!!! ****\n')
      self.SignIn()
      if self._do_deploy:
        self.Deploy()
      else:
        self._LogLn('Skipping deploy step.')

      if not self.CheckPackageIsDeployed():
        raise IOError('Could not resolve ' + _DEFAULT_PACKAGE_NAME + ' to\n' +
                      'it\'s full package name after install! This means that' +
                      '\n the package is not deployed correctly!\n\n')

    if not self._do_run:
      self._LogLn('Skipping running step.')
      return 0

    try:
      self.Kill()  # Kill existing running app.

      # These threads must start before the app executes or else it is possible
      # the app will hang at _network_api.ExecuteBinary()
      self.net_args_thread = net_args.NetArgsThread(self.device_id,
                                                    _XB1_NET_ARG_PORT,
                                                    self._target_args)
      # While binary is running, extract the net log and stream it to
      # the output.
      self.net_log_thread = net_log.NetLogThread(self.device_id,
                                                 _XB1_NET_LOG_PORT)

      appx_name = ToAppxFriendlyName(self.target_name)

      self._network_api.ExecuteBinary(_DEFAULT_PACKAGE_NAME, appx_name)

      while self._network_api.IsBinaryRunning(self.target_name):
        self._Log(self.net_log_thread.GetLog())

      self._Log(self.net_log_thread.GetLog())
      self._LogLn('Program Exited...')

    except KeyboardInterrupt:
      self._LogLn('User cancelled...')
    except IOError as io_err:
      _, _, exc_tb = sys.exc_info()
      fname = os.path.split(exc_tb.tb_frame.f_code.co_filename)[1]
      msg = (f'Exception happened at {fname}({exc_tb.tb_lineno}) during '
             f'to run cycle of {self.target_name} this '
             'is common if no one is signed in to the xbox.\n'
             f'  Error: {repr(io_err)}')
      raise IOError(msg) from io_err
    finally:
      if hasattr(self, 'net_args_thread'):
        self.net_args_thread.join()
      if hasattr(self, 'net_log_thread'):
        self.net_log_thread.join()

    self.Kill()
    self._LogLn('Finished running...')
    crashed = self._DetectAndHandleAnyCrashes(self.target_name)
    return crashed

  def _DetectAndHandleAnyCrashes(self, target_name):
    crashes_detected = False
    (files,
     _) = self._network_api.ListPackageFilesAndDirs(_DEFAULT_PACKAGE_NAME, 'AC')
    for f in files:
      if f.startswith(target_name) and f.endswith('dmp'):
        self._LogLn('\n***** Application ' + target_name +
                    ' crashed! *****\nDump file: ' + f)
        crashes_detected = True
        data = self._network_api.FetchPackageFile(
            _DEFAULT_PACKAGE_NAME,
            'AC/' + f,
            raise_on_failure=True,
            is_text=False)
        # Ensure that directory exists.
        dump_dir_path = os.path.join(os.environ['LOCALAPPDATA'], 'CrashDumps',
                                     'xbox_remote')
        if not os.path.exists(dump_dir_path):
          os.makedirs(dump_dir_path)

        dump_file_path = os.path.join(dump_dir_path, f)
        # Clean previous dump file, if it exists.
        if os.path.exists(dump_file_path):
          try:
            os.remove(dump_file_path)
          except IOError as io_err:
            self._LogLn('Error, could not remove old file ' + str(io_err))

        try:
          with open(dump_file_path, 'wb') as fp:
            fp.write(data)
          self._LogLn('Copied dump to ' + dump_file_path)
          mini_dump_printer.PrintMiniDump(dump_file_path, self.output_file)
        except Exception as err:  # pylint: disable=broad-except
          self._LogLn('Failed to dump file ' + dump_file_path + ' because: ' +
                      str(err))
    return crashes_detected

  def Kill(self):
    self._Log('Killing Process (if running).\n')
    running_processes = self._network_api.GetRunningProcesses()
    exe_name = self.target_name + '.exe'
    filtered_processes = list(
        filter(lambda p: p['ImageName'] == exe_name, running_processes))
    if len(filtered_processes) == 0:
      return
    process_under_test = filtered_processes[0]
    if not process_under_test['IsRunning']:
      return
    pid = process_under_test['ProcessId']
    self._Log('Going to kill pid ' + str(pid) + '\n')

    self._network_api.KillProcess(pid)
    self._Log('Kill signal sent. Waiting for the process to die.\n')
    self._network_api.WaitForBinaryToFinishRunning(
        self.target_name, _PROCESS_KILL_TIMEOUT_SECONDS)
    self._Log('Done killing.\n')

  def GetDeviceIp(self):
    """Gets the device IP."""
    return self.device_id

  def GetDeviceOutputPath(self):
    # TODO: Implement
    return None
