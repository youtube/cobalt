#
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
#
"""Android implementation of Starboard launcher abstraction."""

import os
import re
import socket
import subprocess
import sys
import threading
import time
from six.moves import queue

from starboard.android.shared.sdk_utils import SDK_PATH
from starboard.tools import abstract_launcher

_APP_PACKAGE_NAME = 'dev.cobalt.coat'

_APP_START_INTENT = 'dev.cobalt.coat/dev.cobalt.app.MainActivity'

# Matches an "adb shell am monitor" error line.
_RE_ADB_AM_MONITOR_ERROR = re.compile(r'\*\* ERROR')

# String added to queue to indicate process has crashed
_QUEUE_CODE_CRASHED = 'crashed'

# How long to keep logging after a crash in order to emit the stack trace.
_CRASH_LOG_SECONDS = 1.0

_RUNTIME_PERMISSIONS = [
    'android.permission.RECORD_AUDIO',
]


def TargetOsPathJoin(*path_elements):
  """os.path.join for the target (Android)."""
  return '/'.join(path_elements)


def CleanLine(line):
  """Removes trailing carriages returns from ADB output."""
  return line.replace('\r', '')


class StepTimer(object):
  """Class for timing how long install/run steps take."""

  def __init__(self, step_name):
    self.step_name = step_name
    self.start_time = time.time()
    self.end_time = None

  def Stop(self):
    if self.start_time is None:
      sys.stderr.write('Cannot stop timer; not started\n')
    else:
      self.end_time = time.time()
      total_time = self.end_time - self.start_time
      sys.stderr.write(f'Step "{self.step_name}" took {total_time} seconds.\n')


class AdbCommandBuilder(object):
  """Builder for 'adb' commands."""

  def __init__(self, adb, device_id=None):
    self.adb = adb
    self.device_id = device_id

  def Build(self, *args):
    """Builds an 'adb' commandline with the given args."""
    result = [self.adb]
    if self.device_id:
      result.append('-s')
      result.append(self.device_id)
    result += list(args)
    return result


class AdbAmMonitorWatcher(object):
  """Watches an "adb shell am monitor" process to detect crashes."""

  def __init__(self, launcher, done_queue):
    self.launcher = launcher
    self.process = launcher._PopenAdb(
        'shell', 'am', 'monitor', stdout=subprocess.PIPE)
    if abstract_launcher.ARG_DRYRUN in launcher.launcher_args:
      self.thread = None
      return
    self.thread = threading.Thread(target=self._Run)
    self.thread.start()
    self.done_queue = done_queue

  def Shutdown(self):
    self.process.kill()
    if self.thread:
      self.thread.join()

  def _Run(self):
    while True:
      line = CleanLine(self.process.stdout.readline().decode())
      if not line:
        return
      # Show the crash lines reported by "am monitor".
      sys.stderr.write(line)
      if re.search(_RE_ADB_AM_MONITOR_ERROR, line):
        self.done_queue.put(_QUEUE_CODE_CRASHED)
        # This log line will wake up the main thread
        self.launcher.CallAdb('shell', 'log', '-t', 'starboard',
                              'am monitor detected crash')


class Launcher(abstract_launcher.AbstractLauncher):
  """Run an application on Android."""

  def __init__(self, platform, target_name, config, device_id, **kwargs):

    super().__init__(platform, target_name, config, device_id, **kwargs)

    if abstract_launcher.ARG_SYSTOOLS in self.launcher_args:
      # Use default adb binary from path.
      self.adb = 'adb'
    else:
      self.adb = os.path.join(SDK_PATH, 'platform-tools', 'adb')

    self.adb_builder = AdbCommandBuilder(self.adb)

    if not self.device_id:
      self.device_id = self._IdentifyDevice()
    else:
      self._ConnectIfNecessary()

    self.adb_builder.device_id = self.device_id

    # Verify connection and dump target build fingerprint.
    self._CheckCallAdb('shell', 'getprop', 'ro.build.fingerprint')

    if abstract_launcher.ARG_NOINSTALL not in self.launcher_args:
      out_directory = os.path.split(self.GetTargetPath())[0]
      self.apk_path = os.path.join(out_directory, f'{target_name}.apk')
      if not os.path.exists(self.apk_path):
        raise ValueError(f"Can't find APK {self.apk_path}")

    # This flag is set when the main Run() loop exits.  If Kill() is called
    # after this flag is set, it will not do anything.
    self.killed = threading.Event()

    # Keep track of the port used by ADB forward in order to remove it later
    # on.
    self.local_port = None

    # Keep track of the port used by ADB reverse in order to remove it later
    # on.
    self.web_server_port = None

  def _IsValidIPv4Address(self, address):
    """Returns True if address is a valid IPv4 address, False otherwise."""
    try:
      # inet_aton throws an exception if the address is not a valid IPv4
      # address. However addresses such as '127.1' might still be considered
      # valid, hence the check for 3 '.' in the address.
      # pylint: disable=g-socket-inet-aton
      if socket.inet_aton(address) and address.count('.') == 3:
        return True
    except Exception:  # pylint: disable=broad-except
      pass
    return False

  def _GetAdbDevices(self):
    """Returns a list of names of connected devices, or empty list if none."""

    # Does not use the ADBCommandBuilder class because this command should be
    # run without targeting a specific device.
    p = self._PopenAdb('devices', stdout=subprocess.PIPE)
    result = p.stdout.readlines()[1:-1]
    p.wait()

    names = []
    for device in result:
      name_info = device.decode().split('\t')
      # Some devices may not have authorization for USB debugging.
      try:
        if 'unauthorized' not in name_info[1]:
          names.append(name_info[0])
      # Sometimes happens when device is found, even though none are connected.
      except IndexError:
        continue
    return names

  def _IdentifyDevice(self):
    """Picks a device to be used to run the executable.

    In the event that no device_id is provided, but multiple
    devices are connected, this method chooses the first device
    listed.

    Returns:
      The name of an attached device, or None if no devices are present.
    """
    device_name = None

    devices = self._GetAdbDevices()
    if devices:
      device_name = devices[0]

    return device_name

  def _ConnectIfNecessary(self):
    """Run ADB connect if needed for devices connected over IP."""
    if not self._IsValidIPv4Address(self.device_id):
      return
    for device in self._GetAdbDevices():
      # Devices returned by _GetAdbDevices might include port number, so cannot
      # simply check if self.device_id is in the returned list.
      if self.device_id in device:
        return

    # Device isn't connected. Run ADB connect.
    # Does not use the ADBCommandBuilder class because this command should be
    # run without targeting a specific device.
    p = self._PopenAdb(
        'connect',
        f'{self.device_id}:5555',
        stderr=subprocess.STDOUT,
        stdout=subprocess.PIPE,
    )
    result = p.stdout.readlines()[0]
    p.wait()

    if 'connected to' not in result:
      sys.stderr.write(f'Failed to connect to {self.device_id}\n')
      sys.stderr.write(f'connect command exited with code {p.returncode} '
                       f'and returned: {result}')

  def _Call(self, *args):
    sys.stderr.write(f"{' '.join(args)}\n")
    if abstract_launcher.ARG_DRYRUN not in self.launcher_args:
      subprocess.call(args, close_fds=True)

  def CallAdb(self, *in_args):
    args = self.adb_builder.Build(*in_args)
    self._Call(*args)

  def _CheckCall(self, *args):
    sys.stderr.write(f"{' '.join(args)}\n")
    if abstract_launcher.ARG_DRYRUN not in self.launcher_args:
      subprocess.check_call(args, close_fds=True)

  def _CheckCallAdb(self, *in_args):
    args = self.adb_builder.Build(*in_args)
    self._CheckCall(*args)

  def _PopenAdb(self, *args, **kwargs):
    build_args = self.adb_builder.Build(*args)
    sys.stderr.write(f"{' '.join(build_args)}\n")
    if abstract_launcher.ARG_DRYRUN in self.launcher_args:
      return subprocess.Popen(['echo', 'dry-run'])
    return subprocess.Popen(build_args, close_fds=True, **kwargs)

  def Run(self):
    # The return code for binaries run on Android is read from a log line that
    # it emitted in android_main.cc.  This return_code variable will be assigned
    # the value read when we see that line, or left at 1 in the event of a crash
    # or early exit.
    return_code = 1

    # Setup for running executable
    self._CheckCallAdb('wait-for-device')
    self._Shutdown()
    # TODO: Need to wait until cobalt fully shutdown. Otherwise, it may get
    # dirty logs from previous test, and logs like "***Application Stopped***"
    # will cause unexpected errors.
    # Simply wait 5s as a temporary solution.
    time.sleep(5)
    # Clear logcat
    self._CheckCallAdb('logcat', '-c')

    # Remove the old package if it exists
    self.CallAdb('uninstall', _APP_PACKAGE_NAME)

    # Install the APK, unless "noinstall" was specified.
    if abstract_launcher.ARG_NOINSTALL not in self.launcher_args:
      install_timer = StepTimer('install')
      self._CheckCallAdb('install', '-r', self.apk_path)
      install_timer.Stop()

    # Send the wakeup key to ensure daydream isn't running, otherwise Activity
    # Manager may get in a loop running the test over and over again.
    self._CheckCallAdb('shell', 'input', 'keyevent', 'KEYCODE_WAKEUP')

    # Grant runtime permissions to avoid prompts during testing.
    if abstract_launcher.ARG_NOINSTALL not in self.launcher_args:
      for permission in _RUNTIME_PERMISSIONS:
        self._CheckCallAdb('shell', 'pm', 'grant', _APP_PACKAGE_NAME,
                           permission)

    done_queue = queue.Queue()
    am_monitor = AdbAmMonitorWatcher(self, done_queue)

    # Increases the size of the logcat buffer.  Without this, the log buffer
    # will not flush quickly enough and output will be cut off.
    self._CheckCallAdb('logcat', '-G', '2M')

    #  Ctrl + C will kill this process
    logcat_process = self._PopenAdb(
        'logcat',
        '-v',
        'raw',
        '-s',
        '*:F',
        '*:E',
        'DEBUG:*',
        'System.err:*',
        'starboard:*',
        'starboard_media:*',
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT)

    # Actually running executable
    run_timer = StepTimer('running executable')
    app_crashed = False
    try:
      args = ['shell', 'am', 'start']
      command_line_params = [
          '--android_log_sleep_time=1000',
          '--disable_sign_in',
      ]
      for param in self.target_command_line_params:
        if param.startswith('--link='):
          # Android deeplinks go in the Intent data
          link = param.split('=')[1]
          args += ['-d', f"'{link}'"]
        else:
          command_line_params.append(param)
      args += ['--esa', 'args', f"'{','.join(command_line_params)}'"]
      args += [_APP_START_INTENT]

      self._CheckCallAdb(*args)

      run_loop = abstract_launcher.ARG_DRYRUN not in self.launcher_args

      while run_loop:
        if not done_queue.empty():
          done_queue_code = done_queue.get_nowait()
          if done_queue_code == _QUEUE_CODE_CRASHED:
            app_crashed = True
            threading.Timer(_CRASH_LOG_SECONDS, logcat_process.kill).start()

        # Note we cannot use "for line in logcat_process.stdout" because
        # that uses a large buffer which will cause us to deadlock.
        line = CleanLine(logcat_process.stdout.readline().decode())

        # Some crashes are not caught by the am_monitor thread, but they do
        # produce the following string in logcat before they exit.
        if 'beginning of crash' in line:
          app_crashed = True
          threading.Timer(_CRASH_LOG_SECONDS, logcat_process.kill).start()

        if not line:  # Logcat exited, or was killed
          break
        else:
          self._WriteLine(line)
          # Don't break until we see the below text in logcat, which should be
          # written when the Starboard application event loop finishes.
          if '***Application Stopped***' in line:
            try:
              return_code = int(line.split(' ')[-1])
            except ValueError:  # Error message was printed to stdout
              pass
            logcat_process.kill()
            break

    finally:
      if app_crashed:
        self._WriteLine('***Application Crashed***\n')
        # Set return code to mimic segfault code on Linux
        return_code = 11
      else:
        self._Shutdown()
      if self.local_port is not None:
        self.CallAdb('forward', '--remove', f'tcp:{self.local_port}')
      if self.web_server_port is not None:
        self.RemoveDeviceToHostTunnel(self.web_server_port)
      am_monitor.Shutdown()
      self.killed.set()
      run_timer.Stop()
      if logcat_process.poll() is None:
        # This could happen when using SIGINT to kill the launcher
        # (e.g. when using starboard/tools/example/app_launcher_client.py).
        sys.stderr.write('Logcat process is still running. Killing it now.\n')
        logcat_process.kill()

    return return_code

  def _Shutdown(self):
    self.CallAdb('shell', 'am', 'force-stop', _APP_PACKAGE_NAME)

  def SupportsDeepLink(self):
    return True

  def SendDeepLink(self, link):
    shell_cmd = f'am start -d "{link}" {_APP_START_INTENT}'
    args = ['shell', shell_cmd]
    self._CheckCallAdb(*args)
    return True

  def SupportsSystemSuspendResume(self):
    return True

  def SendSystemResume(self):
    self.CallAdb('shell', 'am', 'start', _APP_PACKAGE_NAME)
    return True

  def SendSystemSuspend(self):
    self.CallAdb('shell', 'input', 'keyevent', 'KEYCODE_HOME')
    return True

  def Kill(self):
    if not self.killed.is_set():
      sys.stderr.write('***Killing Launcher***\n')
      self._CheckCallAdb('shell', 'log', '-t', 'starboard',
                         '***Application Stopped*** 1')
      self._Shutdown()
    else:
      sys.stderr.write('Cannot kill launcher: already dead.\n')

  def _WriteLine(self, line):
    """Write log output to stdout."""
    self.output_file.write(line)
    self.output_file.flush()

  def GetHostAndPortGivenPort(self, port):
    forward_p = self._PopenAdb(
        'forward', 'tcp:0', f'tcp:{port}', stdout=subprocess.PIPE)
    forward_p.wait()

    self.local_port = CleanLine(
        forward_p.stdout.readline().decode()).rstrip('\n')
    sys.stderr.write(f'ADB forward local port {self.local_port} '
                     '=> device port {port}\n')
    # pylint: disable=g-socket-gethostbyname
    return socket.gethostbyname('localhost'), self.local_port

  def CreateDeviceToHostTunnel(self, host_port, device_port):
    self.web_server_port = host_port
    reverse_p = self._PopenAdb(
        'reverse',
        f'tcp:{device_port}',
        f'tcp:{host_port}',
        stdout=subprocess.PIPE)
    reverse_p.wait()
    sys.stderr.write(f'ADB reverse host port {host_port} '
                     f'=> device port {device_port}\n')
    return True

  def RemoveDeviceToHostTunnel(self, host_port):
    reverse_p = self._PopenAdb(
        'reverse --remove ', f'tcp:{host_port}', stdout=subprocess.PIPE)
    reverse_p.wait()
    sys.stderr.write(f'ADB reverse --remove tcp:{host_port}\n')

  def GetDeviceIp(self):
    """Gets the device IP. TODO: Implement."""
    return None

  def GetDeviceOutputPath(self):
    """Writable path where test targets can output files"""
    return f'/data/data/{_APP_PACKAGE_NAME}/cache/'
