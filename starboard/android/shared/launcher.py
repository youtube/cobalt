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

import hashlib
import os
import Queue
import re
import socket
import subprocess
import sys
import threading
import time

import _env  # pylint: disable=unused-import
from starboard.tools import abstract_launcher


# Content directory, relative to the executable path
# The executable path is typically in OUTDIR/lib and the content
# directory is in OUTDIR.
_DIR_SOURCE_ROOT_DIRECTORY_RELATIVE_PATH = os.path.join('content',
                                                        'dir_source_root')

# APK file to use, relative to the executable path
# The executable path is typically in OUTDIR/lib and the APK
# is in OUTDIR.
_APK_RELATIVE_PATH = 'coat-debug.apk'

_APP_PACKAGE_NAME = 'foo.cobalt.coat'

_APP_START_INTENT = 'foo.cobalt.coat/foo.cobalt.app.MainActivity'

# The name of the native code lib inside the APK.
_APK_LIB_NAME = 'libcoat.so'

# Matches dataDir in "adb shell pm dump".
_RE_APP_DATA_DIR = re.compile(r'dataDir=(.*)')

# Matches an "adb shell am monitor" error line.
_RE_ADB_AM_MONITOR_ERROR = re.compile(r'\*\* ERROR')

# Matches the prefix that logcat prepends to starboad log lines.
_RE_STARBOARD_LOGCAT_PREFIX = re.compile(r'^.* starboard: ')

# String added to queue to indicate process has exited normally
_QUEUE_CODE_EXITED = 'exited'

# String added to queue to indicate process has crashed
_QUEUE_CODE_CRASHED = 'crashed'

# Args to ***REMOVED***crow, which is started if no other device is attached.
_CROW_COMMANDLINE = ['/***REMOVED***/teams/mobile_eng_prod/crow/crow.par',
                     '--api_level', '24', '--device', 'tv', '--open_gl',
                     '--noenable_g3_monitor']

_DEV_NULL = open('/dev/null')

_RE_CHECKSUM = re.compile(r'Checksum=(.*)')


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
    result = ['adb']
    if self.device_id:
      result.append('-s')
      result.append(self.device_id)
    result += list(args)
    return result


class ExitFileWatch(object):
  """Watches for an exitcode file to be created on the target."""

  def __init__(self, adb_builder, exitcode_path, done_queue,
               return_code_queue):
    self.adb_builder = adb_builder
    self.shutdown_event = threading.Event()
    self.done_queue = done_queue
    self.return_code_queue = return_code_queue
    self.exitcode_path = exitcode_path
    self.thread = threading.Thread(target=self._Run)
    self.thread.start()

  def Shutdown(self):
    self.shutdown_event.set()

  def Restart(self):
    self.thread = threading.Thread(target=self._Run)
    self.thread.start()

  def _Run(self):
    """Continually checks if the launcher's executable has exited.

    This process can seemingly detect the existance of the exit file
    before it actually exists.  If this is the case, this class'
    Restart() function can be used to start the search process again.
    """
    while not self.shutdown_event.is_set():
      return_code = None
      try:
        p = subprocess.Popen(
            self.adb_builder.Build('shell', 'cat', self.exitcode_path),
            stdout=subprocess.PIPE,
            stderr=_DEV_NULL,
            close_fds=True)
      except IOError:
        pass
      p.wait()

      line = CleanLine(p.stdout.readline().rstrip('\n'))
      if not line:
        continue
      try:
        return_code = int(line)
      except ValueError:  # Error message was printed to stdout
        continue

      if return_code is not None:
        self.done_queue.put(_QUEUE_CODE_EXITED)
        self.return_code_queue.put(return_code)
        # This log line will wake up the main thread
        subprocess.call(
            self.adb_builder.Build('shell', 'log', '-t', 'starboard',
                                   'exitcode file created'),
            close_fds=True)
        break
      else:
        time.sleep(1)


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

    self.adb_builder = AdbCommandBuilder(self.device_id)

    self.out_directory = os.path.split(self.GetTargetPath())[0]

    self.executable_dir_path = os.path.join(self.out_directory, 'lib')

    self.apk_path = os.path.join(self.out_directory, _APK_RELATIVE_PATH)
    if not os.path.exists(self.apk_path):
      raise Exception("Can't find APK {}".format(self.apk_path))

    self.host_content_path = os.path.join(
        self.out_directory, _DIR_SOURCE_ROOT_DIRECTORY_RELATIVE_PATH)

    # This flag is set when the main Run() loop exits.  If Kill() is called
    # after this flag is set, it will not do anything.
    self.killed = threading.Event()

  def _GetAdbDevices(self):
    """Returns a list of names of connected devices, or empty list if none."""

    # Does not use the ADBCommandBuilder class because this command should be
    # run without targeting a specific device.
    p = subprocess.Popen(['adb', 'devices'], stderr=_DEV_NULL,
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

  def _LaunchCrowIfNecessary(self):
    if self.device_id:
      return

    # Note that we just leave Crow running, since we uninstall/reinstall
    # each time anyway.
    self._CheckCall(*_CROW_COMMANDLINE)

  def _IsAppInstalled(self):
    """Determines if the app is present on the target device.

    Returns:
      True if the app is installed on the device, False otherwise.
    """
    package_string = 'package:{}'.format(_APP_PACKAGE_NAME)

    p = self._PopenAdb(
        'shell', 'pm', 'list',
        'packages', _APP_PACKAGE_NAME,
        stdout=subprocess.PIPE,
        stderr=_DEV_NULL)

    p.wait()
    line = CleanLine(p.stdout.readline())
    return package_string in line

  def _GetInstalledAppDataDir(self):
    """Returns the "dataDir" for the installed android app.

    Returns:
      Location of the installed app's data.

    Raises:
      Exception: The "dataDir" is not accessible.
    """
    p = self._PopenAdb(
        'shell',
        'pm',
        'dump',
        _APP_PACKAGE_NAME,
        stdout=subprocess.PIPE,
        stderr=_DEV_NULL)

    result = None
    for line in p.stdout:
      if result is not None:
        continue
      m = re.search(_RE_APP_DATA_DIR, line)
      if m is None:
        continue
      result = m.group(1)

    if p.wait() != 0:
      raise Exception('Could not get installed APK codePath')
    return result

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

  def _CalculateAppChecksum(self):
    """Calculates a checksum using metadata from local Android app files.

    This checksum is used to verify if the application or extra data has changed
    since it was last installed on the target device.

    The metadata used to construct the hash is the name, size, and modification
    time of each file.

    Returns:
      A string representation of a checksum.
    """
    app_checksum = hashlib.sha256()

    apk_metadata = os.stat(self.apk_path)
    apk_hash_input = '{}{}{}'.format(self.apk_path,
                                     apk_metadata.st_size,
                                     apk_metadata.st_mtime)
    app_checksum.update(apk_hash_input)

    for root, _, files in os.walk(self.host_content_path):
      for content_file in files:
        file_path = os.path.join(root, content_file)
        file_metadata = os.stat(file_path)
        file_hash_input = '{}{}{}'.format(file_path, file_metadata.st_size,
                                          file_metadata.st_mtime)
        app_checksum.update(file_hash_input)

    return app_checksum.hexdigest()

  def _ReadAppChecksumFile(self, target_checksum_path):
    """Reads the checksum file on the target device.

    Args:
      target_checksum_path: The path on the target device where the checksum
        file is located.

    Returns:
      The checksum on the device, or None if there isn't one.
    """
    p = self._PopenAdb('shell', 'cat', target_checksum_path,
                       stdout=subprocess.PIPE,
                       stderr=_DEV_NULL)

    for line in p.stdout:
      cleaned_line = CleanLine(line).rstrip('\n')
      checksum_result = re.search(_RE_CHECKSUM, cleaned_line)
      if checksum_result:
        return checksum_result.group(1)

    if p.wait() != 0:
      return None

  def _AppNeedsUpdate(self, local_checksum_value, target_checksum_path):
    """Determines if the application needs to be re-initialized.

    Compares the checksum values locally and on the target device.
    If there is no checksum on the device, or the checksums are different,
    then the updated apk and extra content must be pushed to the device
    before launching.

    Args:
      local_checksum_value:  Value of the checksum calculated locally.
      target_checksum_path:  Location of the checksum file on the target device.

    Returns:
      True if the application needs to be re-initialized, False if not.
    """

    app_checksum_value = self._ReadAppChecksumFile(target_checksum_path)
    if not app_checksum_value:
      return True
    else:
      return local_checksum_value != app_checksum_value

  def _PushChecksum(self, local_checksum_value, target_checksum_path):
    """Pushes the calculated checksum to the target device.

    Args:
      local_checksum_value: Value of the locally calculated checksum.
      target_checksum_path: Location to store the checksum on the target device.
    """
    checksum_str = 'Checksum={}'.format(local_checksum_value)
    self._CheckCallAdb('shell', 'echo', checksum_str, '>', target_checksum_path)

  def _SetupEnvironment(self):
    self._LaunchCrowIfNecessary()
    self._CheckCallAdb('wait-for-device')
    self._CheckCallAdb('root')
    self._CheckCallAdb('wait-for-device')
    self._Shutdown()

    # This flag is set if the app needs to be installed.
    needs_install = False

    # This flag is set if the apk or content files have changed since the last
    # install.
    needs_update = False

    if not self._IsAppInstalled():
      needs_install = True

    if needs_install:
      install_timer = Timer('initial install')
      self._CheckCallAdb('install', self.apk_path)
      install_timer.Stop()

    # Sometimes the app data directory has a carriage return attached.
    self.data_dir = self._GetInstalledAppDataDir().rstrip('\r')

    if self.data_dir is None:
      raise Exception('Could not find installed app data dir')

    self.exit_code_path = TargetOsPathJoin(self.data_dir, 'files', 'exitcode')
    self.log_file_path = TargetOsPathJoin(self.data_dir, 'files', 'log')

    android_files_path = TargetOsPathJoin(self.data_dir, 'files')
    android_content_path = TargetOsPathJoin(android_files_path,
                                            'dir_source_root')
    android_checksum_path = TargetOsPathJoin(android_files_path, 'checksum')

    # We're going to directly upload our new .so file over
    # the .so file that was originally in the APK.
    android_lib_path = TargetOsPathJoin(self.data_dir, 'lib', _APK_LIB_NAME)

    host_lib_path = os.path.join(self.executable_dir_path,
                                 'lib{}.so'.format(self.target_name))

    local_checksum = self._CalculateAppChecksum()

    needs_update = self._AppNeedsUpdate(local_checksum, android_checksum_path)

    if needs_update:
      # If the app has not already been installed this run, re-install it.
      if not needs_install:
        reinstall_timer = Timer('reinstall')
        self._CheckCallAdb('uninstall', _APP_PACKAGE_NAME)
        self._CheckCallAdb('install', self.apk_path)
        reinstall_timer.Stop()

      content_timer = Timer('pushing content files')
      self._CheckCallAdb('push', self.host_content_path, android_content_path)
      content_timer.Stop()

      # Without this, the 'files' dir won't be writable by the app
      self._CheckCallAdb('shell', 'chmod', 'a+rwx', android_files_path)

      self._PushChecksum(local_checksum, android_checksum_path)

    # Regardless of whether the apk or extra data needed to be copied, we still
    # push the shared libary containing the binary.
    so_timer = Timer('pushing .so file')
    self._CheckCallAdb('push', host_lib_path, android_lib_path)
    so_timer.Stop()

  def Run(self):

    # The return code for binaries run on Android is fetched from an exitcode
    # file on the device.  This return_code variable will be assigned the
    # value in that file, or left at 1 in the event of a crash or early exit.
    return_code = 1

    # Setup for running executable
    self._SetupEnvironment()
    self._CheckCallAdb('logcat', '-c')

    self._CheckCallAdb('shell', 'rm', '-f', self.exit_code_path)
    self._CheckCallAdb('shell', 'rm', '-f', self.log_file_path)

    done_queue = Queue.Queue()
    return_code_queue = Queue.Queue()
    am_monitor = AdbAmMonitorWatcher(self.adb_builder, done_queue)
    exit_watcher = ExitFileWatch(self.adb_builder, self.exit_code_path,
                                 done_queue, return_code_queue)

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
      args = [_APP_START_INTENT]

      extra_args = ['--android_exit_file={}'.format(self.exit_code_path),
                    '--android_log_file={}'.format(self.log_file_path)]
      extra_args += self.target_command_line_params

      extra_args_string = ','.join(extra_args)
      args = ['shell', 'am', 'start', '--esa', 'args', extra_args_string] + args

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
          # written after all of our executable's output.
          if 'exitcode file created' in line:
            return_code = return_code_queue.get_nowait()
            logcat_process.kill()
            break

    finally:
      self._Shutdown()
      self._CallAdb('forward', '--remove-all')
      am_monitor.Shutdown()
      exit_watcher.Shutdown()
      self.killed.set()
      run_timer.Stop()

    return return_code

  def _Shutdown(self):
    self._CallAdb('shell', 'am', 'force-stop', _APP_PACKAGE_NAME)

  # TODO:  Ensure this is always a clean exit on CTRL + C
  def Kill(self):
    if not self.killed.is_set():
      sys.stderr.write('***Killing Launcher***\n')
      try:
        self._CheckCallAdb('shell', 'echo', '1', '>', self.exit_code_path)
      except AttributeError:  # exit_code_path isn't defined yet, so ignore it.
        pass
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
    return socket.gethostbyname('localhost'), local_port
