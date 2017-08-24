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
"""Android implementation of Starboard launcher abstraction."""

import importlib
import os
import sys

if 'environment' in sys.modules:
  environment = sys.modules['environment']
else:
  env_path = os.path.abspath(os.path.dirname(__file__))
  if env_path not in sys.path:
    sys.path.append(env_path)
  environment = importlib.import_module('environment')

import Queue
import re
import signal
import subprocess
import threading
import time

import starboard.tools.abstract_launcher as abstract_launcher

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
                     '--', '--noenable_g3_monitor']

_DEV_NULL = open('/dev/null')


def TargetOsPathJoin(*path_elements):
  """os.path.join for the target (Android)."""
  return '/'.join(path_elements)


class AdbCommandBuilder(object):
  """Builder for 'adb' commands."""

  def __init__(self, device_id):
    if not device_id:
      self.device_id = None
    else:
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

  def __init__(self, adb_builder, exitcode_path, done_queue):
    self.adb_builder = adb_builder
    self.shutdown_event = threading.Event()
    self.done_queue = done_queue
    self.exitcode_path = exitcode_path
    self.thread = threading.Thread(target=self._Run)
    self.thread.start()

  def Shutdown(self):
    self.shutdown_event.set()

  def _Run(self):
    while not self.shutdown_event.is_set():
      p = subprocess.Popen(
          self.adb_builder.Build('shell', 'cat', self.exitcode_path),
          stdout=_DEV_NULL,
          stderr=_DEV_NULL)
      if 0 == p.wait():
        self.done_queue.put(_QUEUE_CODE_EXITED)
        # This log line will wake up the main thread
        subprocess.call(
            self.adb_builder.Build('shell', 'log', '-t', 'starboard',
                                   'exitcode file created'))
        break
      time.sleep(1)


class AdbAmMonitorWatcher(object):
  """Watches an "adb shell am monitor" process to detect crashes."""

  def __init__(self, adb_builder, done_queue):
    self.adb_builder = adb_builder
    self.process = subprocess.Popen(
        adb_builder.Build('shell', 'am', 'monitor'),
        stdout=subprocess.PIPE,
        stderr=_DEV_NULL)
    self.thread = threading.Thread(target=self._Run)
    self.thread.start()
    self.done_queue = done_queue

  def Shutdown(self):
    self.process.kill()
    self.thread.join()

  def _Run(self):
    while True:
      line = self.process.stdout.readline()
      if not line:
        return
      if re.search(_RE_ADB_AM_MONITOR_ERROR, line):
        self.done_queue.put(_QUEUE_CODE_CRASHED)
        # This log line will wake up the main thread
        subprocess.call([
            'adb', 'shell', 'log', '-t', 'starboard',
            'am monitor detected crash'
        ])


class Launcher(abstract_launcher.AbstractLauncher):
  """Run an application on Android."""

  # Do this only once per process
  initial_install = False

  def __init__(self, platform, target_name, config, device_id, args):

    super(Launcher, self).__init__(platform, target_name, config, device_id,
                                   args)

    self.adb_builder = AdbCommandBuilder(self.device_id)

    self.executable_dir_name = "{}_{}".format(self.platform, self.config)
    self.executable_out_path = os.path.abspath(
        os.path.join(os.path.dirname(__file__), os.pardir,
                     os.pardir, os.pardir, 'out', self.executable_dir_name))

    self.executable_dir_path = os.path.join(self.executable_out_path, 'lib')

    self.apk_path = os.path.join(self.executable_out_path, _APK_RELATIVE_PATH)
    if not os.path.exists(self.apk_path):
      raise Exception("Can't find APK {}".format(self.apk_path))

    self.host_content_path = os.path.join(
        self.executable_out_path, _DIR_SOURCE_ROOT_DIRECTORY_RELATIVE_PATH)

  def _GetAdbDevices(self):
    """Returns a list of devices connected, or empty list if none."""
    p = self._PopenAdb('devices', stderr=_DEV_NULL, stdout=subprocess.PIPE)
    result = p.stdout.readlines()[1:-1]
    p.wait()
    return result

  def _LaunchCrowIfNecessary(self):
    if self.device_id or self._GetAdbDevices():
      return

    # Note that we just leave Crow running, since we uninstall/reinstall
    # each time anyway.
    self._CheckCall(*_CROW_COMMANDLINE)

  def _GetInstalledAppDataDir(self):
    """Returns the "dataDir" for the installed android app."""
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

  def _CheckCall(self, *args):
    sys.stderr.write('{}'.format(' '.join(args)))
    subprocess.check_call(args, stdout=_DEV_NULL, stderr=_DEV_NULL)

  def _CheckCallAdb(self, *in_args):
    args = self.adb_builder.Build(*in_args)
    self._CheckCall(*args)

  def _PopenAdb(self, *args, **kwargs):
    return subprocess.Popen(self.adb_builder.Build(*args), **kwargs)

  def _SetupEnvironment(self):
    self._LaunchCrowIfNecessary()
    self._CheckCallAdb('wait-for-device')
    self._CheckCallAdb('root')
    self._CheckCallAdb('wait-for-device')
    self.Kill()

    do_initial_install = not Launcher.initial_install

    if do_initial_install:
      self._CheckCallAdb('uninstall', _APP_PACKAGE_NAME)
      self._CheckCallAdb('install', self.apk_path)

    self.data_dir = self._GetInstalledAppDataDir()

    if self.data_dir is None:
      raise Exception('Could not find installed app data dir')

    android_files_path = TargetOsPathJoin(self.data_dir, 'files')
    android_content_path = TargetOsPathJoin(android_files_path,
                                            'dir_source_root')

    # We're going to directly upload our new .so file over
    # the .so file that was originally in the APK.
    android_lib_path = TargetOsPathJoin(self.data_dir, 'lib', _APK_LIB_NAME)

    host_lib_path = os.path.join(self.executable_dir_path,
                                 'lib{}.so'.format(self.target_name))

    self._CheckCallAdb('push', host_lib_path, android_lib_path)

    if do_initial_install:
      # TODO:  Implement checksum methods for verifying application contents.
      # If nothing has changed about the application since the last install,
      # we should not need to do this again.
      self._CheckCallAdb('push', self.host_content_path, android_content_path)
      # Without this, the 'files' dir won't be writable by the app
      self._CheckCallAdb('shell', 'chmod', 'a+rwx', android_files_path)
      Launcher.initial_install = True

  def Run(self):

    signal.signal(signal.SIGTERM, lambda signum, frame: self.Kill())
    signal.signal(signal.SIGINT, lambda signum, frame: self.Kill())

    self._SetupEnvironment()
    self._CheckCallAdb('logcat', '-c')

    logcat_process = self._PopenAdb(
        'logcat', '-s', 'starboard:*', '*:W', stdout=subprocess.PIPE)

    exit_code_path = TargetOsPathJoin(self.data_dir, 'files', 'exitcode')
    self._CheckCallAdb('shell', 'rm', '-f', exit_code_path)

    queue = Queue.Queue()
    am_monitor = AdbAmMonitorWatcher(self.adb_builder, queue)
    exit_watcher = ExitFileWatch(self.adb_builder, exit_code_path, queue)

    # The return code for binaries run on Android is fetched from an exitcode
    # file on the device.  This return_code variable will be assigned the
    # value in that file, or left at 1 in the event of a crash or early exit.
    return_code = 1

    try:
      args = [_APP_START_INTENT]

      extra_args = ['--android_exit_file={}'.format(exit_code_path)]
      extra_args += self.target_command_line_params

      extra_args_string = ','.join(extra_args)
      args = ['shell', 'am', 'start', '--esa', 'args', extra_args_string] + args

      self._CheckCallAdb(*args)
      self.time_started = time.time()

      # Note we cannot use "for line in logcat_process.stdout" because
      # that uses a large buffer which will cause us to deadlock.
      while True:
        line = logcat_process.stdout.readline()
        if not line:
          # Something caused "adb logcat" to exit.
          print '***Logcat Exited***'
          break
        if not queue.empty():
          queue_code = queue.get_nowait()

          if queue_code == _QUEUE_CODE_CRASHED:
            print '***Application Crashed***'
          else:
            p = self._PopenAdb(
                'shell', 'cat', exit_code_path, stdout=subprocess.PIPE)

            lines = p.stdout.readlines()
            p.wait()
            print 'Exiting with return code {}'.format(lines[0])
            return_code = int(lines[0])
          break

        # Trim header from log lines with the "starboard" logcat tag so
        # that downstream log processing tools can recognize them.
        # Leave the prefixes on the non-starboard log lines to help
        # identify the source later.
        line = re.sub(_RE_STARBOARD_LOGCAT_PREFIX, '', line, count=1)
        self._WriteLine(line)

      logcat_process.kill()
    finally:
      am_monitor.Shutdown()
      exit_watcher.Shutdown()
      return return_code

  # TODO:  This doesn't seem to handle CTRL + C cleanly.  Need to fix.
  def Kill(self):
    self._CheckCallAdb('shell', 'am', 'force-stop', _APP_PACKAGE_NAME)

  def _WriteLine(self, line):
    """Write log output to stdout."""
    print line
