#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Provides an interface to start and stop Android emulator.

Assumes system environment ANDROID_NDK_ROOT has been set.

  Emulator: The class provides the methods to launch/shutdown the emulator with
            the android virtual device named 'buildbot' .
"""

import logging
import os
import signal
import subprocess
import sys
import time

import android_commands

# adb_interface.py is under ../../third_party/android/testrunner/
sys.path.append(os.path.join(os.path.abspath(os.path.dirname(__file__)), '..',
   '..', 'third_party', 'android', 'testrunner'))
import adb_interface
import cmd_helper
import errors
import run_command

class EmulatorLaunchException(Exception):
  """Emulator failed to launch."""
  pass

def _KillAllEmulators():
  """Kill all running emulators that look like ones we started.

  There are odd 'sticky' cases where there can be no emulator process
  running but a device slot is taken.  A little bot trouble and and
  we're out of room forever.
  """
  emulators = android_commands.GetEmulators()
  if not emulators:
    return
  for emu_name in emulators:
    cmd_helper.GetCmdOutput(['adb', '-s', emu_name, 'emu', 'kill'])
  logging.info('Emulator killing is async; give a few seconds for all to die.')
  for i in range(5):
    if not android_commands.GetEmulators():
      return
    time.sleep(1)

def _GetAvailablePort():
  """Returns an available TCP port for the console."""
  used_ports = []
  emulators = android_commands.GetEmulators()
  for emulator in emulators:
    used_ports.append(emulator.split('-')[1])
  # The port must be an even number between 5554 and 5584.
  for port in range(5554, 5585, 2):
    if str(port) not in used_ports:
      return port


class Emulator(object):
  """Provides the methods to lanuch/shutdown the emulator.

  The emulator has the android virtual device named 'buildbot'.

  The emulator could use any even TCP port between 5554 and 5584 for the
  console communication, and this port will be part of the device name like
  'emulator-5554'. Assume it is always True, as the device name is the id of
  emulator managed in this class.

  Attributes:
    emulator: Path of Android's emulator tool.
    popen: Popen object of the running emulator process.
    device: Device name of this emulator.
  """

  # Signals we listen for to kill the emulator on
  _SIGNALS = (signal.SIGINT, signal.SIGHUP)

  # Time to wait for an emulator launch, in seconds.
  _EMULATOR_LAUNCH_TIMEOUT = 120

  # Timeout interval of wait-for-device command before bouncing to a a
  # process life check.
  _EMULATOR_WFD_TIMEOUT = 5

  def __init__(self):
    try:
      android_sdk_root = os.environ['ANDROID_SDK_ROOT']
    except KeyError:
      logging.critical('The ANDROID_SDK_ROOT must be set to run the test on '
                       'emulator.')
      raise
    self.emulator = os.path.join(android_sdk_root, 'tools', 'emulator')
    self.popen = None
    self.device = None

  def _DeviceName(self):
    """Return our device name."""
    port = _GetAvailablePort()
    return ('emulator-%d' % port, port)

  def Launch(self):
    """Launches the emulator and waits for package manager to startup.

    If fails, an exception will be raised.
    """
    _KillAllEmulators()  # just to be sure
    (self.device, port) = self._DeviceName()
    self.popen = subprocess.Popen(args=[
        self.emulator,
        # Speed up emulator launch by 40%.  Really.
        '-no-boot-anim',
        # The default /data size is 64M.
        # That's not enough for 4 unit test bundles and their data.
        '-partition-size', '256',
        # Use a familiar name and port.
        '-avd', 'buildbot',
        '-port', str(port)],
        stderr=subprocess.STDOUT)
    self._InstallKillHandler()
    self._ConfirmLaunch()

  def _ConfirmLaunch(self):
    """Confirm the emulator launched properly.

    Loop on a wait-for-device with a very small timeout.  On each
    timeout, check the emulator process is still alive.
    After confirming a wait-for-device can be successful, make sure
    it returns the right answer.
    """
    a = android_commands.AndroidCommands(self.device, False)
    seconds_waited = 0
    number_of_waits = 2  # Make sure we can wfd twice
    adb_cmd = "adb -s %s %s" % (self.device, 'wait-for-device')
    while seconds_waited < self._EMULATOR_LAUNCH_TIMEOUT:
      try:
        run_command.RunCommand(adb_cmd, timeout_time=self._EMULATOR_WFD_TIMEOUT,
                               retry_count=1)
        number_of_waits -= 1
        if not number_of_waits:
          break
      except errors.WaitForResponseTimedOutError as e:
        seconds_waited += self._EMULATOR_WFD_TIMEOUT
        adb_cmd = "adb -s %s %s" % (self.device, 'kill-server')
        run_command.RunCommand(adb_cmd)
      self.popen.poll()
      if self.popen.returncode != None:
        raise EmulatorLaunchException('EMULATOR DIED')
    if seconds_waited >= self._EMULATOR_LAUNCH_TIMEOUT:
      raise EmulatorLaunchException('TIMEOUT with wait-for-device')
    logging.info('Seconds waited on wait-for-device: %d', seconds_waited)
    # Now that we checked for obvious problems, wait for a boot complete.
    # Waiting for the package manager has been problematic.
    a.Adb().WaitForBootComplete()

  def Shutdown(self):
    """Shuts down the process started by launch."""
    if self.popen:
      self.popen.poll()
      if self.popen.returncode == None:
        self.popen.kill()
      self.popen = None

  def _ShutdownOnSignal(self, signum, frame):
    logging.critical('emulator _ShutdownOnSignal')
    for sig in self._SIGNALS:
      signal.signal(sig, signal.SIG_DFL)
    self.Shutdown()
    raise KeyboardInterrupt  # print a stack

  def _InstallKillHandler(self):
    """Install a handler to kill the emulator when we exit unexpectedly."""
    for sig in self._SIGNALS:
      signal.signal(sig, self._ShutdownOnSignal)

def main(argv):
  Emulator().launch()


if __name__ == '__main__':
  main(sys.argv)
