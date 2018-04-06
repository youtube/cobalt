#
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
#
"""Android implementation of Starboard launcher abstraction."""

import os
import Queue
import re
import socket
import subprocess
import sys
import threading
import time

import _env  # pylint: disable=unused-import

from starboard.android.shared import sdk_utils
from starboard.tools import abstract_launcher

_APP_PACKAGE_NAME = 'dev.cobalt.coat'

_APP_START_INTENT = 'dev.cobalt.coat/dev.cobalt.app.MainActivity'

# Matches an "adb shell am monitor" error line.
_RE_ADB_AM_MONITOR_ERROR = re.compile(r'\*\* ERROR')

# Matches the prefix that logcat prepends to starboad log lines.
_RE_STARBOARD_LOGCAT_PREFIX = re.compile(r'^.* starboard: ')

# Matches an IPv4 address with an optional port number
_RE_IP_ADDRESS = re.compile(r'^\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}(:\d+)?$')

# String added to queue to indicate process has crashed
_QUEUE_CODE_CRASHED = 'crashed'

# Args to ***REMOVED***crow, which is started if no other device is attached.
_CROW_COMMANDLINE = ['/***REMOVED***/teams/mobile_eng_prod/crow/crow.par',
                     '--api_level', '24', '--device', 'tv', '--open_gl',
                     '--noenable_g3_monitor']

_DEV_NULL = open('/dev/null')

_ADB = os.path.join(sdk_utils.GetSdkPath(), 'platform-tools', 'adb')

_RUNTIME_PERMISSIONS = [
    'android.permission.GET_ACCOUNTS',
    'android.permission.RECORD_AUDIO',
]


def TargetOsPathJoin(*path_elements):
  """os.path.join for the target (Android)."""
  return '/'.join(path_elements)


def CleanLine(line):
  """Removes trailing carriages returns from ADB output."""
  return line.replace('\r', '')


class Timer(object):
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
      sys.stderr.write('Step \"{}\" took {} seconds.\n'.format(
          self.step_name, total_time))


class AdbCommandBuilder(object):
  """Builder for 'adb' commands."""

  def __init__(self, device_id):
    self.device_id = device_id

  def Build(self, *args):
    """Builds an 'adb' commandline with the given args."""
    result = [_ADB]
    if self.device_id:
      result.append('-s')
      result.append(self.device_id)
    result += list(args)
    return result


class AdbAmMonitorWatcher(object):
  """Watches an "adb shell am monitor" process to detect crashes."""

  def __init__(self, adb_builder, done_queue):
    self.adb_builder = adb_builder
    self.process = subprocess.Popen(
        adb_builder.Build('shell', 'am', 'monitor'),
        stdout=subprocess.PIPE,
        stderr=_DEV_NULL,
        close_fds=True)
    self.thread = threading.Thread(target=self._Run)
    self.thread.start()
    self.done_queue = done_queue

  def Shutdown(self):
    self.process.kill()
    self.thread.join()

  def _Run(self):
    while True:
      line = CleanLine(self.process.stdout.readline())
      if not line:
        return
      if re.search(_RE_ADB_AM_MONITOR_ERROR, line):
        self.done_queue.put(_QUEUE_CODE_CRASHED)
        # This log line will wake up the main thread
        subprocess.call(
            self.adb_builder.Build('shell', 'log', '-t', 'starboard',
                                   'am monitor detected crash'),
            close_fds=True)


class Launcher(abstract_launcher.AbstractLauncher):
  """Run an application on Android."""

  def __init__(self, platform, target_name, config, device_id, **kwargs):

    super(Launcher, self).__init__(platform, target_name, config, device_id,
                                   **kwargs)

    if not self.device_id:
      self.device_id = self._IdentifyDevice()
    else:
      self._ConnectIfNecessary()

    self.adb_builder = AdbCommandBuilder(self.device_id)

    out_directory = os.path.split(self.GetTargetPath())[0]
    self.apk_path = os.path.join(out_directory, '{}.apk'.format(target_name))
    if not os.path.exists(self.apk_path):
      raise Exception("Can't find APK {}".format(self.apk_path))

    # This flag is set when the main Run() loop exits.  If Kill() is called
    # after this flag is set, it will not do anything.
    self.killed = threading.Event()

  def _GetAdbDevices(self):
    """Returns a list of names of connected devices, or empty list if none."""

    # Does not use the ADBCommandBuilder class because this command should be
    # run without targeting a specific device.
    p = subprocess.Popen([_ADB, 'devices'], stderr=_DEV_NULL,
                         stdout=subprocess.PIPE, close_fds=True)
    result = p.stdout.readlines()[1:-1]
    p.wait()

    names = []
    for device in result:
      name_info = device.split('\t')
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
    if not self.device_id or not re.search(_RE_IP_ADDRESS, self.device_id):
      return
    for device in self._GetAdbDevices():
      # Devices returned by _GetAdbDevices might include port number, so cannot
      # simply check if self.device_id is in the returned list.
      if self.device_id in device:
        return

    # Device isn't connected. Run ADB connect.
    # Does not use the ADBCommandBuilder class because this command should be
    # run without targeting a specific device.
    p = subprocess.Popen([_ADB, 'connect', self.device_id], stderr=_DEV_NULL,
                         stdout=subprocess.PIPE, close_fds=True)
    result = p.stdout.readlines()[0]
    p.wait()

    if 'connected to' not in result:
      sys.stderr.write('Failed to connect to {}\n'.format(self.device_id))

  def _LaunchCrowIfNecessary(self):
    if self.device_id:
      return

    # Note that we just leave Crow running, since we uninstall/reinstall
    # each time anyway.
    self._CheckCall(*_CROW_COMMANDLINE)

  def _Call(self, *args):
    sys.stderr.write('{}\n'.format(' '.join(args)))
    subprocess.call(args, stdout=_DEV_NULL, stderr=_DEV_NULL,
                    close_fds=True)

  def _CallAdb(self, *in_args):
    args = self.adb_builder.Build(*in_args)
    self._Call(*args)

  def _CheckCall(self, *args):
    sys.stderr.write('{}\n'.format(' '.join(args)))
    subprocess.check_call(args, stdout=_DEV_NULL, stderr=_DEV_NULL,
                          close_fds=True)

  def _CheckCallAdb(self, *in_args):
    args = self.adb_builder.Build(*in_args)
    self._CheckCall(*args)

  def _PopenAdb(self, *args, **kwargs):
    return subprocess.Popen(self.adb_builder.Build(*args), close_fds=True,
                            **kwargs)

  def Run(self):
    # The return code for binaries run on Android is read from a log line that
    # it emitted in android_main.cc.  This return_code variable will be assigned
    # the value read when we see that line, or left at 1 in the event of a crash
    # or early exit.
    return_code = 1

    # Setup for running executable
    self._LaunchCrowIfNecessary()
    self._CheckCallAdb('wait-for-device')
    self._Shutdown()

    # Clear logcat
    self._CheckCallAdb('logcat', '-c')

    # Install the APK.
    install_timer = Timer('install')
    self._CheckCallAdb('install', '-r', self.apk_path)
    install_timer.Stop()

    # Send the wakeup key to ensure daydream isn't running, otherwise Activity
    # Manager may get in a loop running the test over and over again.
    self._CheckCallAdb('shell', 'input', 'keyevent', 'KEY_WAKEUP')

    # Grant runtime permissions to avoid prompts during testing.
    for permission in _RUNTIME_PERMISSIONS:
      self._CheckCallAdb('shell', 'pm', 'grant', _APP_PACKAGE_NAME, permission)

    done_queue = Queue.Queue()
    am_monitor = AdbAmMonitorWatcher(self.adb_builder, done_queue)

    # Increases the size of the logcat buffer.  Without this, the log buffer
    # will not flush quickly enough and output will be cut off.
    self._CheckCallAdb('logcat', '-G', '2M')

    #  Ctrl + C will kill this process
    logcat_process = self._PopenAdb(
        'logcat', '-s', 'starboard:*', stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT)

    # Actually running executable
    run_timer = Timer('running executable')
    try:
      args = ['shell', 'am', 'start']
      command_line_params = [
          '--android_log_sleep_time=1000',
          '--disable_sign_in',
      ]
      if self.target_command_line_params:
        command_line_params += self.target_command_line_params
      args += ['--esa', 'args', "'{}'".format(','.join(command_line_params))]
      args += [_APP_START_INTENT]

      self._CheckCallAdb(*args)

      while True:
        if not done_queue.empty():
          done_queue_code = done_queue.get_nowait()

          if done_queue_code == _QUEUE_CODE_CRASHED:
            self.output_file.write('***Application Crashed***\n')
            break

        # Note we cannot use "for line in logcat_process.stdout" because
        # that uses a large buffer which will cause us to deadlock.
        line = CleanLine(logcat_process.stdout.readline())
        # Some crashes are not caught by the crash monitor thread, but
        # They do produce the following string in logcat before they exit.
        if 'beginning of crash' in line:
          sys.stderr.write('***Application Crashed***\n')
          break
        if not line:  # Logcat exited
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
      self._Shutdown()
      self._CallAdb('forward', '--remove-all')
      am_monitor.Shutdown()
      self.killed.set()
      run_timer.Stop()

    return return_code

  def _Shutdown(self):
    self._CallAdb('shell', 'am', 'force-stop', _APP_PACKAGE_NAME)

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
    line = re.sub(_RE_STARBOARD_LOGCAT_PREFIX, '', line, count=1)
    self.output_file.write(line)
    self.output_file.flush()

  def GetHostAndPortGivenPort(self, port):
    forward_p = self._PopenAdb(
        'forward', 'tcp:0', 'tcp:{}'.format(port),
        stdout=subprocess.PIPE,
        stderr=_DEV_NULL)
    forward_p.wait()

    local_port = CleanLine(forward_p.stdout.readline()).rstrip('\n')
    sys.stderr.write('ADB forward local port {} '
                     '=> device port {}\n'.format(local_port, port))
    return socket.gethostbyname('localhost'), local_port
